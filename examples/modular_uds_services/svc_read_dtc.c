#include "svc_read_dtc.h"
#include <string.h>

/* Mock DTC entry structure for the modular service example */
typedef struct {
    uint32_t dtc;
    uint8_t status;
    bool has_snapshot;
    uint8_t snapshot[8];
    uint8_t snapshot_len;
    bool has_extdata;
    uint8_t extdata[8];
    uint8_t extdata_len;
} dtc_entry_t;

static const dtc_entry_t s_mock_dtcs[] = {
    {0x010000U, 0x2FU, true, {0x01, 0x02}, 2U, true, {0x10, 0x20}, 2U},
    {0xC10000U, 0x08U, false, {0}, 0U, false, {0}, 0U},
    {0x800100U, 0x01U, false, {0}, 0U, false, {0}, 0U},
};

static uint8_t dtc_store_count(void) {
    return (uint8_t)(sizeof(s_mock_dtcs) / sizeof(s_mock_dtcs[0]));
}

static const dtc_entry_t *dtc_store_get_by_index(uint8_t index) {
    if (index >= dtc_store_count()) return NULL;
    return &s_mock_dtcs[index];
}

static const dtc_entry_t *dtc_store_find(uint32_t dtc) {
    for (uint8_t i = 0U; i < dtc_store_count(); ++i) {
        if (s_mock_dtcs[i].dtc == dtc) return &s_mock_dtcs[i];
    }
    return NULL;
}

static uint8_t dtc_store_get_status_mask(void) {
    return 0xFFU;
}

static uint16_t dtc_store_count_by_mask(uint8_t status_mask) {
    uint16_t count = 0U;
    for (uint8_t i = 0U; i < dtc_store_count(); ++i) {
        if ((s_mock_dtcs[i].status & status_mask) != 0U) {
            count++;
        }
    }
    return count;
}

void svc_read_dtc_info(const uds_request_t *req, uds_response_t *resp) {
    if (req->data_len < 1U) {
        resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
        resp->data[1] = req->sid;
        resp->data[2] = NRC_INCORRECT_MSG_LEN_OR_FORMAT;
        resp->len = 3U;
        return;
    }

    uint8_t raw_sub = req->data[0];
    uint8_t sub = raw_sub & 0x7FU;
    bool suppress = (raw_sub & 0x80U) != 0U;
    uint8_t avail_mask = dtc_store_get_status_mask();

    switch (sub) {
    case 0x01: {
        /* reportNumberOfDTCByStatusMask */
        if (req->data_len < 2U) {
            resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
            resp->data[1] = req->sid;
            resp->data[2] = NRC_INCORRECT_MSG_LEN_OR_FORMAT;
            resp->len = 3U;
            return;
        }
        uint8_t req_mask = req->data[1];
        uint16_t count = dtc_store_count_by_mask(req_mask);

        resp->data[0] = UDS_POSITIVE_RESPONSE(req->sid);
        resp->data[1] = sub;
        resp->data[2] = avail_mask;
        resp->data[3] = 0x01U; /* DTC format: ISO 14229-1 */
        resp->data[4] = (uint8_t)(count >> 8U);
        resp->data[5] = (uint8_t)(count & 0xFFU);
        resp->len = 6U;
        break;
    }

    case 0x02: {
        /* reportDTCByStatusMask */
        if (req->data_len < 2U) {
            resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
            resp->data[1] = req->sid;
            resp->data[2] = NRC_INCORRECT_MSG_LEN_OR_FORMAT;
            resp->len = 3U;
            return;
        }
        uint8_t req_mask = req->data[1];

        resp->data[0] = UDS_POSITIVE_RESPONSE(req->sid);
        resp->data[1] = sub;
        resp->data[2] = avail_mask;
        uint16_t pos = 3U;

        uint8_t total = dtc_store_count();
        for (uint8_t i = 0U; i < total; i++) {
            const dtc_entry_t *e = dtc_store_get_by_index(i);
            if ((e == NULL) || ((e->status & req_mask) == 0U)) continue;

            if ((pos + 4U) > ISOTP_TX_BUF_SIZE) {
                resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
                resp->data[1] = req->sid;
                resp->data[2] = NRC_RESPONSE_TOO_LONG;
                resp->len = 3U;
                return;
            }

            resp->data[pos++] = (uint8_t)(e->dtc >> 16U);
            resp->data[pos++] = (uint8_t)(e->dtc >> 8U);
            resp->data[pos++] = (uint8_t)(e->dtc & 0xFFU);
            resp->data[pos++] = e->status;
        }

        resp->len = pos;
        break;
    }

    case 0x0A: {
        /* reportSupportedDTC (ISO 14229-1 Section 11.3.1.10) */
        resp->data[0] = UDS_POSITIVE_RESPONSE(req->sid);
        resp->data[1] = sub;
        resp->data[2] = avail_mask;
        uint16_t pos = 3U;

        uint8_t total = dtc_store_count();
        for (uint8_t i = 0U; i < total; i++) {
            const dtc_entry_t *e = dtc_store_get_by_index(i);
            if (e == NULL) continue;

            if ((pos + 4U) > ISOTP_TX_BUF_SIZE) {
                resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
                resp->data[1] = req->sid;
                resp->data[2] = NRC_RESPONSE_TOO_LONG;
                resp->len = 3U;
                return;
            }

            resp->data[pos++] = (uint8_t)(e->dtc >> 16U);
            resp->data[pos++] = (uint8_t)(e->dtc >> 8U);
            resp->data[pos++] = (uint8_t)(e->dtc & 0xFFU);
            resp->data[pos++] = e->status;
        }

        resp->len = pos;
        break;
    }

    case 0x04: {
        /* reportDTCSnapshotRecordByDTCNumber */
        if (req->data_len < 4U) {
            resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
            resp->data[1] = req->sid;
            resp->data[2] = NRC_INCORRECT_MSG_LEN_OR_FORMAT;
            resp->len = 3U;
            return;
        }
        uint32_t dtc = ((uint32_t)req->data[1] << 16U) |
                       ((uint32_t)req->data[2] << 8U)  |
                       (uint32_t)req->data[3];

        const dtc_entry_t *e = dtc_store_find(dtc);
        if (e == NULL) {
            resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
            resp->data[1] = req->sid;
            resp->data[2] = NRC_REQUEST_OUT_OF_RANGE;
            resp->len = 3U;
            return;
        }

        resp->data[0] = UDS_POSITIVE_RESPONSE(req->sid);
        resp->data[1] = sub;
        resp->data[2] = (uint8_t)(e->dtc >> 16U);
        resp->data[3] = (uint8_t)(e->dtc >> 8U);
        resp->data[4] = (uint8_t)(e->dtc & 0xFFU);
        resp->data[5] = e->status;
        uint16_t pos = 6U;

        if (e->has_snapshot) {
            resp->data[pos++] = 0x01U;
            if ((pos + e->snapshot_len) <= ISOTP_TX_BUF_SIZE) {
                memcpy(&resp->data[pos], e->snapshot, e->snapshot_len);
                pos = (uint16_t)(pos + e->snapshot_len);
            }
        }

        resp->len = pos;
        break;
    }

    case 0x06: {
        /* reportDTCExtDataRecordByDTCNumber */
        if (req->data_len < 4U) {
            resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
            resp->data[1] = req->sid;
            resp->data[2] = NRC_INCORRECT_MSG_LEN_OR_FORMAT;
            resp->len = 3U;
            return;
        }
        uint32_t dtc = ((uint32_t)req->data[1] << 16U) |
                       ((uint32_t)req->data[2] << 8U)  |
                       (uint32_t)req->data[3];

        const dtc_entry_t *e = dtc_store_find(dtc);
        if (e == NULL) {
            resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
            resp->data[1] = req->sid;
            resp->data[2] = NRC_REQUEST_OUT_OF_RANGE;
            resp->len = 3U;
            return;
        }

        resp->data[0] = UDS_POSITIVE_RESPONSE(req->sid);
        resp->data[1] = sub;
        resp->data[2] = (uint8_t)(e->dtc >> 16U);
        resp->data[3] = (uint8_t)(e->dtc >> 8U);
        resp->data[4] = (uint8_t)(e->dtc & 0xFFU);
        resp->data[5] = e->status;
        uint16_t pos = 6U;

        if (e->has_extdata) {
            resp->data[pos++] = 0x01U;
            if ((pos + e->extdata_len) <= ISOTP_TX_BUF_SIZE) {
                memcpy(&resp->data[pos], e->extdata, e->extdata_len);
                pos = (uint16_t)(pos + e->extdata_len);
            }
        }

        resp->len = pos;
        break;
    }

    default:
        resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
        resp->data[1] = req->sid;
        resp->data[2] = NRC_SUBFUNCTION_NOT_SUPPORTED;
        resp->len = 3U;
        return;
    }

    if (suppress) {
        resp->suppress = true;
        resp->len = 0U;
    }
}
