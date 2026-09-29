/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#include "uds_iso_tp/isotp.h"
#include "uds_iso_tp/uds.h"
#include "uds_iso_tp/uds_did.h"

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static UdsCallbackResult read_did(void *context, uint16_t did, uint8_t *data, uint16_t *length,
                                  uint16_t capacity) {
    (void)context;
    if (did != 0xF190U) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    static const uint8_t value[] = {'F', '7', '6', '7', 'R', 'E', 'F'};
    if (capacity < sizeof(value)) {
        return UDS_RESULT_RESPONSE_TOO_LONG;
    }
    memcpy(data, value, sizeof(value));
    *length = (uint16_t)sizeof(value);
    return UDS_RESULT_OK;
}

static uint8_t s_written_did_data[16];
static uint16_t s_written_did_length = 0U;

static UdsCallbackResult write_did(void *context, uint16_t did, const uint8_t *data,
                                   uint16_t length) {
    (void)context;
    if (did != 0xF190U) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    if (length > sizeof(s_written_did_data)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    memcpy(s_written_did_data, data, length);
    s_written_did_length = length;
    return UDS_RESULT_OK;
}

static UdsCallbackResult security_seed(void *context, uint8_t level, uint8_t *seed,
                                       uint16_t *length, uint16_t capacity) {
    (void)context;
    if ((level != 1U) || (capacity < 2U)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    seed[0] = 0x12U;
    seed[1] = 0x34U;
    *length = 2U;
    return UDS_RESULT_OK;
}

static UdsCallbackResult security_key(void *context, uint8_t level, const uint8_t *key,
                                      uint16_t length) {
    (void)context;
    if ((level != 1U) || (length != 2U)) {
        return UDS_RESULT_INVALID_KEY;
    }
    return ((key[0] == 0xCAU) && (key[1] == 0xFEU)) ? UDS_RESULT_OK : UDS_RESULT_INVALID_KEY;
}

static uint16_t ecu_reset_calls;

static UdsCallbackResult ecu_reset(void *context, uint8_t subfunction) {
    (void)context;
    ++ecu_reset_calls;
    return ((subfunction >= UDS_RESET_TYPE_HARD) &&
            (subfunction <= UDS_RESET_TYPE_DISABLE_RAPID_POWER_SHUTDOWN))
               ? UDS_RESULT_OK
               : UDS_RESULT_OUT_OF_RANGE;
}

static UdsCallbackResult communication_control(void *context, uint8_t subfunction,
                                               uint8_t communication_type) {
    (void)context;
    return ((subfunction == 0U) && (communication_type == 0x01U)) ? UDS_RESULT_OK
                                                                  : UDS_RESULT_OUT_OF_RANGE;
}

static UdsCallbackResult routine_control(void *context, uint8_t subfunction, uint16_t routine_id,
                                         const uint8_t *request, uint16_t request_len,
                                         uint8_t *response, uint16_t *response_len,
                                         uint16_t capacity) {
    (void)context;
    (void)request;
    if (routine_id == 0xFF00U) {
        return UDS_RESULT_RESPONSE_PENDING;
    }
    if ((subfunction != 1U) || (routine_id != 0x0203U) || (request_len != 1U) || (capacity < 1U)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    response[0] = 0xAAU;
    *response_len = 1U;
    return UDS_RESULT_OK;
}

static UdsCallbackResult request_download(void *context, uint32_t address, uint32_t length,
                                          uint16_t *max_block_length) {
    (void)context;
    if ((address != 0x08080000UL) || (length != 0x1000UL)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    *max_block_length = 0x0100U;
    return UDS_RESULT_OK;
}

static UdsCallbackResult transfer_data(void *context, uint8_t block, const uint8_t *data,
                                       uint16_t length) {
    (void)context;
    (void)data;
    return ((block != 0U) && (length > 0U)) ? UDS_RESULT_OK : UDS_RESULT_OUT_OF_RANGE;
}

static UdsCallbackResult transfer_exit(void *context, const uint8_t *request, uint16_t request_len,
                                       uint8_t *response, uint16_t *response_len,
                                       uint16_t capacity) {
    (void)context;
    (void)request;
    if ((request_len != 0U) || (capacity < 1U)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    response[0] = 0x55U;
    *response_len = 1U;
    return UDS_RESULT_OK;
}

static void test_isotp(void) {
    IsoTpConfig config;
    isotp_config_default(&config);
    config.block_size = 2U;
    IsoTpRx rx;
    IsoTpTx tx;
    isotp_rx_init(&rx, &config, 0x7E8U, 0x7E0U);
    isotp_tx_init(&tx, &config, 0x7E0U, 0x7E8U);

    IsoTpCanFrame frame;
    uint8_t payload[] = {0x22U, 0xF1U, 0x90U};
    assert(isotp_tx_start(&tx, payload, sizeof(payload), 0U, &frame) == ISOTP_TX_FRAME_READY);
    assert(frame.can_id == 0x7E8U && frame.dlc == 4U && frame.data[0] == 3U);
    IsoTpRxEvent event;
    assert(isotp_rx_feed(&rx, &frame, 0U, &event) == ISOTP_COMPLETE);
    assert(event.length == sizeof(payload));
    assert(memcmp(event.payload, payload, sizeof(payload)) == 0);

    uint8_t long_payload[20];
    for (size_t index = 0U; index < sizeof(long_payload); ++index) {
        long_payload[index] = (uint8_t)index;
    }
    assert(isotp_tx_start(&tx, long_payload, sizeof(long_payload), 0U, &frame) ==
           ISOTP_TX_FRAME_READY);
    assert((frame.data[0] >> 4U) == 1U);
    assert(isotp_rx_feed(&rx, &frame, 0U, &event) == ISOTP_NEED_FLOW_CONTROL);
    assert(event.has_flow_control && event.flow_control.data[0] == 0x30U);
    assert(isotp_tx_feed_flow_control(&tx, &event.flow_control, 0U) == ISOTP_OK);
    uint8_t expected_sequence = 1U;
    while (isotp_tx_state(&tx) != ISOTP_TX_STATE_IDLE) {
        IsoTpStatus status = isotp_tx_next(&tx, 0U, &frame);
        if (status == ISOTP_OK) {
            status = isotp_tx_next(&tx, 1U, &frame);
        }
        if (status == ISOTP_COMPLETE) {
            break;
        }
        assert(status == ISOTP_TX_FRAME_READY);
        assert((frame.data[0] & 0x0FU) == expected_sequence);
        expected_sequence = (uint8_t)((expected_sequence + 1U) & 0x0FU);
        assert(isotp_rx_feed(&rx, &frame, 1U, &event) == ((rx.active) ? ISOTP_OK : ISOTP_COMPLETE));
        if (event.has_flow_control) {
            assert(isotp_tx_feed_flow_control(&tx, &event.flow_control, 1U) == ISOTP_OK);
        }
    }
    assert(event.length == sizeof(long_payload));
    assert(memcmp(event.payload, long_payload, sizeof(long_payload)) == 0);

    isotp_rx_init(&rx, &config, 0x7E8U, 0x7E0U);
    frame.can_id = 0x7E8U;
    frame.dlc = 8U;
    frame.data[0] = 0x10U;
    frame.data[1] = 8U;
    memset(&frame.data[2], 0, 6U);
    assert(isotp_rx_feed(&rx, &frame, 0U, &event) == ISOTP_NEED_FLOW_CONTROL);
    frame.data[0] = 0x22U;
    frame.dlc = 2U;
    assert(isotp_rx_feed(&rx, &frame, 1U, &event) == ISOTP_ERR_SEQUENCE);
    assert(isotp_rx_tick(&rx, 2000U) == ISOTP_OK);
}

static void test_service_attributes(void) {
    uint8_t level = 0U;
    bool is_seed = false;
    const uint8_t request_subfunctions[] = {
        UDS_SECURITY_REQUEST_SEED_LEVEL_1, UDS_SECURITY_REQUEST_SEED_LEVEL_2,
        UDS_SECURITY_REQUEST_SEED_LEVEL_3, UDS_SECURITY_REQUEST_SEED_LEVEL_4,
        UDS_SECURITY_REQUEST_SEED_LEVEL_5};
    for (uint8_t index = 0U; index < 5U; ++index) {
        assert(uds_security_subfunction_level(request_subfunctions[index], &level, &is_seed));
        assert(level == (uint8_t)(index + 1U) && is_seed);
        assert(uds_security_subfunction_level((uint8_t)(request_subfunctions[index] + 1U), &level,
                                              &is_seed));
        assert(level == (uint8_t)(index + 1U) && !is_seed);
    }
    assert(!uds_security_subfunction_level(0x0BU, &level, &is_seed));

    const UdsServiceAttribute *read_attribute = uds_service_attribute(0x22U, 0U);
    assert(read_attribute->session_mask == UDS_SESSION_MASK_ALL);
    assert(read_attribute->address_mode == UDS_ADDRESS_MODE_BOTH);
    assert(read_attribute->security_mask == UDS_SECURITY_MASK_NONE);
    assert(uds_service_attribute_allows(read_attribute, UDS_SESSION_DEFAULT, 0U,
                                        UDS_ADDRESS_PHYSICAL));
    assert(uds_service_attribute_allows(read_attribute, UDS_SESSION_EXTENDED, 0U,
                                        UDS_ADDRESS_FUNCTIONAL));

    const UdsServiceAttribute *download_attribute = uds_service_attribute(0x34U, 0U);
    assert(download_attribute->session_mask == UDS_SESSION_MASK_PROGRAMMING);
    assert(download_attribute->address_mode == UDS_ADDRESS_PHYSICAL);
    assert(!uds_service_attribute_allows(download_attribute, UDS_SESSION_EXTENDED, 0U,
                                         UDS_ADDRESS_PHYSICAL));
    assert(uds_service_attribute_allows(download_attribute, UDS_SESSION_PROGRAMMING, 0U,
                                        UDS_ADDRESS_PHYSICAL));
    assert(!uds_service_attribute_allows(download_attribute, UDS_SESSION_PROGRAMMING, 0U,
                                         UDS_ADDRESS_FUNCTIONAL));

    const UdsServiceAttribute *reset_attr = uds_service_attribute(0x11U, 0U);
    assert(reset_attr->address_mode == UDS_ADDRESS_MODE_BOTH);
    assert(
        uds_service_attribute_allows(reset_attr, UDS_SESSION_DEFAULT, 0U, UDS_ADDRESS_FUNCTIONAL));

    const UdsServiceAttribute *clear_attr = uds_service_attribute(0x14U, 0U);
    assert(clear_attr->address_mode == UDS_ADDRESS_MODE_BOTH);
    assert(
        uds_service_attribute_allows(clear_attr, UDS_SESSION_DEFAULT, 0U, UDS_ADDRESS_FUNCTIONAL));

    const UdsServiceAttribute *control_dtc_attr = uds_service_attribute(0x85U, 0U);
    assert(control_dtc_attr->address_mode == UDS_ADDRESS_MODE_BOTH);
    assert(uds_service_attribute_allows(control_dtc_attr, UDS_SESSION_DEFAULT, 0U,
                                        UDS_ADDRESS_FUNCTIONAL));

    const UdsServiceAttribute protected_attribute = {
        0x99U, UDS_SERVICE_ANY_SUBFUNCTION, UDS_SESSION_MASK_EXTENDED, UDS_SECURITY_MASK_LEVEL_1,
        UDS_ADDRESS_PHYSICAL};
    assert(!uds_service_attribute_allows(&protected_attribute, UDS_SESSION_EXTENDED, 0U,
                                         UDS_ADDRESS_PHYSICAL));
    assert(uds_service_attribute_allows(&protected_attribute, UDS_SESSION_EXTENDED,
                                        UDS_SECURITY_LEVEL_1, UDS_ADDRESS_PHYSICAL));
}

static uint8_t s_mock_reset_subfunction = 0U;
static UdsCallbackResult mock_ecu_reset(void *context, uint8_t subfunction) {
    (void)context;
    s_mock_reset_subfunction = subfunction;
    return UDS_RESULT_OK;
}

static uint32_t s_mock_clear_group = 0U;
static UdsCallbackResult mock_clear_dtc(void *context, uint32_t group) {
    (void)context;
    s_mock_clear_group = group;
    return UDS_RESULT_OK;
}

static uint8_t s_mock_dtc_setting_subfunction = 0U;
static UdsCallbackResult mock_control_dtc_setting(void *context, uint8_t subfunction) {
    (void)context;
    s_mock_dtc_setting_subfunction = subfunction;
    return UDS_RESULT_OK;
}

static void test_addressed_dispatch(void) {
    UdsCallbacks callbacks = {.read_did = read_did,
                              .security_seed = security_seed,
                              .security_key = security_key,
                              .ecu_reset = mock_ecu_reset,
                              .clear_dtc = mock_clear_dtc,
                              .control_dtc_setting = mock_control_dtc_setting};
    UdsServer server;
    uds_server_init(&server, &callbacks, NULL, 0U);
    uint8_t response[64];
    uint16_t response_len = 0U;
    uint8_t read_request[] = {0x22U, 0xF1U, 0x90U};
    assert(uds_server_handle_addressed(&server, read_request, sizeof(read_request), response,
                                       &response_len, sizeof(response), UDS_ADDRESS_FUNCTIONAL,
                                       0U) == UDS_RESULT_OK);
    assert(response[0] == 0x62U && response[1] == 0xF1U && response[2] == 0x90U);

    uint8_t security_request[] = {0x27U, UDS_SECURITY_REQUEST_SEED_LEVEL_1};
    assert(uds_server_handle_addressed(&server, security_request, sizeof(security_request),
                                       response, &response_len, sizeof(response),
                                       UDS_ADDRESS_FUNCTIONAL, 1U) == UDS_RESULT_NO_RESPONSE);
    assert(response_len == 0U);

    /* 0x11 ECUReset on Functional addressing */
    uint8_t reset_request[] = {0x11U, 0x01U};
    assert(uds_server_handle_addressed(&server, reset_request, sizeof(reset_request), response,
                                       &response_len, sizeof(response), UDS_ADDRESS_FUNCTIONAL,
                                       2U) == UDS_RESULT_OK);
    assert(response_len == 2U && response[0] == 0x51U && response[1] == 0x01U);
    assert(s_mock_reset_subfunction == 0x01U);

    /* 0x11 with Suppress Positive Response Message Indication Bit on Functional addressing */
    uint8_t reset_suppress_request[] = {0x11U, 0x81U};
    assert(uds_server_handle_addressed(&server, reset_suppress_request,
                                       sizeof(reset_suppress_request), response, &response_len,
                                       sizeof(response), UDS_ADDRESS_FUNCTIONAL,
                                       3U) == UDS_RESULT_NO_RESPONSE);
    assert(response_len == 0U);

    /* 0x14 ClearDiagnosticInformation on Functional addressing */
    uint8_t clear_request[] = {0x14U, 0xFFU, 0xFFU, 0xFFU};
    assert(uds_server_handle_addressed(&server, clear_request, sizeof(clear_request), response,
                                       &response_len, sizeof(response), UDS_ADDRESS_FUNCTIONAL,
                                       4U) == UDS_RESULT_OK);
    assert(response_len == 1U && response[0] == 0x54U);
    assert(s_mock_clear_group == 0xFFFFFFU);

    /* 0x85 ControlDTCSetting on Functional addressing */
    uint8_t dtc_setting_request[] = {0x85U, 0x02U};
    assert(uds_server_handle_addressed(&server, dtc_setting_request, sizeof(dtc_setting_request),
                                       response, &response_len, sizeof(response),
                                       UDS_ADDRESS_FUNCTIONAL, 5U) == UDS_RESULT_OK);
    assert(response_len == 2U && response[0] == 0xC5U && response[1] == 0x02U);
    assert(!server.dtc_setting_enabled);

    /* 0x85 with Suppress Positive Response on Functional addressing */
    uint8_t dtc_setting_suppress[] = {0x85U, 0x81U};
    assert(uds_server_handle_addressed(&server, dtc_setting_suppress, sizeof(dtc_setting_suppress),
                                       response, &response_len, sizeof(response),
                                       UDS_ADDRESS_FUNCTIONAL, 6U) == UDS_RESULT_NO_RESPONSE);
    assert(response_len == 0U);
    assert(server.dtc_setting_enabled);

    /* 0x27 SecurityAccess on Physical addressing rejected in default session with NRC */
    assert(uds_server_handle_addressed(&server, security_request, sizeof(security_request),
                                       response, &response_len, sizeof(response),
                                       UDS_ADDRESS_PHYSICAL, 7U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x27U &&
           response[2] == UDS_NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);

    /* Physical 0x11 with unsupported subfunction returns NRC 0x12 */
    uint8_t invalid_reset_request[] = {0x11U, 0x7FU};
    assert(uds_server_handle_addressed(
               &server, invalid_reset_request, sizeof(invalid_reset_request), response,
               &response_len, sizeof(response), UDS_ADDRESS_PHYSICAL, 8U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x11U &&
           response[2] == UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
}

static void test_uds(void) {

    UdsCallbacks callbacks = {
        .read_did = read_did,
        .write_did = write_did,
        .security_seed = security_seed,
        .security_key = security_key,
        .ecu_reset = ecu_reset,
        .communication_control = communication_control,
        .routine_control = routine_control,
        .request_download = request_download,
        .transfer_data = transfer_data,
        .request_transfer_exit = transfer_exit,
    };
    UdsServer server;
    uds_server_init(&server, &callbacks, NULL, 0U);
    uint8_t response[64];
    uint16_t response_len;

    uint8_t request[] = {0x10U, 0x03U};
    assert(uds_server_handle(&server, request, sizeof(request), response, &response_len,
                             sizeof(response), 0U) == UDS_RESULT_OK);
    assert(response_len == 6U && response[0] == 0x50U && response[1] == 0x03U);

    uint8_t read_request[] = {0x22U, 0xF1U, 0x90U};
    assert(uds_server_handle(&server, read_request, sizeof(read_request), response, &response_len,
                             sizeof(response), 1U) == UDS_RESULT_OK);
    assert(response_len == 10U && response[0] == 0x62U && response[1] == 0xF1U &&
           response[2] == 0x90U);

    uint8_t bad_did[] = {0x22U, 0x12U, 0x34U};
    assert(uds_server_handle(&server, bad_did, sizeof(bad_did), response, &response_len,
                             sizeof(response), 2U) == UDS_RESULT_OK);
    assert(response_len == 3U && response[0] == 0x7FU && response[2] == 0x31U);

    uds_server_apply_reset(&server, UDS_RESET_PROGRAMMING, 10000U);
    uint8_t session_request[] = {0x10U, UDS_SESSION_EXTENDED};
    assert(uds_server_handle(&server, session_request, sizeof(session_request), response,
                             &response_len, sizeof(response), 10000U) == UDS_RESULT_OK);
    uint8_t seed_request[] = {0x27U, 0x01U};
    assert(uds_server_handle(&server, seed_request, sizeof(seed_request), response, &response_len,
                             sizeof(response), 10001U) == UDS_RESULT_OK);
    assert(response_len == 4U && response[0] == 0x67U && response[2] == 0x12U);
    uint8_t key_request[] = {0x27U, 0x02U, 0xCAU, 0xFEU};
    assert(uds_server_handle(&server, key_request, sizeof(key_request), response, &response_len,
                             sizeof(response), 10002U) == UDS_RESULT_OK);
    assert(uds_server_security_level(&server) == 1U);

    uint8_t write_request[] = {0x2EU, 0xF1U, 0x90U, 'W', 'R', 'I', 'T', 'E'};
    assert(uds_server_handle(&server, write_request, sizeof(write_request), response, &response_len,
                             sizeof(response), 10003U) == UDS_RESULT_OK);
    assert(response_len == 3U && response[0] == 0x6EU && response[1] == 0xF1U &&
           response[2] == 0x90U);
    assert(s_written_did_length == 5U && memcmp(s_written_did_data, "WRITE", 5) == 0);

    /* Negative test: Write to read-only / unmapped DID returns NRC 0x31 (RequestOutOfRange) */
    uint8_t write_ro_did[] = {0x2EU, 0xF1U, 0x86U, 'N', 'O', 'P', 'E'};
    assert(uds_server_handle(&server, write_ro_did, sizeof(write_ro_did), response, &response_len,
                             sizeof(response), 10003U) == UDS_RESULT_OK);
    assert(response_len == 3U && response[0] == 0x7FU && response[1] == 0x2EU &&
           response[2] == 0x31U);

    /* Negative test: Truncated 0x2E request (< 4 bytes) returns NRC 0x13 */
    uint8_t write_trunc[] = {0x2EU, 0xF1U};
    assert(uds_server_handle(&server, write_trunc, sizeof(write_trunc), response, &response_len,
                             sizeof(response), 10003U) == UDS_RESULT_OK);
    assert(response_len == 3U && response[0] == 0x7FU && response[1] == 0x2EU &&
           response[2] == 0x13U);

    uint8_t seed_unlocked[] = {0x27U, 0x01U};
    assert(uds_server_handle(&server, seed_unlocked, sizeof(seed_unlocked), response, &response_len,
                             sizeof(response), 10004U) == UDS_RESULT_OK);
    assert(response_len == 4U && response[0] == 0x67U && response[1] == 0x01U &&
           response[2] == 0x00U && response[3] == 0x00U);
    assert(uds_server_security_level(&server) == 1U);

    uint8_t reset_request[] = {0x11U, 0x01U};
    assert(uds_server_handle(&server, reset_request, sizeof(reset_request), response, &response_len,
                             sizeof(response), 5U) == UDS_RESULT_OK);
    assert(uds_server_reset_pending(&server));
    assert(uds_server_complete_reset(&server) == UDS_RESULT_NOT_SUPPORTED);
    uds_server_clear_reset(&server);
    assert(!uds_server_reset_pending(&server));

    for (uint8_t reset_type = UDS_RESET_TYPE_HARD;
         reset_type <= UDS_RESET_TYPE_DISABLE_RAPID_POWER_SHUTDOWN; ++reset_type) {
        reset_request[1] = reset_type;
        assert(uds_server_handle(&server, reset_request, sizeof(reset_request), response,
                                 &response_len, sizeof(response),
                                 20U + reset_type) == UDS_RESULT_OK);
        uint16_t expected_len =
            (reset_type == UDS_RESET_TYPE_ENABLE_RAPID_POWER_SHUTDOWN) ? 3U : 2U;
        assert(response_len == expected_len && response[0] == 0x51U && response[1] == reset_type);
        if (reset_type == UDS_RESET_TYPE_ENABLE_RAPID_POWER_SHUTDOWN) {
            assert(response[2] == 0x00U);
        }
        assert(uds_server_reset_pending(&server));
        uds_server_clear_reset(&server);
    }
    uint16_t calls_before_suppressed = ecu_reset_calls;
    for (uint8_t reset_type = UDS_RESET_TYPE_HARD;
         reset_type <= UDS_RESET_TYPE_DISABLE_RAPID_POWER_SHUTDOWN; ++reset_type) {
        reset_request[1] = (uint8_t)(reset_type | UDS_SUPPRESS_POSITIVE_RESPONSE);
        response_len = 0U;
        assert(uds_server_handle(&server, reset_request, sizeof(reset_request), response,
                                 &response_len, sizeof(response),
                                 40U + reset_type) == UDS_RESULT_NO_RESPONSE);
        assert(uds_server_reset_pending(&server));
        assert(ecu_reset_calls == (uint16_t)(calls_before_suppressed + reset_type));
        uds_server_clear_reset(&server);
    }
    reset_request[1] = 0x06U;
    assert(uds_server_handle(&server, reset_request, sizeof(reset_request), response, &response_len,
                             sizeof(response), 26U) == UDS_RESULT_OK);
    assert(response_len == 3U && response[0] == 0x7FU && response[1] == 0x11U &&
           response[2] == UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);

    uint8_t communication_request[] = {0x28U, 0x00U, 0x01U};
    assert(uds_server_handle(&server, communication_request, sizeof(communication_request),
                             response, &response_len, sizeof(response), 6U) == UDS_RESULT_OK);
    assert(response[0] == 0x68U);

    uint8_t routine_request[] = {0x31U, 0x01U, 0x02U, 0x03U, 0xAAU};
    assert(uds_server_handle(&server, routine_request, sizeof(routine_request), response,
                             &response_len, sizeof(response), 7U) == UDS_RESULT_OK);
    assert(response_len == 5U && response[0] == 0x71U && response[4] == 0xAAU);

    uint8_t routine_suppressed[] = {0x31U, (uint8_t)(0x01U | UDS_SUPPRESS_POSITIVE_RESPONSE), 0x02U,
                                    0x03U, 0xAAU};
    assert(uds_server_handle(&server, routine_suppressed, sizeof(routine_suppressed), response,
                             &response_len, sizeof(response), 10005U) == UDS_RESULT_NO_RESPONSE);

    uint8_t routine_pending[] = {0x31U, 0x01U, 0xFFU, 0x00U};
    assert(uds_server_handle(&server, routine_pending, sizeof(routine_pending), response,
                             &response_len, sizeof(response), 10007U) == UDS_RESULT_OK);
    assert(response_len == 3U && response[0] == 0x7FU && response[1] == 0x31U &&
           response[2] == UDS_NRC_REQUEST_CORRECTLY_RECEIVED_RESPONSE_PENDING);

    uint8_t dtc_sprmib[] = {0x19U, 0x82U, 0xFFU};
    assert(uds_server_handle(&server, dtc_sprmib, sizeof(dtc_sprmib), response, &response_len,
                             sizeof(response), 10006U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x19U &&
           response[2] == UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);

    uint8_t programming_request[] = {0x10U, UDS_SESSION_PROGRAMMING};
    assert(uds_server_handle(&server, programming_request, sizeof(programming_request), response,
                             &response_len, sizeof(response), 8U) == UDS_RESULT_OK);
    assert(response[0] == 0x50U && response[1] == UDS_SESSION_PROGRAMMING);

    uint8_t download_request[] = {0x34U, 0x00U, 0x44U, 0x08U, 0x08U, 0x00U,
                                  0x00U, 0x00U, 0x00U, 0x10U, 0x00U};
    assert(uds_server_handle(&server, download_request, sizeof(download_request), response,
                             &response_len, sizeof(response), 9U) == UDS_RESULT_OK);
    assert(response[0] == 0x74U && server.download_active);
    uint8_t transfer_request[] = {0x36U, 0x01U, 0xAAU, 0xBBU};
    assert(uds_server_handle(&server, transfer_request, sizeof(transfer_request), response,
                             &response_len, sizeof(response), 10U) == UDS_RESULT_OK);
    assert(response[0] == 0x76U);
    uint8_t exit_request[] = {0x37U};
    assert(uds_server_handle(&server, exit_request, sizeof(exit_request), response, &response_len,
                             sizeof(response), 11U) == UDS_RESULT_OK);
    assert(response_len == 2U && response[0] == 0x77U && response[1] == 0x55U);

    uint8_t unsupported[] = {0x99U};
    assert(uds_server_handle(&server, unsupported, sizeof(unsupported), response, &response_len,
                             sizeof(response), 12U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x99U && response[2] == 0x11U);
}

static void test_reentrant_multi_instance_servers(void) {
    UdsServer server_phy;
    UdsServer server_func;
    uint8_t response_phy[32];
    uint8_t response_func[32];
    uint16_t resp_phy_len = 0U;
    uint16_t resp_func_len = 0U;

    uds_server_init(&server_phy, NULL, NULL, 1000U);
    uds_server_init(&server_func, NULL, NULL, 1000U);

    /* Unsupported service 0x99 */
    uint8_t unsupported[] = {0x99U};

    /* Physical dispatch should respond with NRC 0x11 (ServiceNotSupported) */
    assert(uds_server_handle_addressed(&server_phy, unsupported, sizeof(unsupported), response_phy,
                                       &resp_phy_len, sizeof(response_phy), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_OK);
    assert(resp_phy_len == 3U);
    assert(response_phy[0] == 0x7FU && response_phy[1] == 0x99U && response_phy[2] == 0x11U);

    /* Functional dispatch should suppress NRC 0x11 and return NO_RESPONSE */
    assert(uds_server_handle_addressed(&server_func, unsupported, sizeof(unsupported),
                                       response_func, &resp_func_len, sizeof(response_func),
                                       UDS_ADDRESS_FUNCTIONAL, 1000U) == UDS_RESULT_NO_RESPONSE);
    assert(resp_func_len == 0U);

    /* Interleaved calls: physical again, must not be affected by prior functional call */
    assert(uds_server_handle_addressed(&server_phy, unsupported, sizeof(unsupported), response_phy,
                                       &resp_phy_len, sizeof(response_phy), UDS_ADDRESS_PHYSICAL,
                                       1001U) == UDS_RESULT_OK);
    assert(resp_phy_len == 3U);
    assert(response_phy[0] == 0x7FU && response_phy[1] == 0x99U && response_phy[2] == 0x11U);
}

static UdsDidResult custom_did_write_stub(void *context, uint16_t did, const uint8_t *data,
                                          uint16_t length) {
    (void)context;
    (void)did;
    (void)data;
    (void)length;
    return UDS_DID_OK;
}

static void test_did_registry(void) {
    uint8_t sw_ver[] = "1.6.0";
    uint8_t sn[] = "SN123456";
    UdsProjectDidSource src;
    memset(&src, 0, sizeof(src));
    src.software_version.data = sw_ver;
    src.software_version.length = (uint16_t)strlen((char *)sw_ver);
    src.serial_number.data = sn;
    src.serial_number.length = (uint16_t)strlen((char *)sn);

    UdsDidRegistry reg;
    uds_did_registry_init(&reg, &src);

    uint8_t out[64];
    uint16_t len = 0;
    /* Read software version (security 0) */
    assert(uds_did_registry_read(&reg, UDS_DID_SOFTWARE_VERSION, 1U, 0U, out, &len, sizeof(out)) ==
           UDS_DID_OK);
    assert(len == src.software_version.length);
    assert(memcmp(out, sw_ver, len) == 0);

    /* Read serial number with security 0 -> SECURITY_DENIED */
    assert(uds_did_registry_read(&reg, UDS_DID_SERIAL_NUMBER, 1U, 0U, out, &len, sizeof(out)) ==
           UDS_DID_SECURITY_DENIED);

    /* Read serial number with security 1 -> OK */
    assert(uds_did_registry_read(&reg, UDS_DID_SERIAL_NUMBER, 1U, 1U, out, &len, sizeof(out)) ==
           UDS_DID_OK);
    assert(len == src.serial_number.length);

    /* Buffer capacity too small */
    assert(uds_did_registry_read(&reg, UDS_DID_SOFTWARE_VERSION, 1U, 0U, out, &len, 2U) ==
           UDS_DID_RESPONSE_TOO_LONG);

    /* Write to read-only DID -> NOT_WRITABLE */
    assert(uds_did_registry_write(&reg, UDS_DID_SOFTWARE_VERSION, 1U, 0U, out, len) ==
           UDS_DID_NOT_WRITABLE);

    /* Non-existent DID -> NOT_FOUND */
    assert(uds_did_registry_read(&reg, 0x1234U, 1U, 0U, out, &len, sizeof(out)) ==
           UDS_DID_NOT_FOUND);
    assert(uds_did_registry_write(&reg, 0x1234U, 1U, 0U, out, len) == UDS_DID_NOT_FOUND);

    /* Test session denial and unreadable DID */
    assert(uds_did_registry_read(&reg, UDS_DID_SOFTWARE_VERSION, 0x99U, 0U, out, &len,
                                 sizeof(out)) == UDS_DID_SESSION_DENIED);

    /* Test read_allowed == false and NULL read pointer */
    reg.entries[0].read_allowed = false;
    assert(uds_did_registry_read(&reg, UDS_DID_SOFTWARE_VERSION, 1U, 0U, out, &len, sizeof(out)) ==
           UDS_DID_NOT_READABLE);
    reg.entries[0].read_allowed = true;
    reg.entries[0].read = NULL;
    assert(uds_did_registry_read(&reg, UDS_DID_SOFTWARE_VERSION, 1U, 0U, out, &len, sizeof(out)) ==
           UDS_DID_NOT_READABLE);

    /* Configure entry[0] as writable */
    reg.entries[0].write_allowed = true;
    reg.entries[0].write = custom_did_write_stub;
    reg.entries[0].maximum_length = 4U;
    reg.entries[0].minimum_security_level = 1U;
    reg.entries[0].session_mask = UDS_DID_SESSION_ALL_MASK;

    /* Write with wrong session -> SESSION_DENIED */
    assert(uds_did_registry_write(&reg, UDS_DID_SOFTWARE_VERSION, 0x99U, 1U, out, 2U) ==
           UDS_DID_SESSION_DENIED);
    /* Write with session 0x02 (Programming) and 0x03 (Extended) -> allowed sessions */
    assert(uds_did_registry_write(&reg, UDS_DID_SOFTWARE_VERSION, 0x02U, 1U, out, 2U) ==
           UDS_DID_OK);
    assert(uds_did_registry_write(&reg, UDS_DID_SOFTWARE_VERSION, 0x03U, 1U, out, 2U) ==
           UDS_DID_OK);
    /* Write with insufficient security -> SECURITY_DENIED */
    assert(uds_did_registry_write(&reg, UDS_DID_SOFTWARE_VERSION, 0x01U, 0U, out, 2U) ==
           UDS_DID_SECURITY_DENIED);
    /* Write exceeding max length -> INVALID_WRITE */
    assert(uds_did_registry_write(&reg, UDS_DID_SOFTWARE_VERSION, 0x01U, 1U, out, 10U) ==
           UDS_DID_INVALID_WRITE);
    /* Valid write */
    assert(uds_did_registry_write(&reg, UDS_DID_SOFTWARE_VERSION, 0x01U, 1U, out, 4U) ==
           UDS_DID_OK);

    /* NULL argument checks */
    assert(uds_did_registry_read(NULL, UDS_DID_SOFTWARE_VERSION, 1U, 0U, out, &len, sizeof(out)) ==
           UDS_DID_NOT_FOUND);
    assert(uds_did_registry_write(NULL, UDS_DID_SOFTWARE_VERSION, 1U, 0U, out, len) ==
           UDS_DID_NOT_FOUND);
    assert(uds_did_registry_find(NULL, UDS_DID_SOFTWARE_VERSION) == NULL);
    uds_did_registry_init(NULL, &src);
    uds_did_registry_init(&reg, NULL);
}

static void test_uds_branches(void) {
    /* 1. uds_server_init NULL guard */
    uds_server_init(NULL, NULL, NULL, 0U);

    /* 2. uds_security_subfunction_level edge cases */
    uint8_t lvl = 0U;
    bool is_seed = false;
    assert(!uds_security_subfunction_level(0x01U, NULL, &is_seed));
    assert(!uds_security_subfunction_level(0x01U, &lvl, NULL));
    assert(!uds_security_subfunction_level(0x00U, &lvl, &is_seed));
    assert(!uds_security_subfunction_level(0x0BU, &lvl, &is_seed));
    assert(!uds_security_subfunction_level(0x81U, &lvl, &is_seed)); /* Seed with SPRMIB */
    assert(uds_security_subfunction_level(0x82U, &lvl, &is_seed) && !is_seed && (lvl == 1U));

    /* 3. uds_service_attribute_allows edge cases */
    assert(!uds_service_attribute_allows(NULL, UDS_SESSION_DEFAULT, 0U, UDS_ADDRESS_PHYSICAL));
    const UdsServiceAttribute custom_attr = {0xAAU, 0x01U, UDS_SESSION_MASK_EXTENDED, 0x0002U,
                                             (uint8_t)UDS_ADDRESS_PHYSICAL};
    /* Session mismatch */
    assert(
        !uds_service_attribute_allows(&custom_attr, UDS_SESSION_DEFAULT, 1U, UDS_ADDRESS_PHYSICAL));
    /* Address mode mismatch */
    assert(!uds_service_attribute_allows(&custom_attr, UDS_SESSION_EXTENDED, 1U,
                                         UDS_ADDRESS_FUNCTIONAL));
    /* Security mismatch */
    assert(!uds_service_attribute_allows(&custom_attr, UDS_SESSION_EXTENDED, 0U,
                                         UDS_ADDRESS_PHYSICAL));
    /* Matching */
    assert(
        uds_service_attribute_allows(&custom_attr, UDS_SESSION_EXTENDED, 1U, UDS_ADDRESS_PHYSICAL));

    /* 4. Functional suppression of NRCs */
    UdsServer server;
    uds_server_init(&server, NULL, NULL, 1000U);
    uint8_t resp[32];
    uint16_t rlen = 0U;

    /* Unsupported service on functional -> no response */
    uint8_t unsupp_req[] = {0xAAU};
    assert(uds_server_handle_addressed(&server, unsupp_req, sizeof(unsupp_req), resp, &rlen,
                                       sizeof(resp), UDS_ADDRESS_FUNCTIONAL,
                                       1000U) == UDS_RESULT_NO_RESPONSE);

    /* Capacity < 3 on error -> RESPONSE_TOO_LONG */
    assert(uds_server_handle_addressed(&server, unsupp_req, sizeof(unsupp_req), resp, &rlen, 2U,
                                       UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_RESPONSE_TOO_LONG);

    /* Invalid arguments to uds_server_handle_addressed */
    assert(uds_server_handle_addressed(&server, NULL, 0U, resp, &rlen, sizeof(resp),
                                       UDS_ADDRESS_PHYSICAL, 1000U) == UDS_RESULT_ERROR);
    assert(uds_server_handle_addressed(&server, unsupp_req, sizeof(unsupp_req), NULL, &rlen,
                                       sizeof(resp), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_ERROR);
    assert(uds_server_handle_addressed(&server, unsupp_req, sizeof(unsupp_req), resp, NULL,
                                       sizeof(resp), UDS_ADDRESS_PHYSICAL,
                                       1000U) == UDS_RESULT_ERROR);

    /* NULL getters */
    assert(uds_server_session(NULL) == UDS_SESSION_DEFAULT);
    assert(uds_server_security_level(NULL) == 0U);
    assert(!uds_server_reset_pending(NULL));
    uds_server_clear_reset(NULL);
    assert(uds_server_complete_reset(NULL) == UDS_RESULT_ERROR);
    assert(uds_server_request_session(NULL, UDS_SESSION_DEFAULT, 0U) == UDS_RESULT_OUT_OF_RANGE);
    assert(uds_server_tick(NULL, 0U) == UDS_RESULT_ERROR);

    /* Safety session transition denied from default session */
    assert(uds_server_request_session(&server, UDS_SESSION_SAFETY, 1000U) == UDS_RESULT_DENIED);
}

static void test_uds_result_to_nrc_mapping(void) {
    assert(uds_result_to_nrc(UDS_RESULT_OK) == UDS_NRC_CONDITIONS_NOT_CORRECT);
    assert(uds_result_to_nrc(UDS_RESULT_NO_RESPONSE) == UDS_NRC_CONDITIONS_NOT_CORRECT);
    assert(uds_result_to_nrc(UDS_RESULT_NOT_SUPPORTED) == UDS_NRC_SERVICE_NOT_SUPPORTED);
    assert(uds_result_to_nrc(UDS_RESULT_SUBFUNCTION_NOT_SUPPORTED) ==
           UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    assert(uds_result_to_nrc(UDS_RESULT_DENIED) == UDS_NRC_CONDITIONS_NOT_CORRECT);
    assert(uds_result_to_nrc(UDS_RESULT_OUT_OF_RANGE) == UDS_NRC_REQUEST_OUT_OF_RANGE);
    assert(uds_result_to_nrc(UDS_RESULT_BUSY) == UDS_NRC_BUSY_REPEAT_REQUEST);
    assert(uds_result_to_nrc(UDS_RESULT_SEQUENCE_ERROR) == UDS_NRC_REQUEST_SEQUENCE_ERROR);
    assert(uds_result_to_nrc(UDS_RESULT_INVALID_KEY) == UDS_NRC_INVALID_KEY);
    assert(uds_result_to_nrc(UDS_RESULT_ATTEMPTS_EXCEEDED) == UDS_NRC_EXCEEDED_NUMBER_OF_ATTEMPTS);
    assert(uds_result_to_nrc(UDS_RESULT_DELAY_ACTIVE) == UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED);
    assert(uds_result_to_nrc(UDS_RESULT_PROGRAMMING_FAILURE) ==
           UDS_NRC_GENERAL_PROGRAMMING_FAILURE);
    assert(uds_result_to_nrc(UDS_RESULT_RESPONSE_TOO_LONG) == UDS_NRC_RESPONSE_TOO_LONG);
    assert(uds_result_to_nrc(UDS_RESULT_RESPONSE_PENDING) ==
           UDS_NRC_REQUEST_CORRECTLY_RECEIVED_RESPONSE_PENDING);
    assert(uds_result_to_nrc(UDS_RESULT_INVALID_FORMAT) ==
           UDS_NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
    assert(uds_result_to_nrc(UDS_RESULT_SECURITY_DENIED) == UDS_NRC_SECURITY_ACCESS_DENIED);
    assert(uds_result_to_nrc(UDS_RESULT_ERROR) == UDS_NRC_CONDITIONS_NOT_CORRECT);
    /* Out-of-bounds value fallback */
    assert(uds_result_to_nrc((UdsCallbackResult)99U) == UDS_NRC_CONDITIONS_NOT_CORRECT);
}

int main(void) {
    test_service_attributes();
    test_addressed_dispatch();
    test_isotp();
    test_uds();
    test_reentrant_multi_instance_servers();
    test_did_registry();
    test_uds_branches();
    test_uds_result_to_nrc_mapping();
    return 0;
}
