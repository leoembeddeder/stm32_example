#ifndef BOOT_JUMP_H
#define BOOT_JUMP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Jump to application reset handler at the specified vector address.
 * 
 * Performs vector table sanity checking, disables interrupts/peripherals,
 * sets VTOR, cleans and invalidates caches (if M7), resets stack pointer,
 * and branches to application reset vector.
 *
 * @param app_vector_addr Base address of application vector table
 */
void uds_bootloader_jump_to_app(uint32_t app_vector_addr);

#ifdef __cplusplus
}
#endif

#endif /* BOOT_JUMP_H */
