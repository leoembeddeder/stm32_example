#include "uds_iso_tp/uds_security_gate.h"
#include <stddef.h>

static bool time_expired(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

void uds_security_gate_init(UdsSecurityGate *gate, uint32_t now_ms) {
    if (gate == NULL) {
        return;
    }
    gate->failed_attempts = 0U;
    gate->max_attempts = 3U;
    gate->active_level = 0U;
    gate->seed_level = 0U;
    gate->state = UDS_SECURITY_STATE_LOCKED_READY;
    gate->seed_valid = false;
    gate->seed_timer_active = false;
    gate->lockout_active = false;
    gate->initial_delay_active = false;
    gate->seed_timeout_ms = 0U;
    gate->seed_expiry_ms = 0U;
    gate->lockout_ms = 10000U;
    gate->lockout_until_ms = 0U;
    gate->initial_delay_ms = 0U;
    gate->initial_delay_until_ms = now_ms;
    gate->persist_fn = NULL;
    gate->persist_context = NULL;
}

void uds_security_gate_set_timing(UdsSecurityGate *gate, uint32_t initial_delay_ms,
                                  uint32_t lockout_ms, uint32_t seed_timeout_ms,
                                  uint8_t max_attempts, uint32_t now_ms) {
    if (gate == NULL) {
        return;
    }
    gate->initial_delay_ms = initial_delay_ms;
    gate->initial_delay_until_ms = now_ms + initial_delay_ms;
    gate->initial_delay_active = (initial_delay_ms != 0U);
    /* Enforce non-zero minimum lockout delay to eliminate brute-force bypass */
    if (lockout_ms == 0U) {
        lockout_ms = 10000U;
    } else if (lockout_ms < 1000U) {
        lockout_ms = 1000U;
    }
    gate->lockout_ms = lockout_ms;
    gate->seed_timeout_ms = seed_timeout_ms;
    gate->max_attempts = (max_attempts > 0U) ? max_attempts : 3U;
}

void uds_security_gate_tick(UdsSecurityGate *gate, uint32_t now_ms) {
    if (gate == NULL) {
        return;
    }
    if (gate->initial_delay_active && time_expired(now_ms, gate->initial_delay_until_ms)) {
        gate->initial_delay_active = false;
    }
    if (gate->lockout_active && time_expired(now_ms, gate->lockout_until_ms)) {
        gate->lockout_active = false;
        gate->failed_attempts = 0U;
        if (gate->state == UDS_SECURITY_STATE_LOCKOUT) {
            gate->state = UDS_SECURITY_STATE_LOCKED_READY;
        }
        if (gate->persist_fn != NULL) {
            gate->persist_fn(0U, 0U, gate->persist_context);
        }
    }
    if (gate->seed_timer_active && time_expired(now_ms, gate->seed_expiry_ms)) {
        uds_security_gate_invalidate_seed(gate);
    }
}

bool uds_security_gate_delay_active(const UdsSecurityGate *gate, uint32_t now_ms) {
    if (gate == NULL) {
        return false;
    }
    if (gate->initial_delay_active && !time_expired(now_ms, gate->initial_delay_until_ms)) {
        return true;
    }
    if (gate->lockout_active && !time_expired(now_ms, gate->lockout_until_ms)) {
        return true;
    }
    return false;
}

void uds_security_gate_invalidate_seed(UdsSecurityGate *gate) {
    if (gate == NULL) {
        return;
    }
    gate->seed_valid = false;
    gate->seed_timer_active = false;
    gate->seed_level = 0U;
    if (gate->state == UDS_SECURITY_STATE_WAITING_FOR_KEY) {
        gate->state = UDS_SECURITY_STATE_LOCKED_READY;
    }
}

void uds_security_gate_record_failure(UdsSecurityGate *gate, uint32_t now_ms) {
    if (gate == NULL) {
        return;
    }
    uds_security_gate_invalidate_seed(gate);
    gate->active_level = 0U;
    gate->failed_attempts = (uint8_t)(gate->failed_attempts + 1U);
    if (gate->failed_attempts >= gate->max_attempts) {
        gate->lockout_active = (gate->lockout_ms != 0U);
        gate->lockout_until_ms = now_ms + gate->lockout_ms;
        gate->state =
            gate->lockout_active ? UDS_SECURITY_STATE_LOCKOUT : UDS_SECURITY_STATE_LOCKED_READY;
    }
    if (gate->persist_fn != NULL) {
        uint32_t remaining = (gate->lockout_active && (gate->lockout_until_ms > now_ms))
                                 ? (gate->lockout_until_ms - now_ms)
                                 : 0U;
        gate->persist_fn(gate->failed_attempts, remaining, gate->persist_context);
    }
}

void uds_security_gate_record_success(UdsSecurityGate *gate, uint8_t level) {
    if (gate == NULL) {
        return;
    }
    gate->active_level = level;
    gate->failed_attempts = 0U;
    uds_security_gate_invalidate_seed(gate);
    gate->state = UDS_SECURITY_STATE_UNLOCKED;
    if (gate->persist_fn != NULL) {
        gate->persist_fn(0U, 0U, gate->persist_context);
    }
}

void uds_security_gate_grant_seed(UdsSecurityGate *gate, uint8_t level, uint32_t now_ms) {
    if (gate == NULL) {
        return;
    }
    gate->seed_level = level;
    gate->seed_valid = true;
    gate->seed_timer_active = (gate->seed_timeout_ms != 0U);
    gate->seed_expiry_ms = now_ms + gate->seed_timeout_ms;
    gate->state = UDS_SECURITY_STATE_WAITING_FOR_KEY;
}

void uds_security_gate_reset_session(UdsSecurityGate *gate) {
    if (gate == NULL) {
        return;
    }
    gate->active_level = 0U;
    uds_security_gate_invalidate_seed(gate);
    if (gate->state == UDS_SECURITY_STATE_UNLOCKED) {
        gate->state = UDS_SECURITY_STATE_LOCKED_READY;
    }
}

void uds_security_gate_reset_ecu(UdsSecurityGate *gate, uint32_t now_ms) {
    if (gate == NULL) {
        return;
    }
    gate->active_level = 0U;
    uds_security_gate_invalidate_seed(gate);
    if (!gate->lockout_active) {
        gate->state = UDS_SECURITY_STATE_LOCKED_READY;
    }
    /* P0-3: Lockout state and failed attempts counter survive ECU Reset (0x11) */
    (void)now_ms;
}

void uds_security_gate_set_persistence(UdsSecurityGate *gate, UdsSecurityStateSaveFn persist_fn,
                                       void *context) {
    if (gate == NULL) {
        return;
    }
    gate->persist_fn = persist_fn;
    gate->persist_context = context;
}

void uds_security_gate_restore_state(UdsSecurityGate *gate, uint8_t failed_attempts,
                                     uint32_t lockout_remaining_ms, uint32_t now_ms) {
    if (gate == NULL) {
        return;
    }
    gate->failed_attempts = failed_attempts;
    if (lockout_remaining_ms > 0U) {
        gate->lockout_active = true;
        gate->lockout_until_ms = now_ms + lockout_remaining_ms;
        gate->state = UDS_SECURITY_STATE_LOCKOUT;
    } else if ((failed_attempts >= gate->max_attempts) && (gate->max_attempts > 0U)) {
        gate->lockout_active = true;
        gate->lockout_until_ms = now_ms + gate->lockout_ms;
        gate->state = UDS_SECURITY_STATE_LOCKOUT;
    }
}
