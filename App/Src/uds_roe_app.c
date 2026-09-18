/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#include "uds_roe_app.h"

#include <stddef.h>
#include <string.h>

#define UDS_ROE_MAX_SERVICE_RECORD 16U

typedef struct {
    bool configured;
    bool active;
    uint8_t event_type;
    uint8_t event_window_time;
    uint8_t dtc_status_mask;
    uint16_t monitored_did;
    uint8_t service_record_len;
    uint8_t service_record[UDS_ROE_MAX_SERVICE_RECORD];
} UdsRoeEventState;

static UdsRoeEventState s_roe_state;

static const UdsPeriodicEventServiceBackend s_roe_backend = {
    .max_pending_items = 4U,
    .periodic_data = NULL,
    .event_response = uds_roe_app_handler,
};

void uds_roe_app_init(void) {
    (void)memset(&s_roe_state, 0, sizeof(s_roe_state));
}

const UdsPeriodicEventServiceBackend *uds_roe_app_get_backend(void) {
    return &s_roe_backend;
}

static UdsCallbackResult handle_stop(const uint8_t *request, uint16_t request_length,
                                     uint8_t *response, uint16_t *response_length,
                                     uint16_t response_capacity) {
    (void)request;
    if (request_length < 2U) {
        return UDS_RESULT_INVALID_FORMAT;
    }
    if (response_capacity < 4U) {
        return UDS_RESULT_RESPONSE_TOO_LONG;
    }

    s_roe_state.active = false;

    response[0] = 0xC6U;
    response[1] = UDS_ROE_STOP_RESPONSE_ON_EVENT;
    response[2] = 0x00U; /* eventStatus: stopped */
    response[3] = s_roe_state.configured ? s_roe_state.event_window_time : 0x00U;
    *response_length = 4U;
    return UDS_RESULT_OK;
}

static UdsCallbackResult handle_on_dtc_change(const uint8_t *request, uint16_t request_length,
                                              uint8_t *response, uint16_t *response_length,
                                              uint16_t response_capacity) {
    if (request_length < 4U) {
        return UDS_RESULT_INVALID_FORMAT;
    }

    uint8_t extra_len = (uint8_t)(request_length - 4U);
    if (extra_len > UDS_ROE_MAX_SERVICE_RECORD) {
        extra_len = UDS_ROE_MAX_SERVICE_RECORD;
    }

    uint16_t needed_len = (uint16_t)(5U + extra_len);
    if (response_capacity < needed_len) {
        return UDS_RESULT_RESPONSE_TOO_LONG;
    }

    s_roe_state.configured = true;
    s_roe_state.active = false;
    s_roe_state.event_type = UDS_ROE_ON_DTC_STATUS_CHANGE;
    s_roe_state.event_window_time = request[2];
    s_roe_state.dtc_status_mask = request[3];
    s_roe_state.monitored_did = 0U;
    s_roe_state.service_record_len = extra_len;
    if (extra_len > 0U) {
        (void)memcpy(s_roe_state.service_record, &request[4], extra_len);
    }

    response[0] = 0xC6U;
    response[1] = UDS_ROE_ON_DTC_STATUS_CHANGE;
    response[2] = 0x00U; /* eventStatus: configured / stopped */
    response[3] = s_roe_state.event_window_time;
    response[4] = s_roe_state.dtc_status_mask;
    if (extra_len > 0U) {
        (void)memcpy(&response[5], s_roe_state.service_record, extra_len);
    }
    *response_length = needed_len;
    return UDS_RESULT_OK;
}

static UdsCallbackResult handle_on_did_change(const uint8_t *request, uint16_t request_length,
                                              uint8_t *response, uint16_t *response_length,
                                              uint16_t response_capacity) {
    if (request_length < 5U) {
        return UDS_RESULT_INVALID_FORMAT;
    }

    uint8_t extra_len = (uint8_t)(request_length - 5U);
    if (extra_len > UDS_ROE_MAX_SERVICE_RECORD) {
        extra_len = UDS_ROE_MAX_SERVICE_RECORD;
    }

    uint16_t needed_len = (uint16_t)(6U + extra_len);
    if (response_capacity < needed_len) {
        return UDS_RESULT_RESPONSE_TOO_LONG;
    }

    s_roe_state.configured = true;
    s_roe_state.active = false;
    s_roe_state.event_type = UDS_ROE_ON_CHANGE_OF_DATA_IDENTIFIER;
    s_roe_state.event_window_time = request[2];
    s_roe_state.dtc_status_mask = 0U;
    s_roe_state.monitored_did = (uint16_t)(((uint16_t)request[3] << 8U) | (uint16_t)request[4]);
    s_roe_state.service_record_len = extra_len;
    if (extra_len > 0U) {
        (void)memcpy(s_roe_state.service_record, &request[5], extra_len);
    }

    response[0] = 0xC6U;
    response[1] = UDS_ROE_ON_CHANGE_OF_DATA_IDENTIFIER;
    response[2] = 0x00U; /* eventStatus: configured / stopped */
    response[3] = s_roe_state.event_window_time;
    response[4] = request[3];
    response[5] = request[4];
    if (extra_len > 0U) {
        (void)memcpy(&response[6], s_roe_state.service_record, extra_len);
    }
    *response_length = needed_len;
    return UDS_RESULT_OK;
}

static UdsCallbackResult handle_report_activated(const uint8_t *request, uint16_t request_length,
                                                 uint8_t *response, uint16_t *response_length,
                                                 uint16_t response_capacity) {
    (void)request;
    if (request_length != 2U) {
        return UDS_RESULT_INVALID_FORMAT;
    }

    if (!s_roe_state.active) {
        if (response_capacity < 3U) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        response[0] = 0xC6U;
        response[1] = UDS_ROE_REPORT_ACTIVATED_EVENTS;
        response[2] = 0x00U; /* 0 active events */
        *response_length = 3U;
        return UDS_RESULT_OK;
    }

    uint16_t needed_len = (uint16_t)(5U + s_roe_state.service_record_len);
    if (s_roe_state.event_type == UDS_ROE_ON_CHANGE_OF_DATA_IDENTIFIER) {
        needed_len = (uint16_t)(needed_len + 1U);
    }
    if (response_capacity < needed_len) {
        return UDS_RESULT_RESPONSE_TOO_LONG;
    }

    response[0] = 0xC6U;
    response[1] = UDS_ROE_REPORT_ACTIVATED_EVENTS;
    response[2] = 0x01U; /* 1 active event */
    response[3] = s_roe_state.event_type;
    response[4] = s_roe_state.event_window_time;

    uint16_t offset = 5U;
    if (s_roe_state.event_type == UDS_ROE_ON_DTC_STATUS_CHANGE) {
        response[offset] = s_roe_state.dtc_status_mask;
        offset = (uint16_t)(offset + 1U);
    } else if (s_roe_state.event_type == UDS_ROE_ON_CHANGE_OF_DATA_IDENTIFIER) {
        response[offset] = (uint8_t)(s_roe_state.monitored_did >> 8U);
        offset = (uint16_t)(offset + 1U);
        response[offset] = (uint8_t)(s_roe_state.monitored_did & 0xFFU);
        offset = (uint16_t)(offset + 1U);
    }
    if (s_roe_state.service_record_len > 0U) {
        (void)memcpy(&response[offset], s_roe_state.service_record, s_roe_state.service_record_len);
        offset = (uint16_t)(offset + s_roe_state.service_record_len);
    }

    *response_length = offset;
    return UDS_RESULT_OK;
}

static UdsCallbackResult handle_start(const uint8_t *request, uint16_t request_length,
                                      uint8_t *response, uint16_t *response_length,
                                      uint16_t response_capacity) {
    (void)request;
    if (request_length < 2U) {
        return UDS_RESULT_INVALID_FORMAT;
    }
    if (!s_roe_state.configured) {
        return UDS_RESULT_DENIED;
    }
    if (response_capacity < 4U) {
        return UDS_RESULT_RESPONSE_TOO_LONG;
    }

    s_roe_state.active = true;

    response[0] = 0xC6U;
    response[1] = UDS_ROE_START_RESPONSE_ON_EVENT;
    response[2] = 0x01U; /* eventStatus: started */
    response[3] = s_roe_state.event_window_time;
    *response_length = 4U;
    return UDS_RESULT_OK;
}

static UdsCallbackResult handle_clear(const uint8_t *request, uint16_t request_length,
                                      uint8_t *response, uint16_t *response_length,
                                      uint16_t response_capacity) {
    (void)request;
    if (request_length != 2U) {
        return UDS_RESULT_INVALID_FORMAT;
    }
    if (response_capacity < 4U) {
        return UDS_RESULT_RESPONSE_TOO_LONG;
    }

    s_roe_state.configured = false;
    s_roe_state.active = false;
    s_roe_state.event_type = 0U;
    s_roe_state.event_window_time = 0U;
    s_roe_state.dtc_status_mask = 0U;
    s_roe_state.monitored_did = 0U;
    s_roe_state.service_record_len = 0U;

    response[0] = 0xC6U;
    response[1] = UDS_ROE_CLEAR_RESPONSE_ON_EVENT;
    response[2] = 0x00U; /* eventStatus: cleared */
    response[3] = 0x00U; /* eventWindowTime: 0 */
    *response_length = 4U;
    return UDS_RESULT_OK;
}

UdsCallbackResult uds_roe_app_handler(void *context, const uint8_t *request,
                                      uint16_t request_length, uint8_t *response,
                                      uint16_t *response_length, uint16_t response_capacity) {
    (void)context;
    if ((request == NULL) || (response == NULL) || (response_length == NULL)) {
        return UDS_RESULT_ERROR;
    }
    if (request_length < 2U) {
        return UDS_RESULT_INVALID_FORMAT;
    }

    uint8_t subfunction = request[1] & 0x7FU;

    switch (subfunction) {
    case UDS_ROE_STOP_RESPONSE_ON_EVENT:
        return handle_stop(request, request_length, response, response_length, response_capacity);

    case UDS_ROE_ON_DTC_STATUS_CHANGE:
        return handle_on_dtc_change(request, request_length, response, response_length,
                                    response_capacity);

    case UDS_ROE_ON_CHANGE_OF_DATA_IDENTIFIER:
        return handle_on_did_change(request, request_length, response, response_length,
                                    response_capacity);

    case UDS_ROE_REPORT_ACTIVATED_EVENTS:
        return handle_report_activated(request, request_length, response, response_length,
                                       response_capacity);

    case UDS_ROE_START_RESPONSE_ON_EVENT:
        return handle_start(request, request_length, response, response_length, response_capacity);

    case UDS_ROE_CLEAR_RESPONSE_ON_EVENT:
        return handle_clear(request, request_length, response, response_length, response_capacity);

    default:
        return UDS_RESULT_SUBFUNCTION_NOT_SUPPORTED;
    }
}

bool uds_roe_app_is_active(void) {
    return s_roe_state.active;
}

bool uds_roe_app_is_configured(void) {
    return s_roe_state.configured;
}

uint8_t uds_roe_app_get_event_type(void) {
    return s_roe_state.event_type;
}

uint8_t uds_roe_app_get_event_window_time(void) {
    return s_roe_state.event_window_time;
}

uint8_t uds_roe_app_get_dtc_status_mask(void) {
    return s_roe_state.dtc_status_mask;
}

uint16_t uds_roe_app_get_monitored_did(void) {
    return s_roe_state.monitored_did;
}

void uds_roe_app_clear(void) {
    (void)memset(&s_roe_state, 0, sizeof(s_roe_state));
}
