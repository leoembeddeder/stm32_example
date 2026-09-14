#ifndef STM32_UDS_ISO_TP_UDS_DID_APP_H
#define STM32_UDS_ISO_TP_UDS_DID_APP_H

#include "uds_iso_tp/uds.h"

#include <stdbool.h>
#include <stdint.h>

#define UDS_DID_HIB_SPARE_PART_NUMBER 0xF181U
#define UDS_DID_BOOT_SW_IDENTIFIER 0xF182U
#define UDS_DID_ECU_SOFTWARE_NUMBER 0xF183U
#define UDS_DID_ECU_APP_SOFTWARE_NUMBER 0xF184U
#define UDS_DID_ECU_HARDWARE_NUMBER 0xF185U
#define UDS_DID_ACTIVE_DIAGNOSTIC_SESSION 0xF186U
#define UDS_DID_SYSTEM_SUPPLIER_IDENTIFIER 0xF18AU
#define UDS_DID_VIN 0xF190U

void uds_did_app_init(void);
void uds_did_app_set_active_session(uint8_t session);

UdsCallbackResult uds_did_app_read(void *context, uint16_t did, uint8_t *data, uint16_t *length,
                                   uint16_t capacity);
UdsCallbackResult uds_did_app_write(void *context, uint16_t did, const uint8_t *data,
                                    uint16_t length);

#endif /* STM32_UDS_ISO_TP_UDS_DID_APP_H */