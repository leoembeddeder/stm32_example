#include "can_port.h"
#include "flash_port.h"
#include "clock_port.h"
#include "reset_port.h"
#include "ports.h"
#include <string.h>

#if defined(STM32C092xx) || defined(STM32C0xx) || defined(USE_HAL_DRIVER)
#include "main.h"
#endif

/* STM32C0 Clock Port */
static uint32_t c0_get_tick_ms(void) {
#if defined(STM32C092xx) || defined(STM32C0xx)
    return HAL_GetTick();
#else
    return 0U;
#endif
}

static uint64_t c0_get_tick_us(void) {
#if defined(STM32C092xx) || defined(STM32C0xx)
    return (uint64_t)HAL_GetTick() * 1000ULL;
#else
    return 0ULL;
#endif
}

static void c0_delay_ms(uint32_t ms) {
#if defined(STM32C092xx) || defined(STM32C0xx)
    HAL_Delay(ms);
#else
    (void)ms;
#endif
}

static void c0_delay_us(uint32_t us) {
#if defined(STM32C092xx) || defined(STM32C0xx)
    /* Spin loop calibrated for C0 at 48 MHz */
    volatile uint32_t count = us * 8U;
    while (count > 0U) {
        count--;
    }
#else
    (void)us;
#endif
}

static const ClockPortInterface s_c0_clock_port = {
    .get_tick_ms = c0_get_tick_ms,
    .get_tick_us = c0_get_tick_us,
    .delay_ms = c0_delay_ms,
    .delay_us = c0_delay_us,
};

const ClockPortInterface *ports_stm32c0_clock(void) {
    return &s_c0_clock_port;
}

/* STM32C0 Flash Port: 64-bit (8-byte doubleword) programming granularity */
static bool c0_flash_unlock(void) {
#if defined(HAL_FLASH_MODULE_ENABLED) && (defined(STM32C092xx) || defined(STM32C0xx))
    return (HAL_FLASH_Unlock() == HAL_OK);
#else
    return true;
#endif
}

static bool c0_flash_lock(void) {
#if defined(HAL_FLASH_MODULE_ENABLED) && (defined(STM32C092xx) || defined(STM32C0xx))
    return (HAL_FLASH_Lock() == HAL_OK);
#else
    return true;
#endif
}

static bool c0_flash_erase_page(uint32_t page_addr) {
    (void)page_addr;
#if defined(HAL_FLASH_MODULE_ENABLED) && (defined(STM32C092xx) || defined(STM32C0xx))
    FLASH_EraseInitTypeDef erase;
    uint32_t error = 0U;
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Page = (page_addr - 0x08000000UL) / 2048U;
    erase.NbPages = 1U;
    return (HAL_FLASHEx_Erase(&erase, &error) == HAL_OK);
#else
    return true;
#endif
}

static bool c0_flash_program(uint32_t addr, const uint8_t *data, size_t len) {
    (void)addr;
    (void)data;
    (void)len;
#if defined(HAL_FLASH_MODULE_ENABLED) && (defined(STM32C092xx) || defined(STM32C0xx))
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    size_t i = 0U;
    while ((i + 8U) <= len) {
        uint64_t dword = 0U;
        (void)memcpy(&dword, &data[i], 8U);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + (uint32_t)i, dword) != HAL_OK) {
            return false;
        }
        i += 8U;
    }
    if (i < len) {
        uint64_t dword = 0xFFFFFFFFFFFFFFFFULL;
        (void)memcpy(&dword, &data[i], len - i);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + (uint32_t)i, dword) != HAL_OK) {
            return false;
        }
    }
    return true;
#else
    return true;
#endif
}

static const FlashPortInterface s_c0_flash_port = {
    .program_granule = 8U,
    .erased_byte = 0xFFU,
    .unlock = c0_flash_unlock,
    .lock = c0_flash_lock,
    .erase_sector = c0_flash_erase_page,
    .erase_range = NULL,
    .program = c0_flash_program,
    .read = NULL,
};

const FlashPortInterface *ports_stm32c0_flash(void) {
    return &s_c0_flash_port;
}
