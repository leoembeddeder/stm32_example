/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#include "uds_io_control_app.h"

#include <stddef.h>

static uint8_t s_air_inlet_pos = UDS_IO_AIR_INLET_DEFAULT_POS;
static bool s_air_inlet_under_control = false;
static bool s_air_inlet_frozen = false;

static uint8_t s_egr_duty = UDS_IO_EGR_DEFAULT_DUTY;
static uint8_t s_iac_steps = UDS_IO_IAC_DEFAULT_STEPS;
static bool s_egr_under_control = false;
static bool s_iac_under_control = false;
static bool s_egr_iac_frozen = false;

void uds_io_control_app_init(void) {
    s_air_inlet_pos = UDS_IO_AIR_INLET_DEFAULT_POS;
    s_air_inlet_under_control = false;
    s_air_inlet_frozen = false;

    s_egr_duty = UDS_IO_EGR_DEFAULT_DUTY;
    s_iac_steps = UDS_IO_IAC_DEFAULT_STEPS;
    s_egr_under_control = false;
    s_iac_under_control = false;
    s_egr_iac_frozen = false;
}

static UdsCallbackResult handle_air_inlet_door(const uint8_t *parameter, uint16_t parameter_len,
                                               uint8_t *response, uint16_t *response_len,
                                               uint16_t capacity) {
    uint8_t control_param = parameter[0];

    switch (control_param) {
    case UDS_IOCP_RETURN_CONTROL_TO_ECU:
        if (parameter_len != 1U) {
            return UDS_RESULT_INVALID_FORMAT;
        }
        s_air_inlet_pos = UDS_IO_AIR_INLET_DEFAULT_POS;
        s_air_inlet_under_control = false;
        s_air_inlet_frozen = false;
        break;

    case UDS_IOCP_RESET_TO_DEFAULT:
        if (parameter_len != 1U) {
            return UDS_RESULT_INVALID_FORMAT;
        }
        s_air_inlet_pos = UDS_IO_AIR_INLET_DEFAULT_POS;
        s_air_inlet_under_control = true;
        s_air_inlet_frozen = false;
        break;

    case UDS_IOCP_FREEZE_CURRENT_STATE:
        if (parameter_len != 1U) {
            return UDS_RESULT_INVALID_FORMAT;
        }
        s_air_inlet_under_control = true;
        s_air_inlet_frozen = true;
        break;

    case UDS_IOCP_SHORT_TERM_ADJUSTMENT:
        if (parameter_len != 2U) {
            return UDS_RESULT_INVALID_FORMAT;
        }
        if (parameter[1] > 100U) {
            return UDS_RESULT_OUT_OF_RANGE;
        }
        s_air_inlet_pos = parameter[1];
        s_air_inlet_under_control = true;
        s_air_inlet_frozen = false;
        break;

    default:
        return UDS_RESULT_SUBFUNCTION_NOT_SUPPORTED;
    }

    if (capacity < 2U) {
        return UDS_RESULT_RESPONSE_TOO_LONG;
    }
    response[0] = control_param;
    response[1] = s_air_inlet_pos;
    *response_len = 2U;
    return UDS_RESULT_OK;
}

static UdsCallbackResult handle_egr_iac(const uint8_t *parameter, uint16_t parameter_len,
                                        uint8_t *response, uint16_t *response_len,
                                        uint16_t capacity) {
    uint8_t control_param = parameter[0];

    switch (control_param) {
    case UDS_IOCP_RETURN_CONTROL_TO_ECU:
        if (parameter_len == 1U) {
            s_egr_duty = UDS_IO_EGR_DEFAULT_DUTY;
            s_egr_under_control = false;
            s_iac_steps = UDS_IO_IAC_DEFAULT_STEPS;
            s_iac_under_control = false;
            s_egr_iac_frozen = false;
        } else if (parameter_len == 3U) {
            /* With mask record */
            if ((parameter[1] & UDS_IO_MASK_EGR_ENABLE) != 0U) {
                s_egr_duty = UDS_IO_EGR_DEFAULT_DUTY;
                s_egr_under_control = false;
            }
            if ((parameter[2] & UDS_IO_MASK_IAC_ENABLE) != 0U) {
                s_iac_steps = UDS_IO_IAC_DEFAULT_STEPS;
                s_iac_under_control = false;
            }
            s_egr_iac_frozen = false;
        } else {
            return UDS_RESULT_INVALID_FORMAT;
        }
        break;

    case UDS_IOCP_RESET_TO_DEFAULT:
        if (parameter_len == 1U) {
            s_egr_duty = UDS_IO_EGR_DEFAULT_DUTY;
            s_egr_under_control = true;
            s_iac_steps = UDS_IO_IAC_DEFAULT_STEPS;
            s_iac_under_control = true;
            s_egr_iac_frozen = false;
        } else if (parameter_len == 3U) {
            if ((parameter[1] & UDS_IO_MASK_EGR_ENABLE) != 0U) {
                s_egr_duty = UDS_IO_EGR_DEFAULT_DUTY;
                s_egr_under_control = true;
            }
            if ((parameter[2] & UDS_IO_MASK_IAC_ENABLE) != 0U) {
                s_iac_steps = UDS_IO_IAC_DEFAULT_STEPS;
                s_iac_under_control = true;
            }
            s_egr_iac_frozen = false;
        } else {
            return UDS_RESULT_INVALID_FORMAT;
        }
        break;

    case UDS_IOCP_FREEZE_CURRENT_STATE:
        if ((parameter_len != 1U) && (parameter_len != 3U)) {
            return UDS_RESULT_INVALID_FORMAT;
        }
        s_egr_iac_frozen = true;
        break;

    case UDS_IOCP_SHORT_TERM_ADJUSTMENT:
        if (parameter_len == 3U) {
            /* No mask: parameter[1] = EGR, parameter[2] = IAC */
            if (parameter[1] > 100U) {
                return UDS_RESULT_OUT_OF_RANGE;
            }
            s_egr_duty = parameter[1];
            s_egr_under_control = true;
            s_iac_steps = parameter[2];
            s_iac_under_control = true;
            s_egr_iac_frozen = false;
        } else if (parameter_len == 5U) {
            /* With mask: parameter[1] = EGR, parameter[2] = IAC, parameter[3] = mask_egr, parameter[4] = mask_iac */
            uint8_t mask_egr = parameter[3];
            uint8_t mask_iac = parameter[4];
            if ((mask_egr & UDS_IO_MASK_EGR_ENABLE) != 0U) {
                if (parameter[1] > 100U) {
                    return UDS_RESULT_OUT_OF_RANGE;
                }
                s_egr_duty = parameter[1];
                s_egr_under_control = true;
            }
            if ((mask_iac & UDS_IO_MASK_IAC_ENABLE) != 0U) {
                s_iac_steps = parameter[2];
                s_iac_under_control = true;
            }
            s_egr_iac_frozen = false;
        } else {
            return UDS_RESULT_INVALID_FORMAT;
        }
        break;

    default:
        return UDS_RESULT_SUBFUNCTION_NOT_SUPPORTED;
    }

    if (capacity < 3U) {
        return UDS_RESULT_RESPONSE_TOO_LONG;
    }
    response[0] = control_param;
    response[1] = s_egr_duty;
    response[2] = s_iac_steps;
    *response_len = 3U;
    return UDS_RESULT_OK;
}

UdsCallbackResult uds_io_control_app_handler(void *context, uint16_t did, const uint8_t *parameter,
                                             uint16_t parameter_len, uint8_t *response,
                                             uint16_t *response_len, uint16_t capacity) {
    (void)context;
    if ((parameter == NULL) || (response == NULL) || (response_len == NULL)) {
        return UDS_RESULT_ERROR;
    }
    if (parameter_len < 1U) {
        return UDS_RESULT_INVALID_FORMAT;
    }

    if (did == UDS_IO_DID_AIR_INLET_DOOR) {
        return handle_air_inlet_door(parameter, parameter_len, response, response_len, capacity);
    }
    if (did == UDS_IO_DID_EGR_AND_IAC) {
        return handle_egr_iac(parameter, parameter_len, response, response_len, capacity);
    }

    return UDS_RESULT_OUT_OF_RANGE;
}

uint8_t uds_io_control_app_get_air_inlet_position(void) {
    return s_air_inlet_pos;
}

bool uds_io_control_app_get_air_inlet_under_control(void) {
    return s_air_inlet_under_control;
}

bool uds_io_control_app_get_air_inlet_frozen(void) {
    return s_air_inlet_frozen;
}

uint8_t uds_io_control_app_get_egr_duty(void) {
    return s_egr_duty;
}

uint8_t uds_io_control_app_get_iac_steps(void) {
    return s_iac_steps;
}

bool uds_io_control_app_get_egr_under_control(void) {
    return s_egr_under_control;
}

bool uds_io_control_app_get_iac_under_control(void) {
    return s_iac_under_control;
}

bool uds_io_control_app_get_egr_iac_frozen(void) {
    return s_egr_iac_frozen;
}
