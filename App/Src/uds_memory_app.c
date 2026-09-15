/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#include "uds_memory_app.h"

#include <string.h>

static uint8_t s_calibration_ram[UDS_MEMORY_APP_CALIBRATION_SIZE];
static uint8_t s_nvm_config[UDS_MEMORY_APP_NVM_CONFIG_SIZE];
static uint8_t s_flash_sim[2048];

static UdsCallbackResult calibration_read(void *driver_ctx, uint32_t address, uint8_t *data,
                                          uint32_t length) {
    (void)driver_ctx;
    uint32_t offset = address - UDS_MEMORY_APP_CALIBRATION_ADDR;
    if ((offset + length) > sizeof(s_calibration_ram)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(data, &s_calibration_ram[offset], length);
    return UDS_RESULT_OK;
}

static UdsCallbackResult calibration_write(void *driver_ctx, uint32_t address, const uint8_t *data,
                                           uint32_t length) {
    (void)driver_ctx;
    uint32_t offset = address - UDS_MEMORY_APP_CALIBRATION_ADDR;
    if ((offset + length) > sizeof(s_calibration_ram)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(&s_calibration_ram[offset], data, length);
    return UDS_RESULT_OK;
}

static UdsCallbackResult nvm_read(void *driver_ctx, uint32_t address, uint8_t *data,
                                  uint32_t length) {
    (void)driver_ctx;
    uint32_t offset = address - UDS_MEMORY_APP_NVM_CONFIG_ADDR;
    if ((offset + length) > sizeof(s_nvm_config)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(data, &s_nvm_config[offset], length);
    return UDS_RESULT_OK;
}

static UdsCallbackResult nvm_write(void *driver_ctx, uint32_t address, const uint8_t *data,
                                   uint32_t length) {
    (void)driver_ctx;
    uint32_t offset = address - UDS_MEMORY_APP_NVM_CONFIG_ADDR;
    if ((offset + length) > sizeof(s_nvm_config)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(&s_nvm_config[offset], data, length);
    return UDS_RESULT_OK;
}

static UdsCallbackResult flash_read(void *driver_ctx, uint32_t address, uint8_t *data,
                                    uint32_t length) {
    (void)driver_ctx;
    uint32_t offset = address - UDS_MEMORY_APP_FLASH_APP_ADDR;
    if ((offset + length) > sizeof(s_flash_sim)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(data, &s_flash_sim[offset], length);
    return UDS_RESULT_OK;
}

static UdsCallbackResult flash_write(void *driver_ctx, uint32_t address, const uint8_t *data,
                                     uint32_t length) {
    (void)driver_ctx;
    uint32_t offset = address - UDS_MEMORY_APP_FLASH_APP_ADDR;
    if ((offset + length) > sizeof(s_flash_sim)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(&s_flash_sim[offset], data, length);
    return UDS_RESULT_OK;
}

static const UdsMemoryRegion s_app_memory_regions[] = {
    {
        .start_address = UDS_MEMORY_APP_CALIBRATION_ADDR,
        .size = UDS_MEMORY_APP_CALIBRATION_SIZE,
        .flags = UDS_MEMORY_FLAG_READ | UDS_MEMORY_FLAG_WRITE,
        .read = calibration_read,
        .write = calibration_write,
        .driver_ctx = NULL,
    },
    {
        .start_address = UDS_MEMORY_APP_NVM_CONFIG_ADDR,
        .size = UDS_MEMORY_APP_NVM_CONFIG_SIZE,
        .flags = UDS_MEMORY_FLAG_READ | UDS_MEMORY_FLAG_WRITE | UDS_MEMORY_FLAG_SECURE_READ |
                 UDS_MEMORY_FLAG_SECURE_WRITE,
        .read = nvm_read,
        .write = nvm_write,
        .driver_ctx = NULL,
    },
    {
        .start_address = UDS_MEMORY_APP_FLASH_APP_ADDR,
        .size = sizeof(s_flash_sim),
        .flags = UDS_MEMORY_FLAG_READ | UDS_MEMORY_FLAG_WRITE | UDS_MEMORY_FLAG_PROG_ONLY |
                 UDS_MEMORY_FLAG_SECURE_WRITE,
        .read = flash_read,
        .write = flash_write,
        .driver_ctx = NULL,
    },
};

static UdsMemoryManager s_app_memory_mgr;

void uds_memory_app_init(void) {
    (void)memset(s_calibration_ram, 0x55, sizeof(s_calibration_ram));
    (void)memset(s_nvm_config, 0xAA, sizeof(s_nvm_config));
    (void)memset(s_flash_sim, 0xFF, sizeof(s_flash_sim));

    const UdsMemoryConfig config = {
        .regions = s_app_memory_regions,
        .region_count = sizeof(s_app_memory_regions) / sizeof(s_app_memory_regions[0]),
        .max_read_size = 4096U,
        .max_write_size = 4096U,
        .condition_check = NULL,
        .user_ctx = NULL,
    };
    uds_memory_init(&s_app_memory_mgr, &config);
}

const UdsMemoryServiceBackend *uds_memory_app_get_backend(void) {
    return uds_memory_get_backend(&s_app_memory_mgr);
}
