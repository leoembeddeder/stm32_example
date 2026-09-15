/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#include "uds_iso_tp/uds_memory.h"

#include <string.h>

static const UdsMemoryManager *s_active_memory_manager = NULL;

bool uds_memory_decode_alfid(uint8_t alfid, uint8_t *addr_len, uint8_t *size_len) {
    uint8_t a = (uint8_t)(alfid & 0x0FU);
    uint8_t s = (uint8_t)((alfid >> 4U) & 0x0FU);
    if ((a < 1U) || (a > 4U) || (s < 1U) || (s > 4U)) {
        return false;
    }
    if (addr_len != NULL) {
        *addr_len = a;
    }
    if (size_len != NULL) {
        *size_len = s;
    }
    return true;
}

bool uds_memory_decode_u32(const uint8_t *buf, uint8_t len, uint32_t *val) {
    if ((buf == NULL) || (len < 1U) || (len > 4U) || (val == NULL)) {
        return false;
    }
    uint32_t result = 0U;
    for (uint8_t i = 0U; i < len; ++i) {
        result = (result << 8U) | (uint32_t)buf[i];
    }
    *val = result;
    return true;
}

static const UdsMemoryManager *resolve_manager(void *context) {
    if (context != NULL) {
        return (const UdsMemoryManager *)context;
    }
    return s_active_memory_manager;
}

static const UdsMemoryRegion *find_region(const UdsMemoryConfig *config, uint32_t address,
                                          uint32_t size) {
    if ((config == NULL) || (config->regions == NULL) || (size == 0U)) {
        return NULL;
    }
    uint64_t end_addr = (uint64_t)address + (uint64_t)size;
    if (end_addr > 0x100000000ULL) {
        return NULL;
    }
    for (size_t i = 0U; i < config->region_count; ++i) {
        const UdsMemoryRegion *reg = &config->regions[i];
        uint64_t reg_end = (uint64_t)reg->start_address + (uint64_t)reg->size;
        if ((address >= reg->start_address) && (end_addr <= reg_end)) {
            return reg;
        }
    }
    return NULL;
}

UdsCallbackResult uds_memory_read_handler(void *context, const uint8_t *request,
                                          uint16_t request_len, uint8_t *response,
                                          uint16_t *response_len, uint16_t capacity) {
    if ((request == NULL) || (response == NULL) || (response_len == NULL)) {
        return UDS_RESULT_ERROR;
    }

    /* 1. Min length check: SID (1) + ALFID (1) + min addr (1) + min size (1) = 4 */
    if (request_len < 4U) {
        return UDS_RESULT_INVALID_FORMAT;
    }

    /* 2. ALFID validation: 1 to 4 bytes for both address and length */
    uint8_t addr_len = 0U;
    uint8_t size_len = 0U;
    if (!uds_memory_decode_alfid(request[1], &addr_len, &size_len)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }

    /* 3. Total length check: strictly 2 + addr_len + size_len */
    uint16_t expected_len = (uint16_t)(2U + addr_len + size_len);
    if (request_len != expected_len) {
        return UDS_RESULT_INVALID_FORMAT;
    }

    /* 4. Decode memoryAddress and memorySize */
    uint32_t address = 0U;
    uint32_t size = 0U;
    (void)uds_memory_decode_u32(&request[2], addr_len, &address);
    (void)uds_memory_decode_u32(&request[2U + addr_len], size_len, &size);

    /* 5. memorySize check: zero is invalid */
    if (size == 0U) {
        return UDS_RESULT_OUT_OF_RANGE;
    }

    const UdsMemoryManager *mgr = resolve_manager(context);
    if ((mgr == NULL) || (mgr->config.regions == NULL)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }

    uint32_t max_read = (mgr->config.max_read_size > 0U) ? mgr->config.max_read_size : 0xFFFFFFFFU;
    if (size > max_read) {
        return UDS_RESULT_OUT_OF_RANGE;
    }

    /* 6. Condition check */
    if (mgr->config.condition_check != NULL) {
        if (!mgr->config.condition_check(mgr->config.user_ctx, 0x23U, address, size)) {
            return UDS_RESULT_DENIED;
        }
    }

    /* 7. Address range and permission check */
    const UdsMemoryRegion *reg = find_region(&mgr->config, address, size);
    if ((reg == NULL) || ((reg->flags & UDS_MEMORY_FLAG_READ) == 0U)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }

    /* 8. Security check */
    const UdsServer *server = uds_server_get_current();
    uint8_t security_level = (server != NULL) ? uds_server_security_level(server) : 0U;
    if (((reg->flags & UDS_MEMORY_FLAG_SECURE_READ) != 0U) && (security_level == 0U)) {
        return UDS_RESULT_SECURITY_DENIED;
    }

    /* 9. Capacity check */
    if (capacity < (uint16_t)(1U + size)) {
        return UDS_RESULT_RESPONSE_TOO_LONG;
    }

    /* 10. Execute read */
    if (reg->read != NULL) {
        UdsCallbackResult read_res = reg->read(reg->driver_ctx, address, &response[1], size);
        if (read_res != UDS_RESULT_OK) {
            return read_res;
        }
    } else {
        (void)memcpy(&response[1], (const void *)(uintptr_t)address, size);
    }

    /* 11. Format positive response */
    response[0] = 0x63U;
    *response_len = (uint16_t)(1U + size);
    return UDS_RESULT_OK;
}

UdsCallbackResult uds_memory_write_handler(void *context, const uint8_t *request,
                                           uint16_t request_len, uint8_t *response,
                                           uint16_t *response_len, uint16_t capacity) {
    if ((request == NULL) || (response == NULL) || (response_len == NULL)) {
        return UDS_RESULT_ERROR;
    }

    /* 1. Min length check: SID (1) + ALFID (1) + min addr (1) + min size (1) + min data (1) = 5 */
    if (request_len < 5U) {
        return UDS_RESULT_INVALID_FORMAT;
    }

    /* 2. ALFID validation: 1 to 4 bytes for both address and length */
    uint8_t addr_len = 0U;
    uint8_t size_len = 0U;
    if (!uds_memory_decode_alfid(request[1], &addr_len, &size_len)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }

    /* 3. Intermediate length check */
    uint16_t header_len = (uint16_t)(2U + addr_len + size_len);
    if (request_len <= header_len) {
        return UDS_RESULT_INVALID_FORMAT;
    }

    /* 4. Decode memoryAddress and memorySize */
    uint32_t address = 0U;
    uint32_t size = 0U;
    (void)uds_memory_decode_u32(&request[2], addr_len, &address);
    (void)uds_memory_decode_u32(&request[2U + addr_len], size_len, &size);

    /* 5. Exact total length check: strictly 2 + addr_len + size_len + size */
    uint32_t expected_total = (uint32_t)header_len + size;
    if ((uint32_t)request_len != expected_total) {
        return UDS_RESULT_INVALID_FORMAT;
    }

    /* 6. memorySize check: zero is invalid */
    if (size == 0U) {
        return UDS_RESULT_OUT_OF_RANGE;
    }

    const UdsMemoryManager *mgr = resolve_manager(context);
    if ((mgr == NULL) || (mgr->config.regions == NULL)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }

    uint32_t max_write =
        (mgr->config.max_write_size > 0U) ? mgr->config.max_write_size : 0xFFFFFFFFU;
    if (size > max_write) {
        return UDS_RESULT_OUT_OF_RANGE;
    }

    /* 7. Condition check */
    if (mgr->config.condition_check != NULL) {
        if (!mgr->config.condition_check(mgr->config.user_ctx, 0x3DU, address, size)) {
            return UDS_RESULT_DENIED;
        }
    }

    /* 8. Address range and permission check */
    const UdsMemoryRegion *reg = find_region(&mgr->config, address, size);
    if ((reg == NULL) || ((reg->flags & UDS_MEMORY_FLAG_WRITE) == 0U)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }

    const UdsServer *server = uds_server_get_current();
    uint8_t session = (server != NULL) ? uds_server_session(server) : UDS_SESSION_DEFAULT;
    uint8_t security_level = (server != NULL) ? uds_server_security_level(server) : 0U;

    /* Check programming session requirement for prog-only regions */
    if (((reg->flags & UDS_MEMORY_FLAG_PROG_ONLY) != 0U) && (session != UDS_SESSION_PROGRAMMING)) {
        return UDS_RESULT_DENIED;
    }

    /* 9. Security check */
    if (((reg->flags & UDS_MEMORY_FLAG_SECURE_WRITE) != 0U) && (security_level == 0U)) {
        return UDS_RESULT_SECURITY_DENIED;
    }

    /* 10. Execute write */
    const uint8_t *data = &request[header_len];
    if (reg->write != NULL) {
        UdsCallbackResult write_res = reg->write(reg->driver_ctx, address, data, size);
        if (write_res != UDS_RESULT_OK) {
            return write_res;
        }
    } else {
        (void)memcpy((void *)(uintptr_t)address, data, size);
    }

    /* 11. Format positive response: echo ALFID, address, size */
    if (capacity < header_len) {
        return UDS_RESULT_RESPONSE_TOO_LONG;
    }
    response[0] = 0x7DU;
    (void)memcpy(&response[1], &request[1], (size_t)(header_len - 1U));
    *response_len = header_len;
    return UDS_RESULT_OK;
}

UdsCallbackResult uds_memory_check_access(void *context, uint8_t sid, const uint8_t *request,
                                          uint16_t request_len) {
    (void)context;
    (void)sid;
    (void)request;
    (void)request_len;
    return UDS_RESULT_OK;
}

void uds_memory_init(UdsMemoryManager *mgr, const UdsMemoryConfig *config) {
    if ((mgr == NULL) || (config == NULL)) {
        return;
    }
    mgr->config = *config;
    mgr->backend.check_access = uds_memory_check_access;
    mgr->backend.read_memory = uds_memory_read_handler;
    mgr->backend.write_memory = uds_memory_write_handler;
    s_active_memory_manager = mgr;
}

const UdsMemoryServiceBackend *uds_memory_get_backend(const UdsMemoryManager *mgr) {
    return (mgr != NULL) ? &mgr->backend : NULL;
}
