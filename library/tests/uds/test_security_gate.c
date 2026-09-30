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

static void test_null_guards(void) {
    uds_security_gate_init(NULL, 100U);
    uds_security_gate_set_timing(NULL, 100U, 200U, 300U, 3U, 400U);
    uds_security_gate_tick(NULL, 100U);
    assert(uds_security_gate_delay_active(NULL, 100U) == false);
    uds_security_gate_invalidate_seed(NULL);
    uds_security_gate_record_failure(NULL, 100U);
    uds_security_gate_record_success(NULL, 1U);
    uds_security_gate_grant_seed(NULL, 1U, 100U);
    uds_security_gate_reset_session(NULL);
    uds_security_gate_reset_ecu(NULL, 100U);
}

static void test_zero_timing_and_branches(void) {
    UdsSecurityGate gate;
    uds_security_gate_init(&gate, 0U);

    /* initial_delay_ms = 0 -> initial_delay_active is false */
    uds_security_gate_set_timing(&gate, 0U, 0U, 0U, 1U, 0U);
    assert(gate.initial_delay_active == false);
    assert(uds_security_gate_delay_active(&gate, 0U) == false);

    /* seed_timeout_ms = 0 -> seed_timer_active is false */
    uds_security_gate_grant_seed(&gate, 1U, 100U);
    assert(gate.seed_timer_active == false);
    uds_security_gate_tick(&gate, 1000U);
    assert(gate.seed_valid == true); /* No timer, seed remains valid until used or failure */

    /* lockout_ms = 0 -> clamped to 10000U non-zero minimum, lockout_active is true on failure */
    uds_security_gate_record_failure(&gate, 1100U);
    assert(gate.failed_attempts == 1U);
    assert(gate.lockout_active == true);
    assert(gate.state == UDS_SECURITY_STATE_LOCKOUT);

    /* Tick when lockout_active is true but state is not LOCKOUT */
    gate.lockout_active = true;
    gate.lockout_until_ms = 1200U;
    gate.state = UDS_SECURITY_STATE_UNLOCKED;
    uds_security_gate_tick(&gate, 1300U);
    assert(gate.lockout_active == false);
    assert(gate.state == UDS_SECURITY_STATE_UNLOCKED); /* Unlocked state unchanged */

    /* Invalidate seed when state is already LOCKED_READY */
    gate.state = UDS_SECURITY_STATE_LOCKED_READY;
    uds_security_gate_invalidate_seed(&gate);
    assert(gate.state == UDS_SECURITY_STATE_LOCKED_READY);
}

static void test_reset_session_and_ecu(void) {
    UdsSecurityGate gate;
    uds_security_gate_init(&gate, 0U);
    uds_security_gate_set_timing(&gate, 1000U, 5000U, 2000U, 2U, 0U);

    /* Unlock gate */
    uds_security_gate_grant_seed(&gate, 1U, 1500U);
    uds_security_gate_record_success(&gate, 1U);
    assert(gate.state == UDS_SECURITY_STATE_UNLOCKED);
    assert(gate.active_level == 1U);

    /* Reset session revokes unlocked state */
    uds_security_gate_reset_session(&gate);
    assert(gate.state == UDS_SECURITY_STATE_LOCKED_READY);
    assert(gate.active_level == 0U);

    /* Reset session when already in LOCKED_READY does not crash */
    uds_security_gate_reset_session(&gate);
    assert(gate.state == UDS_SECURITY_STATE_LOCKED_READY);

    /* Cause lockout then ECU reset */
    uds_security_gate_grant_seed(&gate, 1U, 2000U);
    uds_security_gate_record_failure(&gate, 2100U);
    uds_security_gate_grant_seed(&gate, 1U, 2200U);
    uds_security_gate_record_failure(&gate, 2300U);
    assert(gate.state == UDS_SECURITY_STATE_LOCKOUT);
    assert(gate.failed_attempts == 2U);

    /* P0-3 Acceptance: Full ECU reset (0x11) does NOT clear lockout or failed attempt counter */
    uds_security_gate_reset_ecu(&gate, 3000U);
    assert(gate.state == UDS_SECURITY_STATE_LOCKOUT);
    assert(gate.failed_attempts == 2U);
    assert(gate.lockout_active == true);
    assert(uds_security_gate_delay_active(&gate, 3000U) == true);

    /* Lockout expires only after lockout timer (5000ms from t=2300 -> 7300ms) */
    uds_security_gate_tick(&gate, 7300U);
    assert(gate.state == UDS_SECURITY_STATE_LOCKED_READY);
    assert(gate.failed_attempts == 0U);
    assert(gate.lockout_active == false);
    assert(uds_security_gate_delay_active(&gate, 7300U) == false);
}

static uint8_t s_persisted_attempts = 0U;
static uint32_t s_persisted_remaining = 0U;
static bool test_save_fn(uint8_t failed_attempts, uint32_t lockout_remaining_ms, void *context) {
    (void)context;
    s_persisted_attempts = failed_attempts;
    s_persisted_remaining = lockout_remaining_ms;
    return true;
}

static void test_persistence_and_restore(void) {
    UdsSecurityGate gate;
    uds_security_gate_init(&gate, 0U);
    uds_security_gate_set_timing(&gate, 0U, 10000U, 2000U, 2U, 0U);
    uds_security_gate_set_persistence(&gate, test_save_fn, NULL);

    uds_security_gate_grant_seed(&gate, 1U, 100U);
    uds_security_gate_record_failure(&gate, 200U);
    assert(s_persisted_attempts == 1U);
    assert(s_persisted_remaining == 0U);

    uds_security_gate_grant_seed(&gate, 1U, 300U);
    uds_security_gate_record_failure(&gate, 400U);
    assert(s_persisted_attempts == 2U);
    assert(s_persisted_remaining == 10000U);

    /* Simulate power cycle: new gate initialized, then restore state from persistent store */
    UdsSecurityGate restored_gate;
    uds_security_gate_init(&restored_gate, 5000U);
    uds_security_gate_set_timing(&restored_gate, 0U, 10000U, 2000U, 2U, 5000U);
    uds_security_gate_restore_state(&restored_gate, s_persisted_attempts, s_persisted_remaining,
                                    5000U);

    /* Restored gate is still locked out! */
    assert(restored_gate.state == UDS_SECURITY_STATE_LOCKOUT);
    assert(restored_gate.failed_attempts == 2U);
    assert(uds_security_gate_delay_active(&restored_gate, 5000U) == true);
    assert(uds_security_gate_delay_active(&restored_gate, 14999U) == true);

    /* After 10s delay expires at t=15000 */
    uds_security_gate_tick(&restored_gate, 15000U);
    assert(restored_gate.state == UDS_SECURITY_STATE_LOCKED_READY);
    assert(uds_security_gate_delay_active(&restored_gate, 15000U) == false);

    /* NULL guards for new persistence and restore functions */
    uds_security_gate_set_persistence(NULL, NULL, NULL);
    uds_security_gate_restore_state(NULL, 0U, 0U, 0U);
}

int main(void) {
    test_poweron_delay();
    test_lockout_after_n_failed_attempts();
    test_seed_reuse_and_expiration();
    test_null_guards();
    test_zero_timing_and_branches();
    test_reset_session_and_ecu();
    test_persistence_and_restore();
    printf("UDS SecurityGate tests passed successfully.\n");
    return 0;
}
