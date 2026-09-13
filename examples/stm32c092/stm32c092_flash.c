/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */

#include "stm32c092_flash.h"

#include <string.h>

#if defined(__has_include)
#if __has_include("stm32c0xx_hal.h")
#define HAVE_STM32C0XX_HAL 1
#include "stm32c0xx_hal.h"
#endif
#endif

#if defined(HAVE_STM32C0XX_HAL)

static int stm32c092_hal_erase(uint32_t addr) {
    (void)HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    uint32_t page_index = stm32c092_get_page(addr);
    FLASH_EraseInitTypeDef erase_init;
    memset(&erase_init, 0, sizeof(erase_init));
    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.Page = page_index;
    erase_init.NbPages = 1U;

    uint32_t page_error = 0U;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase_init, &page_error);
    (void)HAL_FLASH_Lock();

    return (status == HAL_OK) ? UDS_PARAM_OK : UDS_PARAM_ERR;
}

static int stm32c092_hal_write(uint32_t addr, const void *buf, size_t len) {
    if (buf == NULL) {
        return UDS_PARAM_INVALID_PARAM;
    }

    (void)HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    const uint8_t *data = (const uint8_t *)buf;
    size_t i = 0U;

    /* Write 64-bit doubleword chunks */
    while ((i + 8U) <= len) {
        uint64_t dword_val = 0U;
        memcpy(&dword_val, &data[i], 8U);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i, dword_val) != HAL_OK) {
            (void)HAL_FLASH_Lock();
            return UDS_PARAM_ERR;
        }
        i += 8U;
    }

    /* Write trailing bytes padded with 0xFF to 64 bits */
    if (i < len) {
        uint64_t dword_val = 0xFFFFFFFFFFFFFFFFULL;
        memcpy(&dword_val, &data[i], len - i);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i, dword_val) != HAL_OK) {
            (void)HAL_FLASH_Lock();
            return UDS_PARAM_ERR;
        }
    }

    (void)HAL_FLASH_Lock();
    return UDS_PARAM_OK;
}

static int stm32c092_hal_read(uint32_t addr, void *buf, size_t len) {
    if (buf == NULL) {
        return UDS_PARAM_INVALID_PARAM;
    }
    memcpy(buf, (const void *)(uintptr_t)addr, len);
    return UDS_PARAM_OK;
}

static uint32_t stm32c092_hal_sector_size(uint32_t addr) {
    (void)addr;
    return STM32C092_PAGE_SIZE;
}

static const UdsFlashPort s_stm32c092_flash_port = {
    .erase = stm32c092_hal_erase,
    .read = stm32c092_hal_read,
    .write = stm32c092_hal_write,
    .sector_size = stm32c092_hal_sector_size,
};

#else /* Host / mock fallback when HAL header is not in include path */

#define MOCK_SECTOR_SIZE STM32C092_PAGE_SIZE
#define MOCK_SECTOR_COUNT 4U
static uint8_t s_mock_flash_mem[MOCK_SECTOR_SIZE * MOCK_SECTOR_COUNT];

static int mock_erase(uint32_t addr) {
    uint32_t offset = (addr >= STM32C092_FLASH_BASE) ? (addr - STM32C092_FLASH_BASE) : addr;
    uint32_t page_idx = offset / MOCK_SECTOR_SIZE;
    if (page_idx >= MOCK_SECTOR_COUNT) {
        page_idx = 0U;
    }
    memset(&s_mock_flash_mem[page_idx * MOCK_SECTOR_SIZE], 0xFF, MOCK_SECTOR_SIZE);
    return UDS_PARAM_OK;
}

static int mock_write(uint32_t addr, const void *buf, size_t len) {
    if (buf == NULL) {
        return UDS_PARAM_INVALID_PARAM;
    }
    uint32_t offset = (addr >= STM32C092_FLASH_BASE) ? (addr - STM32C092_FLASH_BASE) : addr;
    if ((offset + len) > sizeof(s_mock_flash_mem)) {
        return UDS_PARAM_ERR;
    }
    const uint8_t *src = (const uint8_t *)buf;
    for (size_t i = 0U; i < len; ++i) {
        s_mock_flash_mem[offset + i] &= src[i]; /* NOR flash bit-clear semantics */
    }
    return UDS_PARAM_OK;
}

static int mock_read(uint32_t addr, void *buf, size_t len) {
    if (buf == NULL) {
        return UDS_PARAM_INVALID_PARAM;
    }
    uint32_t offset = (addr >= STM32C092_FLASH_BASE) ? (addr - STM32C092_FLASH_BASE) : addr;
    if ((offset + len) > sizeof(s_mock_flash_mem)) {
        return UDS_PARAM_ERR;
    }
    memcpy(buf, &s_mock_flash_mem[offset], len);
    return UDS_PARAM_OK;
}

static uint32_t mock_sector_size(uint32_t addr) {
    (void)addr;
    return MOCK_SECTOR_SIZE;
}

static const UdsFlashPort s_stm32c092_flash_port = {
    .erase = mock_erase,
    .read = mock_read,
    .write = mock_write,
    .sector_size = mock_sector_size,
};

#endif

const UdsFlashPort *stm32c092_flash_get_port(void) {
    return &s_stm32c092_flash_port;
}

int stm32c092_flash_init_wear_leveling(UdsParamStore *store, uint32_t flash_base,
                                       uint8_t sector_count, uint16_t data_size) {
    return uds_param_init(store, &s_stm32c092_flash_port, flash_base, sector_count, data_size);
}
