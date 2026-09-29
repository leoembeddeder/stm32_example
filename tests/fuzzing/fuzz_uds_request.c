#include "uds_iso_tp/uds.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static uint8_t s_mock_did_store[64] = {0x11, 0x22, 0x33, 0x44};

static UdsCallbackResult fuzz_read_did(void *ctx, uint16_t did, uint8_t *data, uint16_t *len,
                                       uint16_t cap) {
    (void)ctx;
    (void)did;
    if (cap < 4U) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    memcpy(data, s_mock_did_store, 4U);
    *len = 4U;
    return UDS_RESULT_OK;
}

static UdsCallbackResult fuzz_write_did(void *ctx, uint16_t did, const uint8_t *data,
                                        uint16_t len) {
    (void)ctx;
    (void)did;
    if (len > sizeof(s_mock_did_store)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    memcpy(s_mock_did_store, data, len);
    return UDS_RESULT_OK;
}

static UdsCallbackResult fuzz_security_seed(void *ctx, uint8_t level, uint8_t *seed, uint16_t *len,
                                            uint16_t cap) {
    (void)ctx;
    (void)level;
    if (cap < 4U) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    seed[0] = 0xAAU;
    seed[1] = 0xBBU;
    seed[2] = 0xCCU;
    seed[3] = 0xDDU;
    *len = 4U;
    return UDS_RESULT_OK;
}

static UdsCallbackResult fuzz_security_key(void *ctx, uint8_t level, const uint8_t *key,
                                           uint16_t len) {
    (void)ctx;
    (void)level;
    if (len != 4U) {
        return UDS_RESULT_INVALID_KEY;
    }
    if ((key[0] == 0xAAU) && (key[1] == 0xBBU) && (key[2] == 0xCCU) && (key[3] == 0xDDU)) {
        return UDS_RESULT_OK;
    }
    return UDS_RESULT_INVALID_KEY;
}

static UdsCallbackResult fuzz_routine(void *ctx, uint8_t subfunction, uint16_t routine_id,
                                      const uint8_t *req, uint16_t req_len, uint8_t *resp,
                                      uint16_t *resp_len, uint16_t cap) {
    (void)ctx;
    (void)subfunction;
    (void)routine_id;
    (void)req;
    (void)req_len;
    if (cap < 1U) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    resp[0] = 0x00U;
    *resp_len = 1U;
    return UDS_RESULT_OK;
}

static UdsCallbackResult fuzz_req_download(void *ctx, uint32_t address, uint32_t length,
                                           uint16_t *max_block_length) {
    (void)ctx;
    (void)address;
    (void)length;
    *max_block_length = 128U;
    return UDS_RESULT_OK;
}

static UdsCallbackResult fuzz_transfer_data(void *ctx, uint8_t block_sequence_counter,
                                            const uint8_t *data, uint16_t length) {
    (void)ctx;
    (void)block_sequence_counter;
    (void)data;
    (void)length;
    return UDS_RESULT_OK;
}

static UdsCallbackResult fuzz_transfer_exit(void *ctx, const uint8_t *request, uint16_t request_len,
                                            uint8_t *response, uint16_t *response_len,
                                            uint16_t capacity) {
    (void)ctx;
    (void)request;
    (void)request_len;
    (void)response;
    (void)capacity;
    *response_len = 0U;
    return UDS_RESULT_OK;
}

static UdsCallbackResult fuzz_ecu_reset(void *ctx, uint8_t subfunction) {
    (void)ctx;
    (void)subfunction;
    return UDS_RESULT_OK;
}

static void fuzz_ecu_reset_execute(void *ctx, uint8_t subfunction) {
    (void)ctx;
    (void)subfunction;
}

static void setup_fuzz_callbacks(UdsCallbacks *callbacks) {
    callbacks->read_did = fuzz_read_did;
    callbacks->write_did = fuzz_write_did;
    callbacks->security_seed = fuzz_security_seed;
    callbacks->security_key = fuzz_security_key;
    callbacks->routine_control = fuzz_routine;
    callbacks->request_download = fuzz_req_download;
    callbacks->transfer_data = fuzz_transfer_data;
    callbacks->request_transfer_exit = fuzz_transfer_exit;
    callbacks->ecu_reset = fuzz_ecu_reset;
    callbacks->ecu_reset_execute = fuzz_ecu_reset_execute;
}

/**
 * @brief libFuzzer entry point for ISO 14229-1 server parser and state machine.
 * Injects arbitrary diagnostic request payloads across sessions and security levels.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if ((data == NULL) || (size == 0U) || (size > 4096U)) {
        return 0;
    }

    UdsServer server;
    UdsCallbacks callbacks;
    (void)memset(&callbacks, 0, sizeof(callbacks));
    setup_fuzz_callbacks(&callbacks);
    uds_server_init(&server, &callbacks, NULL, 0U);

    uint8_t resp_buf[512];
    uint16_t resp_len = 0U;

    (void)uds_server_handle(&server, data, (uint16_t)size, resp_buf, &resp_len, sizeof(resp_buf),
                            100U);

    uint8_t sessions[] = {UDS_SESSION_DEFAULT, UDS_SESSION_PROGRAMMING, UDS_SESSION_EXTENDED};
    (void)uds_server_request_session(&server, sessions[data[0] % 3U], 200U);

    (void)uds_server_handle_addressed(&server, data, (uint16_t)size, resp_buf, &resp_len,
                                      sizeof(resp_buf), UDS_ADDRESS_FUNCTIONAL, 300U);

    (void)uds_server_tick(&server, 1000U);

    /* Stateful multi-step injection if input has enough bytes */
    if (size > 8U) {
        size_t half = size / 2U;
        (void)uds_server_handle(&server, &data[half], (uint16_t)(size - half), resp_buf, &resp_len,
                                sizeof(resp_buf), 1100U);
        (void)uds_server_tick(&server, 2000U);
    }

    return 0;
}

#if !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) && !defined(__AFL_COMPILER)
int main(void) {
    /* Corpus seed 1: DiagnosticSessionControl */
    const uint8_t s1[] = {0x10, 0x01};
    (void)LLVMFuzzerTestOneInput(s1, sizeof(s1));

    /* Corpus seed 2: TesterPresent */
    const uint8_t s2[] = {0x3E, 0x00};
    (void)LLVMFuzzerTestOneInput(s2, sizeof(s2));

    /* Corpus seed 3: ReadDataByIdentifier */
    const uint8_t s3[] = {0x22, 0xF1, 0x90};
    (void)LLVMFuzzerTestOneInput(s3, sizeof(s3));

    /* Corpus seed 4: SecurityAccess */
    const uint8_t s4[] = {0x27, 0x01};
    (void)LLVMFuzzerTestOneInput(s4, sizeof(s4));

    /* Corpus seed 5: Malformed arbitrary request */
    const uint8_t s5[] = {0x7F, 0x22, 0x31, 0xFF, 0x00, 0x12, 0x34};
    (void)LLVMFuzzerTestOneInput(s5, sizeof(s5));

    return 0;
}
#endif
