#include "uds_bootloader.h"
#include "uds_dtc_app.h"
#include "uds_iso_tp/uds.h"

#include <assert.h>
#include <string.h>

static void test_dtc_app_all_subfunctions(void) {
    uds_dtc_app_init();
    const UdsDtcBackend *backend = uds_dtc_app_get_backend();
    assert(backend != NULL);
    assert(backend->report != NULL);

    /* Check capabilities cover all subfunctions */
    assert((backend->capabilities & UDS_DTC_CAP_REPORT_NUMBER_BY_STATUS) != 0U);
    assert((backend->capabilities & UDS_DTC_CAP_REPORT_BY_STATUS_MASK) != 0U);

    uint8_t response[256];
    uint16_t resp_len = 0U;

    /* 1. Subfunction 0x01: reportNumberOfDTCByStatusMask */
    uint8_t req_01[] = {0x19U, 0x01U, 0xFFU};
    assert(backend->report(NULL, 0x01U, req_01, sizeof(req_01), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len == 5U);
    assert(response[0] == 0x01U);
    assert(response[1] == 0xFFU); /* Availability mask */
    assert(response[2] == 0x01U); /* Format ISO 14229-1 */
    uint16_t count = (uint16_t)(((uint16_t)response[3] << 8U) | (uint16_t)response[4]);
    assert(count == 3U);

    /* 2. Subfunction 0x02: reportDTCByStatusMask */
    uint8_t req_02[] = {0x19U, 0x02U, 0xFFU};
    assert(backend->report(NULL, 0x02U, req_02, sizeof(req_02), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len == (2U + 3U * 4U)); /* 2-byte header + 3 DTCs * 4 bytes */
    assert(response[0] == 0x02U);

    /* 3. Subfunction 0x03: reportDTCSnapshotIdentification */
    uint8_t req_03[] = {0x19U, 0x03U};
    assert(backend->report(NULL, 0x03U, req_03, sizeof(req_03), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len >= 5U);
    assert(response[0] == 0x03U);

    /* 4. Subfunction 0x04: reportDTCSnapshotRecordByDTCNumber for P0100 (0x010000) */
    uint8_t req_04[] = {0x19U, 0x04U, 0x01U, 0x00U, 0x00U, 0x01U};
    assert(backend->report(NULL, 0x04U, req_04, sizeof(req_04), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(response[0] == 0x04U);
    assert(response[1] == 0x01U); /* DTC high */
    assert(response[2] == 0x00U);
    assert(response[3] == 0x00U);

    /* 5. Subfunction 0x06: reportDTCExtDataRecordByDTCNumber */
    uint8_t req_06[] = {0x19U, 0x06U, 0x01U, 0x00U, 0x00U, 0x01U};
    assert(backend->report(NULL, 0x06U, req_06, sizeof(req_06), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(response[0] == 0x06U);

    /* 6. Subfunction 0x14: reportDTCFaultDetectionCounter */
    uint8_t req_14[] = {0x19U, 0x14U};
    assert(backend->report(NULL, 0x14U, req_14, sizeof(req_14), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(response[0] == 0x14U);

    /* 7. Clear DTCs via service 0x14 */
    assert(uds_dtc_app_clear(NULL, 0xFFFFFFUL) == UDS_RESULT_OK);

    /* Verify count is now 0 */
    assert(backend->report(NULL, 0x01U, req_01, sizeof(req_01), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    uint16_t cleared_count = (uint16_t)(((uint16_t)response[3] << 8U) | (uint16_t)response[4]);
    assert(cleared_count == 0U);

    /* 8. Add a new fault dynamically at runtime */
    assert(uds_dtc_app_set_fault(0x020000UL, 0x24U, 0x20U, 15));
    assert(backend->report(NULL, 0x01U, req_01, sizeof(req_01), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    uint16_t updated_count = (uint16_t)(((uint16_t)response[3] << 8U) | (uint16_t)response[4]);
    assert(updated_count == 1U);
}

static void test_bootloader_flow(void) {
    uds_bootloader_init();
    assert(uds_bootloader_get_active_version() == 1U);
    assert(uds_bootloader_get_active_slot() == UDS_BL_APP_SLOT_A_START);
    assert(!uds_bootloader_is_activation_pending());

    UdsDownloadMemoryMap mmap = uds_bootloader_get_memory_map();
    assert(mmap.staging_image.start == UDS_BL_APP_SLOT_B_START);
    assert(mmap.active_application.start == UDS_BL_APP_SLOT_A_START);

    /* 1. RequestDownload (0x34) boundary checks */
    uint16_t max_block = 0U;
    /* Outside Slot B -> must fail */
    assert(uds_bootloader_request_download(NULL, 0x08000000UL, 1024U, &max_block) ==
           UDS_RESULT_OUT_OF_RANGE);
    assert(uds_bootloader_request_download(NULL, 0x08040000UL, 1024U, &max_block) ==
           UDS_RESULT_OUT_OF_RANGE);

    /* Valid target in Slot B -> must succeed */
    assert(uds_bootloader_request_download(NULL, UDS_BL_APP_SLOT_B_START, 1024U, &max_block) ==
           UDS_RESULT_OK);
    assert(max_block == 256U);

    /* 2. TransferData (0x36) */
    uint8_t chunk[64] = {0xAAU};
    assert(uds_bootloader_transfer_data(NULL, 1U, chunk, sizeof(chunk)) == UDS_RESULT_OK);

    /* Sequence error check */
    assert(uds_bootloader_transfer_data(NULL, 3U, chunk, sizeof(chunk)) ==
           UDS_RESULT_SEQUENCE_ERROR);

    /* 3. RequestTransferExit (0x37) */
    uint8_t exit_resp[4];
    uint16_t exit_resp_len = 0U;
    assert(uds_bootloader_transfer_exit(NULL, NULL, 0U, exit_resp, &exit_resp_len,
                                        sizeof(exit_resp)) == UDS_RESULT_OK);

    /* 4. RoutineControl 0x0202: CheckMemory & Anti-Rollback tests */
    FirmwareMetadata_t meta;
    (void)memset(&meta, 0, sizeof(meta));
    meta.magic = UDS_BL_METADATA_MAGIC;
    meta.version = 0U; /* Downgrade attempt! Active version is 1 */
    meta.image_size = sizeof(FirmwareMetadata_t);

    uint8_t routine_out[16];
    uint16_t routine_out_len = 0U;

    /* Anti-rollback downgrade must be rejected */
    assert(uds_bootloader_routine_control(
               NULL, 0x01U, UDS_BL_ROUTINE_CHECK_MEMORY, (const uint8_t *)&meta, sizeof(meta),
               routine_out, &routine_out_len, sizeof(routine_out)) == UDS_RESULT_OUT_OF_RANGE);
    assert(routine_out[0] == 0x02U); /* 0x02: Rejected downgrade */

    /* Valid version (version 2 >= active version 1) */
    meta.version = 2U;
    /* Compute correct SHA-256 for zero-payload */
    /* SHA-256 of empty string is e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 */
    const uint8_t empty_sha256[32] = {0xe3U, 0xb0U, 0xc4U, 0x42U, 0x98U, 0xfcU, 0x1cU, 0x14U,
                                      0x9aU, 0xfbU, 0xf4U, 0xc8U, 0x99U, 0x6fU, 0xb9U, 0x24U,
                                      0x27U, 0xaeU, 0x41U, 0xe4U, 0x64U, 0x9bU, 0x93U, 0x4cU,
                                      0xa4U, 0x95U, 0x99U, 0x1bU, 0x78U, 0x52U, 0xb8U, 0x55U};
    (void)memcpy(meta.sha256, empty_sha256, sizeof(empty_sha256));

    assert(uds_bootloader_routine_control(NULL, 0x01U, UDS_BL_ROUTINE_CHECK_MEMORY,
                                          (const uint8_t *)&meta, sizeof(meta), routine_out,
                                          &routine_out_len, sizeof(routine_out)) == UDS_RESULT_OK);
    assert(routine_out[0] == 0x00U); /* Verification Passed */
    assert(uds_bootloader_is_activation_pending());
}

int main(void) {
    test_dtc_app_all_subfunctions();
    test_bootloader_flow();
    return 0;
}
