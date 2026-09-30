/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 *
 * Flash-backed storage for the SecurityAccess lockout state, so that neither an ECU reset
 * (0x11) nor a power cycle gives an attacker fresh guesses. Built on the wear-leveled
 * UdsParamStore, so it works with any UdsFlashPort (any program granule).
 */
#ifndef UDS_SECURITY_NVM_H
#define UDS_SECURITY_NVM_H

#include "uds_iso_tp/uds_security_gate.h"
#include "uds_iso_tp/uds_wear_leveling.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    UdsParamStore store;
    bool ready;
} UdsSecurityNvm;

/** Prepare the store. Returns false if the flash port/region is unusable. */
bool uds_security_nvm_init(UdsSecurityNvm *nvm, const UdsFlashPort *port, uint32_t flash_base,
                           uint8_t sector_count);

/** Persist state. Matches UdsSecurityStateSaveFn so it can be handed to the gate directly. */
bool uds_security_nvm_gate_persist(uint8_t failed_attempts, uint32_t lockout_remaining_ms,
                                   void *context);

/**
 * Restore the stored state into `gate` and register the persist callback. Call once at boot,
 * AFTER uds_security_gate_init()/set_timing().
 *
 *  - first boot (nothing stored yet): starts clean;
 *  - readable record: restored (lockout resumes with its full remaining time);
 *  - record present but unreadable/corrupt: FAILS CLOSED, starts in lockout.
 */
void uds_security_nvm_attach(UdsSecurityGate *gate, UdsSecurityNvm *nvm, uint32_t now_ms);

#endif /* UDS_SECURITY_NVM_H */
