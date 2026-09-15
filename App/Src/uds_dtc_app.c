/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */

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
            bool include = false;
            if (subfunction == 0x0AU) {
                include = true; /* All supported DTCs */
            } else if (rec->active) {
                if (subfunction == 0x15U || subfunction == 0x55U) {
                    include = ((rec->status_byte & UDS_DTC_STATUS_CONFIRMED) != 0U);
                } else {
                    include = ((rec->status_byte & status_mask) != 0U);
                }
            }
            if (include) {
                if (!append_dtc(response, &len, response_capacity, rec)) {
                    return UDS_RESULT_RESPONSE_TOO_LONG;
                }
            }
        }
        break;
    }

    /* 0x03: Snapshot identification */
    case 0x03U: {
        for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
            const UdsDtcAppRecord *rec = &s_dtc_storage.records[i];
            if (rec->active) {
                if (!append_dtc(response, &len, response_capacity, rec) ||
                    !append_byte(response, &len, response_capacity,
                                 0x01U /* Snapshot record 1 */)) {
                    return UDS_RESULT_RESPONSE_TOO_LONG;
                }
            }
        }
        break;
    }

    /* 0x04, 0x05, 0x18: Snapshot Record by DTC */
    case 0x04U:
    case 0x05U:
    case 0x18U: {
        bool found = false;
        for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
            const UdsDtcAppRecord *rec = &s_dtc_storage.records[i];
            if (rec->active && dtc_matches_request(rec, request, request_length)) {
                found = true;
                uint8_t record_num =
                    (request_length >= 6U && request[5] != 0xFFU) ? request[5] : 0x01U;
                if (!append_dtc(response, &len, response_capacity, rec) ||
                    !append_byte(response, &len, response_capacity, record_num)) {
                    return UDS_RESULT_RESPONSE_TOO_LONG;
                }

                if (rec->snapshot_length > 0U) {
                    if (!append_byte(response, &len, response_capacity, 0x01U /* 1 DID */) ||
                        !append_byte(response, &len, response_capacity, 0xDFU) ||
                        !append_byte(response, &len, response_capacity, 0x00U)) {
                        return UDS_RESULT_RESPONSE_TOO_LONG;
                    }
                    for (uint8_t b = 0U; b < rec->snapshot_length; ++b) {
                        if (!append_byte(response, &len, response_capacity,
                                         rec->snapshot_data[b])) {
                            return UDS_RESULT_RESPONSE_TOO_LONG;
                        }
                    }
                } else {
                    /* Standard OEM snapshot layout per Diagnostic sheet.xlsx:
                     * DID DF00: Battery Voltage (2 bytes, 12000 mV = 0x2EE0)
                     * DID DF01: Vehicle Speed (2 bytes, 0 km/h)
                     * DID DF02: Occurrence Counter (1 byte)
                     */
                    if (!append_byte(response, &len, response_capacity, 0x03U /* 3 DIDs */) ||
                        !append_byte(response, &len, response_capacity, 0xDFU) ||
                        !append_byte(response, &len, response_capacity, 0x00U) ||
                        !append_byte(response, &len, response_capacity, 0x2EU) ||
                        !append_byte(response, &len, response_capacity, 0xE0U) ||
                        !append_byte(response, &len, response_capacity, 0xDFU) ||
                        !append_byte(response, &len, response_capacity, 0x01U) ||
                        !append_byte(response, &len, response_capacity, 0x00U) ||
                        !append_byte(response, &len, response_capacity, 0x00U) ||
                        !append_byte(response, &len, response_capacity, 0xDFU) ||
                        !append_byte(response, &len, response_capacity, 0x02U) ||
                        !append_byte(response, &len, response_capacity, rec->occurrence_counter)) {
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
                if (!append_dtc(response, &len, response_capacity, rec)) {
                    return UDS_RESULT_RESPONSE_TOO_LONG;
                }

                if (rec->extended_length > 0U) {
                    if (!append_byte(response, &len, response_capacity, 0x01U /* Record #1 */)) {
                        return UDS_RESULT_RESPONSE_TOO_LONG;
                    }
                    for (uint8_t b = 0U; b < rec->extended_length; ++b) {
                        if (!append_byte(response, &len, response_capacity,
                                         rec->extended_data[b])) {
                            return UDS_RESULT_RESPONSE_TOO_LONG;
                        }
                    }
                } else {
                    /* Standard Extended Data per Diagnostic sheet.xlsx:
                     * Record 0x01: Occurrence counter (1 byte)
                     * Record 0x02: Aging counter (1 byte)
                     */
                    if (!append_byte(response, &len, response_capacity,
                                     UDS_DTC_EXT_DATA_OCCURRENCES) ||
                        !append_byte(response, &len, response_capacity, rec->occurrence_counter) ||
                        !append_byte(response, &len, response_capacity,
                                     UDS_DTC_EXT_DATA_AGING_COUNTER) ||
                        !append_byte(response, &len, response_capacity, rec->aging_counter)) {
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

    /* 0x14: Fault Detection Counter */
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
        return UDS_RESULT_OUT_OF_RANGE;
    }

    *response_length = len;
    return UDS_RESULT_OK;
}

void uds_dtc_app_attach_nvm(UdsParamStore *store) {
    s_dtc_storage.nvm_store = store;
}

bool uds_dtc_app_save_to_nvm(void) {
    if (s_dtc_storage.nvm_store == NULL) {
        return false;
    }
    UdsDtcNvBlock block;
    memset(&block, 0, sizeof(block));
    block.record_count = s_dtc_storage.record_count;
    for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
        block.records[i].dtc_number = s_dtc_storage.records[i].dtc_number;
        block.records[i].status_byte = s_dtc_storage.records[i].status_byte;
        block.records[i].severity = s_dtc_storage.records[i].severity;
        block.records[i].functional_unit = s_dtc_storage.records[i].functional_unit;
        block.records[i].fault_counter = s_dtc_storage.records[i].fault_counter;
        block.records[i].occurrence_counter = s_dtc_storage.records[i].occurrence_counter;
        block.records[i].aging_counter = s_dtc_storage.records[i].aging_counter;
        block.records[i].active = s_dtc_storage.records[i].active;
    }
    return (uds_param_save(s_dtc_storage.nvm_store, &block) == UDS_PARAM_OK);
}

bool uds_dtc_app_load_from_nvm(void) {
    if (s_dtc_storage.nvm_store == NULL) {
        return false;
    }
    UdsDtcNvBlock block;
    if (uds_param_load(s_dtc_storage.nvm_store, &block) != UDS_PARAM_OK) {
        return false;
    }
    for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
        for (uint8_t j = 0U; j < block.record_count; ++j) {
            if (s_dtc_storage.records[i].dtc_number == block.records[j].dtc_number) {
                s_dtc_storage.records[i].status_byte = block.records[j].status_byte;
                s_dtc_storage.records[i].severity = block.records[j].severity;
                s_dtc_storage.records[i].functional_unit = block.records[j].functional_unit;
                s_dtc_storage.records[i].fault_counter = block.records[j].fault_counter;
                s_dtc_storage.records[i].occurrence_counter = block.records[j].occurrence_counter;
                s_dtc_storage.records[i].aging_counter = block.records[j].aging_counter;
                s_dtc_storage.records[i].active = block.records[j].active;
                break;
            }
        }
    }
    return true;
}

typedef struct {
    uint32_t dtc_number;
    uint8_t severity;
    uint8_t functional_unit;
} UdsOemDtcDef;

static const UdsOemDtcDef s_oem_dtc_defs[UDS_DTC_OEM_COUNT] = {
    {0xF00614UL, 0x80U, 0x01U}, /* U300614 ASW_DTC_BatteryVoltHighWarn */
    {0xF00615UL, 0xA0U, 0x01U}, /* U300615 ASW_DTC_BatteryVoltLowWarn */
    {0xD00616UL, 0x80U, 0x01U}, /* C100616 ASW_DTC_BatteryVoltHigh */
    {0xD00617UL, 0xA0U, 0x01U}, /* C100617 ASW_DTC_BatteryVoltLow */
    {0xD00618UL, 0x80U, 0x01U}, /* C100618 ASW_DTC_CanBusOff */
    {0xD00619UL, 0xA0U, 0x01U}, /* C100619 ASW_DTC_SysPrs_SigLost */
    {0xD00620UL, 0x80U, 0x01U}, /* C100620 ASW_DTC_SysPrs_OverLimit */
    {0xD00621UL, 0xA0U, 0x01U}, /* C100621 ASW_DTC_SysPrs_JumpErr */
    {0xD00622UL, 0x80U, 0x01U}, /* C100622 ASW_DTC_AccuPrs_SigLost */
    {0xD00623UL, 0xA0U, 0x01U}, /* C100623 ASW_DTC_AccuPrs_OverLimit */
    {0xD00624UL, 0x80U, 0x01U}, /* C100624 ASW_DTC_AccuPrs_JumpErr */
    {0xD00625UL, 0xA0U, 0x01U}, /* C100625 ASW_DTC_PfsPrs_SigLost */
    {0xD00626UL, 0x80U, 0x01U}, /* C100626 ASW_DTC_PfsPrs_OverLimit */
    {0xD00627UL, 0xA0U, 0x01U}, /* C100627 ASW_DTC_PfsPrs_JumpErr */
    {0xD00628UL, 0x80U, 0x01U}, /* C100628 ASW_DTC_PipeLineErrFL */
    {0xD00629UL, 0xA0U, 0x01U}, /* C100629 ASW_DTC_PipeLineErrFR */
    {0xD00630UL, 0x80U, 0x01U}, /* C100630 ASW_DTC_PipeLineErrRL */
    {0xD00631UL, 0xA0U, 0x01U}, /* C100631 ASW_DTC_PipeLineErrRR */
    {0xD00632UL, 0x80U, 0x01U}, /* C100632 ASW_DTC_WssFL_Err */
    {0xD00633UL, 0xA0U, 0x01U}, /* C100633 ASW_DTC_WssFR_Err */
    {0xD00634UL, 0x80U, 0x01U}, /* C100634 ASW_DTC_WssRL_Err */
    {0xD00635UL, 0xA0U, 0x01U}, /* C100635 ASW_DTC_WssRR_Err */
    {0xD00636UL, 0x80U, 0x01U}, /* C100636 ASW_DTC_AccuLowPrs_Err */
    {0xD00637UL, 0xA0U, 0x01U}, /* C100637 ASW_DTC_AccuPrsHoldErr */
    {0xD00638UL, 0x80U, 0x01U}, /* C100638 ASW_DTC_PconErr */
    {0xD00639UL, 0xA0U, 0x01U}, /* C100639 ASW_DTC_AccuPump_DG */
    {0xD00640UL, 0x80U, 0x01U}, /* C100640 ASW_DTC_PumpMotLockedRotFault */
    {0xD00641UL, 0xA0U, 0x01U}, /* C100641 ASW_DTC_AccuPump_Err */
    {0xD00642UL, 0x80U, 0x01U}, /* C100642 ASW_DTC_PTS_Err */
    {0xD00643UL, 0xA0U, 0x01U}, /* C100643 ASW_DTC_PTS_Chanl_1Err */
    {0xD00644UL, 0x80U, 0x01U}, /* C100644 ASW_DTC_PTS_Chanl_2Err */
    {0xD00645UL, 0xA0U, 0x01U}, /* C100645 ASW_DTC_MC_OutleakFault */
    {0xD00646UL, 0x80U, 0x01U}, /* C100646 ASW_DTC_MC_InleakFault */
    {0xD00647UL, 0xA0U, 0x01U}, /* C100647 ASW_DTC_IMUyawrateSignalErrLevel */
    {0xD00648UL, 0x80U, 0x01U}, /* C100648 ASW_DTC_IMUaySignalErrLevel */
    {0xD00649UL, 0xA0U, 0x01U}, /* C100649 ASW_DTC_IMUaxSignalErrLevel */
    {0xD00650UL, 0x80U, 0x01U}, /* C100650 ASW_DTC_SASSignalErrLevel */
    {0xD00651UL, 0xA0U, 0x01U}, /* C100651 ASW_DTC_BrkConValveDriverFault */
    {0xD00652UL, 0x80U, 0x01U}, /* C100652 ASW_DTC_WhelPrsConValveDriverFault */
    {0xD00653UL, 0xA0U, 0x01U}, /* C100653 ASW_DTC_ISV1_Fault */
    {0xD00654UL, 0x80U, 0x01U}, /* C100654 ASW_DTC_ISV1_OverTempWarn */
    {0xD00655UL, 0xA0U, 0x01U}, /* C100655 ASW_DTC_ISV2_Fault */
    {0xD00656UL, 0x80U, 0x01U}, /* C100656 ASW_DTC_ISV2_OverTempWarn */
    {0xD00657UL, 0xA0U, 0x01U}, /* C100657 ASW_DTC_ISV3_Fault */
    {0xD00658UL, 0x80U, 0x01U}, /* C100658 ASW_DTC_ISV3_OverTempWarn */
    {0xD00659UL, 0xA0U, 0x01U}, /* C100659 ASW_DTC_ISV4_Fault */
    {0xD00660UL, 0x80U, 0x01U}, /* C100660 ASW_DTC_ISV4_OverTempWarn */
    {0xD00661UL, 0xA0U, 0x01U}, /* C100661 ASW_DTC_PAV_Fault */
    {0xD00662UL, 0x80U, 0x01U}, /* C100662 ASW_DTC_PAV_OverTempWarn */
    {0xD00663UL, 0xA0U, 0x01U}, /* C100663 ASW_DTC_PRV_Fault */
    {0xD00664UL, 0x80U, 0x01U}, /* C100664 ASW_DTC_PRV_OverTempWarn */
    {0xD00665UL, 0xA0U, 0x01U}, /* C100665 ASW_DTC_BAV_Fault */
    {0xD00666UL, 0x80U, 0x01U}, /* C100666 ASW_DTC_CSV_Fault */
    {0xD00667UL, 0xA0U, 0x01U}, /* C100667 ASW_DTC_SSV_Fault */
    {0xD00668UL, 0x80U, 0x01U}, /* C100668 ASW_DTC_USV_Fault */
    {0xD00669UL, 0x80U, 0x01U}, /* C100669 ASW_DTC_SASangErr */
    {0xD00670UL, 0xA0U, 0x01U}, /* C100670 ASW_DTC_BrkSwitchErr */
    {0xD00671UL, 0x80U, 0x01U}, /* C100671 ASW_DTC_PowerSysErr */
    {0xD00672UL, 0xA0U, 0x01U}, /* C100672 ASW_DTC_AccPosErr */
    {0xD00673UL, 0x80U, 0x01U}, /* C100673 ASW_DTC_GearPosErr */
    {0xD00674UL, 0xA0U, 0x01U}, /* C100674 ASW_DTC_BtrErr */
    {0xD00675UL, 0x80U, 0x01U}, /* C100675 ASW_DTC_BrkFluidLevel_Low */
    {0xD00676UL, 0xA0U, 0x01U}, /* C100676 ASW_DTC_SeatBltErr */
    {0xD00677UL, 0x80U, 0x01U}, /* C100677 ASW_DTC_DoorsErr */
    {0xD00678UL, 0xA0U, 0x01U}, /* C100678 ASW_DTC_AdasConnectErr */
    {0xD00679UL, 0x80U, 0x01U}, /* C100679 ASW_DTC_ExternalEPBErr */
};

void uds_dtc_app_init(void) {
    memset(&s_dtc_storage, 0, sizeof(s_dtc_storage));
    s_dtc_storage.status_availability_mask = 0xFFU;

    /* Populate 66 OEM DTC records */
    for (uint8_t i = 0U; i < UDS_DTC_OEM_COUNT; ++i) {
        s_dtc_storage.records[i].dtc_number = s_oem_dtc_defs[i].dtc_number;
        s_dtc_storage.records[i].status_byte = UDS_DTC_STATUS_CLEARED;
        s_dtc_storage.records[i].severity = s_oem_dtc_defs[i].severity;
        s_dtc_storage.records[i].functional_unit = s_oem_dtc_defs[i].functional_unit;
        s_dtc_storage.records[i].fault_counter = 0;
        s_dtc_storage.records[i].occurrence_counter = 0U;
        s_dtc_storage.records[i].aging_counter = 0U;
        s_dtc_storage.records[i].snapshot_length = 0U;
        s_dtc_storage.records[i].extended_length = 0U;
        s_dtc_storage.records[i].active = false;
    }
    s_dtc_storage.record_count = UDS_DTC_OEM_COUNT;

    /* Baseline DTC 1: P0100 - Mass Air Flow Sensor A Circuit (0x010000) */
    uint8_t b1 = s_dtc_storage.record_count++;
    s_dtc_storage.records[b1].dtc_number = 0x010000UL;
    s_dtc_storage.records[b1].status_byte = 0x2FU; /* Confirmed, pending, test failed */
    s_dtc_storage.records[b1].severity = 0x40U;    /* Check at next halt */
    s_dtc_storage.records[b1].functional_unit = 0x01U;
    s_dtc_storage.records[b1].fault_counter = 127;
    s_dtc_storage.records[b1].occurrence_counter = 5U;
    s_dtc_storage.records[b1].aging_counter = 0x28U;
    s_dtc_storage.records[b1].snapshot_length = 4U;
    s_dtc_storage.records[b1].snapshot_data[0] = 0x0CU; /* Engine RPM: 3072 */
    s_dtc_storage.records[b1].snapshot_data[1] = 0x00U;
    s_dtc_storage.records[b1].snapshot_data[2] = 0x32U; /* Coolant Temp: 50 C */
    s_dtc_storage.records[b1].snapshot_data[3] = 0x88U; /* Battery 13.6 V */
    s_dtc_storage.records[b1].extended_length = 2U;
    s_dtc_storage.records[b1].extended_data[0] = 0x05U; /* Occurrence count: 5 */
    s_dtc_storage.records[b1].extended_data[1] = 0x28U; /* Aging counter */
    s_dtc_storage.records[b1].active = true;

    /* Baseline DTC 2: U0100 - Lost Communication with ECM/PCM (0xC10000) */
    uint8_t b2 = s_dtc_storage.record_count++;
    s_dtc_storage.records[b2].dtc_number = 0xC10000UL;
    s_dtc_storage.records[b2].status_byte = 0x08U; /* Confirmed */
    s_dtc_storage.records[b2].severity = 0x80U;    /* Immediate check */
    s_dtc_storage.records[b2].functional_unit = 0x02U;
    s_dtc_storage.records[b2].fault_counter = 50;
    s_dtc_storage.records[b2].occurrence_counter = 1U;
    s_dtc_storage.records[b2].aging_counter = 0x28U;
    s_dtc_storage.records[b2].snapshot_length = 2U;
    s_dtc_storage.records[b2].snapshot_data[0] = 0x00U;
    s_dtc_storage.records[b2].snapshot_data[1] = 0x00U;
    s_dtc_storage.records[b2].extended_length = 2U;
    s_dtc_storage.records[b2].extended_data[0] = 0x01U;
    s_dtc_storage.records[b2].extended_data[1] = 0x28U;
    s_dtc_storage.records[b2].active = true;

    /* Baseline DTC 3: B0001 - Driver Airbag Deployment Loop (0x800100) */
    uint8_t b3 = s_dtc_storage.record_count++;
    s_dtc_storage.records[b3].dtc_number = 0x800100UL;
    s_dtc_storage.records[b3].status_byte = 0x01U; /* Test failed */
    s_dtc_storage.records[b3].severity = 0xA0U;    /* Maintenance immediately */
    s_dtc_storage.records[b3].functional_unit = 0x03U;
    s_dtc_storage.records[b3].fault_counter = 10;
    s_dtc_storage.records[b3].occurrence_counter = 2U;
    s_dtc_storage.records[b3].aging_counter = 0x28U;
    s_dtc_storage.records[b3].snapshot_length = 2U;
    s_dtc_storage.records[b3].snapshot_data[0] = 0x12U;
    s_dtc_storage.records[b3].snapshot_data[1] = 0x34U;
    s_dtc_storage.records[b3].extended_length = 2U;
    s_dtc_storage.records[b3].extended_data[0] = 0x02U;
    s_dtc_storage.records[b3].extended_data[1] = 0x28U;
    s_dtc_storage.records[b3].active = true;

    /* Enable all subfunctions capability bits */
    s_dtc_storage.backend.capabilities = 0x03FFFFFFUL;
    s_dtc_storage.backend.report = uds_dtc_app_report;
}

const UdsDtcBackend *uds_dtc_app_get_backend(void) {
    return &s_dtc_storage.backend;
}

UdsCallbackResult uds_dtc_app_clear(void *context, uint32_t group_of_dtc) {
    (void)context;
    bool cleared_any = false;

    if (group_of_dtc == 0xFFFFFFUL) {
        /* Clear all DTCs - set to 0x50 per AUTOSAR Dem specification */
        for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
            s_dtc_storage.records[i].active = false;
            s_dtc_storage.records[i].status_byte = UDS_DTC_STATUS_CLEARED;
            s_dtc_storage.records[i].fault_counter = 0;
        }
        (void)uds_dtc_app_save_to_nvm();
        return UDS_RESULT_OK;
    }

    /* Check standard ISO 14229-1 functional group masks */
    uint32_t group_high = group_of_dtc & 0xFF0000UL;
    bool is_group_mask =
        ((group_of_dtc & 0x00FFFFUL) == 0x000000UL) || ((group_of_dtc & 0x00FFFFUL) == 0x00FF00UL);

    if (is_group_mask) {
        for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
            uint32_t dtc = s_dtc_storage.records[i].dtc_number;
            bool match = false;
            if ((group_high == 0x000000UL) && (dtc < 0x400000UL)) {
                match = true; /* Powertrain */
            } else if ((group_high == 0x400000UL) && (dtc >= 0x400000UL) && (dtc < 0x800000UL)) {
                match = true; /* Chassis */
            } else if ((group_high == 0x800000UL) && (dtc >= 0x800000UL) && (dtc < 0xC00000UL)) {
                match = true; /* Body */
            } else if ((group_high == 0xC00000UL) && (dtc >= 0xC00000UL)) {
                match = true; /* Network Communication */
            }
            if (match) {
                s_dtc_storage.records[i].active = false;
                s_dtc_storage.records[i].status_byte = UDS_DTC_STATUS_CLEARED;
                s_dtc_storage.records[i].fault_counter = 0;
                cleared_any = true;
            }
        }
        if (cleared_any) {
            (void)uds_dtc_app_save_to_nvm();
            return UDS_RESULT_OK;
        }
    }

    /* Match individual DTC */
    for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
        if (s_dtc_storage.records[i].dtc_number == group_of_dtc) {
            s_dtc_storage.records[i].active = false;
            s_dtc_storage.records[i].status_byte = UDS_DTC_STATUS_CLEARED;
            s_dtc_storage.records[i].fault_counter = 0;
            (void)uds_dtc_app_save_to_nvm();
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
            if ((status & UDS_DTC_STATUS_TEST_FAILED) != 0U) {
                if (s_dtc_storage.records[i].occurrence_counter < 255U) {
                    s_dtc_storage.records[i].occurrence_counter++;
                }
                s_dtc_storage.records[i].aging_counter = 0U;
            }
            (void)uds_dtc_app_save_to_nvm();
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
        s_dtc_storage.records[idx].occurrence_counter =
            ((status & UDS_DTC_STATUS_TEST_FAILED) != 0U) ? 1U : 0U;
        s_dtc_storage.records[idx].aging_counter = 0U;
        s_dtc_storage.records[idx].active = true;
        (void)uds_dtc_app_save_to_nvm();
        return true;
    }
    return false;
}

bool uds_dtc_app_clear_fault(uint32_t dtc) {
    for (uint8_t i = 0U; i < s_dtc_storage.record_count; ++i) {
        if (s_dtc_storage.records[i].dtc_number == dtc) {
            s_dtc_storage.records[i].active = false;
            s_dtc_storage.records[i].status_byte = UDS_DTC_STATUS_CLEARED;
            (void)uds_dtc_app_save_to_nvm();
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
                    uint8_t failed_mask =
                        (uint8_t)(UDS_DTC_STATUS_TEST_FAILED |
                                  UDS_DTC_STATUS_TEST_FAILED_THIS_CYCLE | UDS_DTC_STATUS_PENDING |
                                  UDS_DTC_STATUS_CONFIRMED | UDS_DTC_STATUS_TEST_FAILED_SLC);
                    uint8_t completed_mask = (uint8_t)(UDS_DTC_STATUS_TEST_NOT_COMPLETED_SLC |
                                                       UDS_DTC_STATUS_TEST_NOT_COMPLETED_TOC);
                    rec->fault_counter = 127;
                    rec->active = true;
                    rec->status_byte |= failed_mask;
                    rec->status_byte &= (uint8_t)~completed_mask;
                    if (rec->occurrence_counter < 255U) {
                        rec->occurrence_counter++;
                    }
                    rec->aging_counter = 0U;
                }
            } else {
                if (rec->fault_counter >= (-128 + 16)) {
                    rec->fault_counter = (int8_t)(rec->fault_counter - 16);
                } else {
                    rec->fault_counter = -128;
                }
                if (rec->fault_counter <= -128) {
                    uint8_t completed_mask = (uint8_t)(UDS_DTC_STATUS_TEST_NOT_COMPLETED_SLC |
                                                       UDS_DTC_STATUS_TEST_NOT_COMPLETED_TOC);
                    rec->fault_counter = -128;
                    rec->status_byte &= (uint8_t)~UDS_DTC_STATUS_TEST_FAILED;
                    rec->status_byte &= (uint8_t)~completed_mask;
                    if (rec->aging_counter < 255U) {
                        rec->aging_counter++;
                    }
                    if (rec->aging_counter >= 40U) {
                        rec->status_byte &= (uint8_t)~UDS_DTC_STATUS_CONFIRMED;
                    }
                }
            }
            (void)uds_dtc_app_save_to_nvm();
            return true;
        }
    }
    return false;
}
