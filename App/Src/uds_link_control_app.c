/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#include "uds_link_control_app.h"

#include <stddef.h>

static bool s_verified = false;
static uint32_t s_pending_baudrate = UDS_LINK_DEFAULT_BAUDRATE;
static uint32_t s_active_baudrate = UDS_LINK_DEFAULT_BAUDRATE;

static const UdsLinkControlServiceBackend s_link_control_backend = {
    .link_control = uds_link_control_app_handler,
};

void uds_link_control_app_init(void) {
    s_verified = false;
    s_pending_baudrate = UDS_LINK_DEFAULT_BAUDRATE;
    s_active_baudrate = UDS_LINK_DEFAULT_BAUDRATE;
}

const UdsLinkControlServiceBackend *uds_link_control_app_get_backend(void) {
    return &s_link_control_backend;
}

static bool fixed_mode_to_baudrate(uint8_t mode, uint32_t *baudrate) {
    switch (mode) {
    case UDS_LINK_MODE_9600_BAUD:
        *baudrate = 9600U;
        return true;
    case UDS_LINK_MODE_19200_BAUD:
        *baudrate = 19200U;
        return true;
    case UDS_LINK_MODE_38400_BAUD:
        *baudrate = 38400U;
        return true;
    case UDS_LINK_MODE_57600_BAUD:
        *baudrate = 57600U;
        return true;
    case UDS_LINK_MODE_115200_BAUD:
        *baudrate = 115200U;
        return true;
    case UDS_LINK_MODE_125K_CAN:
        *baudrate = 125000U;
        return true;
    case UDS_LINK_MODE_250K_CAN:
        *baudrate = 250000U;
        return true;
    case UDS_LINK_MODE_500K_CAN:
        *baudrate = 500000U;
        return true;
    case UDS_LINK_MODE_1M_CAN:
        *baudrate = 1000000U;
        return true;
    default:
        return false;
    }
}

static bool is_supported_specific_baudrate(uint32_t baudrate) {
    return (baudrate == 125000U) || (baudrate == 250000U) || (baudrate == 500000U) ||
           (baudrate == 1000000U);
}

UdsCallbackResult uds_link_control_app_handler(void *context, const uint8_t *request,
                                               uint16_t request_length, uint8_t *response,
                                               uint16_t *response_length,
                                               uint16_t response_capacity) {
    (void)context;
    if ((request == NULL) || (response == NULL) || (response_length == NULL)) {
        return UDS_RESULT_ERROR;
    }
    if (request_length < 2U) {
        return UDS_RESULT_INVALID_FORMAT;
    }

    uint8_t subfunction = request[1] & 0x7FU;

    switch (subfunction) {
    case UDS_LINK_CONTROL_VERIFY_FIXED: {
        if (request_length != 3U) {
            return UDS_RESULT_INVALID_FORMAT;
        }
        uint32_t target_baud = 0U;
        if (!fixed_mode_to_baudrate(request[2], &target_baud)) {
            return UDS_RESULT_OUT_OF_RANGE;
        }
        s_pending_baudrate = target_baud;
        s_verified = true;
        break;
    }

    case UDS_LINK_CONTROL_VERIFY_SPECIFIC: {
        if (request_length != 5U) {
            return UDS_RESULT_INVALID_FORMAT;
        }
        uint32_t target_baud =
            ((uint32_t)request[2] << 16U) | ((uint32_t)request[3] << 8U) | (uint32_t)request[4];
        if (!is_supported_specific_baudrate(target_baud)) {
            return UDS_RESULT_OUT_OF_RANGE;
        }
        s_pending_baudrate = target_baud;
        s_verified = true;
        break;
    }

    case UDS_LINK_CONTROL_TRANSITION_MODE: {
        if (request_length != 2U) {
            return UDS_RESULT_INVALID_FORMAT;
        }
        if (!s_verified) {
            return UDS_RESULT_SEQUENCE_ERROR;
        }
        s_active_baudrate = s_pending_baudrate;
        s_verified = false;
        break;
    }

    default:
        return UDS_RESULT_SUBFUNCTION_NOT_SUPPORTED;
    }

    if (response_capacity < 2U) {
        return UDS_RESULT_RESPONSE_TOO_LONG;
    }

    response[0] = 0xC7U;
    response[1] = subfunction;
    *response_length = 2U;
    return UDS_RESULT_OK;
}

bool uds_link_control_app_is_verified(void) {
    return s_verified;
}

uint32_t uds_link_control_app_get_baudrate(void) {
    return s_active_baudrate;
}

void uds_link_control_app_reset(void) {
    s_verified = false;
    s_pending_baudrate = UDS_LINK_DEFAULT_BAUDRATE;
    s_active_baudrate = UDS_LINK_DEFAULT_BAUDRATE;
}
