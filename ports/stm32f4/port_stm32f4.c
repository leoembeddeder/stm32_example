#include "can_port.h"
#include "flash_port.h"
#include "clock_port.h"
#include "reset_port.h"

#if defined(STM32F407xx) || defined(STM32F4xx) || defined(USE_HAL_DRIVER)
#include "main.h"
#endif

/* STM32F4 Clock Port */
static uint32_t f4_get_tick_ms(void) {
#if defined(STM32F4xx)
    return HAL_GetTick();
#else
    return 0U;
#endif
}

static uint64_t f4_get_tick_us(void) {
#if defined(STM32F4xx)
    return (uint64_t)HAL_GetTick() * 1000ULL;
#else
    return 0ULL;
#endif
}

static void f4_delay_ms(uint32_t ms) {
#if defined(STM32F4xx)
    HAL_Delay(ms);
#else
    (void)ms;
#endif
}

static void f4_delay_us(uint32_t us) {
#if defined(STM32F4xx)
    /* Spin loop calibrated for F407 at 168 MHz */
    volatile uint32_t count = us * 28U;
    while (count > 0U) {
        count--;
    }
#else
    (void)us;
#endif
}

static const ClockPortInterface s_f4_clock_port = {
    .get_tick_ms = f4_get_tick_ms,
    .get_tick_us = f4_get_tick_us,
    .delay_ms = f4_delay_ms,
    .delay_us = f4_delay_us,
};

const ClockPortInterface *ports_stm32f4_clock(void) {
    return &s_f4_clock_port;
}

/* STM32F4 Flash Port: 32-bit (4-byte word) programming granularity */
static bool f4_flash_unlock(void) {
#if defined(HAL_FLASH_MODULE_ENABLED) && defined(STM32F4xx)
    return (HAL_FLASH_Unlock() == HAL_OK);
#else
    return true;
#endif
}

static bool f4_flash_lock(void) {
#if defined(HAL_FLASH_MODULE_ENABLED) && defined(STM32F4xx)
    return (HAL_FLASH_Lock() == HAL_OK);
#else
    return true;
#endif
}

static bool f4_flash_erase_sector(uint32_t sector_addr) {
    (void)sector_addr;
#if defined(HAL_FLASH_MODULE_ENABLED) && defined(STM32F4xx)
    FLASH_EraseInitTypeDef erase;
    uint32_t error = 0U;
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = FLASH_SECTOR_5;
    erase.NbSectors = 1U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    return (HAL_FLASHEx_Erase(&erase, &error) == HAL_OK);
#else
    return true;
#endif
}

static bool f4_flash_program(uint32_t addr, const uint8_t *data, size_t len) {
    (void)addr;
    (void)data;
    (void)len;
#if defined(HAL_FLASH_MODULE_ENABLED) && defined(STM32F4xx)
    for (size_t i = 0U; i < len; i += 4U) {
        uint32_t word = 0xFFFFFFFFUL;
        size_t chunk = ((len - i) < 4U) ? (len - i) : 4U;
        (void)memcpy(&word, &data[i], chunk);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + (uint32_t)i, (uint64_t)word) !=
            HAL_OK) {
            return false;
        }
    }
    return true;
#else
    return true;
#endif
}

static const FlashPortInterface s_f4_flash_port = {
    .program_granule = 4U,
    .erased_byte = 0xFFU,
    .unlock = f4_flash_unlock,
    .lock = f4_flash_lock,
    .erase_sector = f4_flash_erase_sector,
    .erase_range = NULL,
    .program = f4_flash_program,
    .read = NULL,
};

const FlashPortInterface *ports_stm32f4_flash(void) {
    return &s_f4_flash_port;
}
