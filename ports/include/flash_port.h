#ifndef FLASH_PORT_H
#define FLASH_PORT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Standard MCU on-chip flash memory port interface.
 * Explicitly exposes write granularity (Issue #91) and erased byte state.
 */
typedef struct {
    uint8_t program_granule; /* Minimum programmable unit in bytes: 2, 4, 8, 16, 32, 64 */
    uint8_t erased_byte;     /* Value of erased flash cells (typically 0xFF) */
    bool (*unlock)(void);
    bool (*lock)(void);
    bool (*erase_sector)(uint32_t sector_addr);
    bool (*erase_range)(uint32_t start_addr, uint32_t length);
    bool (*program)(uint32_t addr, const uint8_t *data, size_t len);
    const uint8_t *(*read)(uint32_t addr, size_t len);
} FlashPortInterface;

#ifdef __cplusplus
}
#endif

#endif /* FLASH_PORT_H */
