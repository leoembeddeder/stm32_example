#include "uds_dtc_app.h"

#include <string.h>

static UdsDtcAppStorage s_dtc_storage;

static bool append_byte(uint8_t *resp, uint16_t *len, uint16_t cap, uint8_t val) {
    if ((*len >= cap) || (resp == NULL)) {
        return false;
    }
    resp[(*len)++] = val;
    return true;
}

static bool append_dtc(uint8_t *resp, uint16_t *len, uint16_t cap, const UdsDtcAppRecord *rec) {
    return append_byte(resp, len, cap, (uint8_t)(rec->dtc_number >> 16U)) &&
           append_byte(resp, len, cap, (uint8_t)(rec->dtc_number >> 8U)) &&
           append_byte(resp, len, cap, (uint8_t)rec->dtc_number) &&
           append_byte(resp, len, cap, rec->status_byte);
}

static bool dtc_matches_request(const UdsDtcAppRecord *rec, const uint8_t *req, uint16_t req_len) {
    if (req_len < 5U) {
        return true;
    }
    uint32_t target_dtc = ((uint32_t)req[2] << 16U) | ((uint32_t)req[3] << 8U) | (uint32_t)req[4];
    return (target_dtc == 0xFFFFFFUL) || (target_dtc == rec->dtc_number);
}

static UdsCallbackResult uds_dtc_app_report(void *context, uint8_t subfunction,
                                            const uint8_t *request, uint16_t request_length,
                                            uint8_t *response, uint16_t *response_length,
                                            uint16_t response_capacity) {
    (void)context;
    if ((request == NULL) || (response == NULL) || (response_length == NULL) ||
        (response_capacity < 2U)) {
        return UDS_RESULT_ERROR;
    }

    uint16_t len = 0U;
    if (!append_byte(response, &len, response_capacity, subfunction)) {
        return UDS_RESULT_ERROR;
    }

    uint8_t status_mask = (request_length >= 3U) ? request[2] : 0xFFU;

    switch (subfunction) {
    /* 0x01, 0x07, 0x11, 0x12: Count reporting */
    case 0x01U:
    case 0x07U:
    case 0x11U:
    case 0x12U: {
        uint16_t count = 0U;
        for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
            const UdsDtcAppRecord *rec = &s_dtc_storage.records[i];
            if (rec->active && ((rec->status_byte & status_mask) != 0U)) {
                count++;
            }
        }
        if (!append_byte(response, &len, response_capacity,
                         s_dtc_storage.status_availability_mask) ||
            !append_byte(response, &len, response_capacity, 0x01U) /* ISO14229-1 format */ ||
            !append_byte(response, &len, response_capacity, (uint8_t)(count >> 8U)) ||
            !append_byte(response, &len, response_capacity, (uint8_t)count)) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        break;
    }

    /* 0x02, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x13, 0x15, 0x17, 0x42, 0x55: List reporting */
    case 0x02U:
    case 0x0AU:
    case 0x0BU:
    case 0x0CU:
    case 0x0DU:
    case 0x0EU:
    case 0x0FU:
    case 0x13U:
    case 0x15U:
    case 0x17U:
    case 0x42U:
    case 0x55U: {
        if (!append_byte(response, &len, response_capacity,
                         s_dtc_storage.status_availability_mask)) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
            const UdsDtcAppRecord *rec = &s_dtc_storage.records[i];
            if (rec->active &&
                ((subfunction == 0x0AU) || ((rec->status_byte & status_mask) != 0U))) {
                if (!append_dtc(response, &len, response_capacity, rec)) {
                    return UDS_RESULT_RESPONSE_TOO_LONG;
                }
                /* Single record subfunctions terminate after first match */
                if ((subfunction == 0x0BU) || (subfunction == 0x0CU) || (subfunction == 0x0DU) ||
                    (subfunction == 0x0EU)) {
                    break;
                }
            }
        }
        break;
    }

    /* 0x03: Snapshot Identification */
    case 0x03U: {
        for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
            const UdsDtcAppRecord *rec = &s_dtc_storage.records[i];
            if (rec->active && (rec->snapshot_length > 0U)) {
                if (!append_byte(response, &len, response_capacity,
                                 (uint8_t)(rec->dtc_number >> 16U)) ||
                    !append_byte(response, &len, response_capacity,
                                 (uint8_t)(rec->dtc_number >> 8U)) ||
                    !append_byte(response, &len, response_capacity, (uint8_t)rec->dtc_number) ||
                    !append_byte(response, &len, response_capacity,
                                 0x01U /* Snapshot record number */)) {
                    return UDS_RESULT_RESPONSE_TOO_LONG;
                }
            }
        }
        break;
    }

    /* 0x04, 0x05, 0x18: Snapshot Record by DTC Number / Record Number */
    case 0x04U:
    case 0x05U:
    case 0x18U: {
        bool found = false;
        for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
            const UdsDtcAppRecord *rec = &s_dtc_storage.records[i];
            if (rec->active && dtc_matches_request(rec, request, request_length)) {
                found = true;
                if (!append_dtc(response, &len, response_capacity, rec) ||
                    !append_byte(response, &len, response_capacity, 0x01U /* Record # */) ||
                    !append_byte(response, &len, response_capacity, 0x01U /* 1 DID */) ||
                    !append_byte(response, &len, response_capacity, 0x01U /* DID high */) ||
                    !append_byte(response, &len, response_capacity, 0x00U /* DID low */)) {
                    return UDS_RESULT_RESPONSE_TOO_LONG;
                }
                for (uint8_t b = 0U; b < rec->snapshot_length; ++b) {
                    if (!append_byte(response, &len, response_capacity, rec->snapshot_data[b])) {
                        return UDS_RESULT_RESPONSE_TOO_LONG;
                    }
                }
                break;
            }
        }
        if (!found) {
            return UDS_RESULT_OUT_OF_RANGE;
        }
        break;
    }

    /* 0x06, 0x10, 0x16, 0x19: Extended Data Records */
    case 0x06U:
    case 0x10U:
    case 0x16U:
    case 0x19U: {
        bool found = false;
        for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
            const UdsDtcAppRecord *rec = &s_dtc_storage.records[i];
            if (rec->active && dtc_matches_request(rec, request, request_length)) {
                found = true;
                if (!append_dtc(response, &len, response_capacity, rec) ||
                    !append_byte(response, &len, response_capacity, 0x01U /* Record # */)) {
                    return UDS_RESULT_RESPONSE_TOO_LONG;
                }
                for (uint8_t b = 0U; b < rec->extended_length; ++b) {
                    if (!append_byte(response, &len, response_capacity, rec->extended_data[b])) {
                        return UDS_RESULT_RESPONSE_TOO_LONG;
                    }
                }
                break;
            }
        }
        if (!found) {
            return UDS_RESULT_OUT_OF_RANGE;
        }
        break;
    }

    /* 0x08: Severity Record */
    case 0x08U: {
        if (!append_byte(response, &len, response_capacity,
                         s_dtc_storage.status_availability_mask)) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
            const UdsDtcAppRecord *rec = &s_dtc_storage.records[i];
            if (rec->active && ((rec->status_byte & status_mask) != 0U)) {
                if (!append_byte(response, &len, response_capacity, rec->severity) ||
                    !append_byte(response, &len, response_capacity, rec->functional_unit) ||
                    !append_dtc(response, &len, response_capacity, rec)) {
                    return UDS_RESULT_RESPONSE_TOO_LONG;
                }
            }
        }
        break;
    }

    /* 0x09: Severity Information of DTC */
    case 0x09U: {
        bool found = false;
        for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
            const UdsDtcAppRecord *rec = &s_dtc_storage.records[i];
            if (rec->active && dtc_matches_request(rec, request, request_length)) {
                found = true;
                if (!append_byte(response, &len, response_capacity,
                                 s_dtc_storage.status_availability_mask) ||
                    !append_byte(response, &len, response_capacity, rec->severity) ||
                    !append_byte(response, &len, response_capacity, rec->functional_unit) ||
                    !append_dtc(response, &len, response_capacity, rec)) {
                    return UDS_RESULT_RESPONSE_TOO_LONG;
                }
                break;
            }
        }
        if (!found) {
            return UDS_RESULT_OUT_OF_RANGE;
        }
        break;
    }

    /* 0x14: Fault Detection Counters */
    case 0x14U: {
        for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
            const UdsDtcAppRecord *rec = &s_dtc_storage.records[i];
            if (rec->active) {
                if (!append_byte(response, &len, response_capacity,
                                 (uint8_t)(rec->dtc_number >> 16U)) ||
                    !append_byte(response, &len, response_capacity,
                                 (uint8_t)(rec->dtc_number >> 8U)) ||
                    !append_byte(response, &len, response_capacity, (uint8_t)rec->dtc_number) ||
                    !append_byte(response, &len, response_capacity, (uint8_t)rec->fault_counter)) {
                    return UDS_RESULT_RESPONSE_TOO_LONG;
                }
            }
        }
        break;
    }

    default:
        return UDS_RESULT_NOT_SUPPORTED;
    }

    *response_length = len;
    return UDS_RESULT_OK;
}

void uds_dtc_app_init(void) {
    s_dtc_storage.status_availability_mask = 0xFFU;
    s_dtc_storage.record_count = 3U;

    /* Initialize with realistic automotive DTCs */
    /* DTC 1: P0100 - Mass Air Flow Sensor A Circuit (0x010000) */
    s_dtc_storage.records[0].dtc_number = 0x010000UL;
    s_dtc_storage.records[0].status_byte = 0x2FU; /* Confirmed, pending, test failed */
    s_dtc_storage.records[0].severity = 0x40U;    /* Check at next halt */
    s_dtc_storage.records[0].functional_unit = 0x01U;
    s_dtc_storage.records[0].fault_counter = 127;
    s_dtc_storage.records[0].snapshot_length = 4U;
    s_dtc_storage.records[0].snapshot_data[0] = 0x0CU; /* Engine RPM: 3072 */
    s_dtc_storage.records[0].snapshot_data[1] = 0x00U;
    s_dtc_storage.records[0].snapshot_data[2] = 0x32U; /* Coolant Temp: 50 C */
    s_dtc_storage.records[0].snapshot_data[3] = 0x88U; /* Battery 13.6 V */
    s_dtc_storage.records[0].extended_length = 2U;
    s_dtc_storage.records[0].extended_data[0] = 0x05U; /* Occurrence count: 5 */
    s_dtc_storage.records[0].extended_data[1] = 0x28U; /* Aging counter */
    s_dtc_storage.records[0].active = true;

    /* DTC 2: U0100 - Lost Communication with ECM/PCM (0xC10000) */
    s_dtc_storage.records[1].dtc_number = 0xC10000UL;
    s_dtc_storage.records[1].status_byte = 0x08U; /* Confirmed */
    s_dtc_storage.records[1].severity = 0x80U;    /* Immediate check */
    s_dtc_storage.records[1].functional_unit = 0x02U;
    s_dtc_storage.records[1].fault_counter = 50;
    s_dtc_storage.records[1].snapshot_length = 2U;
    s_dtc_storage.records[1].snapshot_data[0] = 0x00U;
    s_dtc_storage.records[1].snapshot_data[1] = 0x00U;
    s_dtc_storage.records[1].extended_length = 2U;
    s_dtc_storage.records[1].extended_data[0] = 0x01U;
    s_dtc_storage.records[1].extended_data[1] = 0x28U;
    s_dtc_storage.records[1].active = true;

    /* DTC 3: B0001 - Driver Airbag Deployment Loop (0x800100) */
    s_dtc_storage.records[2].dtc_number = 0x800100UL;
    s_dtc_storage.records[2].status_byte = 0x01U; /* Test failed */
    s_dtc_storage.records[2].severity = 0xA0U;    /* Maintenance immediately */
    s_dtc_storage.records[2].functional_unit = 0x03U;
    s_dtc_storage.records[2].fault_counter = 10;
    s_dtc_storage.records[2].snapshot_length = 2U;
    s_dtc_storage.records[2].snapshot_data[0] = 0x12U;
    s_dtc_storage.records[2].snapshot_data[1] = 0x34U;
    s_dtc_storage.records[2].extended_length = 2U;
    s_dtc_storage.records[2].extended_data[0] = 0x02U;
    s_dtc_storage.records[2].extended_data[1] = 0x28U;
    s_dtc_storage.records[2].active = true;

    /* Enable all subfunctions capability bits */
    s_dtc_storage.backend.capabilities = 0x03FFFFFFUL;
    s_dtc_storage.backend.report = uds_dtc_app_report;
}

const UdsDtcBackend *uds_dtc_app_get_backend(void) {
    return &s_dtc_storage.backend;
}

UdsCallbackResult uds_dtc_app_clear(void *context, uint32_t group_of_dtc) {
    (void)context;
    if (group_of_dtc == 0xFFFFFFUL) {
        /* Clear all DTCs - set to 0x50 per AUTOSAR Dem specification */
        for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
            s_dtc_storage.records[i].active = false;
            s_dtc_storage.records[i].status_byte = UDS_DTC_STATUS_CLEARED;
            s_dtc_storage.records[i].fault_counter = 0;
        }
        return UDS_RESULT_OK;
    }
    for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
        if (s_dtc_storage.records[i].dtc_number == group_of_dtc) {
            s_dtc_storage.records[i].active = false;
            s_dtc_storage.records[i].status_byte = UDS_DTC_STATUS_CLEARED;
            s_dtc_storage.records[i].fault_counter = 0;
            return UDS_RESULT_OK;
        }
    }
    return UDS_RESULT_OUT_OF_RANGE;
}

bool uds_dtc_app_set_fault(uint32_t dtc, uint8_t status, uint8_t severity, int8_t counter) {
    for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
        if (s_dtc_storage.records[i].dtc_number == dtc) {
            s_dtc_storage.records[i].active = true;
            s_dtc_storage.records[i].status_byte = status;
            s_dtc_storage.records[i].severity = severity;
            s_dtc_storage.records[i].fault_counter = counter;
            return true;
        }
    }
    if (s_dtc_storage.record_count < UDS_DTC_APP_MAX_RECORDS) {
        uint8_t idx = s_dtc_storage.record_count++;
        s_dtc_storage.records[idx].dtc_number = dtc;
        s_dtc_storage.records[idx].status_byte = status;
        s_dtc_storage.records[idx].severity = severity;
        s_dtc_storage.records[idx].fault_counter = counter;
        s_dtc_storage.records[idx].functional_unit = 0x01U;
        s_dtc_storage.records[idx].snapshot_length = 0U;
        s_dtc_storage.records[idx].extended_length = 0U;
        s_dtc_storage.records[idx].active = true;
        return true;
    }
    return false;
}

bool uds_dtc_app_clear_fault(uint32_t dtc) {
    for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
        if (s_dtc_storage.records[i].dtc_number == dtc) {
            s_dtc_storage.records[i].active = false;
            s_dtc_storage.records[i].status_byte = UDS_DTC_STATUS_CLEARED;
            return true;
        }
    }
    return false;
}

/* AUTOSAR Dem counter-based fault debouncing engine */
bool uds_dtc_app_report_event(uint32_t dtc, bool failed) {
    for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
        if (s_dtc_storage.records[i].dtc_number == dtc) {
            UdsDtcAppRecord *rec = &s_dtc_storage.records[i];
            if (failed) {
                if (rec->fault_counter <= (127 - 16)) {
                    rec->fault_counter = (int8_t)(rec->fault_counter + 16);
                } else {
                    rec->fault_counter = 127;
                }
                if (rec->fault_counter >= 127) {
                    rec->fault_counter = 127;
                    rec->active = true;
                    rec->status_byte |=
                        (uint8_t)(UDS_DTC_STATUS_TEST_FAILED |
                                  UDS_DTC_STATUS_TEST_FAILED_THIS_CYCLE | UDS_DTC_STATUS_PENDING |
                                  UDS_DTC_STATUS_CONFIRMED | UDS_DTC_STATUS_TEST_FAILED_SLC);
                    rec->status_byte &= (uint8_t)~(UDS_DTC_STATUS_TEST_NOT_COMPLETED_SLC |
                                                   UDS_DTC_STATUS_TEST_NOT_COMPLETED_TOC);
                }
            } else {
                if (rec->fault_counter >= (-128 + 16)) {
                    rec->fault_counter = (int8_t)(rec->fault_counter - 16);
                } else {
                    rec->fault_counter = -128;
                }
                if (rec->fault_counter <= -128) {
                    rec->fault_counter = -128;
                    rec->status_byte &= (uint8_t)~UDS_DTC_STATUS_TEST_FAILED;
                    rec->status_byte &= (uint8_t)~(UDS_DTC_STATUS_TEST_NOT_COMPLETED_SLC |
                                                   UDS_DTC_STATUS_TEST_NOT_COMPLETED_TOC);
                }
            }
            return true;
        }
    }
    return false;
}
