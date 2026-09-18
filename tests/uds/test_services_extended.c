/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#include "uds_io_control_app.h"
#include "uds_iso_tp/uds.h"
#include "uds_iso_tp/uds_services.h"
#include "uds_link_control_app.h"
#include "uds_roe_app.h"

#include <assert.h>
#include <string.h>

static void test_io_control_air_inlet_door(void) {
    uds_io_control_app_init();

    UdsServer server;
    UdsCallbacks callbacks = {0};
    callbacks.io_control = uds_io_control_app_handler;
    uds_server_init(&server, &callbacks, NULL, 1000U);

    uint8_t response[64];
    uint16_t resp_len = 0U;

    /* Initial state check */
    assert(uds_io_control_app_get_air_inlet_position() == 50U);
    assert(!uds_io_control_app_get_air_inlet_under_control());
    assert(!uds_io_control_app_get_air_inlet_frozen());

    /* In Default Session (0x01), 0x2F should be rejected with NRC 0x7F (serviceNotSupportedInActiveSession) */
    const uint8_t req_adjust[] = {0x2FU, 0x9BU, 0x00U, 0x03U, 0x4BU};
    UdsCallbackResult res = uds_server_handle(&server, req_adjust, sizeof(req_adjust), response,
                                              &resp_len, sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 3U);
    assert(response[0] == 0x7FU && response[1] == 0x2FU &&
           response[2] == UDS_NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);

    /* Switch to Extended Diagnostic Session (0x03) */
    assert(uds_server_request_session(&server, 0x03U, 1000U) == UDS_RESULT_OK);

    /* 1. Short Term Adjustment to 75% (0x4B): 2F 9B 00 03 4B */
    res = uds_server_handle(&server, req_adjust, sizeof(req_adjust), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 5U);
    assert(response[0] == 0x6FU);
    assert(response[1] == 0x9BU);
    assert(response[2] == 0x00U);
    assert(response[3] == 0x03U);
    assert(response[4] == 0x4BU);
    assert(uds_io_control_app_get_air_inlet_position() == 75U);
    assert(uds_io_control_app_get_air_inlet_under_control());
    assert(!uds_io_control_app_get_air_inlet_frozen());

    /* 2. Freeze Current State: 2F 9B 00 02 */
    const uint8_t req_freeze[] = {0x2FU, 0x9BU, 0x00U, 0x02U};
    res = uds_server_handle(&server, req_freeze, sizeof(req_freeze), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 5U);
    assert(response[0] == 0x6FU);
    assert(response[1] == 0x9BU);
    assert(response[2] == 0x00U);
    assert(response[3] == 0x02U);
    assert(response[4] == 0x4BU);
    assert(uds_io_control_app_get_air_inlet_frozen());

    /* 3. Reset To Default: 2F 9B 00 01 */
    const uint8_t req_reset_def[] = {0x2FU, 0x9BU, 0x00U, 0x01U};
    res = uds_server_handle(&server, req_reset_def, sizeof(req_reset_def), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 5U);
    assert(response[0] == 0x6FU);
    assert(response[1] == 0x9BU);
    assert(response[2] == 0x00U);
    assert(response[3] == 0x01U);
    assert(response[4] == 50U);
    assert(uds_io_control_app_get_air_inlet_position() == 50U);
    assert(!uds_io_control_app_get_air_inlet_frozen());

    /* 4. Return Control To ECU: 2F 9B 00 00 */
    const uint8_t req_return[] = {0x2FU, 0x9BU, 0x00U, 0x00U};
    res = uds_server_handle(&server, req_return, sizeof(req_return), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 5U);
    assert(response[0] == 0x6FU);
    assert(response[1] == 0x9BU);
    assert(response[2] == 0x00U);
    assert(response[3] == 0x00U);
    assert(response[4] == 50U);
    assert(!uds_io_control_app_get_air_inlet_under_control());

    /* 5. Error: Out of range position (>100): 2F 9B 00 03 65 */
    const uint8_t req_oor[] = {0x2FU, 0x9BU, 0x00U, 0x03U, 101U};
    res = uds_server_handle(&server, req_oor, sizeof(req_oor), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 3U);
    assert(response[0] == 0x7FU);
    assert(response[1] == 0x2FU);
    assert(response[2] == UDS_NRC_REQUEST_OUT_OF_RANGE);

    /* 6. Error: Invalid length (missing position byte for 0x03): 2F 9B 00 03 */
    const uint8_t req_short[] = {0x2FU, 0x9BU, 0x00U, 0x03U};
    res = uds_server_handle(&server, req_short, sizeof(req_short), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 3U);
    assert(response[0] == 0x7FU);
    assert(response[1] == 0x2FU);
    assert(response[2] == UDS_NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

static void test_io_control_egr_iac(void) {
    uds_io_control_app_init();

    UdsServer server;
    UdsCallbacks callbacks = {0};
    callbacks.io_control = uds_io_control_app_handler;
    uds_server_init(&server, &callbacks, NULL, 1000U);
    assert(uds_server_request_session(&server, 0x03U, 1000U) == UDS_RESULT_OK);

    uint8_t response[64];
    uint16_t resp_len = 0U;

    /* Defaults: EGR = 20%, IAC = 80 */
    assert(uds_io_control_app_get_egr_duty() == 20U);
    assert(uds_io_control_app_get_iac_steps() == 80U);

    /* 1. Adjust with masks: EGR=40%, IAC=120: 2F 01 55 03 28 78 01 01 */
    const uint8_t req_adjust_both[] = {0x2FU, 0x01U, 0x55U, 0x03U, 40U, 120U, 0x01U, 0x01U};
    UdsCallbackResult res = uds_server_handle(&server, req_adjust_both, sizeof(req_adjust_both),
                                              response, &resp_len, sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 6U);
    assert(response[0] == 0x6FU);
    assert(response[1] == 0x01U);
    assert(response[2] == 0x55U);
    assert(response[3] == 0x03U);
    assert(response[4] == 40U);
    assert(response[5] == 120U);
    assert(uds_io_control_app_get_egr_duty() == 40U);
    assert(uds_io_control_app_get_iac_steps() == 120U);

    /* 2. Adjust EGR only using mask: EGR=60%, mask_egr=1, mask_iac=0: 2F 01 55 03 3C FF 01 00 */
    const uint8_t req_adjust_egr_only[] = {0x2FU, 0x01U, 0x55U, 0x03U, 60U, 255U, 0x01U, 0x00U};
    res = uds_server_handle(&server, req_adjust_egr_only, sizeof(req_adjust_egr_only), response,
                            &resp_len, sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 6U);
    assert(response[4] == 60U);
    assert(response[5] == 120U); /* IAC unchanged because mask was 0 */
    assert(uds_io_control_app_get_egr_duty() == 60U);
    assert(uds_io_control_app_get_iac_steps() == 120U);

    /* 3. Return control to ECU: 2F 01 55 00 */
    const uint8_t req_return[] = {0x2FU, 0x01U, 0x55U, 0x00U};
    res = uds_server_handle(&server, req_return, sizeof(req_return), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 6U);
    assert(response[3] == 0x00U);
    assert(response[4] == 20U);
    assert(response[5] == 80U);
    assert(uds_io_control_app_get_egr_duty() == 20U);
    assert(uds_io_control_app_get_iac_steps() == 80U);

    /* 4. Unsupported DID: 2F 12 34 00 */
    const uint8_t req_bad_did[] = {0x2FU, 0x12U, 0x34U, 0x00U};
    res = uds_server_handle(&server, req_bad_did, sizeof(req_bad_did), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 3U);
    assert(response[0] == 0x7FU);
    assert(response[1] == 0x2FU);
    assert(response[2] == UDS_NRC_REQUEST_OUT_OF_RANGE);
}

static void test_link_control(void) {
    uds_link_control_app_init();

    UdsServiceBackends backends = {0};
    backends.link_control = uds_link_control_app_get_backend();

    UdsServer server;
    UdsCallbacks callbacks = {0};
    callbacks.service_backends = &backends;
    uds_server_init(&server, &callbacks, NULL, 1000U);

    uint8_t response[64];
    uint16_t resp_len = 0U;

    /* 1. Sequence enforcement: sending transitionMode (87 03) without prior verify must fail */
    const uint8_t req_transition_unverified[] = {0x87U, 0x03U};
    UdsCallbackResult res =
        uds_server_handle(&server, req_transition_unverified, sizeof(req_transition_unverified),
                          response, &resp_len, sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 3U);
    assert(response[0] == 0x7FU);
    assert(response[1] == 0x87U);
    assert(response[2] == UDS_NRC_REQUEST_SEQUENCE_ERROR);

    /* 2. Verify with fixed parameter: 500k baud (0x12): 87 01 12 */
    const uint8_t req_verify_fixed[] = {0x87U, 0x01U, 0x12U};
    res = uds_server_handle(&server, req_verify_fixed, sizeof(req_verify_fixed), response,
                            &resp_len, sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 2U);
    assert(response[0] == 0xC7U);
    assert(response[1] == 0x01U);
    assert(uds_link_control_app_is_verified());

    /* 3. Now transitionMode succeeds: 87 03 */
    const uint8_t req_transition[] = {0x87U, 0x03U};
    res = uds_server_handle(&server, req_transition, sizeof(req_transition), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 2U);
    assert(response[0] == 0xC7U);
    assert(response[1] == 0x03U);
    assert(uds_link_control_app_get_baudrate() == 500000U);
    assert(!uds_link_control_app_is_verified());

    /* 4. Second transition without verify fails again */
    res = uds_server_handle(&server, req_transition, sizeof(req_transition), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 3U);
    assert(response[2] == UDS_NRC_REQUEST_SEQUENCE_ERROR);

    /* 5. Verify with specific parameter: 1000000 bps (0x0F 0x42 0x40): 87 02 0F 42 40 */
    const uint8_t req_verify_specific[] = {0x87U, 0x02U, 0x0FU, 0x42U, 0x40U};
    res = uds_server_handle(&server, req_verify_specific, sizeof(req_verify_specific), response,
                            &resp_len, sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 2U);
    assert(response[0] == 0xC7U);
    assert(response[1] == 0x02U);
    assert(uds_link_control_app_is_verified());

    /* Transition to 1M */
    res = uds_server_handle(&server, req_transition, sizeof(req_transition), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(uds_link_control_app_get_baudrate() == 1000000U);

    /* 6. Error cases */
    /* Unsupported fixed mode */
    const uint8_t req_bad_mode[] = {0x87U, 0x01U, 0xFFU};
    res = uds_server_handle(&server, req_bad_mode, sizeof(req_bad_mode), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(response[2] == UDS_NRC_REQUEST_OUT_OF_RANGE);

    /* Invalid subfunction */
    const uint8_t req_bad_subfunc[] = {0x87U, 0x99U};
    res = uds_server_handle(&server, req_bad_subfunc, sizeof(req_bad_subfunc), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(response[2] == UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
}

static void test_response_on_event(void) {
    uds_roe_app_init();

    UdsServiceBackends backends = {0};
    backends.periodic_event = uds_roe_app_get_backend();

    UdsServer server;
    UdsCallbacks callbacks = {0};
    callbacks.service_backends = &backends;
    uds_server_init(&server, &callbacks, NULL, 1000U);

    uint8_t response[64];
    uint16_t resp_len = 0U;

    assert(!uds_roe_app_is_configured());
    assert(!uds_roe_app_is_active());

    /* 1. Try to start when not configured -> conditionsNotCorrect (0x22) */
    const uint8_t req_start_unconfigured[] = {0x86U, 0x05U};
    UdsCallbackResult res =
        uds_server_handle(&server, req_start_unconfigured, sizeof(req_start_unconfigured), response,
                          &resp_len, sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 3U);
    assert(response[0] == 0x7FU);
    assert(response[1] == 0x86U);
    assert(response[2] == UDS_NRC_CONDITIONS_NOT_CORRECT);

    /* 2. Configure onDTCStatusChange: window=0x02, mask=0x08, serviceRecord=19 02 08 */
    const uint8_t req_cfg_dtc[] = {0x86U, 0x01U, 0x02U, 0x08U, 0x19U, 0x02U, 0x08U};
    res = uds_server_handle(&server, req_cfg_dtc, sizeof(req_cfg_dtc), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 8U);
    assert(response[0] == 0xC6U);
    assert(response[1] == 0x01U);
    assert(response[2] == 0x00U); /* eventStatus: configured, stopped */
    assert(response[3] == 0x02U);
    assert(response[4] == 0x08U);
    assert(uds_roe_app_is_configured());
    assert(!uds_roe_app_is_active());
    assert(uds_roe_app_get_event_type() == 0x01U);
    assert(uds_roe_app_get_dtc_status_mask() == 0x08U);

    /* 3. Report activated events when stopped -> returns count 0 */
    const uint8_t req_report[] = {0x86U, 0x04U};
    res = uds_server_handle(&server, req_report, sizeof(req_report), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 3U);
    assert(response[0] == 0xC6U);
    assert(response[1] == 0x04U);
    assert(response[2] == 0x00U);

    /* 4. Start response on event: 86 05 */
    const uint8_t req_start[] = {0x86U, 0x05U};
    res = uds_server_handle(&server, req_start, sizeof(req_start), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 4U);
    assert(response[0] == 0xC6U);
    assert(response[1] == 0x05U);
    assert(response[2] == 0x01U); /* eventStatus: started */
    assert(response[3] == 0x02U);
    assert(uds_roe_app_is_active());

    /* 5. Report activated events when active -> returns count 1 with records */
    res = uds_server_handle(&server, req_report, sizeof(req_report), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 9U);
    assert(response[0] == 0xC6U);
    assert(response[1] == 0x04U);
    assert(response[2] == 0x01U); /* 1 active event */
    assert(response[3] == 0x01U); /* eventType */
    assert(response[4] == 0x02U); /* window */
    assert(response[5] == 0x08U); /* mask */
    assert(response[6] == 0x19U);
    assert(response[7] == 0x02U);
    assert(response[8] == 0x08U);

    /* 6. Stop response on event: 86 00 */
    const uint8_t req_stop[] = {0x86U, 0x00U};
    res = uds_server_handle(&server, req_stop, sizeof(req_stop), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 4U);
    assert(response[0] == 0xC6U);
    assert(response[1] == 0x00U);
    assert(response[2] == 0x00U); /* stopped */
    assert(!uds_roe_app_is_active());
    assert(uds_roe_app_is_configured());

    /* 7. Configure onChangeOfDataIdentifier: DID 0x9B00 */
    const uint8_t req_cfg_did[] = {0x86U, 0x03U, 0x05U, 0x9BU, 0x00U, 0x22U, 0x9BU, 0x00U};
    res = uds_server_handle(&server, req_cfg_did, sizeof(req_cfg_did), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 9U);
    assert(response[0] == 0xC6U);
    assert(response[1] == 0x03U);
    assert(response[2] == 0x00U);
    assert(response[3] == 0x05U);
    assert(response[4] == 0x9BU);
    assert(response[5] == 0x00U);
    assert(uds_roe_app_get_monitored_did() == 0x9B00U);

    /* 8. Clear response on event: 86 06 */
    const uint8_t req_clear[] = {0x86U, 0x06U};
    res = uds_server_handle(&server, req_clear, sizeof(req_clear), response, &resp_len,
                            sizeof(response), 1000U);
    assert(res == UDS_RESULT_OK);
    assert(resp_len == 4U);
    assert(response[0] == 0xC6U);
    assert(response[1] == 0x06U);
    assert(response[2] == 0x00U);
    assert(!uds_roe_app_is_configured());
    assert(!uds_roe_app_is_active());
}

int main(void) {
    test_io_control_air_inlet_door();
    test_io_control_egr_iac();
    test_link_control();
    test_response_on_event();
    return 0;
}
