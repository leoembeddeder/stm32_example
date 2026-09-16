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

void uds_platform_system_reset(uint8_t reset_type) {
    (void)reset_type;
}

void uds_platform_error(void) {}

#endif
