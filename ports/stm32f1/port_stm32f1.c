#include "can_port.h"
#include "flash_port.h"
#include "clock_port.h"
#include "reset_port.h"
#include "ports.h"
#include <string.h>

#if defined(STM32F103xB) || defined(STM32F1xx) || defined(USE_HAL_DRIVER)
#include "main.h"
#endif

/* STM32F1 Clock Port */
static uint32_t f1_get_tick_ms(void) {
#if defined(STM32F1xx)
    return HAL_GetTick();
#else
    return 0U;
#endif
}

static uint64_t f1_get_tick_us(void) {
#if defined(STM32F1xx)
    return (uint64_t)HAL_GetTick() * 1000ULL;
#else
    return 0ULL;
#endif
}

static void f1_delay_ms(uint32_t ms) {
#if defined(STM32F1xx)
    HAL_Delay(ms);
#else
    (void)ms;
#endif
}

static void f1_delay_us(uint32_t us) {
#if defined(STM32F1xx)
    /* Spin loop calibrated for F103 at 72 MHz */
    volatile uint32_t count = us * 12U;
    while (count > 0U) {
        count--;
    }
#else
    (void)us;
#endif
}

static const ClockPortInterface s_f1_clock_port = {
    .get_tick_ms = f1_get_tick_ms,
    .get_tick_us = f1_get_tick_us,
    .delay_ms = f1_delay_ms,
    .delay_us = f1_delay_us,
};

const ClockPortInterface *ports_stm32f1_clock(void) {
    return &s_f1_clock_port;
}

/* STM32F1 Flash Port: 16-bit (2-byte halfword) programming granularity */
static bool f1_flash_unlock(void) {
#if defined(HAL_FLASH_MODULE_ENABLED) && defined(STM32F1xx)
    return (HAL_FLASH_Unlock() == HAL_OK);
#else
    return true;
#endif
}

static bool f1_flash_lock(void) {
#if defined(HAL_FLASH_MODULE_ENABLED) && defined(STM32F1xx)
    return (HAL_FLASH_Lock() == HAL_OK);
#else
    return true;
#endif
}

static bool f1_flash_erase_page(uint32_t page_addr) {
    (void)page_addr;
#if defined(HAL_FLASH_MODULE_ENABLED) && defined(STM32F1xx)
    FLASH_EraseInitTypeDef erase;
    uint32_t error = 0U;
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = page_addr;
    erase.NbPages = 1U;
    return (HAL_FLASHEx_Erase(&erase, &error) == HAL_OK);
#else
    return true;
#endif
}

static bool f1_flash_program(uint32_t addr, const uint8_t *data, size_t len) {
    (void)addr;
    (void)data;
    (void)len;
#if defined(HAL_FLASH_MODULE_ENABLED) && defined(STM32F1xx)
    for (size_t i = 0U; i < len; i += 2U) {
        uint16_t halfword = 0xFFFFU;
        size_t chunk = ((len - i) < 2U) ? (len - i) : 2U;
        (void)memcpy(&halfword, &data[i], chunk);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + (uint32_t)i, (uint64_t)halfword) !=
            HAL_OK) {
            return false;
        }
    }
    return true;
#else
    return true;
#endif
}

static const FlashPortInterface s_f1_flash_port = {
    .program_granule = 2U,
    .erased_byte = 0xFFU,
    .unlock = f1_flash_unlock,
    .lock = f1_flash_lock,
    .erase_sector = f1_flash_erase_page,
    .erase_range = NULL,
    .program = f1_flash_program,
    .read = NULL,
};

const FlashPortInterface *ports_stm32f1_flash(void) {
    return &s_f1_flash_port;
}
