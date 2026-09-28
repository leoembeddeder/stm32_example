#ifndef UDS_SECURITY_GATE_H
#define UDS_SECURITY_GATE_H

#include <stdbool.h>
#include <stdint.h>
#include "uds_iso_tp/uds.h"

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

#endif /* UDS_SECURITY_GATE_H */
