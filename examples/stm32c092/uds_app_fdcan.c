#include "uds_app_fdcan.h"

#include "uds_app_config.h"
#include "uds_platform_fdcan.h"
#include "uds_iso_tp/uds.h"
#include "uds_iso_tp/uds_download.h"
#include "uds_iso_tp/uds_dtc.h"
#include "uds_iso_tp/uds_services.h"

#ifndef STM32_UDS_ISO_TP_UDS_BOOTLOADER_H
typedef enum { UDS_BL_TARGET_STM32F767 = 0, UDS_BL_TARGET_STM32C092 = 1 } UdsBootloaderTarget;
#endif

#include <stddef.h>
#include <string.h>

static UdsC092FdcanTransport *s_transport;
static UdsIsoTpEndpoint s_endpoint;
static IsoTpCanFrame s_rx_frame;
static volatile bool s_rx_pending;
static bool s_initialized;
static UdsC092DiagnosticTrace *s_diagnostics;
static UdsServiceBackends s_service_backends;

#if defined(__GNUC__)
__attribute__((weak))
#endif
void uds_c092_platform_reset_poll(void) {
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak)) void uds_dtc_app_init(void) {}
__attribute__((weak)) const UdsDtcBackend *uds_dtc_app_get_backend(void) {
    return NULL;
}
__attribute__((weak)) UdsCallbackResult uds_dtc_app_clear(void *context, uint32_t group_of_dtc) {
    (void)context;
    (void)group_of_dtc;
    return UDS_RESULT_OUT_OF_RANGE;
}

__attribute__((weak)) void uds_did_app_init(void) {}
__attribute__((weak)) UdsCallbackResult uds_did_app_read(void *context, uint16_t did, uint8_t *data,
                                                         uint16_t *length, uint16_t capacity) {
    (void)context;
    (void)did;
    (void)data;
    (void)length;
    (void)capacity;
    return UDS_RESULT_OUT_OF_RANGE;
}
__attribute__((weak)) UdsCallbackResult uds_did_app_write(void *context, uint16_t did,
                                                          const uint8_t *data, uint16_t length) {
    (void)context;
    (void)did;
    (void)data;
    (void)length;
    return UDS_RESULT_OUT_OF_RANGE;
}

__attribute__((weak)) void uds_security_app_init(void) {}
__attribute__((weak)) UdsCallbackResult uds_security_app_seed(void *context, uint8_t level,
                                                              uint8_t *seed, uint16_t *length,
                                                              uint16_t capacity) {
    (void)context;
    (void)level;
    (void)seed;
    (void)length;
    (void)capacity;
    return UDS_RESULT_OUT_OF_RANGE;
}
__attribute__((weak)) UdsCallbackResult uds_security_app_key(void *context, uint8_t level,
                                                             const uint8_t *key, uint16_t length) {
    (void)context;
    (void)level;
    (void)key;
    (void)length;
    return UDS_RESULT_INVALID_KEY;
}

__attribute__((weak)) void uds_bootloader_init(void) {}
__attribute__((weak)) void uds_bootloader_set_target(UdsBootloaderTarget target) {
    (void)target;
}
__attribute__((weak)) bool uds_bootloader_is_activation_pending(void) {
    return false;
}
__attribute__((weak)) UdsDownloadResult uds_bootloader_activate_candidate(void) {
    return UDS_DOWNLOAD_OK;
}
__attribute__((weak)) void uds_bootloader_jump_to_app(uint32_t app_vector_addr) {
    (void)app_vector_addr;
}
__attribute__((weak)) UdsCallbackResult uds_bootloader_request_download(
    void *context, uint32_t address, uint32_t length, uint16_t *max_block_length) {
    (void)context;
    (void)address;
    (void)length;
    (void)max_block_length;
    return UDS_RESULT_OUT_OF_RANGE;
}
__attribute__((weak)) UdsCallbackResult uds_bootloader_transfer_data(void *context,
                                                                     uint8_t block_sequence,
                                                                     const uint8_t *data,
                                                                     uint16_t length) {
    (void)context;
    (void)block_sequence;
    (void)data;
    (void)length;
    return UDS_RESULT_OUT_OF_RANGE;
}
__attribute__((weak)) UdsCallbackResult
uds_bootloader_transfer_exit(void *context, const uint8_t *request, uint16_t request_len,
                             uint8_t *response, uint16_t *response_len, uint16_t capacity) {
    (void)context;
    (void)request;
    (void)request_len;
    (void)response;
    (void)response_len;
    (void)capacity;
    return UDS_RESULT_OUT_OF_RANGE;
}
__attribute__((weak)) UdsCallbackResult uds_bootloader_routine_control(
    void *context, uint8_t subfunction, uint16_t routine_id, const uint8_t *in, uint16_t in_len,
    uint8_t *out, uint16_t *out_len, uint16_t capacity) {
    (void)context;
    (void)subfunction;
    (void)routine_id;
    (void)in;
    (void)in_len;
    (void)out;
    (void)out_len;
    (void)capacity;
    return UDS_RESULT_OUT_OF_RANGE;
}

__attribute__((weak)) void uds_auth_app_init(void) {}
__attribute__((weak)) const UdsAuthenticationServiceBackend *uds_auth_app_get_backend(void) {
    return NULL;
}

__attribute__((weak)) void uds_memory_app_init(void) {}
__attribute__((weak)) const UdsMemoryServiceBackend *uds_memory_app_get_backend(void) {
    return NULL;
}
#else
void uds_dtc_app_init(void);
const UdsDtcBackend *uds_dtc_app_get_backend(void);
UdsCallbackResult uds_dtc_app_clear(void *context, uint32_t group_of_dtc);
void uds_did_app_init(void);
UdsCallbackResult uds_did_app_read(void *context, uint16_t did, uint8_t *data, uint16_t *length,
                                   uint16_t capacity);
UdsCallbackResult uds_did_app_write(void *context, uint16_t did, const uint8_t *data,
                                    uint16_t length);
void uds_security_app_init(void);
UdsCallbackResult uds_security_app_seed(void *context, uint8_t level, uint8_t *seed,
                                        uint16_t *length, uint16_t capacity);
UdsCallbackResult uds_security_app_key(void *context, uint8_t level, const uint8_t *key,
                                       uint16_t length);
void uds_bootloader_init(void);
void uds_bootloader_set_target(UdsBootloaderTarget target);
bool uds_bootloader_is_activation_pending(void);
UdsDownloadResult uds_bootloader_activate_candidate(void);
void uds_bootloader_jump_to_app(uint32_t app_vector_addr);
UdsCallbackResult uds_bootloader_request_download(void *context, uint32_t address, uint32_t length,
                                                  uint16_t *max_block_length);
UdsCallbackResult uds_bootloader_transfer_data(void *context, uint8_t block_sequence,
                                               const uint8_t *data, uint16_t length);
UdsCallbackResult uds_bootloader_transfer_exit(void *context, const uint8_t *request,
                                               uint16_t request_len, uint8_t *response,
                                               uint16_t *response_len, uint16_t capacity);
UdsCallbackResult uds_bootloader_routine_control(void *context, uint8_t subfunction,
                                                 uint16_t routine_id, const uint8_t *in,
                                                 uint16_t in_len, uint8_t *out, uint16_t *out_len,
                                                 uint16_t capacity);
void uds_auth_app_init(void);
const UdsAuthenticationServiceBackend *uds_auth_app_get_backend(void);
void uds_memory_app_init(void);
const UdsMemoryServiceBackend *uds_memory_app_get_backend(void);
#endif

void uds_c092_app_init(UdsC092FdcanTransport *transport, uint32_t now_ms,
                       const UdsCallbacks *application_callbacks, void *uds_context,
                       UdsIsoTpResetEventFn reset_event, void *reset_event_context) {
    if (transport == NULL)
        return;

    uds_did_app_init();
    uds_dtc_app_init();
    uds_bootloader_init();
    uds_bootloader_set_target(UDS_BL_TARGET_STM32C092);
    uds_auth_app_init();
    uds_memory_app_init();
    uds_security_app_init();

    (void)memset(&s_service_backends, 0, sizeof(s_service_backends));
    s_service_backends.authentication = uds_auth_app_get_backend();
    s_service_backends.memory = uds_memory_app_get_backend();

    UdsIsoTpEndpointConfig config = {0};
    isotp_config_classic_can(&config.isotp_config);
#if UDS_C092_CLASSIC_PADDING_ENABLED
    isotp_config_set_padding(&config.isotp_config, true, UDS_C092_CLASSIC_PADDING_VALUE);
#endif
    config.send_frame = uds_c092_fdcan_send;
    config.tx_complete = uds_c092_fdcan_tx_complete;
    config.tx_error = uds_c092_fdcan_tx_error;
    config.clock_ms = uds_c092_fdcan_clock;
    config.reset_event = reset_event;
    config.context = transport;
    config.reset_event_context = reset_event_context;
    config.request_id = UDS_C092_REQUEST_ID;
    config.response_id = UDS_C092_RESPONSE_ID;
    config.functional_request_id = UDS_C092_FUNCTIONAL_REQUEST_ID;
    if (application_callbacks != NULL)
        config.uds_callbacks = *application_callbacks;
    if (config.uds_callbacks.read_did == NULL)
        config.uds_callbacks.read_did = uds_did_app_read;
    if (config.uds_callbacks.write_did == NULL)
        config.uds_callbacks.write_did = uds_did_app_write;
    if (config.uds_callbacks.security_seed == NULL)
        config.uds_callbacks.security_seed = uds_security_app_seed;
    if (config.uds_callbacks.security_key == NULL)
        config.uds_callbacks.security_key = uds_security_app_key;
    if (config.uds_callbacks.dtc_backend == NULL)
        config.uds_callbacks.dtc_backend = uds_dtc_app_get_backend();
    if (config.uds_callbacks.clear_dtc == NULL)
        config.uds_callbacks.clear_dtc = uds_dtc_app_clear;
    if (config.uds_callbacks.request_download == NULL)
        config.uds_callbacks.request_download = uds_bootloader_request_download;
    if (config.uds_callbacks.transfer_data == NULL)
        config.uds_callbacks.transfer_data = uds_bootloader_transfer_data;
    if (config.uds_callbacks.request_transfer_exit == NULL)
        config.uds_callbacks.request_transfer_exit = uds_bootloader_transfer_exit;
    if (config.uds_callbacks.routine_control == NULL)
        config.uds_callbacks.routine_control = uds_bootloader_routine_control;
    if (config.uds_callbacks.service_backends == NULL)
        config.uds_callbacks.service_backends = &s_service_backends;
    if (config.uds_callbacks.ecu_reset == NULL)
        config.uds_callbacks.ecu_reset = uds_c092_platform_reset_prepare;
    if (config.uds_callbacks.ecu_reset_execute == NULL)
        config.uds_callbacks.ecu_reset_execute = uds_c092_platform_reset_execute;
    config.uds_context = uds_context;

    s_transport = transport;
    s_rx_pending = false;
    s_initialized = uds_isotp_endpoint_init(&s_endpoint, &config, now_ms);
    if (s_diagnostics != NULL) {
        uds_c092_fdcan_attach_diagnostics(transport, s_diagnostics);
        if (s_initialized) {
            uds_c092_diagnostic_mark(s_diagnostics, UDS_C092_BOOT_ISOTP_INIT_DONE, now_ms);
            uds_c092_diagnostic_mark(s_diagnostics, UDS_C092_BOOT_UDS_INIT_DONE, now_ms);
        } else {
            uds_c092_diagnostic_fault(s_diagnostics, now_ms);
        }
    }
}

void uds_c092_app_attach_diagnostics(UdsC092DiagnosticTrace *trace) {
    s_diagnostics = trace;
    if (s_transport != NULL)
        uds_c092_fdcan_attach_diagnostics(s_transport, trace);
}

bool uds_c092_app_is_diagnostic_ready(void) {
    return (s_diagnostics == NULL) || uds_c092_diagnostic_is_ready(s_diagnostics);
}

void uds_c092_app_init_default(UdsC092FdcanTransport *transport, uint32_t now_ms) {
    uds_c092_app_init(transport, now_ms, NULL, NULL, NULL, NULL);
}

bool uds_c092_app_accept_rx(uint32_t can_id, bool is_fd, bool bit_rate_switch, bool is_extended_id,
                            bool is_remote_frame) {
    return uds_c092_filter_accept(can_id, UDS_C092_REQUEST_ID, UDS_C092_FUNCTIONAL_REQUEST_ID,
                                  is_fd, bit_rate_switch, is_extended_id, is_remote_frame);
}

void uds_c092_app_rx_from_isr_ex(uint32_t can_id, const uint8_t *data, uint8_t dlc, bool is_fd,
                                 bool bit_rate_switch, bool is_extended_id, bool is_remote_frame) {
    uint32_t now_ms = uds_c092_fdcan_clock(s_transport);
    if (!s_initialized) {
        uds_c092_diagnostic_count_rx_rejected_not_initialized(s_diagnostics, now_ms);
        return;
    }
    if ((data == NULL) || (dlc == 0U) || (dlc > (is_fd ? ISOTP_MAX_FRAME_DATA : 8U)) ||
        !uds_c092_app_accept_rx(can_id, is_fd, bit_rate_switch, is_extended_id, is_remote_frame)) {
        uds_c092_diagnostic_count_rx_reject(s_diagnostics);
        return;
    }
    if (s_rx_pending) {
        uds_c092_diagnostic_count_rx_mailbox_full_at(s_diagnostics, now_ms);
        return;
    }
    uds_c092_diagnostic_count_rx(s_diagnostics, now_ms);
    uds_c092_diagnostic_count_rx_accepted(s_diagnostics, now_ms);
    s_rx_frame.can_id = can_id;
    s_rx_frame.dlc = dlc;
    s_rx_frame.is_fd = is_fd;
    s_rx_frame.bit_rate_switch = bit_rate_switch;
    (void)memcpy(s_rx_frame.data, data, dlc);
    s_rx_pending = true;
}

void uds_c092_app_rx_from_isr(uint32_t can_id, const uint8_t *data, uint8_t dlc, bool is_fd,
                              bool bit_rate_switch) {
    uds_c092_app_rx_from_isr_ex(can_id, data, dlc, is_fd, bit_rate_switch, false, false);
}

void uds_c092_app_process(uint32_t now_ms) {
    if (!s_initialized || (s_transport == NULL))
        return;

    IsoTpCanFrame frame = {0};
    bool has_frame = false;
    __disable_irq();
    if (s_rx_pending) {
        frame = s_rx_frame;
        s_rx_pending = false;
        has_frame = true;
    }
    __enable_irq();

    __disable_irq();
    uds_c092_fdcan_poll_tx_events(s_transport);
    bool tx_complete = uds_c092_fdcan_tx_complete(s_transport);
    __enable_irq();
    if (tx_complete) {
        uds_c092_diagnostic_count_tx_complete(s_diagnostics, now_ms);
        uds_isotp_endpoint_tx_complete(&s_endpoint);
    }
    if (has_frame) {
        IsoTpStatus receive_status = uds_isotp_endpoint_receive(&s_endpoint, &frame, now_ms);
        if (receive_status != ISOTP_ERR_ARGUMENT)
            uds_c092_diagnostic_count_isotp_rx_at(s_diagnostics, now_ms);
        if ((receive_status == ISOTP_COMPLETE) || (receive_status == ISOTP_TX_FRAME_READY)) {
            uds_c092_diagnostic_count_uds_request(s_diagnostics, now_ms);
            if ((receive_status == ISOTP_TX_FRAME_READY) &&
                (s_endpoint.tx_pending || s_endpoint.queued_response_pending))
                uds_c092_diagnostic_count_uds_response_generated(s_diagnostics, now_ms);
        }
    }
    (void)uds_isotp_endpoint_process(&s_endpoint, now_ms);
    (void)uds_isotp_endpoint_tick(&s_endpoint, now_ms);
    uds_c092_platform_reset_poll();
}
