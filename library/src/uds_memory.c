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

static UdsCallbackResult parse_memory_request(const uint8_t *request, uint16_t request_len,
                                              bool is_write, uint16_t *out_header_len,
                                              uint32_t *out_address, uint32_t *out_size) {
    uint16_t min_len = is_write ? 5U : 4U;
    if (request_len < min_len) {
        return UDS_RESULT_INVALID_FORMAT;
    }

    uint8_t addr_len = 0U;
    uint8_t size_len = 0U;
    if (!uds_memory_decode_alfid(request[1], &addr_len, &size_len)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }

    uint16_t header_len = (uint16_t)(2U + addr_len + size_len);
    if (is_write) {
        if (request_len <= header_len) {
            return UDS_RESULT_INVALID_FORMAT;
        }
        (void)uds_memory_decode_u32(&request[2], addr_len, out_address);
        (void)uds_memory_decode_u32(&request[2U + addr_len], size_len, out_size);
        if (((uint32_t)header_len + *out_size) != (uint32_t)request_len) {
            return UDS_RESULT_INVALID_FORMAT;
        }
    } else {
        if (request_len != header_len) {
            return UDS_RESULT_INVALID_FORMAT;
        }
        (void)uds_memory_decode_u32(&request[2], addr_len, out_address);
        (void)uds_memory_decode_u32(&request[2U + addr_len], size_len, out_size);
    }

    if (*out_size == 0U) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    *out_header_len = header_len;
    return UDS_RESULT_OK;
}

static UdsCallbackResult validate_memory_access(const UdsMemoryManager *mgr, uint8_t sid,
                                                uint32_t address, uint32_t size, uint32_t max_size,
                                                uint32_t flag_req, uint32_t flag_sec,
                                                const UdsMemoryRegion **out_reg) {
    if ((mgr == NULL) || (mgr->config.regions == NULL)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    if ((max_size > 0U) && (size > max_size)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    if (mgr->config.condition_check != NULL) {
        if (!mgr->config.condition_check(mgr->config.user_ctx, sid, address, size)) {
            return UDS_RESULT_DENIED;
        }
    }

    const UdsMemoryRegion *reg = find_region(&mgr->config, address, size);
    if ((reg == NULL) || ((reg->flags & flag_req) == 0U)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }

    const UdsServer *server = mgr->config.server;
    uint8_t session = (server != NULL) ? uds_server_session(server) : UDS_SESSION_DEFAULT;
    uint8_t security_level = (server != NULL) ? uds_server_security_level(server) : 0U;

    if (((reg->flags & UDS_MEMORY_FLAG_PROG_ONLY) != 0U) && (session != UDS_SESSION_PROGRAMMING)) {
        return UDS_RESULT_DENIED;
    }
    if (((reg->flags & flag_sec) != 0U) && (security_level == 0U)) {
        return UDS_RESULT_SECURITY_DENIED;
    }

    *out_reg = reg;
    return UDS_RESULT_OK;
}

UdsCallbackResult uds_memory_read_handler(void *context, const uint8_t *request,
                                          uint16_t request_len, uint8_t *response,
                                          uint16_t *response_len, uint16_t capacity) {
    if ((request == NULL) || (response == NULL) || (response_len == NULL)) {
        return UDS_RESULT_ERROR;
    }

    uint16_t header_len = 0U;
    uint32_t address = 0U;
    uint32_t size = 0U;
    UdsCallbackResult parse_res =
        parse_memory_request(request, request_len, false, &header_len, &address, &size);
    if (parse_res != UDS_RESULT_OK) {
        return parse_res;
    }

    const UdsMemoryManager *mgr = resolve_manager(context);
    const UdsMemoryRegion *reg = NULL;
    UdsCallbackResult val_res = validate_memory_access(
        mgr, 0x23U, address, size, (mgr != NULL) ? mgr->config.max_read_size : 0U,
        UDS_MEMORY_FLAG_READ, UDS_MEMORY_FLAG_SECURE_READ, &reg);
    if (val_res != UDS_RESULT_OK) {
        return val_res;
    }

    if (capacity < (uint16_t)(1U + size)) {
        return UDS_RESULT_RESPONSE_TOO_LONG;
    }

    if (reg->read != NULL) {
        UdsCallbackResult read_res = reg->read(reg->driver_ctx, address, &response[1], size);
        if (read_res != UDS_RESULT_OK) {
            return read_res;
        }
    } else {
        (void)memcpy(&response[1], (const void *)(uintptr_t)address, size);
    }

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

    uint16_t header_len = 0U;
    uint32_t address = 0U;
    uint32_t size = 0U;
    UdsCallbackResult parse_res =
        parse_memory_request(request, request_len, true, &header_len, &address, &size);
    if (parse_res != UDS_RESULT_OK) {
        return parse_res;
    }

    const UdsMemoryManager *mgr = resolve_manager(context);
    const UdsMemoryRegion *reg = NULL;
    UdsCallbackResult val_res = validate_memory_access(
        mgr, 0x3DU, address, size, (mgr != NULL) ? mgr->config.max_write_size : 0U,
        UDS_MEMORY_FLAG_WRITE, UDS_MEMORY_FLAG_SECURE_WRITE, &reg);
    if (val_res != UDS_RESULT_OK) {
        return val_res;
    }

    const uint8_t *data = &request[header_len];
    if (reg->write != NULL) {
        UdsCallbackResult write_res = reg->write(reg->driver_ctx, address, data, size);
        if (write_res != UDS_RESULT_OK) {
            return write_res;
        }
    } else {
        (void)memcpy((void *)(uintptr_t)address, data, size);
    }

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

void uds_memory_set_server(UdsMemoryManager *mgr, const UdsServer *server) {
    if (mgr != NULL) {
        mgr->config.server = server;
    }
}

const UdsMemoryServiceBackend *uds_memory_get_backend(const UdsMemoryManager *mgr) {
    return (mgr != NULL) ? &mgr->backend : NULL;
}
