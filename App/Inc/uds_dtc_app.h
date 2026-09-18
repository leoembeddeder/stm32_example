/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */

#ifndef STM32_UDS_ISO_TP_UDS_DTC_APP_H
#define STM32_UDS_ISO_TP_UDS_DTC_APP_H

#include "uds_iso_tp/uds.h"
#include "uds_iso_tp/uds_dtc.h"
#include "uds_iso_tp/uds_wear_leveling.h"

#include <stdbool.h>
#include <stdint.h>

#define UDS_DTC_APP_MAX_RECORDS 80U
#define UDS_DTC_OEM_COUNT 66U
#define UDS_DTC_APP_SNAPSHOT_SIZE 16U
#define UDS_DTC_APP_EXTENDED_SIZE 8U

/* Standard Freeze Frame Snapshot DIDs per OEM Diagnostic Specification */
#define UDS_DTC_DID_VBAT 0xDF00U               /* ECU power supply voltage (mV, 0-36V) */
#define UDS_DTC_DID_VEHICLE_SPEED 0xDF01U      /* Vehicle speed (1/256 km/h) */
#define UDS_DTC_DID_OCCURRENCE_COUNTER 0xDF02U /* Fault occurrence counter */
#define UDS_DTC_DID_FIRST_ODOMETER 0xDF03U     /* First odometer of malfunction */
#define UDS_DTC_DID_LAST_ODOMETER 0xDF04U      /* Last odometer of malfunction */
#define UDS_DTC_DID_TIMESTAMP 0xDD00U          /* Malfunction timestamp (sec,min,hr,m,d,y) */

/* Extended Data Record Numbers (ISO 14229-1 & AUTOSAR Dem) */
#define UDS_DTC_EXT_DATA_OCCURRENCES 0x01U     /* Fault occurrence counter */
#define UDS_DTC_EXT_DATA_PENDING_COUNTER 0x02U /* Fault pending counter */
#define UDS_DTC_EXT_DATA_AGING_COUNTER 0x03U   /* Aging counter (consecutive unfailed cycles) */
#define UDS_DTC_EXT_DATA_AGED_COUNTER 0x04U    /* Aged / unlearned counter */
#define UDS_DTC_EXT_DATA_ALL 0xFFU             /* All extended data records */

/* OBD Extended Data Format */
typedef struct {
    uint8_t fault_occur_counter;
    uint8_t fault_pending_counter;
    uint8_t aged_counter;
    uint8_t ageing_counter;
} OBD_Extended_Data_Format;

/* Snapshot Time & Date Structure Definition */
typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
} OBD_Global_Snapshot_DataTime_Format;

/* DTC Global Snapshot Data Structure Definition */
typedef struct {
    uint8_t voltage; /* Battery supply voltage in 0.1V units (e.g. 120 = 12.0V) */
    uint8_t
        global_power_mode; /* Global power mode (e.g. 0x01=OFF, 0x02=ACC, 0x03=ON, 0x04=START) */
    OBD_Global_Snapshot_DataTime_Format st_global_snapshot_datatime;
} OBD_Global_Snapshot_Format;

#define UDS_DTC_DID_GLOBAL_SNAPSHOT 0x0100U

#define UDS_DTC_STATIC_COUNT 69U
#define UDS_DTC_DYNAMIC_MAX (UDS_DTC_APP_MAX_RECORDS - UDS_DTC_STATIC_COUNT)

/* Static ROM definition for pre-configured OEM and baseline DTCs */
typedef struct {
    uint32_t dtc_number;     /* 24-bit diagnostic trouble code identifier */
    uint8_t severity;        /* DTC severity mask */
    uint8_t functional_unit; /* Functional unit */
    uint8_t default_status;  /* Initial status byte */
    int8_t default_fault_counter;
    uint8_t default_occurrence_counter;
    uint8_t default_aging_counter;
    bool default_active;
    const uint8_t *snapshot_data;
    uint8_t snapshot_length;
    const uint8_t *extended_data;
    uint8_t extended_length;
} UdsDtcRomDef;

/* Compact runtime status stored in SRAM */
typedef struct {
    uint8_t status_byte;        /* ISO 14229-1 status mask bitfield */
    int8_t fault_counter;       /* Fault detection counter (-128 to 127) */
    uint8_t occurrence_counter; /* Fault occurrence counter */
    uint8_t pending_counter;    /* Fault pending counter */
    uint8_t aging_counter;      /* Aging counter */
    uint8_t aged_counter;       /* Aged counter */
    bool active;
    bool has_snapshot;
    uint8_t snapshot_record_num;
    uint8_t snapshot_length;
    uint8_t snapshot_data[UDS_DTC_APP_SNAPSHOT_SIZE];
    uint8_t extended_data[UDS_DTC_APP_EXTENDED_SIZE];
    uint8_t extended_length;
} UdsDtcRamStatus;

/* Dynamic record for runtime-registered DTCs not present in ROM */
typedef struct {
    uint32_t dtc_number;
    uint8_t severity;
    uint8_t functional_unit;
    UdsDtcRamStatus status;
} UdsDtcDynamicRecord;

/* Transient composite record view used during query and reporting */
typedef struct {
    uint32_t dtc_number;        /* 24-bit diagnostic trouble code identifier */
    uint8_t status_byte;        /* ISO 14229-1 status mask bitfield */
    uint8_t severity;           /* DTC severity mask */
    uint8_t functional_unit;    /* Functional unit */
    int8_t fault_counter;       /* Fault detection counter (-128 to 127) */
    uint8_t occurrence_counter; /* Fault occurrence counter */
    uint8_t pending_counter;    /* Fault pending counter */
    uint8_t aging_counter;      /* Aging counter */
    uint8_t aged_counter;       /* Aged counter */
    const uint8_t *snapshot_data;
    uint8_t snapshot_length;
    const uint8_t *extended_data;
    uint8_t extended_length;
    bool active;
    bool has_snapshot;
    uint8_t snapshot_record_num;
} UdsDtcAppRecord;

typedef struct {
    UdsDtcBackend backend;
    uint8_t status_availability_mask;
    UdsDtcRamStatus static_status[UDS_DTC_STATIC_COUNT];
    UdsDtcDynamicRecord dynamic_records[UDS_DTC_DYNAMIC_MAX];
    uint8_t dynamic_count;
    bool dtc_setting_enabled;
    UdsParamStore *nvm_store;
} UdsDtcAppStorage;

#define UDS_DTC_NV_MAX_SNAPSHOTS 8U

typedef struct {
    uint32_t dtc_number;
    uint8_t record_num;
    uint8_t length;
    uint8_t _pad;
    uint8_t data[UDS_DTC_APP_SNAPSHOT_SIZE];
} UdsDtcNvSnapshot;

/* Layout for NVM persistence across power cycles / resets */
typedef struct {
    uint8_t record_count;
    uint8_t snapshot_count;
    uint8_t _pad[2];
    struct {
        uint32_t dtc_number;
        uint8_t status_byte;
        uint8_t severity;
        uint8_t functional_unit;
        int8_t fault_counter;
        uint8_t occurrence_counter;
        uint8_t pending_counter;
        uint8_t aging_counter;
        uint8_t aged_counter;
        bool active;
    } records[UDS_DTC_APP_MAX_RECORDS];
    UdsDtcNvSnapshot snapshots[UDS_DTC_NV_MAX_SNAPSHOTS];
} UdsDtcNvBlock;

/* ISO 14229-1 Annex D DTC Status Mask Bitfield Definitions */
#define UDS_DTC_STATUS_TEST_FAILED (1U << 0U)            /* 0x01 */
#define UDS_DTC_STATUS_TEST_FAILED_THIS_CYCLE (1U << 1U) /* 0x02 */
#define UDS_DTC_STATUS_PENDING (1U << 2U)                /* 0x04 */
#define UDS_DTC_STATUS_CONFIRMED (1U << 3U)              /* 0x08 */
#define UDS_DTC_STATUS_TEST_NOT_COMPLETED_SLC (1U << 4U) /* 0x10 */
#define UDS_DTC_STATUS_TEST_FAILED_SLC (1U << 5U)        /* 0x20 */
#define UDS_DTC_STATUS_TEST_NOT_COMPLETED_TOC (1U << 6U) /* 0x40 */
#define UDS_DTC_STATUS_WARNING_INDICATOR_REQ (1U << 7U)  /* 0x80 */

/* Status byte after ClearDiagnosticInformation (0x14) per AUTOSAR Dem specification */
#define UDS_DTC_STATUS_CLEARED                                                                     \
    (UDS_DTC_STATUS_TEST_NOT_COMPLETED_SLC | UDS_DTC_STATUS_TEST_NOT_COMPLETED_TOC) /* 0x50 */

void uds_dtc_app_init(void);
const UdsDtcBackend *uds_dtc_app_get_backend(void);
UdsCallbackResult uds_dtc_app_clear(void *context, uint32_t group_of_dtc);
bool uds_dtc_app_set_fault(uint32_t dtc, uint8_t status, uint8_t severity, int8_t counter);
bool uds_dtc_app_clear_fault(uint32_t dtc);
bool uds_dtc_app_report_event(uint32_t dtc, bool failed);

/* ControlDTCSetting (0x85) Service Callbacks & State */
UdsCallbackResult uds_dtc_app_control_setting(void *context, uint8_t subfunction);
bool uds_dtc_app_is_setting_enabled(void);
void uds_dtc_app_set_setting_enabled(bool enabled);

/* Snapshot / Freeze Frame Buffer API */
bool uds_dtc_app_set_snapshot(uint32_t dtc, uint8_t record_num, const uint8_t *data,
                              uint8_t length);
bool uds_dtc_app_set_global_snapshot(uint32_t dtc, const OBD_Global_Snapshot_Format *snapshot);
bool uds_dtc_app_get_global_snapshot(uint32_t dtc, OBD_Global_Snapshot_Format *snapshot);
bool uds_dtc_app_set_extended_data(uint32_t dtc, const OBD_Extended_Data_Format *ext_data);
bool uds_dtc_app_get_extended_data(uint32_t dtc, OBD_Extended_Data_Format *ext_data);

/* NVM / Flash Wear-Leveling Hook */
void uds_dtc_app_attach_nvm(UdsParamStore *store);
bool uds_dtc_app_save_to_nvm(void);
bool uds_dtc_app_load_from_nvm(void);

#endif /* STM32_UDS_ISO_TP_UDS_DTC_APP_H */
