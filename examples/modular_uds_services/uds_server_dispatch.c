/**
 * @file uds_server_dispatch.c
 * @brief UDS Service Dispatcher Implementation with Addressing and Security Masks (ISO 14229-1)
 */

#include "uds_server_dispatch.h"
#include <string.h>

static uds_service_entry_ext_t s_entries[UDS_MAX_SERVICES_EXT];
static uint8_t s_count = 0U;
static uint32_t s_active_security_mask = UDS_SEC_LEVEL_LOCKED;
static uint8_t  s_active_session_mask  = SESSION_MASK_DEFAULT;

void uds_server_dispatch_init(void) {
    s_count = 0U;
    s_active_security_mask = UDS_SEC_LEVEL_LOCKED;
    s_active_session_mask  = SESSION_MASK_DEFAULT;
    memset(s_entries, 0, sizeof(s_entries));
}

bool uds_server_dispatch_register(uint8_t sid, uds_service_handler_t handler,
                                  uint8_t session_mask, uint32_t address_mask, uint32_t security_mask) {
    if (s_count >= UDS_MAX_SERVICES_EXT) return false;
    uds_service_entry_ext_t *e = &s_entries[s_count++];
    e->sid = sid;
    e->handler = handler;
    e->session_mask = session_mask;
    e->addressMask = address_mask;
    e->securityMask = security_mask;
    return true;
}

void uds_server_dispatch_set_security_level(uint32_t active_level_mask) {
    s_active_security_mask = active_level_mask;
}

uint32_t uds_server_dispatch_get_security_level(void) {
    return s_active_security_mask;
}

void uds_server_dispatch_set_session(uint8_t session_mask) {
    s_active_session_mask = session_mask;
}

uint8_t uds_server_dispatch_get_session(void) {
    return s_active_session_mask;
}

static void build_nrc(uds_response_t *resp, uint8_t sid, uint8_t nrc) {
    resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
    resp->data[1] = sid;
    resp->data[2] = nrc;
    resp->len = 3U;
    resp->suppress = false;
}

bool uds_server_dispatch_process(const uint8_t *data, uint16_t len, bool functional, uds_response_t *resp) {
    if ((data == NULL) || (len < 1U) || (resp == NULL)) return false;

    uint8_t sid = data[0];
    memset(resp, 0, sizeof(*resp));

    /* Find service (search reverse for override support) */
    const uds_service_entry_ext_t *entry = NULL;
    for (int32_t i = (int32_t)s_count - 1; i >= 0; i--) {
        if (s_entries[i].sid == sid) {
            entry = &s_entries[i];
            break;
        }
    }

    if (entry == NULL) {
        if (functional) return false; /* Do not respond to unsupported functional requests */
        build_nrc(resp, sid, NRC_SERVICE_NOT_SUPPORTED);
        return true;
    }

    /* 1. Addressing Mode Gate (ISO 14229-1) */
    if (functional) {
        if ((entry->addressMask & UDS_ADDR_FUNCTIONAL) == 0U) {
            /* Functional request for non-functional service: Drop silently without response! */
            return false;
        }
    } else {
        if ((entry->addressMask & UDS_ADDR_PHYSICAL) == 0U) {
            build_nrc(resp, sid, NRC_SERVICE_NOT_SUPPORTED);
            return true;
        }
    }

    /* 2. Diagnostic Session Gate */
    if ((entry->session_mask & s_active_session_mask) == 0U) {
        if (functional) return false;
        build_nrc(resp, sid, NRC_SERVICE_NOT_SUPPORTED_IN_SESSION);
        return true;
    }

    /* 3. Security Level Gate */
    if ((entry->securityMask & s_active_security_mask) == 0U) {
        if (functional) return false;
        build_nrc(resp, sid, NRC_SECURITY_ACCESS_DENIED);
        return true;
    }

    /* Build request and dispatch */
    uds_request_t req;
    req.sid = sid;
    req.data = (len > 1U) ? &data[1] : NULL;
    req.data_len = (len > 1U) ? (len - 1U) : 0U;
    req.functional = functional;

    entry->handler(&req, resp);

    if (resp->suppress) {
        return false;
    }

    return (resp->len > 0U);
}
