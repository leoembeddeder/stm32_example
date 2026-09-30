#include "uds_iso_tp/uds_security_nvm.h"

#include <string.h>

#define SEC_NVM_MAGIC 0x53454331UL /* "SEC1" */

typedef struct {
    uint32_t magic;
    uint32_t remaining_ms;
    uint8_t failed_attempts;
    uint8_t reserved[3];
    uint32_t check; /* ~(magic ^ remaining ^ failed) : detects a half-valid record */
} SecurityRecord;

static uint32_t record_check(const SecurityRecord *r) {
    return ~(r->magic ^ r->remaining_ms ^ (uint32_t)r->failed_attempts);
}

bool uds_security_nvm_init(UdsSecurityNvm *nvm, const UdsFlashPort *port, uint32_t flash_base,
                           uint8_t sector_count) {
    if ((nvm == NULL) || (port == NULL) || (sector_count < 2U)) {
        return false;
    }
    (void)memset(nvm, 0, sizeof(*nvm));
    nvm->ready = (uds_param_init(&nvm->store, port, flash_base, sector_count,
                                 (uint16_t)sizeof(SecurityRecord)) == UDS_PARAM_OK);
    return nvm->ready;
}

bool uds_security_nvm_gate_persist(uint8_t failed_attempts, uint32_t lockout_remaining_ms,
                                   void *context) {
    UdsSecurityNvm *nvm = (UdsSecurityNvm *)context;
    if ((nvm == NULL) || !nvm->ready) {
        return false;
    }
    SecurityRecord rec;
    (void)memset(&rec, 0, sizeof(rec));
    rec.magic = SEC_NVM_MAGIC;
    rec.failed_attempts = failed_attempts;
    rec.remaining_ms = lockout_remaining_ms;
    rec.check = record_check(&rec);
    return (uds_param_save(&nvm->store, &rec) == UDS_PARAM_OK);
}

void uds_security_nvm_attach(UdsSecurityGate *gate, UdsSecurityNvm *nvm, uint32_t now_ms) {
    if (gate == NULL) {
        return;
    }
    uds_security_gate_set_persistence(gate, uds_security_nvm_gate_persist, nvm);
    if ((nvm == NULL) || !nvm->ready) {
        /* No usable storage: fail closed. */
        uds_security_gate_restore_state(gate, gate->max_attempts, gate->lockout_ms, now_ms);
        return;
    }
    if (!nvm->store.has_active_slot) {
        return; /* first boot: nothing stored, start clean */
    }
    SecurityRecord rec;
    if ((uds_param_load(&nvm->store, &rec) != UDS_PARAM_OK) || (rec.magic != SEC_NVM_MAGIC) ||
        (rec.check != record_check(&rec))) {
        uds_security_gate_restore_state(gate, gate->max_attempts, gate->lockout_ms, now_ms);
        return;
    }
    uds_security_gate_restore_state(gate, rec.failed_attempts, rec.remaining_ms, now_ms);
}
