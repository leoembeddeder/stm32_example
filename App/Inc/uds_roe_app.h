/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#ifndef STM32_UDS_ISO_TP_UDS_ROE_APP_H
#define STM32_UDS_ISO_TP_UDS_ROE_APP_H

#include "uds_iso_tp/uds.h"
#include "uds_iso_tp/uds_services.h"

#include <stdbool.h>
#include <stdint.h>

/* ISO 14229-1 ResponseOnEvent (0x86) subfunctions */
#define UDS_ROE_STOP_RESPONSE_ON_EVENT 0x00U
#define UDS_ROE_ON_DTC_STATUS_CHANGE 0x01U
#define UDS_ROE_ON_CHANGE_OF_DATA_IDENTIFIER 0x03U
#define UDS_ROE_REPORT_ACTIVATED_EVENTS 0x04U
#define UDS_ROE_START_RESPONSE_ON_EVENT 0x05U
#define UDS_ROE_CLEAR_RESPONSE_ON_EVENT 0x06U

void uds_roe_app_init(void);

const UdsPeriodicEventServiceBackend *uds_roe_app_get_backend(void);

UdsCallbackResult uds_roe_app_handler(void *context, const uint8_t *request,
                                      uint16_t request_length, uint8_t *response,
                                      uint16_t *response_length, uint16_t response_capacity);

/* Inspect / verify state */
bool uds_roe_app_is_active(void);
bool uds_roe_app_is_configured(void);
uint8_t uds_roe_app_get_event_type(void);
uint8_t uds_roe_app_get_event_window_time(void);
uint8_t uds_roe_app_get_dtc_status_mask(void);
uint16_t uds_roe_app_get_monitored_did(void);
void uds_roe_app_clear(void);

#endif /* STM32_UDS_ISO_TP_UDS_ROE_APP_H */
