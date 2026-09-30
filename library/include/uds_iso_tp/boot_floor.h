/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 *
 * Persistent, monotonic anti-rollback floor. The floor is the lowest firmware version the
 * bootloader will ever install again. It only moves up, it survives resets and power cycles,
 * and it is kept apart from the slot metadata (which a rollback/crash-recovery may rewrite).
 */
#ifndef UDS_BOOT_FLOOR_H
#define UDS_BOOT_FLOOR_H

#include "uds_iso_tp/uds_wear_leveling.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    UdsParamStore store;
    bool ready;
    uint32_t floor;
} UdsBootFloor;

/**
 * Open the store and load the floor.
 *  - nothing stored yet -> floor = 0 (first boot);
 *  - record present but unreadable -> FAIL CLOSED: floor = UINT32_MAX, no image can be installed.
 * Returns false when the flash region cannot be used at all (then the floor is UINT32_MAX too).
 */
bool boot_floor_init(UdsBootFloor *bf, const UdsFlashPort *port, uint32_t flash_base,
                     uint8_t sector_count);

uint32_t boot_floor_get(const UdsBootFloor *bf);

/**
 * Raise the floor to `new_floor` (never lowers it). Returns false if the write failed; the
 * in-RAM floor is only updated after the flash write succeeded and verifies by read-back.
 */
bool boot_floor_raise(UdsBootFloor *bf, uint32_t new_floor);

#endif /* UDS_BOOT_FLOOR_H */
