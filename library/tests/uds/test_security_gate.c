#include "uds_iso_tp/uds_security_gate.h"
#include <assert.h>
#include <stdio.h>

static void test_poweron_delay(void) {
    UdsSecurityGate gate;
    uds_security_gate_init(&gate, 1000U);
    uds_security_gate_set_timing(&gate, 5000U, 10000U, 2000U, 3U, 1000U);

    /* Initial delay is 5000ms from 1000ms -> expires at 6000ms */
    assert(uds_security_gate_delay_active(&gate, 1000U) == true);
    assert(uds_security_gate_delay_active(&gate, 5999U) == true);
    uds_security_gate_tick(&gate, 5999U);
    assert(uds_security_gate_delay_active(&gate, 5999U) == true);

    /* At 6000ms, power-on delay expires */
    uds_security_gate_tick(&gate, 6000U);
    assert(uds_security_gate_delay_active(&gate, 6000U) == false);
}

static void test_lockout_after_n_failed_attempts(void) {
    UdsSecurityGate gate;
    uds_security_gate_init(&gate, 0U);
    uds_security_gate_set_timing(&gate, 0U, 10000U, 2000U, 3U, 0U);

    assert(gate.state == UDS_SECURITY_STATE_LOCKED_READY);
    assert(uds_security_gate_delay_active(&gate, 0U) == false);

    /* Seed granted */
    uds_security_gate_grant_seed(&gate, 1U, 100U);
    assert(gate.state == UDS_SECURITY_STATE_WAITING_FOR_KEY);
    assert(gate.seed_valid == true);

    /* Attempt 1 fail */
    uds_security_gate_record_failure(&gate, 200U);
    assert(gate.failed_attempts == 1U);
    assert(gate.seed_valid == false);
    assert(gate.state == UDS_SECURITY_STATE_LOCKED_READY);
    assert(uds_security_gate_delay_active(&gate, 200U) == false);

    /* Attempt 2 fail */
    uds_security_gate_grant_seed(&gate, 1U, 300U);
    uds_security_gate_record_failure(&gate, 400U);
    assert(gate.failed_attempts == 2U);
    assert(uds_security_gate_delay_active(&gate, 400U) == false);

    /* Attempt 3 fail -> lockout triggered! */
    uds_security_gate_grant_seed(&gate, 1U, 500U);
    uds_security_gate_record_failure(&gate, 600U);
    assert(gate.failed_attempts == 3U);
    assert(gate.state == UDS_SECURITY_STATE_LOCKOUT);
    assert(uds_security_gate_delay_active(&gate, 600U) == true);
    assert(uds_security_gate_delay_active(&gate, 10599U) == true);

    /* After lockout (10000ms later, at 10600ms), lockout expires */
    uds_security_gate_tick(&gate, 10600U);
    assert(uds_security_gate_delay_active(&gate, 10600U) == false);
    assert(gate.state == UDS_SECURITY_STATE_LOCKED_READY);
}

static void test_seed_reuse_and_expiration(void) {
    UdsSecurityGate gate;
    uds_security_gate_init(&gate, 0U);
    uds_security_gate_set_timing(&gate, 0U, 10000U, 2000U, 3U, 0U);

    /* Grant seed at t=1000ms with timeout 2000ms */
    uds_security_gate_grant_seed(&gate, 2U, 1000U);
    assert(gate.seed_valid == true);
    assert(gate.seed_level == 2U);

    /* Before expiry (t=2999ms), seed is still valid */
    uds_security_gate_tick(&gate, 2999U);
    assert(gate.seed_valid == true);

    /* At expiry (t=3000ms), seed is invalidated */
    uds_security_gate_tick(&gate, 3000U);
    assert(gate.seed_valid == false);
    assert(gate.state == UDS_SECURITY_STATE_LOCKED_READY);

    /* Successful unlock invalidates seed immediately (no reuse allowed) */
    uds_security_gate_grant_seed(&gate, 1U, 4000U);
    assert(gate.seed_valid == true);
    uds_security_gate_record_success(&gate, 1U);
    assert(gate.state == UDS_SECURITY_STATE_UNLOCKED);
    assert(gate.active_level == 1U);
    assert(gate.seed_valid == false); /* Seed invalidated */
    assert(gate.failed_attempts == 0U);
}

int main(void) {
    test_poweron_delay();
    test_lockout_after_n_failed_attempts();
    test_seed_reuse_and_expiration();
    printf("UDS SecurityGate tests passed successfully.\n");
    return 0;
}
