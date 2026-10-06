#include "svc_comm_control.h"

static uint8_t s_comm_subfunction = 0x00U;
static uint8_t s_comm_type = 0x01U;

void svc_comm_control_reset(void) {
    s_comm_subfunction = 0x00U;
    s_comm_type = 0x01U;
}

uint8_t svc_comm_control_get_subfunction(void) {
    return s_comm_subfunction;
}

uint8_t svc_comm_control_get_type(void) {
    return s_comm_type;
}

void svc_comm_control(const uds_request_t *req, uds_response_t *resp) {
    if (req->data_len < 2U) {
        resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
        resp->data[1] = req->sid;
        resp->data[2] = NRC_INCORRECT_MSG_LEN_OR_FORMAT;
        resp->len = 3U;
        return;
    }

    uint8_t raw_sub = req->data[0];
    uint8_t sub = raw_sub & 0x7FU;
    bool suppress = (raw_sub & 0x80U) != 0U;
    uint8_t comm_type = req->data[1];

    if (sub > 0x03U) {
        resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
        resp->data[1] = req->sid;
        resp->data[2] = NRC_SUBFUNCTION_NOT_SUPPORTED;
        resp->len = 3U;
        return;
    }

    if ((comm_type < 0x01U) || (comm_type > 0x03U)) {
        resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
        resp->data[1] = req->sid;
        resp->data[2] = NRC_REQUEST_OUT_OF_RANGE;
        resp->len = 3U;
        return;
    }

    s_comm_subfunction = sub;
    s_comm_type = comm_type;

    if (suppress) {
        resp->suppress = true;
        resp->len = 0U;
        return;
    }

    resp->data[0] = UDS_POSITIVE_RESPONSE(req->sid);
    resp->data[1] = sub;
    resp->len = 2U;
}
