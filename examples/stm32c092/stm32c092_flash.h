/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */

#ifndef STM32C092_FLASH_H
#define STM32C092_FLASH_H

#include "uds_iso_tp/uds_wear_leveling.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STM32C092_FLASH_BASE 0x08000000U
#define STM32C092_PAGE_SIZE 0x00000800U /* 2048 bytes */

/**
 * @brief Helper to map flash memory address to STM32C092 page number.
 */
static inline uint32_t stm32c092_get_page(uint32_t addr) {
    return (addr - STM32C092_FLASH_BASE) / STM32C092_PAGE_SIZE;
}

/**
 * @brief Returns the UdsFlashPort instance for STM32C092 internal Flash.
 */
const UdsFlashPort *stm32c092_flash_get_port(void);

/**
 * @brief Convenience helper to initialize wear-leveling store on STM32C092.
 */
int stm32c092_flash_init_wear_leveling(UdsParamStore *store, uint32_t flash_base,
                                       uint8_t sector_count, uint16_t data_size);

#ifdef __cplusplus
}
#endif

#endif /* STM32C092_FLASH_H */
