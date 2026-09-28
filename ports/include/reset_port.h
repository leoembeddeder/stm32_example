#ifndef RESET_PORT_H
#define RESET_PORT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Standard MCU hardware system reset and bootloader jump interface.
 */
typedef struct {
    void (*system_reset)(void);
    void (*jump_to_app)(uint32_t app_vector_addr);
    bool (*is_app_valid)(uint32_t app_vector_addr);
} ResetPortInterface;

#ifdef __cplusplus
}
#endif

#endif /* RESET_PORT_H */
