/**
 * @file svc_clear_dtc.c
 * @brief UDS Service 0x14: ClearDiagnosticInformation (ISO 14229-1)
 * @details Fixes Issue #115: Supports clearing all DTCs (0xFFFFFF), group masks,
 *          and individual 24-bit DTC codes safely without wiping other DTCs.
 */

#include "svc_clear_dtc.h"
#include "dtc_store.h"

static bool s_enforce_session = false;

void svc_clear_dtc_set_session_check(bool enforce) {
    s_enforce_session = enforce;
}

void svc_clear_dtc(const uds_request_t *req, uds_response_t *resp) {
    /* ISO 14229-1: Service 0x14 request MUST contain exactly 3 bytes of groupOfDTC */
    if (req->data_len != 3U) {
        resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
        resp->data[1] = req->sid;
        resp->data[2] = NRC_INCORRECT_MSG_LEN_OR_FORMAT;
        resp->len = 3U;
        return;
    }

    uint32_t group_or_dtc = ((uint32_t)req->data[0] << 16U) |
                            ((uint32_t)req->data[1] << 8U)  |
                            (uint32_t)req->data[2];

    /* Clear matching DTC(s) via unified store */
    bool cleared = dtc_store_clear(group_or_dtc);
    if (!cleared) {
        resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
        resp->data[1] = req->sid;
        resp->data[2] = NRC_REQUEST_OUT_OF_RANGE;
        resp->len = 3U;
        return;
    }

    /* Positive response: 0x54 */
    resp->data[0] = UDS_POSITIVE_RESPONSE(req->sid);
    resp->len = 1U;
}
