/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#include "uds_iso_tp/uds.h"
#include "uds_iso_tp/uds_memory.h"
#include "uds_iso_tp/uds_services.h"
#include "uds_memory_app.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

/* Test buffer backings */
static uint8_t s_ram_4byte[8192];
static uint8_t s_ram_3byte[1024];
static uint8_t s_ram_2byte[256];
static uint8_t s_rom_buffer[1024];
static uint8_t s_secure_buffer[512];
static uint8_t s_flash_buffer[512];
static bool s_fail_flash_write = false;
static bool s_condition_allow = true;

static UdsCallbackResult test_flash_write(void *driver_ctx, uint32_t address, const uint8_t *data,
                                          uint32_t length) {
    (void)driver_ctx;
    if (s_fail_flash_write) {
        return UDS_RESULT_PROGRAMMING_FAILURE;
    }
    uint32_t offset = address - 0x08000000U;
    if ((offset + length) > sizeof(s_flash_buffer)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(&s_flash_buffer[offset], data, length);
    return UDS_RESULT_OK;
}

static UdsCallbackResult test_flash_read(void *driver_ctx, uint32_t address, uint8_t *data,
                                         uint32_t length) {
    (void)driver_ctx;
    uint32_t offset = address - 0x08000000U;
    if ((offset + length) > sizeof(s_flash_buffer)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(data, &s_flash_buffer[offset], length);
    return UDS_RESULT_OK;
}

static UdsCallbackResult test_ram_4byte_read(void *driver_ctx, uint32_t address, uint8_t *data,
                                             uint32_t length) {
    (void)driver_ctx;
    uint32_t offset = address - 0x20480000U;
    if ((offset + length) > sizeof(s_ram_4byte)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(data, &s_ram_4byte[offset], length);
    return UDS_RESULT_OK;
}

static UdsCallbackResult test_ram_4byte_write(void *driver_ctx, uint32_t address,
                                              const uint8_t *data, uint32_t length) {
    (void)driver_ctx;
    uint32_t offset = address - 0x20480000U;
    if ((offset + length) > sizeof(s_ram_4byte)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(&s_ram_4byte[offset], data, length);
    return UDS_RESULT_OK;
}

static UdsCallbackResult test_ram_3byte_read(void *driver_ctx, uint32_t address, uint8_t *data,
                                             uint32_t length) {
    (void)driver_ctx;
    uint32_t offset = address - 0x00204800U;
    if ((offset + length) > sizeof(s_ram_3byte)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(data, &s_ram_3byte[offset], length);
    return UDS_RESULT_OK;
}

static UdsCallbackResult test_ram_3byte_write(void *driver_ctx, uint32_t address,
                                              const uint8_t *data, uint32_t length) {
    (void)driver_ctx;
    uint32_t offset = address - 0x00204800U;
    if ((offset + length) > sizeof(s_ram_3byte)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(&s_ram_3byte[offset], data, length);
    return UDS_RESULT_OK;
}

static UdsCallbackResult test_ram_2byte_read(void *driver_ctx, uint32_t address, uint8_t *data,
                                             uint32_t length) {
    (void)driver_ctx;
    uint32_t offset = address - 0x00002000U;
    if ((offset + length) > sizeof(s_ram_2byte)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(data, &s_ram_2byte[offset], length);
    return UDS_RESULT_OK;
}

static UdsCallbackResult test_ram_2byte_write(void *driver_ctx, uint32_t address,
                                              const uint8_t *data, uint32_t length) {
    (void)driver_ctx;
    uint32_t offset = address - 0x00002000U;
    if ((offset + length) > sizeof(s_ram_2byte)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(&s_ram_2byte[offset], data, length);
    return UDS_RESULT_OK;
}

static UdsCallbackResult test_rom_read(void *driver_ctx, uint32_t address, uint8_t *data,
                                       uint32_t length) {
    (void)driver_ctx;
    uint32_t offset = address - 0x00004800U;
    if ((offset + length) > sizeof(s_rom_buffer)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(data, &s_rom_buffer[offset], length);
    return UDS_RESULT_OK;
}

static UdsCallbackResult test_secure_read(void *driver_ctx, uint32_t address, uint8_t *data,
                                          uint32_t length) {
    (void)driver_ctx;
    uint32_t offset = address - 0x30000000U;
    if ((offset + length) > sizeof(s_secure_buffer)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(data, &s_secure_buffer[offset], length);
    return UDS_RESULT_OK;
}

static UdsCallbackResult test_secure_write(void *driver_ctx, uint32_t address, const uint8_t *data,
                                           uint32_t length) {
    (void)driver_ctx;
    uint32_t offset = address - 0x30000000U;
    if ((offset + length) > sizeof(s_secure_buffer)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(&s_secure_buffer[offset], data, length);
    return UDS_RESULT_OK;
}

static bool test_condition_check(void *user_ctx, uint8_t sid, uint32_t address, uint32_t size) {
    (void)user_ctx;
    (void)sid;
    (void)address;
    (void)size;
    return s_condition_allow;
}

static const UdsMemoryRegion s_test_regions[] = {
    {
        /* 0x20480000 .. 0x20481FFF: R/W RAM (4-byte address) */
        .start_address = 0x20480000U,
        .size = sizeof(s_ram_4byte),
        .flags = UDS_MEMORY_FLAG_READ | UDS_MEMORY_FLAG_WRITE,
        .read = test_ram_4byte_read,
        .write = test_ram_4byte_write,
        .driver_ctx = NULL,
    },
    {
        /* 0x00204800 .. 0x00204BFF: R/W RAM (3-byte address) */
        .start_address = 0x00204800U,
        .size = sizeof(s_ram_3byte),
        .flags = UDS_MEMORY_FLAG_READ | UDS_MEMORY_FLAG_WRITE,
        .read = test_ram_3byte_read,
        .write = test_ram_3byte_write,
        .driver_ctx = NULL,
    },
    {
        /* 0x00002000 .. 0x000020FF: R/W RAM (2-byte address: 0x2048 is offset 0x48) */
        .start_address = 0x00002000U,
        .size = sizeof(s_ram_2byte),
        .flags = UDS_MEMORY_FLAG_READ | UDS_MEMORY_FLAG_WRITE,
        .read = test_ram_2byte_read,
        .write = test_ram_2byte_write,
        .driver_ctx = NULL,
    },
    {
        /* 0x00004800 .. 0x00004BFF: Read-Only ROM */
        .start_address = 0x00004800U,
        .size = sizeof(s_rom_buffer),
        .flags = UDS_MEMORY_FLAG_READ,
        .read = test_rom_read,
        .write = NULL,
        .driver_ctx = NULL,
    },
    {
        /* 0x30000000 .. 0x300001FF: Secure R/W area */
        .start_address = 0x30000000U,
        .size = sizeof(s_secure_buffer),
        .flags = UDS_MEMORY_FLAG_READ | UDS_MEMORY_FLAG_WRITE | UDS_MEMORY_FLAG_SECURE_READ |
                 UDS_MEMORY_FLAG_SECURE_WRITE,
        .read = test_secure_read,
        .write = test_secure_write,
        .driver_ctx = NULL,
    },
    {
        /* 0x08000000 .. 0x080001FF: Flash firmware (Programming session only) */
        .start_address = 0x08000000U,
        .size = sizeof(s_flash_buffer),
        .flags = UDS_MEMORY_FLAG_READ | UDS_MEMORY_FLAG_WRITE | UDS_MEMORY_FLAG_PROG_ONLY,
        .read = test_flash_read,
        .write = test_flash_write,
        .driver_ctx = NULL,
    },
};

static void test_alfid_and_u32_helpers(void) {
    uint8_t addr_len = 0U;
    uint8_t size_len = 0U;

    /* Valid ALFID cases */
    assert(uds_memory_decode_alfid(0x11U, &addr_len, &size_len));
    assert(addr_len == 1U && size_len == 1U);

    assert(uds_memory_decode_alfid(0x24U, &addr_len, &size_len));
    assert(addr_len == 4U && size_len == 2U);

    assert(uds_memory_decode_alfid(0x12U, &addr_len, &size_len));
    assert(addr_len == 2U && size_len == 1U);

    assert(uds_memory_decode_alfid(0x13U, &addr_len, &size_len));
    assert(addr_len == 3U && size_len == 1U);

    assert(uds_memory_decode_alfid(0x44U, &addr_len, &size_len));
    assert(addr_len == 4U && size_len == 4U);

    /* Invalid ALFID cases */
    assert(!uds_memory_decode_alfid(0x00U, &addr_len, &size_len));
    assert(!uds_memory_decode_alfid(0x01U, &addr_len, &size_len));
    assert(!uds_memory_decode_alfid(0x10U, &addr_len, &size_len));
    assert(!uds_memory_decode_alfid(0x51U, &addr_len, &size_len));
    assert(!uds_memory_decode_alfid(0x15U, &addr_len, &size_len));
    assert(!uds_memory_decode_alfid(0xFFU, &addr_len, &size_len));

    /* Decode u32 */
    uint32_t val = 0U;
    const uint8_t b1[] = {0x48U};
    assert(uds_memory_decode_u32(b1, 1U, &val) && (val == 0x48U));

    const uint8_t b2[] = {0x20U, 0x48U};
    assert(uds_memory_decode_u32(b2, 2U, &val) && (val == 0x2048U));

    const uint8_t b3[] = {0x20U, 0x48U, 0x13U};
    assert(uds_memory_decode_u32(b3, 3U, &val) && (val == 0x00204813U));

    const uint8_t b4[] = {0x20U, 0x48U, 0x13U, 0x92U};
    assert(uds_memory_decode_u32(b4, 4U, &val) && (val == 0x20481392U));

    assert(!uds_memory_decode_u32(NULL, 1U, &val));
    assert(!uds_memory_decode_u32(b1, 0U, &val));
    assert(!uds_memory_decode_u32(b1, 5U, &val));
    assert(!uds_memory_decode_u32(b1, 1U, NULL));
}

static void test_iso_0x23_examples_and_nrc(void) {
    UdsMemoryManager mgr;
    const UdsMemoryConfig cfg = {
        .regions = s_test_regions,
        .region_count = sizeof(s_test_regions) / sizeof(s_test_regions[0]),
        .max_read_size = 1024U,
        .max_write_size = 1024U,
        .condition_check = test_condition_check,
        .user_ctx = NULL,
    };
    uds_memory_init(&mgr, &cfg);

    UdsServiceBackends backends = {0};
    backends.memory = uds_memory_get_backend(&mgr);

    UdsCallbacks callbacks = {0};
    callbacks.service_backends = &backends;

    UdsServer server;
    uds_server_init(&server, &callbacks, NULL, 1000U);

    uint8_t response[1500];
    uint16_t resp_len = 0U;

    /* Populate test data for exact ISO Example #1 (0x20481392, 259 bytes) */
    s_ram_4byte[0x1392U] = 0x00U;
    s_ram_4byte[0x1392U + 258U] = 0x8CU;

    /* Populate test data for exact ISO Example #2 (0x4813, 5 bytes) */
    s_rom_buffer[0x13U] = 0xA1U;
    s_rom_buffer[0x14U] = 0xA2U;
    s_rom_buffer[0x15U] = 0xA3U;
    s_rom_buffer[0x16U] = 0xA4U;
    s_rom_buffer[0x17U] = 0xA5U;

    /* Switch to Extended Session (0x03) */
    assert(uds_server_request_session(&server, UDS_SESSION_EXTENDED, 1000U) == UDS_RESULT_OK);

    /* 1. ISO 14229-1:2013 Example #1: ALFID 0x24 (4-byte address 0x20481392, 2-byte size 0x0103 = 259 bytes) */
    uint8_t req_ex1[] = {0x23U, 0x24U, 0x20U, 0x48U, 0x13U, 0x92U, 0x01U, 0x03U};
    assert(uds_server_handle_addressed(&server, req_ex1, sizeof(req_ex1), response, &resp_len,
                                       sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x63U);
    assert(resp_len == (1U + 259U));
    assert(response[1] == 0x00U);
    assert(response[259] == 0x8CU);

    /* 2. ISO 14229-1:2013 Example #2: ALFID 0x12 (2-byte address 0x4813, 1-byte size 0x05 = 5 bytes) */
    uint8_t req_ex2[] = {0x23U, 0x12U, 0x48U, 0x13U, 0x05U};
    assert(uds_server_handle_addressed(&server, req_ex2, sizeof(req_ex2), response, &resp_len,
                                       sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x63U);
    assert(resp_len == 6U);
    assert(response[1] == 0xA1U);
    assert(response[2] == 0xA2U);
    assert(response[3] == 0xA3U);
    assert(response[4] == 0xA4U);
    assert(response[5] == 0xA5U);

    /* 3. NRC 0x13: min length check (< 4 bytes) */
    uint8_t req_short[] = {0x23U, 0x12U, 0x48U};
    assert(uds_server_handle_addressed(&server, req_short, sizeof(req_short), response, &resp_len,
                                       sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x23U && response[2] == 0x13U);

    /* 4. NRC 0x13: total length check mismatch (ALFID 0x12 needs 2+2+1=5 bytes, provided 6) */
    uint8_t req_len_mismatch[] = {0x23U, 0x12U, 0x48U, 0x13U, 0x05U, 0x00U};
    assert(uds_server_handle_addressed(&server, req_len_mismatch, sizeof(req_len_mismatch),
                                       response, &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x23U && response[2] == 0x13U);

    /* 5. NRC 0x31: invalid ALFID (nibble 0 or > 4) */
    uint8_t req_invalid_alfid[] = {0x23U, 0x02U, 0x48U, 0x13U};
    assert(uds_server_handle_addressed(&server, req_invalid_alfid, sizeof(req_invalid_alfid),
                                       response, &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x23U && response[2] == 0x31U);

    /* 6. NRC 0x31: memorySize == 0 */
    uint8_t req_size_zero[] = {0x23U, 0x12U, 0x48U, 0x13U, 0x00U};
    assert(uds_server_handle_addressed(&server, req_size_zero, sizeof(req_size_zero), response,
                                       &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x23U && response[2] == 0x31U);

    /* 7. NRC 0x31: memorySize > max_read_size (1024) */
    uint8_t req_size_toolarge[] = {0x23U, 0x22U, 0x48U, 0x00U, 0x05U, 0x00U};
    assert(uds_server_handle_addressed(&server, req_size_toolarge, sizeof(req_size_toolarge),
                                       response, &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x23U && response[2] == 0x31U);

    /* 8. NRC 0x31: unmapped address */
    uint8_t req_unmapped[] = {0x23U, 0x12U, 0x90U, 0x00U, 0x04U};
    assert(uds_server_handle_addressed(&server, req_unmapped, sizeof(req_unmapped), response,
                                       &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x23U && response[2] == 0x31U);

    /* 9. NRC 0x31: address overflow past 0xFFFFFFFF */
    uint8_t req_overflow[] = {0x23U, 0x14U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x02U};
    assert(uds_server_handle_addressed(&server, req_overflow, sizeof(req_overflow), response,
                                       &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x23U && response[2] == 0x31U);

    /* 10. NRC 0x22: condition check fails */
    s_condition_allow = false;
    assert(uds_server_handle_addressed(&server, req_ex2, sizeof(req_ex2), response, &resp_len,
                                       sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x23U && response[2] == 0x22U);
    s_condition_allow = true;

    /* 11. NRC 0x33: Secure read when security locked */
    uint8_t req_secure[] = {0x23U, 0x14U, 0x30U, 0x00U, 0x00U, 0x00U, 0x04U};
    assert(uds_server_handle_addressed(&server, req_secure, sizeof(req_secure), response, &resp_len,
                                       sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x23U && response[2] == 0x33U);

    /* Mock unlock security */
    server.security_level = 1U;
    s_secure_buffer[0] = 0xDEU;
    s_secure_buffer[1] = 0xADU;
    s_secure_buffer[2] = 0xBEU;
    s_secure_buffer[3] = 0xEFU;
    assert(uds_server_handle_addressed(&server, req_secure, sizeof(req_secure), response, &resp_len,
                                       sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x63U && resp_len == 5U);
    assert(response[1] == 0xDEU && response[2] == 0xADU && response[3] == 0xBEU &&
           response[4] == 0xEFU);
    server.security_level = 0U;

    /* 12. Session check: in Default Session (0x01) -> NRC 0x7E */
    assert(uds_server_request_session(&server, UDS_SESSION_DEFAULT, 1000U) == UDS_RESULT_OK);
    assert(uds_server_handle_addressed(&server, req_ex2, sizeof(req_ex2), response, &resp_len,
                                       sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x23U && response[2] == 0x7EU);

    /* 13. Functional addressing suppression */
    assert(uds_server_handle_addressed(&server, req_ex2, sizeof(req_ex2), response, &resp_len,
                                       sizeof(response), UDS_ADDRESS_FUNCTIONAL,
                                       1000U) == UDS_RESULT_NO_RESPONSE);
}

static void test_iso_0x3D_examples_and_nrc(void) {
    UdsMemoryManager mgr;
    const UdsMemoryConfig cfg = {
        .regions = s_test_regions,
        .region_count = sizeof(s_test_regions) / sizeof(s_test_regions[0]),
        .max_read_size = 1024U,
        .max_write_size = 1024U,
        .condition_check = test_condition_check,
        .user_ctx = NULL,
    };
    uds_memory_init(&mgr, &cfg);

    UdsServiceBackends backends = {0};
    backends.memory = uds_memory_get_backend(&mgr);

    UdsCallbacks callbacks = {0};
    callbacks.service_backends = &backends;

    UdsServer server;
    uds_server_init(&server, &callbacks, NULL, 1000U);

    uint8_t response[256];
    uint16_t resp_len = 0U;

    /* Switch to Extended Session */
    assert(uds_server_request_session(&server, UDS_SESSION_EXTENDED, 1000U) == UDS_RESULT_OK);

    /* 1. Exact ISO 14229-1 Example #1: 2-byte address 0x2048, 1-byte size 0x02, data [0x00, 0x8C] */
    uint8_t req_ex3d_1[] = {0x3DU, 0x12U, 0x20U, 0x48U, 0x02U, 0x00U, 0x8CU};
    assert(uds_server_handle_addressed(&server, req_ex3d_1, sizeof(req_ex3d_1), response, &resp_len,
                                       sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7DU);
    assert(resp_len == 5U);
    assert(response[1] == 0x12U);
    assert(response[2] == 0x20U && response[3] == 0x48U);
    assert(response[4] == 0x02U);
    assert(s_ram_2byte[0x48U] == 0x00U);
    assert(s_ram_2byte[0x49U] == 0x8CU);

    /* 2. Exact ISO 14229-1 Example #2: 3-byte address 0x204813, 1-byte size 0x03, data [0x00, 0x01, 0x8C] */
    uint8_t req_ex3d_2[] = {0x3DU, 0x13U, 0x20U, 0x48U, 0x13U, 0x03U, 0x00U, 0x01U, 0x8CU};
    assert(uds_server_handle_addressed(&server, req_ex3d_2, sizeof(req_ex3d_2), response, &resp_len,
                                       sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7DU);
    assert(resp_len == 6U);
    assert(response[1] == 0x13U);
    assert(response[2] == 0x20U && response[3] == 0x48U && response[4] == 0x13U);
    assert(response[5] == 0x03U);
    assert(s_ram_3byte[0x13U] == 0x00U);
    assert(s_ram_3byte[0x14U] == 0x01U);
    assert(s_ram_3byte[0x15U] == 0x8CU);

    /* 3. Exact ISO 14229-1 Example #3: 4-byte address 0x20481300, 1-byte size 0x03, data [0x00, 0x01, 0x8C] */
    uint8_t req_ex3d_3[] = {0x3DU, 0x14U, 0x20U, 0x48U, 0x13U, 0x00U, 0x03U, 0x00U, 0x01U, 0x8CU};
    assert(uds_server_handle_addressed(&server, req_ex3d_3, sizeof(req_ex3d_3), response, &resp_len,
                                       sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7DU);
    assert(resp_len == 7U);
    assert(response[1] == 0x14U);
    assert(response[2] == 0x20U && response[3] == 0x48U && response[4] == 0x13U &&
           response[5] == 0x00U);
    assert(response[6] == 0x03U);
    assert(s_ram_4byte[0x1300U] == 0x00U);
    assert(s_ram_4byte[0x1301U] == 0x01U);
    assert(s_ram_4byte[0x1302U] == 0x8CU);

    /* 4. NRC 0x13: min length check (< 5 bytes) */
    uint8_t req_short[] = {0x3DU, 0x12U, 0x20U, 0x48U};
    assert(uds_server_handle_addressed(&server, req_short, sizeof(req_short), response, &resp_len,
                                       sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x3DU && response[2] == 0x13U);

    /* 5. NRC 0x13: total length check mismatch (missing 1 data byte) */
    uint8_t req_len_mismatch[] = {0x3DU, 0x12U, 0x20U, 0x48U, 0x02U, 0xAAU};
    assert(uds_server_handle_addressed(&server, req_len_mismatch, sizeof(req_len_mismatch),
                                       response, &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x3DU && response[2] == 0x13U);

    /* 6. NRC 0x31: invalid ALFID */
    uint8_t req_invalid_alfid[] = {0x3DU, 0x51U, 0x20U, 0x01U, 0xAAU};
    assert(uds_server_handle_addressed(&server, req_invalid_alfid, sizeof(req_invalid_alfid),
                                       response, &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x3DU && response[2] == 0x31U);

    /* 7. NRC 0x31: write to read-only ROM region */
    uint8_t req_write_rom[] = {0x3DU, 0x12U, 0x48U, 0x10U, 0x01U, 0x55U};
    assert(uds_server_handle_addressed(&server, req_write_rom, sizeof(req_write_rom), response,
                                       &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x3DU && response[2] == 0x31U);

    /* 8. NRC 0x33: Secure write when locked */
    uint8_t req_sec_write[] = {0x3DU, 0x14U, 0x30U, 0x00U, 0x00U, 0x00U, 0x02U, 0x11U, 0x22U};
    assert(uds_server_handle_addressed(&server, req_sec_write, sizeof(req_sec_write), response,
                                       &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x3DU && response[2] == 0x33U);

    /* Unlock and write */
    server.security_level = 1U;
    assert(uds_server_handle_addressed(&server, req_sec_write, sizeof(req_sec_write), response,
                                       &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7DU);
    assert(s_secure_buffer[0] == 0x11U && s_secure_buffer[1] == 0x22U);
    server.security_level = 0U;

    /* 9. NRC 0x22: Write to programming-only Flash region in Extended session */
    uint8_t req_flash_write[] = {0x3DU, 0x14U, 0x08U, 0x00U, 0x00U, 0x00U, 0x02U, 0x55U, 0xAAU};
    assert(uds_server_handle_addressed(&server, req_flash_write, sizeof(req_flash_write), response,
                                       &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x3DU && response[2] == 0x22U);

    /* Switch to Programming Session */
    assert(uds_server_request_session(&server, UDS_SESSION_PROGRAMMING, 1000U) == UDS_RESULT_OK);
    assert(uds_server_handle_addressed(&server, req_flash_write, sizeof(req_flash_write), response,
                                       &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7DU);
    assert(s_flash_buffer[0] == 0x55U && s_flash_buffer[1] == 0xAAU);

    /* 10. NRC 0x72: GeneralProgrammingFailure on flash write driver failure */
    s_fail_flash_write = true;
    assert(uds_server_handle_addressed(&server, req_flash_write, sizeof(req_flash_write), response,
                                       &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x3DU && response[2] == 0x72U);
    s_fail_flash_write = false;

    /* 11. NRC 0x7E: In Default Session -> rejected */
    assert(uds_server_request_session(&server, UDS_SESSION_DEFAULT, 1000U) == UDS_RESULT_OK);
    assert(uds_server_handle_addressed(&server, req_ex3d_1, sizeof(req_ex3d_1), response, &resp_len,
                                       sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x3DU && response[2] == 0x7EU);
}

static void test_uds_memory_app_integration(void) {
    uds_memory_app_init();

    const UdsMemoryServiceBackend *backend = uds_memory_app_get_backend();
    assert(backend != NULL);
    assert(backend->read_memory != NULL);
    assert(backend->write_memory != NULL);

    UdsServiceBackends backends = {0};
    backends.memory = backend;

    UdsCallbacks callbacks = {0};
    callbacks.service_backends = &backends;

    UdsServer server;
    uds_server_init(&server, &callbacks, NULL, 1000U);

    uint8_t response[256];
    uint16_t resp_len = 0U;

    assert(uds_server_request_session(&server, UDS_SESSION_EXTENDED, 1000U) == UDS_RESULT_OK);

    /* 1. Read calibration RAM default initial pattern (0x55) */
    uint8_t req_read_cal[] = {0x23U, 0x14U, 0x20U, 0x00U, 0x40U, 0x00U, 0x04U};
    assert(uds_server_handle_addressed(&server, req_read_cal, sizeof(req_read_cal), response,
                                       &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x63U && resp_len == 5U);
    assert(response[1] == 0x55U && response[2] == 0x55U && response[3] == 0x55U &&
           response[4] == 0x55U);

    /* 2. Write new calibration values */
    uint8_t req_write_cal[] = {0x3DU, 0x14U, 0x20U, 0x00U, 0x40U, 0x00U,
                               0x04U, 0x12U, 0x34U, 0x56U, 0x78U};
    assert(uds_server_handle_addressed(&server, req_write_cal, sizeof(req_write_cal), response,
                                       &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7DU && resp_len == 7U);

    /* 3. Read back written calibration values */
    assert(uds_server_handle_addressed(&server, req_read_cal, sizeof(req_read_cal), response,
                                       &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x63U && resp_len == 5U);
    assert(response[1] == 0x12U && response[2] == 0x34U && response[3] == 0x56U &&
           response[4] == 0x78U);

    /* 4. Read NVM config when locked -> NRC 0x33 */
    uint8_t req_read_nvm[] = {0x23U, 0x14U, 0x20U, 0x00U, 0x50U, 0x00U, 0x02U};
    assert(uds_server_handle_addressed(&server, req_read_nvm, sizeof(req_read_nvm), response,
                                       &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x23U && response[2] == 0x33U);

    /* Unlock security */
    server.security_level = 1U;
    assert(uds_server_handle_addressed(&server, req_read_nvm, sizeof(req_read_nvm), response,
                                       &resp_len, sizeof(response), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x63U && resp_len == 3U);
    assert(response[1] == 0xAAU && response[2] == 0xAAU);
}

int main(void) {
    test_alfid_and_u32_helpers();
    test_iso_0x23_examples_and_nrc();
    test_iso_0x3D_examples_and_nrc();
    test_uds_memory_app_integration();
    return 0;
}
