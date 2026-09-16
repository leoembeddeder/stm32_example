#include "uds_app.h"

#include "uds_app_config.h"
#include "uds_auth_app.h"
#include "uds_bootloader.h"
#include "uds_did_app.h"
#include "uds_dtc_app.h"
#include "uds_memory_app.h"
#include "uds_security_app.h"
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
static volatile uint32_t s_rx_overflow_count;
static volatile bool s_rx_overflow_flag;
static bool s_initialized;
static UdsServiceBackends s_service_backends;

static UdsCallbackResult uds_app_ecu_reset_prepare(void *context, uint8_t subfunction) {
    (void)context;
    return (subfunction == 0x01U) ? UDS_RESULT_OK : UDS_RESULT_OUT_OF_RANGE;
}

static void uds_app_ecu_reset_execute(void *context, uint8_t subfunction) {
    (void)context;
    if (uds_bootloader_is_activation_pending()) {
        if (uds_bootloader_get_target() == UDS_BL_TARGET_STM32C092) {
            (void)uds_bootloader_activate_candidate();
            uds_bootloader_jump_to_app(UDS_BL_C092_APP_SLOT_A_START);
        } else {
            (void)uds_bootloader_activate_candidate();
            uds_bootloader_jump_to_app(UDS_BL_F767_APP_SLOT_B_START);
        }
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
    uds_memory_app_init();
    uds_security_app_init();

    (void)memset(&s_service_backends, 0, sizeof(s_service_backends));
    s_service_backends.authentication = uds_auth_app_get_backend();
    s_service_backends.memory = uds_memory_app_get_backend();

    UdsIsoTpEndpointConfig config = {0};
    isotp_config_classic_can(&config.isotp_config);
#if UDS_APP_CLASSIC_PADDING_ENABLED
    isotp_config_set_padding(&config.isotp_config, true, UDS_APP_CLASSIC_PADDING_VALUE);
#endif
    config.send_frame = uds_can_transport_send;
    config.tx_complete = uds_can_transport_tx_complete;
    config.tx_error = uds_can_transport_tx_error;
    config.clock_ms = uds_can_transport_clock;
    config.context = transport;
    config.request_id = UDS_APP_REQUEST_ID;
    config.response_id = UDS_APP_RESPONSE_ID;
    config.functional_request_id = UDS_APP_FUNCTIONAL_ID;
    config.uds_callbacks.read_did = uds_did_app_read;
    config.uds_callbacks.write_did = uds_did_app_write;
    config.uds_callbacks.security_seed = uds_security_app_seed;
    config.uds_callbacks.security_key = uds_security_app_key;
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
    s_rx_overflow_count = 0U;
    s_rx_overflow_flag = false;
    s_initialized = uds_isotp_endpoint_init(&s_endpoint, &config, now_ms);
    if (s_initialized) {
        uds_memory_app_set_server(&s_endpoint.uds);
    }
}

void uds_app_rx_from_isr(uint32_t can_id, const uint8_t *data, uint8_t dlc) {
    if (!s_initialized || (data == NULL) || (dlc == 0U) || (dlc > 8U) ||
        ((can_id != UDS_APP_REQUEST_ID) && (can_id != UDS_APP_FUNCTIONAL_ID))) {
        return;
    }
    uint8_t next_head = (uint8_t)((s_rx_head + 1U) % UDS_APP_RX_FIFO_CAPACITY);
    if (next_head == s_rx_tail) {
        s_rx_overflow_count = (uint32_t)(s_rx_overflow_count + 1U);
        s_rx_overflow_flag = true;
        return;
    }
    s_rx_fifo[s_rx_head].can_id = can_id;
    s_rx_fifo[s_rx_head].dlc = dlc;
    s_rx_fifo[s_rx_head].is_fd = false;
    s_rx_fifo[s_rx_head].bit_rate_switch = false;
    (void)memcpy(s_rx_fifo[s_rx_head].data, data, dlc);
    s_rx_head = next_head;
}

uint32_t uds_app_get_rx_overflow_count(void) {
    return s_rx_overflow_count;
}

bool uds_app_get_rx_overflow_flag(void) {
    return s_rx_overflow_flag;
}

void uds_app_clear_rx_overflow_flag(void) {
    s_rx_overflow_flag = false;
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