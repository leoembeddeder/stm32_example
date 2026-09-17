/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */

#include "uds_dtc_app.h"

#include <string.h>

static const uint8_t s_p0100_snapshot[4] = {0x0CU, 0x00U, 0x32U, 0x88U};
static const uint8_t s_p0100_extended[2] = {0x05U, 0x28U};

static const uint8_t s_u0100_snapshot[2] = {0x00U, 0x00U};
static const uint8_t s_u0100_extended[2] = {0x01U, 0x28U};

static const uint8_t s_b0001_snapshot[2] = {0x12U, 0x34U};
static const uint8_t s_b0001_extended[2] = {0x02U, 0x28U};

static const UdsDtcRomDef s_rom_dtc_defs[UDS_DTC_STATIC_COUNT] = {
    {0xF00614UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* U300614 ASW_DTC_BatteryVoltHighWarn */
    {0xF00615UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* U300615 ASW_DTC_BatteryVoltLowWarn */
    {0xD00616UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100616 ASW_DTC_BatteryVoltHigh */
    {0xD00617UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100617 ASW_DTC_BatteryVoltLow */
    {0xD00618UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100618 ASW_DTC_CanBusOff */
    {0xD00619UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100619 ASW_DTC_SysPrs_SigLost */
    {0xD00620UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100620 ASW_DTC_SysPrs_OverLimit */
    {0xD00621UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100621 ASW_DTC_SysPrs_JumpErr */
    {0xD00622UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100622 ASW_DTC_AccuPrs_SigLost */
    {0xD00623UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100623 ASW_DTC_AccuPrs_OverLimit */
    {0xD00624UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100624 ASW_DTC_AccuPrs_JumpErr */
    {0xD00625UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100625 ASW_DTC_PfsPrs_SigLost */
    {0xD00626UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100626 ASW_DTC_PfsPrs_OverLimit */
    {0xD00627UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100627 ASW_DTC_PfsPrs_JumpErr */
    {0xD00628UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100628 ASW_DTC_PipeLineErrFL */
    {0xD00629UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100629 ASW_DTC_PipeLineErrFR */
    {0xD00630UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100630 ASW_DTC_PipeLineErrRL */
    {0xD00631UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100631 ASW_DTC_PipeLineErrRR */
    {0xD00632UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100632 ASW_DTC_WssFL_Err */
    {0xD00633UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100633 ASW_DTC_WssFR_Err */
    {0xD00634UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100634 ASW_DTC_WssRL_Err */
    {0xD00635UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100635 ASW_DTC_WssRR_Err */
    {0xD00636UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100636 ASW_DTC_AccuLowPrs_Err */
    {0xD00637UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100637 ASW_DTC_AccuPrsHoldErr */
    {0xD00638UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100638 ASW_DTC_PconErr */
    {0xD00639UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100639 ASW_DTC_AccuPump_DG */
    {0xD00640UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100640 ASW_DTC_PumpMotLockedRotFault */
    {0xD00641UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100641 ASW_DTC_AccuPump_Err */
    {0xD00642UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100642 ASW_DTC_PTS_Err */
    {0xD00643UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100643 ASW_DTC_PTS_Chanl_1Err */
    {0xD00644UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100644 ASW_DTC_PTS_Chanl_2Err */
    {0xD00645UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100645 ASW_DTC_MC_OutleakFault */
    {0xD00646UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100646 ASW_DTC_MC_InleakFault */
    {0xD00647UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100647 ASW_DTC_IMUyawrateSignalErrLevel */
    {0xD00648UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100648 ASW_DTC_IMUaySignalErrLevel */
    {0xD00649UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100649 ASW_DTC_IMUaxSignalErrLevel */
    {0xD00650UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100650 ASW_DTC_SASSignalErrLevel */
    {0xD00651UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100651 ASW_DTC_BrkConValveDriverFault */
    {0xD00652UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100652 ASW_DTC_WhelPrsConValveDriverFault */
    {0xD00653UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100653 ASW_DTC_ISV1_Fault */
    {0xD00654UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100654 ASW_DTC_ISV1_OverTempWarn */
    {0xD00655UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100655 ASW_DTC_ISV2_Fault */
    {0xD00656UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100656 ASW_DTC_ISV2_OverTempWarn */
    {0xD00657UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100657 ASW_DTC_ISV3_Fault */
    {0xD00658UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100658 ASW_DTC_ISV3_OverTempWarn */
    {0xD00659UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100659 ASW_DTC_ISV4_Fault */
    {0xD00660UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100660 ASW_DTC_ISV4_OverTempWarn */
    {0xD00661UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100661 ASW_DTC_PAV_Fault */
    {0xD00662UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100662 ASW_DTC_PAV_OverTempWarn */
    {0xD00663UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100663 ASW_DTC_PRV_Fault */
    {0xD00664UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100664 ASW_DTC_PRV_OverTempWarn */
    {0xD00665UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100665 ASW_DTC_BAV_Fault */
    {0xD00666UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100666 ASW_DTC_CSV_Fault */
    {0xD00667UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100667 ASW_DTC_SSV_Fault */
    {0xD00668UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100668 ASW_DTC_USV_Fault */
    {0xD00669UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100669 ASW_DTC_SASangErr */
    {0xD00670UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100670 ASW_DTC_BrkSwitchErr */
    {0xD00671UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100671 ASW_DTC_PowerSysErr */
    {0xD00672UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100672 ASW_DTC_AccPosErr */
    {0xD00673UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100673 ASW_DTC_GearPosErr */
    {0xD00674UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100674 ASW_DTC_BtrErr */
    {0xD00675UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100675 ASW_DTC_BrkFluidLevel_Low */
    {0xD00676UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100676 ASW_DTC_SeatBltErr */
    {0xD00677UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100677 ASW_DTC_DoorsErr */
    {0xD00678UL, 0xA0U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100678 ASW_DTC_AdasConnectErr */
    {0xD00679UL, 0x80U, 0x01U, UDS_DTC_STATUS_CLEARED, 0, 0U, 0U, false, NULL, 0U, NULL,
     0U}, /* C100679 ASW_DTC_ExternalEPBErr */

    /* Baseline DTC 1: P0100 - Mass Air Flow Sensor A Circuit (0x010000) */
    {0x010000UL, 0x40U, 0x01U, 0x2FU, 127, 5U, 0x28U, true, s_p0100_snapshot, 4U, s_p0100_extended,
     2U},
    /* Baseline DTC 2: U0100 - Lost Communication with ECM/PCM (0xC10000) */
    {0xC10000UL, 0x80U, 0x02U, 0x08U, 50, 1U, 0x28U, true, s_u0100_snapshot, 2U, s_u0100_extended,
     2U},
    /* Baseline DTC 3: B0001 - Driver Airbag Deployment Loop (0x800100) */
    {0x800100UL, 0xA0U, 0x03U, 0x01U, 10, 2U, 0x28U, true, s_b0001_snapshot, 2U, s_b0001_extended,
     2U},
};

static UdsDtcAppStorage s_dtc_storage;

static uint8_t get_total_record_count(void) {
    return (uint8_t)(UDS_DTC_STATIC_COUNT + s_dtc_storage.dynamic_count);
}

static bool get_dtc_record(uint8_t index, UdsDtcAppRecord *out_rec) {
    if (out_rec == NULL) {
        return false;
    }
    if (index < UDS_DTC_STATIC_COUNT) {
        const UdsDtcRomDef *rom = &s_rom_dtc_defs[index];
        const UdsDtcRamStatus *ram = &s_dtc_storage.static_status[index];
        out_rec->dtc_number = rom->dtc_number;
        out_rec->severity = rom->severity;
        out_rec->functional_unit = rom->functional_unit;
        out_rec->status_byte = ram->status_byte;
        out_rec->fault_counter = ram->fault_counter;
        out_rec->occurrence_counter = ram->occurrence_counter;
        out_rec->aging_counter = ram->aging_counter;
        out_rec->active = ram->active;
        if (ram->has_snapshot) {
            out_rec->has_snapshot = true;
            out_rec->snapshot_record_num = ram->snapshot_record_num;
            out_rec->snapshot_data = ram->snapshot_data;
            out_rec->snapshot_length = ram->snapshot_length;
        } else {
            out_rec->has_snapshot = (rom->snapshot_length > 0U);
            out_rec->snapshot_record_num = 0x01U;
            out_rec->snapshot_data = rom->snapshot_data;
            out_rec->snapshot_length = rom->snapshot_length;
        }
        out_rec->extended_data = rom->extended_data;
        out_rec->extended_length = rom->extended_length;
        return true;
    }
    uint8_t d = (uint8_t)(index - UDS_DTC_STATIC_COUNT);
    if (d < s_dtc_storage.dynamic_count) {
        const UdsDtcDynamicRecord *dyn = &s_dtc_storage.dynamic_records[d];
        out_rec->dtc_number = dyn->dtc_number;
        out_rec->severity = dyn->severity;
        out_rec->functional_unit = dyn->functional_unit;
        out_rec->status_byte = dyn->status.status_byte;
        out_rec->fault_counter = dyn->status.fault_counter;
        out_rec->occurrence_counter = dyn->status.occurrence_counter;
        out_rec->aging_counter = dyn->status.aging_counter;
        out_rec->active = dyn->status.active;
        out_rec->has_snapshot = dyn->status.has_snapshot;
        out_rec->snapshot_record_num = dyn->status.snapshot_record_num;
        out_rec->snapshot_data = dyn->status.snapshot_data;
        out_rec->snapshot_length = dyn->status.snapshot_length;
        out_rec->extended_data = NULL;
        out_rec->extended_length = 0U;
        return true;
    }
    return false;
}

static UdsDtcRamStatus *get_dtc_ram_status(uint8_t index) {
    if (index < UDS_DTC_STATIC_COUNT) {
        return &s_dtc_storage.static_status[index];
    }
    uint8_t d = (uint8_t)(index - UDS_DTC_STATIC_COUNT);
    if (d < s_dtc_storage.dynamic_count) {
        return &s_dtc_storage.dynamic_records[d].status;
    }
    return NULL;
}

static int16_t find_dtc_index(uint32_t dtc) {
    for (uint8_t i = 0U; i < UDS_DTC_STATIC_COUNT; ++i) {
        if (s_rom_dtc_defs[i].dtc_number == dtc) {
            return (int16_t)i;
        }
    }
    for (uint8_t d = 0U; d < s_dtc_storage.dynamic_count; ++d) {
        if (s_dtc_storage.dynamic_records[d].dtc_number == dtc) {
            return (int16_t)(UDS_DTC_STATIC_COUNT + d);
        }
    }
    return -1;
}

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

static bool append_snapshot_payload(uint8_t *resp, uint16_t *len, uint16_t cap,
                                    const UdsDtcAppRecord *rec) {
    if (rec->snapshot_length > 0U) {
        if (!append_byte(resp, len, cap, 0x01U /* 1 DID */) ||
            !append_byte(resp, len, cap, 0xDFU) || !append_byte(resp, len, cap, 0x00U)) {
            return false;
        }
        for (uint8_t b = 0U; b < rec->snapshot_length; ++b) {
            if (!append_byte(resp, len, cap, rec->snapshot_data[b])) {
                return false;
            }
        }
    } else {
        /* Standard OEM snapshot layout per Diagnostic sheet.xlsx:
         * DID DF00: Battery Voltage (2 bytes, 12000 mV = 0x2EE0)
         * DID DF01: Vehicle Speed (2 bytes, 0 km/h)
         * DID DF02: Occurrence Counter (1 byte)
         */
        if (!append_byte(resp, len, cap, 0x03U /* 3 DIDs */) ||
            !append_byte(resp, len, cap, 0xDFU) || !append_byte(resp, len, cap, 0x00U) ||
            !append_byte(resp, len, cap, 0x2EU) || !append_byte(resp, len, cap, 0xE0U) ||
            !append_byte(resp, len, cap, 0xDFU) || !append_byte(resp, len, cap, 0x01U) ||
            !append_byte(resp, len, cap, 0x00U) || !append_byte(resp, len, cap, 0x00U) ||
            !append_byte(resp, len, cap, 0xDFU) || !append_byte(resp, len, cap, 0x02U) ||
            !append_byte(resp, len, cap, rec->occurrence_counter)) {
            return false;
        }
    }
    return true;
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
        uint8_t total_count = get_total_record_count();
        for (uint8_t i = 0U; i < total_count; ++i) {
            UdsDtcAppRecord rec_storage;
            (void)get_dtc_record(i, &rec_storage);
            const UdsDtcAppRecord *rec = &rec_storage;
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

    /* 0x02, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x13, 0x15: List reporting */
    case 0x02U:
    case 0x0AU:
    case 0x0BU:
    case 0x0CU:
    case 0x0DU:
    case 0x0EU:
    case 0x0FU:
    case 0x13U:
    case 0x15U: {
        if (!append_byte(response, &len, response_capacity,
                         s_dtc_storage.status_availability_mask)) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        uint8_t total_count = get_total_record_count();
        for (uint8_t i = 0U; i < total_count; ++i) {
            UdsDtcAppRecord rec_storage;
            (void)get_dtc_record(i, &rec_storage);
            const UdsDtcAppRecord *rec = &rec_storage;
            bool include = false;
            if (subfunction == 0x0AU) {
                include = true; /* All supported DTCs */
            } else if (rec->active) {
                if (subfunction == 0x15U) {
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

    /* 0x17: reportUserDefMemoryDTCByStatusMask */
    case 0x17U: {
        uint8_t mem_selection = (request_length >= 4U) ? request[3] : 0x00U;
        if (!append_byte(response, &len, response_capacity, mem_selection) ||
            !append_byte(response, &len, response_capacity,
                         s_dtc_storage.status_availability_mask)) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        uint8_t total_count = get_total_record_count();
        for (uint8_t i = 0U; i < total_count; ++i) {
            UdsDtcAppRecord rec_storage;
            (void)get_dtc_record(i, &rec_storage);
            const UdsDtcAppRecord *rec = &rec_storage;
            if (rec->active && ((rec->status_byte & status_mask) != 0U)) {
                if (!append_dtc(response, &len, response_capacity, rec)) {
                    return UDS_RESULT_RESPONSE_TOO_LONG;
                }
            }
        }
        break;
    }

    /* 0x42: reportDTCBySeverityMaskRecord */
    case 0x42U: {
        uint8_t group_id = (request_length >= 3U) ? request[2] : 0x00U;
        uint8_t req_status_mask = (request_length >= 4U) ? request[3] : 0xFFU;
        uint8_t req_severity_mask = (request_length >= 5U) ? request[4] : 0xFFU;
        if (!append_byte(response, &len, response_capacity, group_id) ||
            !append_byte(response, &len, response_capacity,
                         s_dtc_storage.status_availability_mask)) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        uint8_t total_count = get_total_record_count();
        for (uint8_t i = 0U; i < total_count; ++i) {
            UdsDtcAppRecord rec_storage;
            (void)get_dtc_record(i, &rec_storage);
            const UdsDtcAppRecord *rec = &rec_storage;
            if (rec->active && ((rec->status_byte & req_status_mask) != 0U) &&
                ((rec->severity & req_severity_mask) != 0U)) {
                if (!append_byte(response, &len, response_capacity, rec->severity) ||
                    !append_byte(response, &len, response_capacity, rec->functional_unit) ||
                    !append_dtc(response, &len, response_capacity, rec)) {
                    return UDS_RESULT_RESPONSE_TOO_LONG;
                }
            }
        }
        break;
    }

    /* 0x55: reportWWHOBDDTCByMaskRecord */
    case 0x55U: {
        uint8_t group_id = (request_length >= 3U) ? request[2] : 0x00U;
        if (!append_byte(response, &len, response_capacity, group_id) ||
            !append_byte(response, &len, response_capacity,
                         s_dtc_storage.status_availability_mask)) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        uint8_t total_count = get_total_record_count();
        for (uint8_t i = 0U; i < total_count; ++i) {
            UdsDtcAppRecord rec_storage;
            (void)get_dtc_record(i, &rec_storage);
            const UdsDtcAppRecord *rec = &rec_storage;
            if (rec->active && ((rec->status_byte & UDS_DTC_STATUS_CONFIRMED) != 0U)) {
                if (!append_byte(response, &len, response_capacity, rec->severity) ||
                    !append_byte(response, &len, response_capacity, rec->functional_unit) ||
                    !append_dtc(response, &len, response_capacity, rec)) {
                    return UDS_RESULT_RESPONSE_TOO_LONG;
                }
            }
        }
        break;
    }

    /* 0x03: Snapshot identification */
    case 0x03U: {
        uint8_t total_count = get_total_record_count();
        for (uint8_t i = 0U; i < total_count; ++i) {
            UdsDtcAppRecord rec_storage;
            (void)get_dtc_record(i, &rec_storage);
            const UdsDtcAppRecord *rec = &rec_storage;
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

    /* 0x04, 0x18: Snapshot Record by DTC */
    case 0x04U:
    case 0x18U: {
        if (request_length < 5U) {
            return UDS_RESULT_ERROR;
        }
        if (subfunction == 0x18U) {
            uint8_t mem_selection = (request_length >= 7U) ? request[6] : 0x00U;
            if (!append_byte(response, &len, response_capacity, mem_selection)) {
                return UDS_RESULT_RESPONSE_TOO_LONG;
            }
        }
        uint32_t target_dtc =
            ((uint32_t)request[2] << 16U) | ((uint32_t)request[3] << 8U) | (uint32_t)request[4];
        uint8_t req_record_num = (request_length >= 6U) ? request[5] : 0xFFU;

        if (target_dtc == 0xFFFFFFUL) {
            uint8_t total_count = get_total_record_count();
            for (uint8_t i = 0U; i < total_count; ++i) {
                UdsDtcAppRecord rec;
                (void)get_dtc_record(i, &rec);
                if (rec.active) {
                    bool match_rec =
                        (req_record_num == 0xFFU) || (req_record_num == 0x00U) ||
                        (req_record_num == 0x01U) ||
                        (rec.has_snapshot && (req_record_num == rec.snapshot_record_num));
                    if (match_rec) {
                        uint8_t out_rec_num = (req_record_num == 0xFFU) ? 0x01U : req_record_num;
                        if (!append_dtc(response, &len, response_capacity, &rec) ||
                            !append_byte(response, &len, response_capacity, out_rec_num) ||
                            !append_snapshot_payload(response, &len, response_capacity, &rec)) {
                            return UDS_RESULT_RESPONSE_TOO_LONG;
                        }
                    }
                }
            }
        } else {
            int16_t idx = find_dtc_index(target_dtc);
            if (idx < 0) {
                return UDS_RESULT_OUT_OF_RANGE;
            }
            UdsDtcAppRecord rec;
            (void)get_dtc_record((uint8_t)idx, &rec);
            if (!append_dtc(response, &len, response_capacity, &rec)) {
                return UDS_RESULT_RESPONSE_TOO_LONG;
            }
            if (rec.active) {
                bool match_rec = (req_record_num == 0xFFU) || (req_record_num == 0x00U) ||
                                 (req_record_num == 0x01U) ||
                                 (rec.has_snapshot && (req_record_num == rec.snapshot_record_num));
                if (match_rec) {
                    uint8_t out_rec_num = (req_record_num == 0xFFU) ? 0x01U : req_record_num;
                    if (!append_byte(response, &len, response_capacity, out_rec_num) ||
                        !append_snapshot_payload(response, &len, response_capacity, &rec)) {
                        return UDS_RESULT_RESPONSE_TOO_LONG;
                    }
                }
            }
        }
        break;
    }

    /* 0x05: Snapshot Record by Record Number */
    case 0x05U: {
        uint8_t req_record_num = (request_length >= 3U) ? request[2] : 0x01U;
        uint8_t total_count = get_total_record_count();
        for (uint8_t i = 0U; i < total_count; ++i) {
            UdsDtcAppRecord rec;
            (void)get_dtc_record(i, &rec);
            if (rec.active) {
                bool match_rec = (req_record_num == 0xFFU) || (req_record_num == 0x00U) ||
                                 (req_record_num == 0x01U) ||
                                 (rec.has_snapshot && (req_record_num == rec.snapshot_record_num));
                if (match_rec) {
                    uint8_t out_rec_num = (req_record_num == 0xFFU) ? 0x01U : req_record_num;
                    if (!append_dtc(response, &len, response_capacity, &rec) ||
                        !append_byte(response, &len, response_capacity, out_rec_num) ||
                        !append_snapshot_payload(response, &len, response_capacity, &rec)) {
                        return UDS_RESULT_RESPONSE_TOO_LONG;
                    }
                }
            }
        }
        break;
    }

    /* 0x16: reportDTCExtDataRecordByRecordNumber */
    case 0x16U: {
        uint8_t rec_num = (request_length >= 3U) ? request[2] : 0x01U;
        if (!append_byte(response, &len, response_capacity, rec_num)) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        uint8_t total_count = get_total_record_count();
        for (uint8_t i = 0U; i < total_count; ++i) {
            UdsDtcAppRecord rec_storage;
            (void)get_dtc_record(i, &rec_storage);
            const UdsDtcAppRecord *rec = &rec_storage;
            if (rec->active) {
                if (!append_dtc(response, &len, response_capacity, rec) ||
                    !append_byte(response, &len, response_capacity, rec_num)) {
                    return UDS_RESULT_RESPONSE_TOO_LONG;
                }
                if (rec_num == UDS_DTC_EXT_DATA_AGING_COUNTER) {
                    if (!append_byte(response, &len, response_capacity, rec->aging_counter)) {
                        return UDS_RESULT_RESPONSE_TOO_LONG;
                    }
                } else {
                    if (!append_byte(response, &len, response_capacity, rec->occurrence_counter)) {
                        return UDS_RESULT_RESPONSE_TOO_LONG;
                    }
                }
            }
        }
        break;
    }

    /* 0x06, 0x10, 0x19: Extended Data Records by DTC */
    case 0x06U:
    case 0x10U:
    case 0x19U: {
        if (request_length < 5U) {
            return UDS_RESULT_ERROR;
        }
        if (subfunction == 0x19U) {
            uint8_t mem_selection = (request_length >= 7U) ? request[6] : 0x00U;
            if (!append_byte(response, &len, response_capacity, mem_selection)) {
                return UDS_RESULT_RESPONSE_TOO_LONG;
            }
        }
        uint32_t target_dtc =
            ((uint32_t)request[2] << 16U) | ((uint32_t)request[3] << 8U) | (uint32_t)request[4];
        uint8_t req_rec_num = (request_length >= 6U) ? request[5] : 0xFFU;

        if (target_dtc == 0xFFFFFFUL) {
            uint8_t total_count = get_total_record_count();
            for (uint8_t i = 0U; i < total_count; ++i) {
                UdsDtcAppRecord rec;
                (void)get_dtc_record(i, &rec);
                if (rec.active) {
                    if (!append_dtc(response, &len, response_capacity, &rec)) {
                        return UDS_RESULT_RESPONSE_TOO_LONG;
                    }
                    if (rec.extended_length > 0U) {
                        if (!append_byte(response, &len, response_capacity,
                                         0x01U /* Record #1 */)) {
                            return UDS_RESULT_RESPONSE_TOO_LONG;
                        }
                        for (uint8_t b = 0U; b < rec.extended_length; ++b) {
                            if (!append_byte(response, &len, response_capacity,
                                             rec.extended_data[b])) {
                                return UDS_RESULT_RESPONSE_TOO_LONG;
                            }
                        }
                    } else {
                        if ((req_rec_num == 0xFFU) ||
                            (req_rec_num == UDS_DTC_EXT_DATA_OCCURRENCES)) {
                            if (!append_byte(response, &len, response_capacity,
                                             UDS_DTC_EXT_DATA_OCCURRENCES) ||
                                !append_byte(response, &len, response_capacity,
                                             rec.occurrence_counter)) {
                                return UDS_RESULT_RESPONSE_TOO_LONG;
                            }
                        }
                        if ((req_rec_num == 0xFFU) ||
                            (req_rec_num == UDS_DTC_EXT_DATA_AGING_COUNTER)) {
                            if (!append_byte(response, &len, response_capacity,
                                             UDS_DTC_EXT_DATA_AGING_COUNTER) ||
                                !append_byte(response, &len, response_capacity,
                                             rec.aging_counter)) {
                                return UDS_RESULT_RESPONSE_TOO_LONG;
                            }
                        }
                    }
                }
            }
        } else {
            int16_t idx = find_dtc_index(target_dtc);
            if (idx < 0) {
                return UDS_RESULT_OUT_OF_RANGE;
            }
            UdsDtcAppRecord rec;
            (void)get_dtc_record((uint8_t)idx, &rec);
            if (!append_dtc(response, &len, response_capacity, &rec)) {
                return UDS_RESULT_RESPONSE_TOO_LONG;
            }
            if (rec.active) {
                if (rec.extended_length > 0U) {
                    if (!append_byte(response, &len, response_capacity, 0x01U /* Record #1 */)) {
                        return UDS_RESULT_RESPONSE_TOO_LONG;
                    }
                    for (uint8_t b = 0U; b < rec.extended_length; ++b) {
                        if (!append_byte(response, &len, response_capacity, rec.extended_data[b])) {
                            return UDS_RESULT_RESPONSE_TOO_LONG;
                        }
                    }
                } else {
                    if ((req_rec_num == 0xFFU) || (req_rec_num == UDS_DTC_EXT_DATA_OCCURRENCES)) {
                        if (!append_byte(response, &len, response_capacity,
                                         UDS_DTC_EXT_DATA_OCCURRENCES) ||
                            !append_byte(response, &len, response_capacity,
                                         rec.occurrence_counter)) {
                            return UDS_RESULT_RESPONSE_TOO_LONG;
                        }
                    }
                    if ((req_rec_num == 0xFFU) || (req_rec_num == UDS_DTC_EXT_DATA_AGING_COUNTER)) {
                        if (!append_byte(response, &len, response_capacity,
                                         UDS_DTC_EXT_DATA_AGING_COUNTER) ||
                            !append_byte(response, &len, response_capacity, rec.aging_counter)) {
                            return UDS_RESULT_RESPONSE_TOO_LONG;
                        }
                    }
                }
            }
        }
        break;
    }

    /* 0x08: Severity Record */
    case 0x08U: {
        if (!append_byte(response, &len, response_capacity,
                         s_dtc_storage.status_availability_mask)) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        uint8_t total_count = get_total_record_count();
        for (uint8_t i = 0U; i < total_count; ++i) {
            UdsDtcAppRecord rec_storage;
            (void)get_dtc_record(i, &rec_storage);
            const UdsDtcAppRecord *rec = &rec_storage;
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
        if (request_length < 5U) {
            return UDS_RESULT_ERROR;
        }
        uint32_t target_dtc =
            ((uint32_t)request[2] << 16U) | ((uint32_t)request[3] << 8U) | (uint32_t)request[4];
        int16_t idx = find_dtc_index(target_dtc);
        if (idx < 0) {
            return UDS_RESULT_OUT_OF_RANGE;
        }
        UdsDtcAppRecord rec;
        (void)get_dtc_record((uint8_t)idx, &rec);
        if (!append_byte(response, &len, response_capacity,
                         s_dtc_storage.status_availability_mask) ||
            !append_byte(response, &len, response_capacity, rec.severity) ||
            !append_byte(response, &len, response_capacity, rec.functional_unit) ||
            !append_dtc(response, &len, response_capacity, &rec)) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        break;
    }

    /* 0x14: Fault Detection Counter */
    case 0x14U: {
        uint8_t total_count = get_total_record_count();
        for (uint8_t i = 0U; i < total_count; ++i) {
            UdsDtcAppRecord rec_storage;
            (void)get_dtc_record(i, &rec_storage);
            const UdsDtcAppRecord *rec = &rec_storage;
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
    if (store != NULL) {
        (void)uds_dtc_app_load_from_nvm();
    }
}

bool uds_dtc_app_save_to_nvm(void) {
    if (s_dtc_storage.nvm_store == NULL) {
        return false;
    }
    UdsDtcNvBlock block;
    memset(&block, 0, sizeof(block));
    uint8_t total_count = get_total_record_count();
    block.record_count = total_count;
    block.snapshot_count = 0U;
    for (uint8_t i = 0U; i < total_count; ++i) {
        UdsDtcAppRecord rec;
        (void)get_dtc_record(i, &rec);
        block.records[i].dtc_number = rec.dtc_number;
        block.records[i].status_byte = rec.status_byte;
        block.records[i].severity = rec.severity;
        block.records[i].functional_unit = rec.functional_unit;
        block.records[i].fault_counter = rec.fault_counter;
        block.records[i].occurrence_counter = rec.occurrence_counter;
        block.records[i].aging_counter = rec.aging_counter;
        block.records[i].active = rec.active;

        if (rec.has_snapshot && (rec.snapshot_length > 0U) &&
            (block.snapshot_count < UDS_DTC_NV_MAX_SNAPSHOTS)) {
            uint8_t s_idx = block.snapshot_count++;
            block.snapshots[s_idx].dtc_number = rec.dtc_number;
            block.snapshots[s_idx].record_num = rec.snapshot_record_num;
            block.snapshots[s_idx].length = rec.snapshot_length;
            (void)memcpy(block.snapshots[s_idx].data, rec.snapshot_data, rec.snapshot_length);
        }
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
    for (uint8_t j = 0U; j < block.record_count; ++j) {
        int16_t idx = find_dtc_index(block.records[j].dtc_number);
        if (idx >= 0) {
            UdsDtcRamStatus *ram = get_dtc_ram_status((uint8_t)idx);
            if (ram != NULL) {
                ram->status_byte = block.records[j].status_byte;
                ram->fault_counter = block.records[j].fault_counter;
                ram->occurrence_counter = block.records[j].occurrence_counter;
                ram->aging_counter = block.records[j].aging_counter;
                ram->active = block.records[j].active;
            }
        } else if (s_dtc_storage.dynamic_count < UDS_DTC_DYNAMIC_MAX) {
            uint8_t d = s_dtc_storage.dynamic_count++;
            s_dtc_storage.dynamic_records[d].dtc_number = block.records[j].dtc_number;
            s_dtc_storage.dynamic_records[d].severity = block.records[j].severity;
            s_dtc_storage.dynamic_records[d].functional_unit = block.records[j].functional_unit;
            s_dtc_storage.dynamic_records[d].status.status_byte = block.records[j].status_byte;
            s_dtc_storage.dynamic_records[d].status.fault_counter = block.records[j].fault_counter;
            s_dtc_storage.dynamic_records[d].status.occurrence_counter =
                block.records[j].occurrence_counter;
            s_dtc_storage.dynamic_records[d].status.aging_counter = block.records[j].aging_counter;
            s_dtc_storage.dynamic_records[d].status.active = block.records[j].active;
        }
    }
    for (uint8_t s = 0U; s < block.snapshot_count; ++s) {
        int16_t idx = find_dtc_index(block.snapshots[s].dtc_number);
        if (idx >= 0) {
            UdsDtcRamStatus *ram = get_dtc_ram_status((uint8_t)idx);
            if (ram != NULL) {
                ram->has_snapshot = true;
                ram->snapshot_record_num = block.snapshots[s].record_num;
                ram->snapshot_length = block.snapshots[s].length;
                (void)memcpy(ram->snapshot_data, block.snapshots[s].data,
                             block.snapshots[s].length);
            }
        }
    }
    return true;
}

void uds_dtc_app_init(void) {
    memset(&s_dtc_storage, 0, sizeof(s_dtc_storage));
    s_dtc_storage.status_availability_mask = 0xFFU;
    s_dtc_storage.dynamic_count = 0U;
    s_dtc_storage.dtc_setting_enabled = true;

    /* Initialize static status from ROM defaults */
    for (uint8_t i = 0U; i < UDS_DTC_STATIC_COUNT; ++i) {
        s_dtc_storage.static_status[i].status_byte = s_rom_dtc_defs[i].default_status;
        s_dtc_storage.static_status[i].fault_counter = s_rom_dtc_defs[i].default_fault_counter;
        s_dtc_storage.static_status[i].occurrence_counter =
            s_rom_dtc_defs[i].default_occurrence_counter;
        s_dtc_storage.static_status[i].aging_counter = s_rom_dtc_defs[i].default_aging_counter;
        s_dtc_storage.static_status[i].active = s_rom_dtc_defs[i].default_active;
        s_dtc_storage.static_status[i].has_snapshot = false;
        s_dtc_storage.static_status[i].snapshot_record_num = 0x01U;
        s_dtc_storage.static_status[i].snapshot_length = 0U;
    }

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
    uint8_t total_count = get_total_record_count();

    if (group_of_dtc == 0xFFFFFFUL) {
        /* Clear all DTCs - set to 0x50 per AUTOSAR Dem specification */
        for (uint8_t i = 0U; i < total_count; ++i) {
            UdsDtcRamStatus *ram = get_dtc_ram_status(i);
            if (ram != NULL) {
                ram->active = false;
                ram->status_byte = UDS_DTC_STATUS_CLEARED;
                ram->fault_counter = 0;
                ram->has_snapshot = false;
                ram->snapshot_length = 0U;
            }
        }
        (void)uds_dtc_app_save_to_nvm();
        return UDS_RESULT_OK;
    }

    /* Check standard ISO 14229-1 functional group masks */
    uint32_t group_high = group_of_dtc & 0xFF0000UL;
    bool is_group_mask =
        ((group_of_dtc & 0x00FFFFUL) == 0x000000UL) || ((group_of_dtc & 0x00FFFFUL) == 0x00FF00UL);

    if (is_group_mask) {
        for (uint8_t i = 0U; i < total_count; ++i) {
            UdsDtcAppRecord rec;
            (void)get_dtc_record(i, &rec);
            uint32_t dtc = rec.dtc_number;
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
                UdsDtcRamStatus *ram = get_dtc_ram_status(i);
                if (ram != NULL) {
                    ram->active = false;
                    ram->status_byte = UDS_DTC_STATUS_CLEARED;
                    ram->fault_counter = 0;
                    ram->has_snapshot = false;
                    ram->snapshot_length = 0U;
                }
                cleared_any = true;
            }
        }
        if (cleared_any) {
            (void)uds_dtc_app_save_to_nvm();
            return UDS_RESULT_OK;
        }
    }

    /* Match individual DTC */
    int16_t idx = find_dtc_index(group_of_dtc);
    if (idx >= 0) {
        UdsDtcRamStatus *ram = get_dtc_ram_status((uint8_t)idx);
        if (ram != NULL) {
            ram->active = false;
            ram->status_byte = UDS_DTC_STATUS_CLEARED;
            ram->fault_counter = 0;
            ram->has_snapshot = false;
            ram->snapshot_length = 0U;
        }
        (void)uds_dtc_app_save_to_nvm();
        return UDS_RESULT_OK;
    }
    return UDS_RESULT_OUT_OF_RANGE;
}

UdsCallbackResult uds_dtc_app_control_setting(void *context, uint8_t subfunction) {
    (void)context;
    if (subfunction == 0x01U) {
        s_dtc_storage.dtc_setting_enabled = true;
        return UDS_RESULT_OK;
    }
    if (subfunction == 0x02U) {
        s_dtc_storage.dtc_setting_enabled = false;
        return UDS_RESULT_OK;
    }
    return UDS_RESULT_SUBFUNCTION_NOT_SUPPORTED;
}

bool uds_dtc_app_is_setting_enabled(void) {
    return s_dtc_storage.dtc_setting_enabled;
}

void uds_dtc_app_set_setting_enabled(bool enabled) {
    s_dtc_storage.dtc_setting_enabled = enabled;
}

bool uds_dtc_app_set_snapshot(uint32_t dtc, uint8_t record_num, const uint8_t *data,
                              uint8_t length) {
    if ((data == NULL) || (length == 0U) || (length > UDS_DTC_APP_SNAPSHOT_SIZE)) {
        return false;
    }
    int16_t idx = find_dtc_index(dtc);
    if (idx < 0) {
        return false;
    }
    UdsDtcRamStatus *ram = get_dtc_ram_status((uint8_t)idx);
    if (ram == NULL) {
        return false;
    }
    ram->has_snapshot = true;
    ram->snapshot_record_num = record_num;
    ram->snapshot_length = length;
    (void)memcpy(ram->snapshot_data, data, length);
    (void)uds_dtc_app_save_to_nvm();
    return true;
}

bool uds_dtc_app_set_fault(uint32_t dtc, uint8_t status, uint8_t severity, int8_t counter) {
    if (!s_dtc_storage.dtc_setting_enabled) {
        return true; /* Setting disabled: updates suppressed per ISO 14229-1 */
    }
    int16_t idx = find_dtc_index(dtc);
    if (idx >= 0) {
        UdsDtcRamStatus *ram = get_dtc_ram_status((uint8_t)idx);
        if (ram != NULL) {
            ram->active = true;
            ram->status_byte = status;
            ram->fault_counter = counter;
            if ((status & UDS_DTC_STATUS_TEST_FAILED) != 0U) {
                if (ram->occurrence_counter < 255U) {
                    ram->occurrence_counter++;
                }
                ram->aging_counter = 0U;
            }
            (void)uds_dtc_app_save_to_nvm();
            return true;
        }
    }
    if (s_dtc_storage.dynamic_count < UDS_DTC_DYNAMIC_MAX) {
        uint8_t d = s_dtc_storage.dynamic_count++;
        s_dtc_storage.dynamic_records[d].dtc_number = dtc;
        s_dtc_storage.dynamic_records[d].severity = severity;
        s_dtc_storage.dynamic_records[d].functional_unit = 0x01U;
        s_dtc_storage.dynamic_records[d].status.status_byte = status;
        s_dtc_storage.dynamic_records[d].status.fault_counter = counter;
        s_dtc_storage.dynamic_records[d].status.occurrence_counter =
            ((status & UDS_DTC_STATUS_TEST_FAILED) != 0U) ? 1U : 0U;
        s_dtc_storage.dynamic_records[d].status.aging_counter = 0U;
        s_dtc_storage.dynamic_records[d].status.active = true;
        s_dtc_storage.dynamic_records[d].status.has_snapshot = false;
        s_dtc_storage.dynamic_records[d].status.snapshot_record_num = 0x01U;
        s_dtc_storage.dynamic_records[d].status.snapshot_length = 0U;
        (void)uds_dtc_app_save_to_nvm();
        return true;
    }
    return false;
}

bool uds_dtc_app_clear_fault(uint32_t dtc) {
    int16_t idx = find_dtc_index(dtc);
    if (idx >= 0) {
        UdsDtcRamStatus *ram = get_dtc_ram_status((uint8_t)idx);
        if (ram != NULL) {
            ram->active = false;
            ram->status_byte = UDS_DTC_STATUS_CLEARED;
            ram->has_snapshot = false;
            ram->snapshot_length = 0U;
            (void)uds_dtc_app_save_to_nvm();
            return true;
        }
    }
    return false;
}

/* AUTOSAR Dem counter-based fault debouncing engine */
bool uds_dtc_app_report_event(uint32_t dtc, bool failed) {
    if (!s_dtc_storage.dtc_setting_enabled) {
        return true; /* Setting disabled: updates suppressed per ISO 14229-1 */
    }
    int16_t idx = find_dtc_index(dtc);
    if (idx >= 0) {
        UdsDtcRamStatus *rec = get_dtc_ram_status((uint8_t)idx);
        if (rec != NULL) {
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
