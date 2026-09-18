/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#ifndef STM32_UDS_ISO_TP_UDS_IO_CONTROL_APP_H
#define STM32_UDS_ISO_TP_UDS_IO_CONTROL_APP_H

#include "uds_iso_tp/uds.h"

#include <stdbool.h>
#include <stdint.h>

/* ISO 14229-1 InputOutputControlParameter values */
#define UDS_IOCP_RETURN_CONTROL_TO_ECU 0x00U
#define UDS_IOCP_RESET_TO_DEFAULT 0x01U
#define UDS_IOCP_FREEZE_CURRENT_STATE 0x02U
#define UDS_IOCP_SHORT_TERM_ADJUSTMENT 0x03U

/* Supported DIDs for IO Control */
#define UDS_IO_DID_AIR_INLET_DOOR 0x9B00U
#define UDS_IO_DID_EGR_AND_IAC 0x0155U

/* Default values */
#define UDS_IO_AIR_INLET_DEFAULT_POS 50U
#define UDS_IO_EGR_DEFAULT_DUTY 20U
#define UDS_IO_IAC_DEFAULT_STEPS 80U

/* Control enable masks for DID 0x0155 (ISO 14229-1 example) */
#define UDS_IO_MASK_EGR_ENABLE 0x01U
#define UDS_IO_MASK_IAC_ENABLE 0x01U

void uds_io_control_app_init(void);

UdsCallbackResult uds_io_control_app_handler(void *context, uint16_t did, const uint8_t *parameter,
                                             uint16_t parameter_len, uint8_t *response,
                                             uint16_t *response_len, uint16_t capacity);

/* Air Inlet Door DID 0x9B00 status */
uint8_t uds_io_control_app_get_air_inlet_position(void);
bool uds_io_control_app_get_air_inlet_under_control(void);
bool uds_io_control_app_get_air_inlet_frozen(void);

/* EGR & IAC DID 0x0155 status */
uint8_t uds_io_control_app_get_egr_duty(void);
uint8_t uds_io_control_app_get_iac_steps(void);
bool uds_io_control_app_get_egr_under_control(void);
bool uds_io_control_app_get_iac_under_control(void);
bool uds_io_control_app_get_egr_iac_frozen(void);

#endif /* STM32_UDS_ISO_TP_UDS_IO_CONTROL_APP_H */
