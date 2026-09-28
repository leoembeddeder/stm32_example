/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#include "uds_iso_tp/uds_download.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t erase_start_calls;
    uint32_t erase_poll_calls;
    uint32_t program_calls;
    uint32_t verify_calls;
    uint32_t abort_calls;
    uint32_t watchdog_calls;
    UdsDownloadResult erase_start_rc;
    UdsDownloadResult erase_poll_rc;
    UdsDownloadResult program_rc;
    UdsDownloadResult verify_rc;
    uint8_t programmed_bytes[1024];
    uint32_t programmed_length;
} TestContext;

static UdsDownloadResult mock_erase_start(void *context, uint32_t address, uint32_t length) {
    TestContext *ctx = (TestContext *)context;
    (void)address;
    (void)length;
    if (ctx != NULL) {
        ctx->erase_start_calls++;
        return ctx->erase_start_rc;
    }
    return UDS_DOWNLOAD_OK;
}

static UdsDownloadResult mock_erase_poll(void *context) {
    TestContext *ctx = (TestContext *)context;
    if (ctx != NULL) {
        ctx->erase_poll_calls++;
        return ctx->erase_poll_rc;
    }
    return UDS_DOWNLOAD_OK;
}

static UdsDownloadResult mock_program(void *context, uint32_t address, const uint8_t *data,
                                      uint16_t length) {
    TestContext *ctx = (TestContext *)context;
    (void)address;
    if (ctx != NULL) {
        ctx->program_calls++;
        if (ctx->program_rc != UDS_DOWNLOAD_OK) {
            return ctx->program_rc;
        }
        if (ctx->programmed_length + length <= sizeof(ctx->programmed_bytes)) {
            memcpy(&ctx->programmed_bytes[ctx->programmed_length], data, length);
            ctx->programmed_length += length;
        }
        return UDS_DOWNLOAD_OK;
    }
    return UDS_DOWNLOAD_OK;
}

static UdsDownloadResult mock_verify(void *context, const UdsDownloadMetadata *metadata,
                                     uint32_t expected_crc32, bool has_expected_crc32) {
    TestContext *ctx = (TestContext *)context;
    (void)metadata;
    (void)expected_crc32;
    (void)has_expected_crc32;
    if (ctx != NULL) {
        ctx->verify_calls++;
        return ctx->verify_rc;
    }
    return UDS_DOWNLOAD_OK;
}

static void mock_abort(void *context) {
    TestContext *ctx = (TestContext *)context;
    if (ctx != NULL) {
        ctx->abort_calls++;
    }
}

static void mock_watchdog(void *context) {
    TestContext *ctx = (TestContext *)context;
    if (ctx != NULL) {
        ctx->watchdog_calls++;
    }
}

static void setup_valid_map(UdsDownloadMemoryMap *map) {
    memset(map, 0, sizeof(*map));
    map->staging_image.start = 0x08040000U;
    map->staging_image.end_exclusive = 0x08080000U; /* 256 KB staging */
    map->bootloader.start = 0x08000000U;
    map->bootloader.end_exclusive = 0x08010000U; /* 64 KB bootloader */
    map->active_application.start = 0x08010000U;
    map->active_application.end_exclusive = 0x08040000U; /* 192 KB app */
    map->persistent_storage.start = 0x08080000U;
    map->persistent_storage.end_exclusive = 0x08090000U;
    map->diagnostic_storage.start = 0x08090000U;
    map->diagnostic_storage.end_exclusive = 0x080A0000U;
    map->erase_alignment = 0x800U; /* 2 KB sector */
    map->program_alignment = 8U;   /* 8-byte write */
    map->max_block_length = 256U;
    map->activation_supported = false;
}

static void setup_valid_callbacks(UdsDownloadCallbacks *cb) {
    memset(cb, 0, sizeof(*cb));
    cb->erase_start = mock_erase_start;
    cb->erase_poll = mock_erase_poll;
    cb->program = mock_program;
    cb->verify_image = mock_verify;
    cb->abort = mock_abort;
    cb->watchdog_kick = mock_watchdog;
}

static void test_init_null_and_defaults(void) {
    UdsDownload dl;
    /* NULL download pointer */
    uds_download_init(NULL, NULL, NULL, NULL);

    /* NULL memory and NULL callbacks */
    uds_download_init(&dl, NULL, NULL, NULL);
    assert(uds_download_state(&dl) == UDS_DOWNLOAD_IDLE);
    assert(dl.memory.staging_image.start == 0U);
    assert(dl.memory.erase_alignment == 0U);
    assert(dl.callbacks.erase_start == NULL);
    assert(uds_download_metadata(&dl) != NULL);
    assert(!uds_download_activation_pending(&dl));

    /* NULL query helpers */
    assert(uds_download_state(NULL) == UDS_DOWNLOAD_ABORTED_STATE);
    assert(uds_download_metadata(NULL) == NULL);
    assert(!uds_download_activation_pending(NULL));
}

static void test_region_validation_and_overlap(void) {
    UdsDownload dl;
    UdsDownloadMemoryMap map;
    UdsDownloadCallbacks cb;
    TestContext ctx;

    setup_valid_map(&map);
    setup_valid_callbacks(&cb);
    memset(&ctx, 0, sizeof(ctx));
    uds_download_init(&dl, &map, &cb, &ctx);

    /* NULL download */
    assert(uds_download_begin(NULL, 0x08040000U, 0x1000U, 100U) == UDS_DOWNLOAD_INVALID_ARGUMENT);

    /* Missing mandatory callbacks */
    UdsDownloadCallbacks bad_cb = cb;
    bad_cb.erase_start = NULL;
    uds_download_init(&dl, &map, &bad_cb, &ctx);
    assert(uds_download_begin(&dl, 0x08040000U, 0x1000U, 100U) == UDS_DOWNLOAD_INVALID_ARGUMENT);

    bad_cb = cb;
    bad_cb.erase_poll = NULL;
    uds_download_init(&dl, &map, &bad_cb, &ctx);
    assert(uds_download_begin(&dl, 0x08040000U, 0x1000U, 100U) == UDS_DOWNLOAD_INVALID_ARGUMENT);

    bad_cb = cb;
    bad_cb.program = NULL;
    uds_download_init(&dl, &map, &bad_cb, &ctx);
    assert(uds_download_begin(&dl, 0x08040000U, 0x1000U, 100U) == UDS_DOWNLOAD_INVALID_ARGUMENT);

    bad_cb = cb;
    bad_cb.verify_image = NULL;
    uds_download_init(&dl, &map, &bad_cb, &ctx);
    assert(uds_download_begin(&dl, 0x08040000U, 0x1000U, 100U) == UDS_DOWNLOAD_INVALID_ARGUMENT);

    /* Restore good callbacks */
    uds_download_init(&dl, &map, &cb, &ctx);

    /* Length == 0 */
    assert(uds_download_begin(&dl, 0x08040000U, 0U, 100U) == UDS_DOWNLOAD_OUT_OF_RANGE);

    /* Address overflow (wrap around 0xFFFFFFFF) */
    assert(uds_download_begin(&dl, 0xFFFFF000U, 0x2000U, 100U) == UDS_DOWNLOAD_OUT_OF_RANGE);

    /* Address before staging region */
    assert(uds_download_begin(&dl, 0x0803F800U, 0x1000U, 100U) == UDS_DOWNLOAD_OUT_OF_RANGE);

    /* Address + length past staging region */
    assert(uds_download_begin(&dl, 0x0807F800U, 0x1000U, 100U) == UDS_DOWNLOAD_OUT_OF_RANGE);

    /* Overlap with bootloader */
    map.staging_image.start = 0x08000000U;
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_begin(&dl, 0x08008000U, 0x1000U, 100U) == UDS_DOWNLOAD_OUT_OF_RANGE);

    /* Overlap with active application */
    setup_valid_map(&map);
    map.staging_image.start = 0x08010000U;
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_begin(&dl, 0x08020000U, 0x1000U, 100U) == UDS_DOWNLOAD_OUT_OF_RANGE);

    /* Overlap with persistent storage */
    setup_valid_map(&map);
    map.staging_image.end_exclusive = 0x08090000U;
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_begin(&dl, 0x08080000U, 0x1000U, 100U) == UDS_DOWNLOAD_OUT_OF_RANGE);

    /* Overlap with diagnostic storage */
    setup_valid_map(&map);
    map.staging_image.end_exclusive = 0x080A0000U;
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_begin(&dl, 0x08090000U, 0x1000U, 100U) == UDS_DOWNLOAD_OUT_OF_RANGE);
}

static void test_alignment_and_block_length_validation(void) {
    UdsDownload dl;
    UdsDownloadMemoryMap map;
    UdsDownloadCallbacks cb;
    TestContext ctx;

    setup_valid_map(&map);
    setup_valid_callbacks(&cb);
    memset(&ctx, 0, sizeof(ctx));

    /* Unaligned address relative to erase alignment */
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_begin(&dl, 0x08040100U, 0x1000U, 100U) == UDS_DOWNLOAD_ALIGNMENT_ERROR);

    /* Unaligned length relative to erase alignment */
    assert(uds_download_begin(&dl, 0x08040000U, 0x1100U, 100U) == UDS_DOWNLOAD_ALIGNMENT_ERROR);

    /* Address unaligned relative to program alignment */
    map.erase_alignment = 8U;
    map.program_alignment = 16U;
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_begin(&dl, 0x08040008U, 0x1000U, 100U) == UDS_DOWNLOAD_ALIGNMENT_ERROR);

    /* Invalid max block length (< 3 or > MAX_CHUNK_LENGTH) */
    setup_valid_map(&map);
    map.max_block_length = 2U;
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_begin(&dl, 0x08040000U, 0x1000U, 100U) == UDS_DOWNLOAD_INVALID_ARGUMENT);

    map.max_block_length = 300U;
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_begin(&dl, 0x08040000U, 0x1000U, 100U) == UDS_DOWNLOAD_INVALID_ARGUMENT);

    /* Erase start error */
    map.max_block_length = 256U;
    ctx.erase_start_rc = UDS_DOWNLOAD_ERASE_ERROR;
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_begin(&dl, 0x08040000U, 0x1000U, 100U) == UDS_DOWNLOAD_ERASE_ERROR);
    assert(uds_download_state(&dl) == UDS_DOWNLOAD_IDLE);
}

static void test_erase_polling_and_timeouts(void) {
    UdsDownload dl;
    UdsDownloadMemoryMap map;
    UdsDownloadCallbacks cb;
    TestContext ctx;

    setup_valid_map(&map);
    setup_valid_callbacks(&cb);
    memset(&ctx, 0, sizeof(ctx));
    uds_download_init(&dl, &map, &cb, &ctx);

    /* NULL checks */
    assert(uds_download_poll_erase(NULL, 100U) == UDS_DOWNLOAD_NOT_READY);

    /* Poll while in IDLE state */
    assert(uds_download_poll_erase(&dl, 100U) == UDS_DOWNLOAD_NOT_READY);

    /* Start erase */
    assert(uds_download_begin(&dl, 0x08040000U, 0x1000U, 1000U) == UDS_DOWNLOAD_OK);
    assert(uds_download_state(&dl) == UDS_DOWNLOAD_ERASING);

    /* Busy poll before timeout */
    ctx.erase_poll_rc = UDS_DOWNLOAD_BUSY;
    assert(uds_download_poll_erase(&dl, 2000U) == UDS_DOWNLOAD_BUSY);
    assert(uds_download_state(&dl) == UDS_DOWNLOAD_ERASING);
    assert(ctx.watchdog_calls > 0U);

    /* Erase timeout (deadline was 1000 + 5000 = 6000ms) */
    assert(uds_download_poll_erase(&dl, 6000U) == UDS_DOWNLOAD_TIMEOUT);
    assert(uds_download_state(&dl) == UDS_DOWNLOAD_ABORTED_STATE);
    assert(ctx.abort_calls == 1U);

    /* Successful erase completion */
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_begin(&dl, 0x08040000U, 0x1000U, 1000U) == UDS_DOWNLOAD_OK);
    ctx.erase_poll_rc = UDS_DOWNLOAD_OK;
    assert(uds_download_poll_erase(&dl, 2000U) == UDS_DOWNLOAD_OK);
    assert(uds_download_state(&dl) == UDS_DOWNLOAD_RECEIVING);
}

static void test_write_and_finish_flow(void) {
    UdsDownload dl;
    UdsDownloadMemoryMap map;
    UdsDownloadCallbacks cb;
    TestContext ctx;

    setup_valid_map(&map);
    setup_valid_callbacks(&cb);
    memset(&ctx, 0, sizeof(ctx));
    uds_download_init(&dl, &map, &cb, &ctx);

    const uint8_t chunk1[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18};
    const uint8_t chunk2[16] = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
                                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38};

    /* Start download with total size 32 bytes (must be aligned to erase sector for begin) */
    map.erase_alignment = 32U;
    map.program_alignment = 8U;
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_begin(&dl, 0x08040000U, 32U, 1000U) == UDS_DOWNLOAD_OK);
    ctx.erase_poll_rc = UDS_DOWNLOAD_OK;
    assert(uds_download_poll_erase(&dl, 1010U) == UDS_DOWNLOAD_OK);

    /* Write NULL checks and invalid arguments */
    assert(uds_download_write(NULL, 1U, chunk1, 16U, 1020U) == UDS_DOWNLOAD_NOT_READY);
    assert(uds_download_write(&dl, 1U, NULL, 16U, 1020U) == UDS_DOWNLOAD_NOT_READY);
    assert(uds_download_write(&dl, 1U, chunk1, 0U, 1020U) == UDS_DOWNLOAD_NOT_READY);
    assert(uds_download_write(&dl, 1U, chunk1, 260U, 1020U) == UDS_DOWNLOAD_NOT_READY);

    /* Sequence error: expected block 1, received block 2 */
    assert(uds_download_write(&dl, 2U, chunk1, 16U, 1020U) == UDS_DOWNLOAD_SEQUENCE_ERROR);

    /* Program alignment error: chunk length not a multiple of 8 */
    assert(uds_download_write(&dl, 1U, chunk1, 10U, 1020U) == UDS_DOWNLOAD_ALIGNMENT_ERROR);

    /* Program failure from hardware driver */
    ctx.program_rc = UDS_DOWNLOAD_PROGRAM_ERROR;
    assert(uds_download_write(&dl, 1U, chunk1, 16U, 1020U) == UDS_DOWNLOAD_PROGRAM_ERROR);
    ctx.program_rc = UDS_DOWNLOAD_OK;

    /* Write block 1 successfully */
    assert(uds_download_write(&dl, 1U, chunk1, 16U, 1020U) == UDS_DOWNLOAD_OK);
    assert(dl.metadata.received_length == 16U);
    assert(dl.expected_block == 2U);

    /* Overflow: write 24 bytes when only 16 remain */
    uint8_t large_chunk[24];
    memset(large_chunk, 0xAA, sizeof(large_chunk));
    assert(uds_download_write(&dl, 2U, large_chunk, 24U, 1030U) == UDS_DOWNLOAD_OVERFLOW);

    /* Write block 2 successfully */
    assert(uds_download_write(&dl, 2U, chunk2, 16U, 1040U) == UDS_DOWNLOAD_OK);
    assert(dl.metadata.received_length == 32U);

    /* Try to write again when already full */
    assert(uds_download_write(&dl, 3U, chunk1, 8U, 1050U) == UDS_DOWNLOAD_OVERFLOW);

    /* Finish NULL & state validation */
    assert(uds_download_finish(NULL, 0U, false, 1060U) == UDS_DOWNLOAD_NOT_READY);

    /* CRC verification mismatch */
    uint32_t real_crc = dl.crc32 ^ 0xFFFFFFFFUL;
    assert(uds_download_finish(&dl, real_crc + 1U, true, 1060U) == UDS_DOWNLOAD_VERIFY_ERROR);
    assert(uds_download_state(&dl) == UDS_DOWNLOAD_ABORTED_STATE);

    /* Re-run and finish with valid CRC */
    memset(&ctx, 0, sizeof(ctx));
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_begin(&dl, 0x08040000U, 32U, 1000U) == UDS_DOWNLOAD_OK);
    ctx.erase_poll_rc = UDS_DOWNLOAD_OK;
    assert(uds_download_poll_erase(&dl, 1010U) == UDS_DOWNLOAD_OK);
    assert(uds_download_write(&dl, 1U, chunk1, 16U, 1020U) == UDS_DOWNLOAD_OK);
    assert(uds_download_write(&dl, 2U, chunk2, 16U, 1030U) == UDS_DOWNLOAD_OK);

    real_crc = dl.crc32 ^ 0xFFFFFFFFUL;
    assert(uds_download_finish(&dl, real_crc, true, 1040U) == UDS_DOWNLOAD_OK);
    assert(uds_download_state(&dl) == UDS_DOWNLOAD_COMPLETE);
    assert(uds_download_activation_pending(&dl) == true); /* activation not supported -> pending */

    /* Verify with activation_supported == true */
    map.activation_supported = true;
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_begin(&dl, 0x08040000U, 32U, 1000U) == UDS_DOWNLOAD_OK);
    ctx.erase_poll_rc = UDS_DOWNLOAD_OK;
    assert(uds_download_poll_erase(&dl, 1010U) == UDS_DOWNLOAD_OK);
    assert(uds_download_write(&dl, 1U, chunk1, 16U, 1020U) == UDS_DOWNLOAD_OK);
    assert(uds_download_write(&dl, 2U, chunk2, 16U, 1030U) == UDS_DOWNLOAD_OK);
    assert(uds_download_finish(&dl, 0U, false, 1040U) == UDS_DOWNLOAD_OK);
    assert(uds_download_activation_pending(&dl) == false);

    /* Verify image callback failure */
    ctx.verify_rc = UDS_DOWNLOAD_VERIFY_ERROR;
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_begin(&dl, 0x08040000U, 32U, 1000U) == UDS_DOWNLOAD_OK);
    ctx.erase_poll_rc = UDS_DOWNLOAD_OK;
    assert(uds_download_poll_erase(&dl, 1010U) == UDS_DOWNLOAD_OK);
    assert(uds_download_write(&dl, 1U, chunk1, 16U, 1020U) == UDS_DOWNLOAD_OK);
    assert(uds_download_write(&dl, 2U, chunk2, 16U, 1030U) == UDS_DOWNLOAD_OK);
    assert(uds_download_finish(&dl, 0U, false, 1040U) == UDS_DOWNLOAD_VERIFY_ERROR);
    assert(uds_download_state(&dl) == UDS_DOWNLOAD_ABORTED_STATE);
}

static void test_tick_and_abort(void) {
    UdsDownload dl;
    UdsDownloadMemoryMap map;
    UdsDownloadCallbacks cb;
    TestContext ctx;

    setup_valid_map(&map);
    setup_valid_callbacks(&cb);
    memset(&ctx, 0, sizeof(ctx));

    /* NULL tick & abort */
    assert(uds_download_tick(NULL, 100U) == UDS_DOWNLOAD_OK);
    uds_download_abort(NULL);

    /* Tick when idle, complete, aborted */
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_tick(&dl, 100U) == UDS_DOWNLOAD_OK);
    dl.state = UDS_DOWNLOAD_COMPLETE;
    assert(uds_download_tick(&dl, 100U) == UDS_DOWNLOAD_OK);
    dl.state = UDS_DOWNLOAD_ABORTED_STATE;
    assert(uds_download_tick(&dl, 100U) == UDS_DOWNLOAD_OK);

    /* Tick before timeout kicks watchdog */
    setup_valid_map(&map);
    map.erase_alignment = 32U;
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_begin(&dl, 0x08040000U, 32U, 1000U) == UDS_DOWNLOAD_OK);
    ctx.watchdog_calls = 0U;
    assert(uds_download_tick(&dl, 3000U) == UDS_DOWNLOAD_OK);
    assert(ctx.watchdog_calls == 1U);

    /* Tick after timeout triggers abort */
    assert(uds_download_tick(&dl, 6000U) == UDS_DOWNLOAD_TIMEOUT);
    assert(uds_download_state(&dl) == UDS_DOWNLOAD_ABORTED_STATE);
    assert(ctx.abort_calls == 1U);

    /* Explicit abort */
    setup_valid_map(&map);
    map.erase_alignment = 32U;
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_begin(&dl, 0x08040000U, 32U, 1000U) == UDS_DOWNLOAD_OK);
    uds_download_abort(&dl);
    assert(uds_download_state(&dl) == UDS_DOWNLOAD_ABORTED_STATE);
    assert(uds_download_activation_pending(&dl) == false);

    /* Abort when callback is NULL */
    setup_valid_map(&map);
    map.erase_alignment = 32U;
    cb.abort = NULL;
    uds_download_init(&dl, &map, &cb, &ctx);
    assert(uds_download_begin(&dl, 0x08040000U, 32U, 1000U) == UDS_DOWNLOAD_OK);
    uds_download_abort(&dl);
    assert(uds_download_state(&dl) == UDS_DOWNLOAD_ABORTED_STATE);
}

int main(void) {
    test_init_null_and_defaults();
    test_region_validation_and_overlap();
    test_alignment_and_block_length_validation();
    test_erase_polling_and_timeouts();
    test_write_and_finish_flow();
    test_tick_and_abort();
    printf("UDS Download Service tests passed successfully.\n");
    return 0;
}
