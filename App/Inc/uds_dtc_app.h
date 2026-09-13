#ifndef STM32_UDS_ISO_TP_UDS_DTC_APP_H
#define STM32_UDS_ISO_TP_UDS_DTC_APP_H

#include "uds_iso_tp/uds.h"
#include "uds_iso_tp/uds_dtc.h"

#include <stdbool.h>
#include <stdint.h>

#define UDS_DTC_APP_MAX_RECORDS 8U
#define UDS_DTC_APP_SNAPSHOT_SIZE 8U
#define UDS_DTC_APP_EXTENDED_SIZE 4U

typedef struct {
    uint32_t dtc_number;     /* 24-bit diagnostic trouble code identifier */
    uint8_t status_byte;     /* ISO 14229-1 status mask bitfield */
    uint8_t severity;        /* DTC severity mask */
    uint8_t functional_unit; /* Functional unit */
    int8_t fault_counter;    /* Fault detection counter (-128 to 127) */
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
} UdsDtcAppStorage;

void uds_dtc_app_init(void);
const UdsDtcBackend *uds_dtc_app_get_backend(void);
UdsCallbackResult uds_dtc_app_clear(void *context, uint32_t group_of_dtc);
bool uds_dtc_app_set_fault(uint32_t dtc, uint8_t status, uint8_t severity, int8_t counter);
bool uds_dtc_app_clear_fault(uint32_t dtc);

#endif /* STM32_UDS_ISO_TP_UDS_DTC_APP_H */
