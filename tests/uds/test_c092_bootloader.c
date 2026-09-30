#include "boot_test_util.h"
#include "uds_bootloader.h"

#include <assert.h>
#include <string.h>

static void test_c092_bootloader_memory_map_and_flow(void) {
    /* 1. Configure target to STM32C092RC (256 KB Flash, 128 x 2 KB pages, 24 KB SRAM) */
    uds_bootloader_set_target(UDS_BL_TARGET_STM32C092);
    assert(uds_bootloader_get_target() == UDS_BL_TARGET_STM32C092);
    assert(uds_bootloader_get_active_version() == 1U);
    assert(uds_bootloader_get_active_slot() == UDS_BL_C092_APP_SLOT_A_START);
    assert(!uds_bootloader_is_activation_pending());

    UdsDownloadMemoryMap mmap = uds_bootloader_get_memory_map();
    assert(mmap.bootloader.start == UDS_BL_C092_BOOTLOADER_START);
    assert(mmap.bootloader.end_exclusive ==
           (UDS_BL_C092_BOOTLOADER_START + UDS_BL_C092_BOOTLOADER_SIZE));
    assert(mmap.active_application.start == UDS_BL_C092_APP_SLOT_A_START);
    assert(mmap.active_application.end_exclusive ==
           (UDS_BL_C092_APP_SLOT_A_START + UDS_BL_C092_APP_SLOT_A_SIZE));
    assert(mmap.staging_image.start == UDS_BL_C092_APP_SLOT_B_START);
    assert(mmap.staging_image.end_exclusive ==
           (UDS_BL_C092_APP_SLOT_B_START + UDS_BL_C092_APP_SLOT_B_SIZE));
    assert(mmap.erase_alignment == 8U); /* 64-bit doubleword erase alignment for download blocks */
    assert(mmap.program_alignment == 8U); /* 64-bit doubleword program alignment */

    /* 2. RequestDownload (0x34) boundary checks */
    uint16_t max_block = 0U;

    /* Addresses outside Slot B (0x08025000 - 0x0803FFFF) must be rejected with OUT_OF_RANGE */
    assert(uds_bootloader_request_download(NULL, 0x08000000UL, 1024U, &max_block) ==
           UDS_RESULT_OUT_OF_RANGE);
    assert(uds_bootloader_request_download(NULL, UDS_BL_C092_APP_SLOT_A_START, 1024U, &max_block) ==
           UDS_RESULT_OUT_OF_RANGE);
    /* Beyond 256 KB Flash boundary */
    assert(uds_bootloader_request_download(NULL, 0x08040000UL, 1024U, &max_block) ==
           UDS_RESULT_OUT_OF_RANGE);

    /* Valid target in STM32C092 Slot B */
    uint8_t chunk[64];
    (void)memset(chunk, 0x55, sizeof(chunk));

    /* Sequence error check before download begins */
    assert(uds_bootloader_transfer_data(NULL, 1U, chunk, sizeof(chunk)) ==
           UDS_RESULT_SEQUENCE_ERROR);

    assert(uds_bootloader_request_download(NULL, UDS_BL_C092_APP_SLOT_B_START, sizeof(chunk),
                                           &max_block) == UDS_RESULT_OK);
    assert(max_block == 256U);

    /* 3. TransferData (0x36) sequence validation */
    assert(uds_bootloader_transfer_data(NULL, 2U, chunk, sizeof(chunk)) ==
           UDS_RESULT_SEQUENCE_ERROR);
    assert(uds_bootloader_transfer_data(NULL, 1U, chunk, sizeof(chunk)) == UDS_RESULT_OK);

    /* 4. RequestTransferExit (0x37) */
    uint8_t exit_resp[4];
    uint16_t exit_resp_len = 0U;
    assert(uds_bootloader_transfer_exit(NULL, NULL, 0U, exit_resp, &exit_resp_len,
                                        sizeof(exit_resp)) == UDS_RESULT_OK);
    assert(exit_resp_len == 1U);
    assert(exit_resp[0] == 0x00U);

    /* 5. RoutineControl 0xFF00: Erase Slot B */
    uint8_t routine_out[16];
    uint16_t routine_out_len = 0U;
    assert(uds_bootloader_routine_control(NULL, 0x01U, UDS_BL_ROUTINE_ERASE_MEMORY, NULL, 0U,
                                          routine_out, &routine_out_len,
                                          sizeof(routine_out)) == UDS_RESULT_OK);
    assert(routine_out[0] == 0x00U);

    /* 6. RoutineControl 0x0202: CheckMemory & Anti-Rollback tests */
    FirmwareMetadata_t meta;
    bt_stage_image(&meta, 0U); /* Downgrade attempt! Active version is 1 */

    /* Anti-rollback downgrade must be rejected */
    assert(uds_bootloader_routine_control(
               NULL, 0x01U, UDS_BL_ROUTINE_CHECK_MEMORY, (const uint8_t *)&meta, sizeof(meta),
               routine_out, &routine_out_len, sizeof(routine_out)) == UDS_RESULT_OUT_OF_RANGE);
    assert(routine_out[0] == 0x02U); /* 0x02: Rejected downgrade */

    /* Valid version 2 with a real payload and correct SHA-256 digest */
    bt_stage_image(&meta, 2U);

    /* Missing / invalid signature must be rejected with 0x04 / SECURITY_DENIED */
    assert(uds_bootloader_routine_control(
               NULL, 0x01U, UDS_BL_ROUTINE_CHECK_MEMORY, (const uint8_t *)&meta, sizeof(meta),
               routine_out, &routine_out_len, sizeof(routine_out)) == UDS_RESULT_SECURITY_DENIED);
    assert(routine_out[0] == 0x04U); /* 0x04: Signature verification failed */

    /* Compute valid cryptographic signature over the manifest */
    bt_sign(&meta);

    /* The signature covers the version: relabelling the same signed bytes must fail. */
    {
        FirmwareMetadata_t relabelled = meta;
        relabelled.version = 50U;
        boot_finalize_metadata(&relabelled);
        assert(uds_bootloader_routine_control(NULL, 0x01U, UDS_BL_ROUTINE_CHECK_MEMORY,
                                              (const uint8_t *)&relabelled, sizeof(relabelled),
                                              routine_out, &routine_out_len,
                                              sizeof(routine_out)) == UDS_RESULT_SECURITY_DENIED);
        assert(routine_out[0] == 0x04U);
        assert(!uds_bootloader_is_activation_pending());
    }

    assert(uds_bootloader_routine_control(NULL, 0x01U, UDS_BL_ROUTINE_CHECK_MEMORY,
                                          (const uint8_t *)&meta, sizeof(meta), routine_out,
                                          &routine_out_len, sizeof(routine_out)) == UDS_RESULT_OK);
    assert(routine_out[0] == 0x00U); /* Verification Passed */
    assert(uds_bootloader_is_activation_pending());
    assert(uds_bootloader_get_slot_status() == UDS_BL_SLOT_CANDIDATE);

    /* 7. RoutineControl 0xFF01: CheckProgrammingDependencies */
    /* 7a. Subfunction 0x01: startRoutine -> Dependencies check should succeed */
    assert(uds_bootloader_routine_control(NULL, UDS_ROUTINE_SUBFUNCTION_START_ROUTINE,
                                          UDS_BL_ROUTINE_CHECK_DEPENDENCIES, NULL, 0U, routine_out,
                                          &routine_out_len, sizeof(routine_out)) == UDS_RESULT_OK);
    assert(routine_out_len == 1U);
    assert(routine_out[0] == 0x00U); /* 0x00: Dependencies satisfied */

    /* 7b. Subfunction 0x03: requestRoutineResults -> Should return 0x00 */
    assert(uds_bootloader_routine_control(NULL, UDS_ROUTINE_SUBFUNCTION_REQUEST_RESULTS,
                                          UDS_BL_ROUTINE_CHECK_DEPENDENCIES, NULL, 0U, routine_out,
                                          &routine_out_len, sizeof(routine_out)) == UDS_RESULT_OK);
    assert(routine_out_len == 1U);
    assert(routine_out[0] == 0x00U);

    /* 8. Vector Table Sanity Validation Gate for STM32C092 (256 KB Flash: 0x08000000 - 0x08040000) */
    assert(!uds_bootloader_is_application_valid(0x00000000UL)); /* NULL address */
    assert(!uds_bootloader_is_application_valid(0x20000000UL)); /* RAM, not Flash */
    assert(!uds_bootloader_is_application_valid(0x08040000UL)); /* Flash boundary */
    assert(!uds_bootloader_is_application_valid(0x08050000UL)); /* Beyond 256 KB Flash */
    assert(!uds_bootloader_is_application_valid(
        0x0800A004UL)); /* Unaligned VTOR (not 256-byte aligned) */
}

static void test_c092_bootloader_flash_verify_and_erase_poll(void) {
    /* Test flash erase poll */
    assert(uds_bootloader_flash_erase_poll() == UDS_DOWNLOAD_OK);

    /* Test flash verify with invalid metadata */
    assert(uds_bootloader_flash_verify(NULL, 0U, false) == UDS_DOWNLOAD_INVALID_ARGUMENT);
    UdsDownloadMetadata empty_meta;
    (void)memset(&empty_meta, 0, sizeof(empty_meta));
    assert(uds_bootloader_flash_verify(&empty_meta, 0U, false) == UDS_DOWNLOAD_INVALID_ARGUMENT);

    /* Test flash verify with matching CRC */
    uint8_t payload[64];
    (void)memset(payload, 0xA5, sizeof(payload));

    uint16_t max_block = 0U;
    assert(uds_bootloader_request_download(NULL, UDS_BL_C092_APP_SLOT_B_START, sizeof(payload),
                                           &max_block) == UDS_RESULT_OK);
    assert(uds_bootloader_transfer_data(NULL, 1U, payload, sizeof(payload)) == UDS_RESULT_OK);

    /* Compute standard CRC32 over payload */
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < sizeof(payload); ++i) {
        crc ^= payload[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = ((crc & 1U) != 0U) ? ((crc >> 1U) ^ 0xEDB88320UL) : (crc >> 1U);
        }
    }
    crc ^= 0xFFFFFFFFUL;

    uint8_t req_crc[4] = {(uint8_t)(crc >> 24U), (uint8_t)(crc >> 16U), (uint8_t)(crc >> 8U),
                          (uint8_t)crc};
    uint8_t exit_resp[4];
    uint16_t exit_resp_len = 0U;
    assert(uds_bootloader_transfer_exit(NULL, req_crc, sizeof(req_crc), exit_resp, &exit_resp_len,
                                        sizeof(exit_resp)) == UDS_RESULT_OK);
    assert(exit_resp[0] == 0x00U);

    /* Direct API verify test */
    UdsDownloadMetadata test_meta;
    (void)memset(&test_meta, 0, sizeof(test_meta));
    test_meta.image_address = UDS_BL_C092_APP_SLOT_B_START;
    test_meta.image_length = sizeof(payload);
    test_meta.crc32 = crc ^ 0xFFFFFFFFUL;
    assert(uds_bootloader_flash_verify(&test_meta, crc, true) == UDS_DOWNLOAD_OK);

    /* Direct API verify with corrupt expected CRC */
    assert(uds_bootloader_flash_verify(&test_meta, crc ^ 0x12345678UL, true) ==
           UDS_DOWNLOAD_VERIFY_ERROR);
}

static void test_c092_activate_candidate(void) {
    uds_bootloader_set_target(UDS_BL_TARGET_STM32C092);
    assert(uds_bootloader_get_active_version() == 1U);
    assert(!uds_bootloader_is_activation_pending());

    /* 1. Attempt activation before verification must fail */
    assert(uds_bootloader_activate_candidate() == UDS_DOWNLOAD_SEQUENCE_ERROR);

    /* 2. Download and verify new candidate image (version 3) */
    FirmwareMetadata_t meta;
    bt_stage_image(&meta, 3U);
    bt_sign(&meta);

    uint8_t routine_out[16];
    uint16_t routine_out_len = 0U;
    assert(uds_bootloader_routine_control(NULL, 0x01U, UDS_BL_ROUTINE_CHECK_MEMORY,
                                          (const uint8_t *)&meta, sizeof(meta), routine_out,
                                          &routine_out_len, sizeof(routine_out)) == UDS_RESULT_OK);
    assert(routine_out[0] == 0x00U);
    assert(uds_bootloader_is_activation_pending());
    assert(uds_bootloader_get_slot_status() == UDS_BL_SLOT_CANDIDATE);

    /* 3. Execute Copy-on-Reset candidate activation from Slot B to Slot A */
    assert(uds_bootloader_activate_candidate() == UDS_DOWNLOAD_OK);
    assert(uds_bootloader_get_active_version() == 3U);
    assert(!uds_bootloader_is_activation_pending());
    assert(uds_bootloader_get_slot_status() == UDS_BL_SLOT_ACTIVE);

    /* Confirm active image in persistent journal */
    assert(uds_bootloader_confirm_active_image() == UDS_DOWNLOAD_OK);
    assert(uds_bootloader_get_slot_status() == UDS_BL_SLOT_CONFIRMED);

    /* 4. Subsequent activation without new candidate must fail */
    assert(uds_bootloader_activate_candidate() == UDS_DOWNLOAD_SEQUENCE_ERROR);
}

static void test_c092_bootloader_crc32_mode(void) {
    uds_bootloader_init();
    uds_bootloader_set_target(UDS_BL_TARGET_STM32C092);
    uds_bootloader_set_verification_mode(UDS_BL_VERIFY_MODE_CRC32);
    assert(uds_bootloader_get_verification_mode() == UDS_BL_VERIFY_MODE_CRC32);

    /* Erase Slot B */
    uint8_t routine_out[16];
    uint16_t routine_out_len = 0U;
    assert(uds_bootloader_routine_control(NULL, 0x01U, UDS_BL_ROUTINE_ERASE_MEMORY, NULL, 0U,
                                          routine_out, &routine_out_len,
                                          sizeof(routine_out)) == UDS_RESULT_OK);

    /* CRC32 mode (integrator-selected): the CRC must match a real payload. */
    FirmwareMetadata_t meta;
    bt_stage_image(&meta, 2U);

    /* A zero CRC is NOT a wildcard any more. */
    FirmwareMetadata_t zero_crc = meta;
    zero_crc.crc32 = 0U;
    boot_finalize_metadata(&zero_crc);
    assert(uds_bootloader_routine_control(NULL, 0x01U, UDS_BL_ROUTINE_CHECK_MEMORY,
                                          (const uint8_t *)&zero_crc, sizeof(zero_crc), routine_out,
                                          &routine_out_len,
                                          sizeof(routine_out)) == UDS_RESULT_ERROR);
    assert(!uds_bootloader_is_activation_pending());

    /* A wrong CRC is rejected too. */
    FirmwareMetadata_t bad_crc = meta;
    bad_crc.crc32 ^= 0x1U;
    boot_finalize_metadata(&bad_crc);
    assert(uds_bootloader_routine_control(
               NULL, 0x01U, UDS_BL_ROUTINE_CHECK_MEMORY, (const uint8_t *)&bad_crc, sizeof(bad_crc),
               routine_out, &routine_out_len, sizeof(routine_out)) == UDS_RESULT_ERROR);

    assert(uds_bootloader_routine_control(NULL, 0x01U, UDS_BL_ROUTINE_CHECK_MEMORY,
                                          (const uint8_t *)&meta, sizeof(meta), routine_out,
                                          &routine_out_len, sizeof(routine_out)) == UDS_RESULT_OK);
    assert(routine_out[0] == 0x00U);
    assert(uds_bootloader_is_activation_pending());
    assert(uds_bootloader_get_slot_status() == UDS_BL_SLOT_CANDIDATE);
}

/* Regression: a tester must not be able to weaken the policy through the request itself. */
static void test_c092_policy_cannot_be_downgraded_by_request(void) {
    uds_bootloader_init();
    uds_bootloader_set_target(UDS_BL_TARGET_STM32C092);
    assert(uds_bootloader_get_verification_mode() == UDS_BL_VERIFY_MODE_SHA256_SECURE);

    uint8_t out[16];
    uint16_t out_len = 0U;

    /* 4-byte "CRC only" form with an all-zero CRC and NO image at all. */
    const uint8_t zero_crc[4] = {0U, 0U, 0U, 0U};
    assert(uds_bootloader_routine_control(NULL, 0x01U, UDS_BL_ROUTINE_CHECK_MEMORY, zero_crc,
                                          sizeof(zero_crc), out, &out_len,
                                          sizeof(out)) != UDS_RESULT_OK);
    assert(!uds_bootloader_is_activation_pending());
    assert(uds_bootloader_get_verification_mode() == UDS_BL_VERIFY_MODE_SHA256_SECURE);

    /* Empty image (header only) with a "valid" hash-of-nothing and a signature is refused. */
    FirmwareMetadata_t meta;
    (void)memset(&meta, 0, sizeof(meta));
    meta.magic = UDS_BL_METADATA_MAGIC;
    meta.version = 0xFFFFFFF0U;
    meta.image_size = (uint32_t)sizeof(FirmwareMetadata_t);
    sha256_hash((const uint8_t *)"", 0U, meta.sha256);
    boot_finalize_metadata(&meta);
    bt_sign(&meta);
    assert(uds_bootloader_routine_control(NULL, 0x01U, UDS_BL_ROUTINE_CHECK_MEMORY,
                                          (const uint8_t *)&meta, sizeof(meta), out, &out_len,
                                          sizeof(out)) == UDS_RESULT_ERROR);
    assert(out[0] == 0x03U);
    assert(!uds_bootloader_is_activation_pending());

    /* A legacy header (no format version / header CRC) is refused. */
    FirmwareMetadata_t legacy;
    bt_stage_image(&legacy, 2U);
    legacy.format_version = 0U;
    legacy.header_crc32 = 0U;
    assert(uds_bootloader_routine_control(NULL, 0x01U, UDS_BL_ROUTINE_CHECK_MEMORY,
                                          (const uint8_t *)&legacy, sizeof(legacy), out, &out_len,
                                          sizeof(out)) == UDS_RESULT_OUT_OF_RANGE);
    assert(out[0] == 0x01U);
}

int main(void) {
    test_c092_policy_cannot_be_downgraded_by_request();
    test_c092_bootloader_memory_map_and_flow();
    test_c092_bootloader_flash_verify_and_erase_poll();
    test_c092_activate_candidate();
    test_c092_bootloader_crc32_mode();
    return 0;
}
