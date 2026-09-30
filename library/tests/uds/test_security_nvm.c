#include "uds_iso_tp/uds_security_nvm.h"

#include <assert.h>
#include <string.h>

/* ---- strict fake flash: 2 sectors x 1 KiB, enforces granule and "no rewrite without erase" ---- */
#define FLASH_BASE 0x08010000UL
#define SECTOR_SZ 1024U
#define SECTORS 2U

static uint8_t s_flash[SECTORS * SECTOR_SZ];
static uint8_t s_granule = 8U;
static bool s_fail_writes = false;

static void flash_reset(uint8_t granule) {
    (void)memset(s_flash, 0xFF, sizeof(s_flash));
    s_granule = granule;
    s_fail_writes = false;
}
static int f_erase(uint32_t addr) {
    uint32_t off = (addr - (uint32_t)FLASH_BASE) / SECTOR_SZ * SECTOR_SZ;
    (void)memset(&s_flash[off], 0xFF, SECTOR_SZ);
    return 0;
}
static int f_read(uint32_t addr, void *buf, size_t len) {
    (void)memcpy(buf, &s_flash[addr - (uint32_t)FLASH_BASE], len);
    return 0;
}
static int f_write(uint32_t addr, const void *buf, size_t len) {
    if (s_fail_writes) {
        return -1;
    }
    const uint32_t off = addr - (uint32_t)FLASH_BASE;
    assert((off % s_granule) == 0U);
    assert((len % s_granule) == 0U);
    for (size_t i = 0U; i < len; ++i) {
        assert(s_flash[off + i] == 0xFFU); /* real flash cannot rewrite a programmed cell */
    }
    (void)memcpy(&s_flash[off], buf, len);
    return 0;
}
static uint32_t f_sector(uint32_t addr) {
    (void)addr;
    return SECTOR_SZ;
}
static UdsFlashPort s_port = {f_erase, f_read, f_write, f_sector, 8U, 0xFFU};

static void new_gate(UdsSecurityGate *g, uint32_t now) {
    uds_security_gate_init(g, now);
    uds_security_gate_set_timing(g, 0U, 10000U, 2000U, 3U, now);
}

static void wrong_key(UdsSecurityGate *g, uint32_t now) {
    uds_security_gate_grant_seed(g, 1U, now);
    uds_security_gate_record_failure(g, now + 1U);
}

static void test_lockout_survives_reset_and_power_cycle(uint8_t granule) {
    flash_reset(granule);
    s_port.program_granule = granule;
    UdsSecurityNvm nvm;
    assert(uds_security_nvm_init(&nvm, &s_port, FLASH_BASE, SECTORS));

    UdsSecurityGate gate;
    new_gate(&gate, 0U);
    uds_security_nvm_attach(&gate, &nvm, 0U); /* first boot: clean */
    assert(gate.failed_attempts == 0U);

    wrong_key(&gate, 100U);
    wrong_key(&gate, 200U);
    wrong_key(&gate, 300U); /* 3rd failure -> lockout */
    assert(gate.state == UDS_SECURITY_STATE_LOCKOUT);

    /* ECU reset (0x11) must not clear it. */
    uds_security_gate_reset_ecu(&gate, 400U);
    assert(gate.state == UDS_SECURITY_STATE_LOCKOUT);
    assert(uds_security_gate_delay_active(&gate, 400U));

    /* Power cycle: brand-new RAM, same flash. */
    UdsSecurityNvm nvm2;
    assert(uds_security_nvm_init(&nvm2, &s_port, FLASH_BASE, SECTORS));
    UdsSecurityGate gate2;
    new_gate(&gate2, 0U);
    uds_security_nvm_attach(&gate2, &nvm2, 0U);
    assert(gate2.failed_attempts == 3U);
    assert(gate2.state == UDS_SECURITY_STATE_LOCKOUT);
    assert(uds_security_gate_delay_active(&gate2, 0U));

    /* Lockout expires, counter cleared AND persisted: a further power cycle starts clean. */
    uds_security_gate_tick(&gate2, 10001U);
    assert(!uds_security_gate_delay_active(&gate2, 10001U));
    UdsSecurityNvm nvm3;
    assert(uds_security_nvm_init(&nvm3, &s_port, FLASH_BASE, SECTORS));
    UdsSecurityGate gate3;
    new_gate(&gate3, 0U);
    uds_security_nvm_attach(&gate3, &nvm3, 0U);
    assert(gate3.failed_attempts == 0U);
    assert(!uds_security_gate_delay_active(&gate3, 0U));
}

static void test_survives_many_writes_across_sector_wrap(void) {
    flash_reset(8U);
    s_port.program_granule = 8U;
    UdsSecurityNvm nvm;
    assert(uds_security_nvm_init(&nvm, &s_port, FLASH_BASE, SECTORS));
    UdsSecurityGate gate;
    new_gate(&gate, 0U);
    uds_security_nvm_attach(&gate, &nvm, 0U);
    for (uint32_t i = 0U; i < 400U; ++i) { /* far more saves than slots -> forces sector erases */
        uds_security_gate_grant_seed(&gate, 1U, i * 10U);
        uds_security_gate_record_success(&gate, 1U);
    }
    wrong_key(&gate, 5000U);
    UdsSecurityNvm nvm2;
    assert(uds_security_nvm_init(&nvm2, &s_port, FLASH_BASE, SECTORS));
    UdsSecurityGate g2;
    new_gate(&g2, 0U);
    uds_security_nvm_attach(&g2, &nvm2, 0U);
    assert(g2.failed_attempts == 1U);
}

static void test_write_failure_fails_closed(void) {
    flash_reset(8U);
    s_port.program_granule = 8U;
    UdsSecurityNvm nvm;
    assert(uds_security_nvm_init(&nvm, &s_port, FLASH_BASE, SECTORS));
    UdsSecurityGate gate;
    new_gate(&gate, 0U);
    uds_security_nvm_attach(&gate, &nvm, 0U);

    s_fail_writes = true;   /* attacker (or wear-out) makes the store unwritable */
    wrong_key(&gate, 100U); /* only ONE failure so far ... */
    assert(gate.state == UDS_SECURITY_STATE_LOCKOUT); /* ... but the gate refuses to continue */
    assert(uds_security_gate_delay_active(&gate, 200U));

    uds_security_gate_tick(&gate, 20000U); /* lockout time passed, storage still broken */
    assert(uds_security_gate_delay_active(&gate, 20000U)); /* still locked */

    s_fail_writes = false; /* storage recovers */
    uds_security_gate_tick(&gate, 40000U);
    assert(!uds_security_gate_delay_active(&gate, 40000U));
}

static void test_no_storage_fails_closed(void) {
    UdsSecurityGate gate;
    new_gate(&gate, 0U);
    uds_security_nvm_attach(&gate, NULL, 0U);
    assert(gate.state == UDS_SECURITY_STATE_LOCKOUT);
    assert(uds_security_gate_delay_active(&gate, 0U));
    UdsSecurityNvm bad;
    assert(!uds_security_nvm_init(&bad, &s_port, FLASH_BASE, 1U)); /* needs >= 2 sectors */
    assert(!uds_security_nvm_init(NULL, &s_port, FLASH_BASE, SECTORS));
    assert(!uds_security_nvm_gate_persist(1U, 0U, NULL));
}

int main(void) {
    const uint8_t granules[] = {2U, 4U, 8U, 16U, 32U};
    for (size_t i = 0U; i < sizeof(granules); ++i) {
        test_lockout_survives_reset_and_power_cycle(granules[i]);
    }
    test_survives_many_writes_across_sector_wrap();
    test_write_failure_fails_closed();
    test_no_storage_fails_closed();
    return 0;
}
