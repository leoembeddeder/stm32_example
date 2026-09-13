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

#define UDS_DTC_APP_MAX_RECORDS 16U
#define UDS_DTC_APP_SNAPSHOT_SIZE 16U
#define UDS_DTC_APP_EXTENDED_SIZE 8U

/* Standard Freeze Frame Snapshot DIDs per OEM Diagnostic Specification */
#define UDS_DTC_DID_VBAT 0xDF00U               /* ECU power supply voltage (mV, 0-36V) */
#define UDS_DTC_DID_VEHICLE_SPEED 0xDF01U      /* Vehicle speed (1/256 km/h) */
#define UDS_DTC_DID_OCCURRENCE_COUNTER 0xDF02U /* Fault occurrence counter */
#define UDS_DTC_DID_FIRST_ODOMETER 0xDF03U     /* First odometer of malfunction */
#define UDS_DTC_DID_LAST_ODOMETER 0xDF04U      /* Last odometer of malfunction */
#define UDS_DTC_DID_TIMESTAMP 0xDD00U          /* Malfunction timestamp (sec,min,hr,m,d,y) */

/* Standard Extended Data Record Numbers */
#define UDS_DTC_EXT_DATA_OCCURRENCES 0x01U   /* Fault occurrence counter */
#define UDS_DTC_EXT_DATA_AGING_COUNTER 0x02U /* Consecutive unfailed cycles */

typedef struct {
    uint32_t dtc_number;        /* 24-bit diagnostic trouble code identifier */
    uint8_t status_byte;        /* ISO 14229-1 status mask bitfield */
    uint8_t severity;           /* DTC severity mask */
    uint8_t functional_unit;    /* Functional unit */
    int8_t fault_counter;       /* Fault detection counter (-128 to 127) */
    uint8_t occurrence_counter; /* Fault occurrence counter */
    uint8_t aging_counter;      /* Aging counter */
    uint8_t snapshot_data[UDS_DTC_APP_SNAPSHOT_SIZE];
    uint8_t snapshot_length;
    uint8_t extended_data[UDS_DTC_APP_EXTENDED_SIZE];
    uint8_t extended_length;
    bool active;
} UdsDtcAppRecord;

typedef struct {
    UdsDtcBackend backend;
    uint8_t status_availability_mask;
    UdsDtcAppRecord records[UDS_DTC_APP_MAX_RECORDS];
    uint8_t record_count;
    UdsParamStore *nvm_store;
} UdsDtcAppStorage;

/* Layout for NVM persistence across power cycles / resets */
typedef struct {
    uint8_t record_count;
    struct {
        uint32_t dtc_number;
        uint8_t status_byte;
        uint8_t severity;
        uint8_t functional_unit;
        int8_t fault_counter;
        uint8_t occurrence_counter;
        uint8_t aging_counter;
        bool active;
    } records[UDS_DTC_APP_MAX_RECORDS];
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

/* NVM / Flash Wear-Leveling Hook */
void uds_dtc_app_attach_nvm(UdsParamStore *store);
bool uds_dtc_app_save_to_nvm(void);
bool uds_dtc_app_load_from_nvm(void);

#endif /* STM32_UDS_ISO_TP_UDS_DTC_APP_H */
