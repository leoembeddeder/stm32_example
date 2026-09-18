/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#ifndef STM32_UDS_ISO_TP_UDS_LINK_CONTROL_APP_H
#define STM32_UDS_ISO_TP_UDS_LINK_CONTROL_APP_H

#include "uds_iso_tp/uds.h"
#include "uds_iso_tp/uds_services.h"

#include <stdbool.h>
#include <stdint.h>

/* ISO 14229-1 LinkControl (0x87) subfunctions */
#define UDS_LINK_CONTROL_VERIFY_FIXED 0x01U
#define UDS_LINK_CONTROL_VERIFY_SPECIFIC 0x02U
#define UDS_LINK_CONTROL_TRANSITION_MODE 0x03U

/* Supported fixed mode identifiers */
#define UDS_LINK_MODE_9600_BAUD 0x01U
#define UDS_LINK_MODE_19200_BAUD 0x02U
#define UDS_LINK_MODE_38400_BAUD 0x03U
#define UDS_LINK_MODE_57600_BAUD 0x04U
#define UDS_LINK_MODE_115200_BAUD 0x05U
#define UDS_LINK_MODE_125K_CAN 0x10U
#define UDS_LINK_MODE_250K_CAN 0x11U
#define UDS_LINK_MODE_500K_CAN 0x12U
#define UDS_LINK_MODE_1M_CAN 0x13U

/* Default baud rate */
#define UDS_LINK_DEFAULT_BAUDRATE 500000U

void uds_link_control_app_init(void);

const UdsLinkControlServiceBackend *uds_link_control_app_get_backend(void);

UdsCallbackResult uds_link_control_app_handler(void *context, const uint8_t *request,
                                               uint16_t request_length, uint8_t *response,
                                               uint16_t *response_length,
                                               uint16_t response_capacity);

/* Inspect / verify state */
bool uds_link_control_app_is_verified(void);
uint32_t uds_link_control_app_get_baudrate(void);
void uds_link_control_app_reset(void);

#endif /* STM32_UDS_ISO_TP_UDS_LINK_CONTROL_APP_H */
