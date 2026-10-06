#include "svc_read_address.h"
#include <string.h>

static bool is_valid_read_address(uint32_t addr, uint32_t size) {
    if ((addr + size) < addr) {
        return false;
    }
    /* SRAM: 0x20000000 to 0x20005000 (20KB) */
    if ((addr >= 0x20000000U) && ((addr + size) <= 0x20005000U)) {
        return true;
    }
    /* Flash: 0x08000000 to 0x08010000 (64KB) */
    if ((addr >= 0x08000000U) && ((addr + size) <= 0x08010000U)) {
        return true;
    }
    return false;
}

void svc_read_memory_by_address(const uds_request_t *req, uds_response_t *resp) {
    if (req->data_len < 3U) {
        resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
        resp->data[1] = req->sid;
        resp->data[2] = NRC_INCORRECT_MSG_LEN_OR_FORMAT;
        resp->len = 3U;
        return;
    }

    uint8_t alfid = req->data[0];
    uint8_t addr_len = (uint8_t)(alfid & 0x0FU);
    uint8_t size_len = (uint8_t)((alfid >> 4U) & 0x0FU);

    if ((addr_len < 1U) || (addr_len > 4U) || (size_len < 1U) || (size_len > 4U)) {
        resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
        resp->data[1] = req->sid;
        resp->data[2] = NRC_REQUEST_OUT_OF_RANGE;
        resp->len = 3U;
        return;
    }

    if (req->data_len != (uint16_t)(1U + addr_len + size_len)) {
        resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
        resp->data[1] = req->sid;
        resp->data[2] = NRC_INCORRECT_MSG_LEN_OR_FORMAT;
        resp->len = 3U;
        return;
    }

    uint32_t address = 0U;
    uint32_t size = 0U;
    for (uint8_t i = 0U; i < addr_len; i++) {
        address = (address << 8U) | (uint32_t)req->data[1U + i];
    }
    for (uint8_t i = 0U; i < size_len; i++) {
        size = (size << 8U) | (uint32_t)req->data[1U + addr_len + i];
    }

    if ((size == 0U) || !is_valid_read_address(address, size)) {
        resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
        resp->data[1] = req->sid;
        resp->data[2] = NRC_REQUEST_OUT_OF_RANGE;
        resp->len = 3U;
        return;
    }

    if ((1U + size) > ISOTP_TX_BUF_SIZE) {
        resp->data[0] = UDS_SID_NEGATIVE_RESPONSE;
        resp->data[1] = req->sid;
        resp->data[2] = NRC_RESPONSE_TOO_LONG;
        resp->len = 3U;
        return;
    }

    resp->data[0] = UDS_POSITIVE_RESPONSE(req->sid);
    memcpy(&resp->data[1], (const void *)address, size);
    resp->len = (uint16_t)(1U + size);
}
