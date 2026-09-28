#include "can_port.h"
#include "flash_port.h"
#include "clock_port.h"
#include "reset_port.h"

#if defined(STM32G474xx) || defined(STM32G4xx) || defined(USE_HAL_DRIVER)
#include "main.h"
#endif

/* STM32G4 Clock Port */
static uint32_t g4_get_tick_ms(void) {
#if defined(STM32G4xx)
    return HAL_GetTick();
#else
    return 0U;
#endif
}

static uint64_t g4_get_tick_us(void) {
#if defined(STM32G4xx)
    return (uint64_t)HAL_GetTick() * 1000ULL;
#else
    return 0ULL;
#endif
}

static void g4_delay_ms(uint32_t ms) {
#if defined(STM32G4xx)
    HAL_Delay(ms);
#else
    (void)ms;
#endif
}

static void g4_delay_us(uint32_t us) {
#if defined(STM32G4xx)
    /* Spin loop calibrated for G4 at 170 MHz */
    volatile uint32_t count = us * 28U;
    while (count > 0U) {
        count--;
    }
#else
    (void)us;
#endif
}

static const ClockPortInterface s_g4_clock_port = {
    .get_tick_ms = g4_get_tick_ms,
    .get_tick_us = g4_get_tick_us,
    .delay_ms = g4_delay_ms,
    .delay_us = g4_delay_us,
};

const ClockPortInterface *ports_stm32g4_clock(void) {
    return &s_g4_clock_port;
}

/* STM32G4 Flash Port: 64-bit (8-byte doubleword) programming granularity */
static bool g4_flash_unlock(void) {
#if defined(HAL_FLASH_MODULE_ENABLED) && defined(STM32G4xx)
    return (HAL_FLASH_Unlock() == HAL_OK);
#else
    return true;
#endif
}

static bool g4_flash_lock(void) {
#if defined(HAL_FLASH_MODULE_ENABLED) && defined(STM32G4xx)
    return (HAL_FLASH_Lock() == HAL_OK);
#else
    return true;
#endif
}

static bool g4_flash_erase_page(uint32_t page_addr) {
    (void)page_addr;
#if defined(HAL_FLASH_MODULE_ENABLED) && defined(STM32G4xx)
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

static bool g4_flash_program(uint32_t addr, const uint8_t *data, size_t len) {
    (void)addr;
    (void)data;
    (void)len;
#if defined(HAL_FLASH_MODULE_ENABLED) && defined(STM32G4xx)
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

static const FlashPortInterface s_g4_flash_port = {
    .program_granule = 8U,
    .erased_byte = 0xFFU,
    .unlock = g4_flash_unlock,
    .lock = g4_flash_lock,
    .erase_sector = g4_flash_erase_page,
    .erase_range = NULL,
    .program = g4_flash_program,
    .read = NULL,
};

const FlashPortInterface *ports_stm32g4_flash(void) {
    return &s_g4_flash_port;
}
