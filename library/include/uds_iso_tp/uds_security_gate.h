#ifndef UDS_SECURITY_GATE_H
#define UDS_SECURITY_GATE_H

#include <stdbool.h>
#include <stdint.h>

#ifndef UDS_SECURITY_STATE_DEFINED
#define UDS_SECURITY_STATE_DEFINED
typedef enum {
    UDS_SECURITY_STATE_LOCKED_READY = 0,
    UDS_SECURITY_STATE_WAITING_FOR_KEY,
    UDS_SECURITY_STATE_UNLOCKED,
    UDS_SECURITY_STATE_LOCKOUT
} UdsSecurityState;
#endif

/**
 * Persist the lockout state. Return false if the write failed: the gate then FAILS CLOSED
 * (it stays in lockout) because an attacker could otherwise defeat the counter by making
 * the storage write fail.
 */
typedef bool (*UdsSecurityStateSaveFn)(uint8_t failed_attempts, uint32_t lockout_remaining_ms,
                                       void *context);

typedef struct {
    uint8_t failed_attempts;
    uint8_t max_attempts;
    uint8_t active_level;
    uint8_t seed_level;
    UdsSecurityState state;
    bool seed_valid;
    bool seed_timer_active;
    bool lockout_active;
    bool initial_delay_active;
    uint32_t seed_timeout_ms;
    uint32_t seed_expiry_ms;
    uint32_t lockout_ms;
    uint32_t lockout_until_ms;
    uint32_t initial_delay_ms;
    uint32_t initial_delay_until_ms;
    UdsSecurityStateSaveFn persist_fn;
    void *persist_context;
} UdsSecurityGate;

void uds_security_gate_init(UdsSecurityGate *gate, uint32_t now_ms);
void uds_security_gate_set_timing(UdsSecurityGate *gate, uint32_t initial_delay_ms,
                                  uint32_t lockout_ms, uint32_t seed_timeout_ms,
                                  uint8_t max_attempts, uint32_t now_ms);
void uds_security_gate_tick(UdsSecurityGate *gate, uint32_t now_ms);
bool uds_security_gate_delay_active(const UdsSecurityGate *gate, uint32_t now_ms);
void uds_security_gate_invalidate_seed(UdsSecurityGate *gate);
void uds_security_gate_record_failure(UdsSecurityGate *gate, uint32_t now_ms);
void uds_security_gate_record_success(UdsSecurityGate *gate, uint8_t level);
void uds_security_gate_grant_seed(UdsSecurityGate *gate, uint8_t level, uint32_t now_ms);
void uds_security_gate_reset_session(UdsSecurityGate *gate);
void uds_security_gate_reset_ecu(UdsSecurityGate *gate, uint32_t now_ms);
void uds_security_gate_set_persistence(UdsSecurityGate *gate, UdsSecurityStateSaveFn persist_fn,
                                       void *context);
void uds_security_gate_restore_state(UdsSecurityGate *gate, uint8_t failed_attempts,
                                     uint32_t lockout_remaining_ms, uint32_t now_ms);

#endif /* UDS_SECURITY_GATE_H */
