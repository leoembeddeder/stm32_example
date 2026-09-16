#include "uds_platform.h"

#if defined(USE_HAL_DRIVER) || defined(STM32F767xx) || defined(STM32C092xx)

uint32_t uds_platform_now_ms(void) {
    return HAL_GetTick();
}

uint32_t uds_platform_systick_val(void) {
#if defined(SysTick)
    return SysTick->VAL;
#else
    return HAL_GetTick();
#endif
}

bool uds_platform_trng_get_random(uint8_t *buffer, size_t length) {
#if defined(RNG)
    if ((buffer == NULL) || (length == 0U)) {
        return false;
    }
    size_t offset = 0U;
    while (offset < length) {
        if ((RNG->SR & 0x01U) != 0U) { /* DRDY: Data ready */
            uint32_t val = RNG->DR;
            size_t copy = ((length - offset) < 4U) ? (length - offset) : 4U;
            for (size_t i = 0U; i < copy; ++i) {
                buffer[offset + i] = (uint8_t)(val >> (i * 8U));
            }
            offset += copy;
        } else {
            break;
        }
    }
    return (offset == length);
#else
    (void)buffer;
    (void)length;
    return false;
#endif
}

void uds_platform_system_reset(uint8_t reset_type) {
    (void)reset_type;
    NVIC_SystemReset();
}

void uds_platform_error(void) {
    Error_Handler();
}

#else

static uint32_t s_mock_systick = 0x12345678UL;

uint32_t uds_platform_now_ms(void) {
    return 0U;
}

uint32_t uds_platform_systick_val(void) {
    s_mock_systick += 0x1020304U;
    return s_mock_systick;
}

bool uds_platform_trng_get_random(uint8_t *buffer, size_t length) {
    (void)buffer;
    (void)length;
    return false;
}

void uds_platform_system_reset(uint8_t reset_type) {
    (void)reset_type;
}

void uds_platform_error(void) {}

#endif
