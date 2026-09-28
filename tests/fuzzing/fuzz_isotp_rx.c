#include "uds_iso_tp/isotp.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/**
 * @brief libFuzzer entry point for ISO 15765-2 receive state machine.
 * Injects arbitrary CAN frames, lengths, and PCI types.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if ((data == NULL) || (size == 0U)) {
        return 0;
    }

    IsoTpConfig config;
    isotp_config_default(&config);

    IsoTpRx rx;
    isotp_rx_init(&rx, &config, 0x7E0U, 0x7E8U);

    size_t offset = 0U;
    uint32_t now_ms = 100U;

    while (offset < size) {
        size_t chunk_len = ((size - offset) > 8U) ? 8U : (size - offset);

        IsoTpCanFrame frame;
        (void)memset(&frame, 0, sizeof(frame));
        frame.can_id = 0x7E0U;
        frame.dlc = (uint8_t)chunk_len;
        frame.is_fd = false;
        frame.bit_rate_switch = false;
        (void)memcpy(frame.data, &data[offset], chunk_len);

        IsoTpRxEvent event;
        (void)memset(&event, 0, sizeof(event));
        (void)isotp_rx_feed(&rx, &frame, now_ms, &event);
        (void)isotp_rx_tick(&rx, now_ms);

        offset += chunk_len;
        now_ms += 10U;
    }

    return 0;
}

#if !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) && !defined(__AFL_COMPILER)
int main(void) {
    /* Corpus seed 1: Single Frame */
    const uint8_t sf[] = { 0x02, 0x10, 0x01, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };
    (void)LLVMFuzzerTestOneInput(sf, sizeof(sf));

    /* Corpus seed 2: First Frame */
    const uint8_t ff[] = { 0x10, 0x14, 0x22, 0xF1, 0x90, 0x00, 0x00, 0x00 };
    (void)LLVMFuzzerTestOneInput(ff, sizeof(ff));

    /* Corpus seed 3: Flow Control */
    const uint8_t fc[] = { 0x30, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00 };
    (void)LLVMFuzzerTestOneInput(fc, sizeof(fc));

    /* Corpus seed 4: Consecutive Frame */
    const uint8_t cf[] = { 0x21, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00 };
    (void)LLVMFuzzerTestOneInput(cf, sizeof(cf));

    /* Corpus seed 5: Malformed frames */
    const uint8_t malformed[] = { 0xFF, 0x00, 0x1F, 0xFF, 0x40, 0x50, 0x60, 0x70 };
    (void)LLVMFuzzerTestOneInput(malformed, sizeof(malformed));

    return 0;
}
#endif
