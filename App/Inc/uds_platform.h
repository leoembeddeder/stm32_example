#ifndef STM32_UDS_ISO_TP_UDS_PLATFORM_H
#define STM32_UDS_ISO_TP_UDS_PLATFORM_H

#if defined(USE_HAL_DRIVER) || defined(STM32F767xx) || defined(STM32C092xx)
#include "main.h"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t uds_platform_now_ms(void);
uint32_t uds_platform_systick_val(void);
bool uds_platform_trng_get_random(uint8_t *buffer, size_t length);
void uds_platform_system_reset(uint8_t reset_type);
void uds_platform_error(void);

#ifdef __cplusplus
}
#endif

#endif
