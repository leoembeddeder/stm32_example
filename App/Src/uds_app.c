#include "uds_app.h"

#include "uds_app_config.h"
#include "uds_auth_app.h"
#include "uds_bootloader.h"
#include "uds_did_app.h"
#include "uds_dtc_app.h"
#include "uds_iso_tp/endpoint.h"
#include "uds_iso_tp/uds.h"
#include "uds_iso_tp/uds_services.h"
#include "uds_platform.h"
#include "uds_security_cmac.h"

#include <stddef.h>
#include <string.h>

#define UDS_APP_REQUEST_ID 0x7E0U
#define UDS_APP_FUNCTIONAL_ID 0x7DFU
#define UDS_APP_RESPONSE_ID 0x7E8U

#define UDS_APP_RX_FIFO_CAPACITY 8U

static UdsCanTransport *s_transport;
static UdsIsoTpEndpoint s_endpoint;
static IsoTpCanFrame s_rx_fifo[UDS_APP_RX_FIFO_CAPACITY];
static volatile uint8_t s_rx_head;
static volatile uint8_t s_rx_tail;
static bool s_initialized;
static UdsServiceBackends s_service_backends;

static const uint8_t s_sec_master_key[16] = {0x2BU, 0x7EU, 0x15U, 0x16U, 0x28U, 0xAEU,
                                             0xD2U, 0xA6U, 0xABU, 0xF7U, 0x15U, 0x88U,
                                             0x09U, 0xCFU, 0x4FU, 0x3CU};

static uint8_t s_sec_active_seed[16] = {0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U, 0x08U,
                                        0x09U, 0x0AU, 0x0BU, 0x0CU, 0x0DU, 0x0EU, 0x0FU, 0x10U};

static UdsCallbackResult uds_app_security_seed(void *context, uint8_t level, uint8_t *seed,
                                               uint16_t *length, uint16_t capacity) {
    (void)context;
    (void)level;
    if ((seed == NULL) || (length == NULL) || (capacity < 16U)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    for (uint8_t i = 0U; i < 16U; ++i) {
        s_sec_active_seed[i] = (uint8_t)(s_sec_active_seed[i] + (uint8_t)(i * 3U + 0x21U));
    }
    (void)memcpy(seed, s_sec_active_seed, 16U);
    *length = 16U;
    return UDS_RESULT_OK;
}

static UdsCallbackResult uds_app_security_key(void *context, uint8_t level, const uint8_t *key,
                                              uint16_t length) {
    (void)context;
    (void)level;
    if ((key == NULL) || (length != 16U)) {
        return UDS_RESULT_INVALID_KEY;
    }
    uint8_t expected_key[16];
    if (!uds_security_cmac_derive_key(s_sec_master_key, s_sec_active_seed, expected_key)) {
        return UDS_RESULT_ERROR;
    }
    if (!uds_security_cmac_constant_time_equal(key, expected_key)) {
        return UDS_RESULT_INVALID_KEY;
    }
    return UDS_RESULT_OK;
}

static UdsCallbackResult uds_app_ecu_reset_prepare(void *context, uint8_t subfunction) {
    (void)context;
    return (subfunction == 0x01U) ? UDS_RESULT_OK : UDS_RESULT_OUT_OF_RANGE;
}

static void uds_app_ecu_reset_execute(void *context, uint8_t subfunction) {
    (void)context;
    if (uds_bootloader_is_activation_pending()) {
        uds_bootloader_jump_to_app(UDS_BL_APP_SLOT_B_START);
    }
    uds_platform_system_reset(subfunction);
}

void uds_app_init(UdsCanTransport *transport, uint32_t now_ms) {
    if (transport == NULL) {
        return;
    }

    uds_did_app_init();
    uds_dtc_app_init();
    uds_bootloader_init();
    uds_auth_app_init();

    (void)memset(&s_service_backends, 0, sizeof(s_service_backends));
    s_service_backends.authentication = uds_auth_app_get_backend();

    UdsIsoTpEndpointConfig config = {0};
    isotp_config_classic_can(&config.isotp_config);
#if UDS_APP_CLASSIC_PADDING_ENABLED
    isotp_config_set_padding(&config.isotp_config, true, UDS_APP_CLASSIC_PADDING_VALUE);
#endif
    config.send_frame = uds_can_transport_send;
    config.tx_complete = uds_can_transport_tx_complete;
    config.clock_ms = uds_can_transport_clock;
    config.context = transport;
    config.request_id = UDS_APP_REQUEST_ID;
    config.response_id = UDS_APP_RESPONSE_ID;
    config.functional_request_id = UDS_APP_FUNCTIONAL_ID;
    config.uds_callbacks.read_did = uds_did_app_read;
    config.uds_callbacks.write_did = uds_did_app_write;
    config.uds_callbacks.security_seed = uds_app_security_seed;
    config.uds_callbacks.security_key = uds_app_security_key;
    config.uds_callbacks.ecu_reset = uds_app_ecu_reset_prepare;
    config.uds_callbacks.ecu_reset_execute = uds_app_ecu_reset_execute;
    config.uds_callbacks.dtc_backend = uds_dtc_app_get_backend();
    config.uds_callbacks.clear_dtc = uds_dtc_app_clear;
    config.uds_callbacks.request_download = uds_bootloader_request_download;
    config.uds_callbacks.transfer_data = uds_bootloader_transfer_data;
    config.uds_callbacks.request_transfer_exit = uds_bootloader_transfer_exit;
    config.uds_callbacks.routine_control = uds_bootloader_routine_control;
    config.uds_callbacks.service_backends = &s_service_backends;
    config.uds_context = transport;

    s_transport = transport;
    s_rx_head = 0U;
    s_rx_tail = 0U;
    s_initialized = uds_isotp_endpoint_init(&s_endpoint, &config, now_ms);
}

void uds_app_rx_from_isr(uint32_t can_id, const uint8_t *data, uint8_t dlc) {
    if (!s_initialized || (data == NULL) || (dlc == 0U) || (dlc > 8U) ||
        ((can_id != UDS_APP_REQUEST_ID) && (can_id != UDS_APP_FUNCTIONAL_ID))) {
        return;
    }
    uint8_t next_head = (uint8_t)((s_rx_head + 1U) % UDS_APP_RX_FIFO_CAPACITY);
    if (next_head == s_rx_tail) {
        return;
    }
    s_rx_fifo[s_rx_head].can_id = can_id;
    s_rx_fifo[s_rx_head].dlc = dlc;
    s_rx_fifo[s_rx_head].is_fd = false;
    s_rx_fifo[s_rx_head].bit_rate_switch = false;
    (void)memcpy(s_rx_fifo[s_rx_head].data, data, dlc);
    s_rx_head = next_head;
}

void uds_app_process(uint32_t now_ms) {
    if (!s_initialized || (s_transport == NULL)) {
        return;
    }

    IsoTpCanFrame frame = {0};
    bool has_frame = false;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (s_rx_head != s_rx_tail) {
        frame = s_rx_fifo[s_rx_tail];
        s_rx_tail = (uint8_t)((s_rx_tail + 1U) % UDS_APP_RX_FIFO_CAPACITY);
        has_frame = true;
    }
    __set_PRIMASK(primask);

    if (uds_can_transport_tx_complete(s_transport)) {
        uds_isotp_endpoint_tx_complete(&s_endpoint);
    }
    if (has_frame) {
        (void)uds_isotp_endpoint_receive(&s_endpoint, &frame, now_ms);
    }
    (void)uds_isotp_endpoint_process(&s_endpoint, now_ms);
    (void)uds_isotp_endpoint_tick(&s_endpoint, now_ms);
}