#include "uds_iso_tp/uds.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

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
    uds_server_init(&server, &callbacks, NULL, 0U);

    uint8_t resp_buf[512];
    uint16_t resp_len = 0U;

    (void)uds_server_handle(&server, data, (uint16_t)size, resp_buf, &resp_len, sizeof(resp_buf), 100U);

    uint8_t sessions[] = { UDS_SESSION_DEFAULT, UDS_SESSION_PROGRAMMING, UDS_SESSION_EXTENDED };
    (void)uds_server_request_session(&server, sessions[data[0] % 3U], 200U);

    (void)uds_server_handle_addressed(&server, data, (uint16_t)size, resp_buf, &resp_len,
                                      sizeof(resp_buf), UDS_ADDRESS_FUNCTIONAL, 300U);

    (void)uds_server_tick(&server, 1000U);

    return 0;
}

#if !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) && !defined(__AFL_COMPILER)
int main(void) {
    /* Corpus seed 1: DiagnosticSessionControl */
    const uint8_t s1[] = { 0x10, 0x01 };
    (void)LLVMFuzzerTestOneInput(s1, sizeof(s1));

    /* Corpus seed 2: TesterPresent */
    const uint8_t s2[] = { 0x3E, 0x00 };
    (void)LLVMFuzzerTestOneInput(s2, sizeof(s2));

    /* Corpus seed 3: ReadDataByIdentifier */
    const uint8_t s3[] = { 0x22, 0xF1, 0x90 };
    (void)LLVMFuzzerTestOneInput(s3, sizeof(s3));

    /* Corpus seed 4: SecurityAccess */
    const uint8_t s4[] = { 0x27, 0x01 };
    (void)LLVMFuzzerTestOneInput(s4, sizeof(s4));

    /* Corpus seed 5: Malformed arbitrary request */
    const uint8_t s5[] = { 0x7F, 0x22, 0x31, 0xFF, 0x00, 0x12, 0x34 };
    (void)LLVMFuzzerTestOneInput(s5, sizeof(s5));

    return 0;
}
#endif
