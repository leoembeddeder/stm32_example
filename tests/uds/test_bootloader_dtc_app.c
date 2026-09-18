#include "uds_bootloader.h"
#include "uds_dtc_app.h"
#include "uds_iso_tp/uds.h"
#include "uds_iso_tp/uds_wear_leveling.h"

#include <assert.h>
#include <string.h>

#define MOCK_FLASH_SECTOR_SIZE 2048U
#define MOCK_FLASH_SECTOR_COUNT 4U
static uint8_t s_mock_flash_storage[MOCK_FLASH_SECTOR_SIZE * MOCK_FLASH_SECTOR_COUNT];

static int mock_flash_erase(uint32_t addr) {
    uint32_t offset = (addr >= 0x08000000U) ? (addr - 0x08000000U) : addr;
    uint32_t sec = offset / MOCK_FLASH_SECTOR_SIZE;
    if (sec >= MOCK_FLASH_SECTOR_COUNT) {
        return UDS_PARAM_ERR;
    }
    (void)memset(&s_mock_flash_storage[sec * MOCK_FLASH_SECTOR_SIZE], 0xFF, MOCK_FLASH_SECTOR_SIZE);
    return UDS_PARAM_OK;
}

static int mock_flash_read(uint32_t addr, void *buf, size_t len) {
    if (buf == NULL) {
        return UDS_PARAM_INVALID_PARAM;
    }
    uint32_t offset = (addr >= 0x08000000U) ? (addr - 0x08000000U) : addr;
    if ((offset + len) > sizeof(s_mock_flash_storage)) {
        return UDS_PARAM_ERR;
    }
    (void)memcpy(buf, &s_mock_flash_storage[offset], len);
    return UDS_PARAM_OK;
}

static int mock_flash_write(uint32_t addr, const void *buf, size_t len) {
    if (buf == NULL) {
        return UDS_PARAM_INVALID_PARAM;
    }
    uint32_t offset = (addr >= 0x08000000U) ? (addr - 0x08000000U) : addr;
    if ((offset + len) > sizeof(s_mock_flash_storage)) {
        return UDS_PARAM_ERR;
    }
    (void)memcpy(&s_mock_flash_storage[offset], buf, len);
    return UDS_PARAM_OK;
}

static uint32_t mock_flash_sector_size(uint32_t addr) {
    (void)addr;
    return MOCK_FLASH_SECTOR_SIZE;
}

static const UdsFlashPort s_mock_flash_port = {
    .erase = mock_flash_erase,
    .read = mock_flash_read,
    .write = mock_flash_write,
    .sector_size = mock_flash_sector_size,
};

static void test_dtc_app_all_subfunctions(void) {
    uds_dtc_app_init();
    const UdsDtcBackend *backend = uds_dtc_app_get_backend();
    assert(backend != NULL);
    assert(backend->report != NULL);

    /* Check capabilities cover all subfunctions */
    assert((backend->capabilities & UDS_DTC_CAP_REPORT_NUMBER_BY_STATUS) != 0U);
    assert((backend->capabilities & UDS_DTC_CAP_REPORT_BY_STATUS_MASK) != 0U);
    assert((backend->capabilities & UDS_DTC_CAP_REPORT_SUPPORTED_DTC) != 0U);

    uint8_t response[512];
    uint16_t resp_len = 0U;

    /* 0. Subfunction 0x0A: reportSupportedDTC (standard ISO 14229-1 2-byte request) */
    uint8_t req_0a[] = {0x19U, 0x0AU};
    assert(backend->report(NULL, 0x0AU, req_0a, sizeof(req_0a), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    /* 2-byte header (subfunction + status availability mask) + 69 DTCs * 4 bytes = 278 bytes */
    assert(resp_len == (2U + 69U * 4U));
    assert(response[0] == 0x0AU);
    assert(response[1] == 0xFFU); /* Availability mask */
    /* Verify first OEM DTC: U300614 (0xF00614) */
    assert(response[2] == 0xF0U && response[3] == 0x06U && response[4] == 0x14U);

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
    assert(resp_len == (2U + 3U * 4U)); /* 2-byte header + 3 active DTCs * 4 bytes */
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

    /* 4b. Subfunction 0x04 with 6 bytes and record 0x00 on OEM DTC C100616 (0xD00616) */
    assert(uds_dtc_app_set_fault(0xD00616UL, 0x08U, 0x80U, 10));
    uint8_t req_04_oem[] = {0x19U, 0x04U, 0xD0U, 0x06U, 0x16U, 0x00U};
    assert(backend->report(NULL, 0x04U, req_04_oem, sizeof(req_04_oem), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(response[0] == 0x04U);
    assert(response[1] == 0xD0U);
    assert(response[2] == 0x06U);
    assert(response[3] == 0x16U);
    assert(response[5] == 0x00U); /* Echoes record 0x00 */

    /* 5. Subfunction 0x05: reportDTCSnapshotRecordByRecordNumber */
    uint8_t req_05[] = {0x19U, 0x05U, 0x01U};
    assert(backend->report(NULL, 0x05U, req_05, sizeof(req_05), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(response[0] == 0x05U);

    /* 6. Subfunction 0x06: reportDTCExtDataRecordByDTCNumber */
    uint8_t req_06[] = {0x19U, 0x06U, 0x01U, 0x00U, 0x00U, 0x01U};
    assert(backend->report(NULL, 0x06U, req_06, sizeof(req_06), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(response[0] == 0x06U);

    /* 6b. Subfunction 0x16: reportDTCExtDataRecordByRecordNumber (3 bytes: 19 16 01) */
    uint8_t req_16[] = {0x19U, 0x16U, 0x01U};
    assert(backend->report(NULL, 0x16U, req_16, sizeof(req_16), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(response[0] == 0x16U);
    assert(response[1] == 0x01U); /* Echoes record number */

    /* 6c. Subfunction 0x17: reportUserDefMemoryDTCByStatusMask (4 bytes: 19 17 FF 01) */
    uint8_t req_17[] = {0x19U, 0x17U, 0xFFU, 0x01U};
    assert(backend->report(NULL, 0x17U, req_17, sizeof(req_17), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(response[0] == 0x17U);
    assert(response[1] == 0x01U); /* Echoes memory selection */

    /* 6d. Subfunction 0x42: reportDTCBySeverityMaskRecord (5 bytes: 19 42 00 FF FF) */
    uint8_t req_42[] = {0x19U, 0x42U, 0x00U, 0xFFU, 0xFFU};
    assert(backend->report(NULL, 0x42U, req_42, sizeof(req_42), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(response[0] == 0x42U);
    assert(response[1] == 0x00U); /* Echoes functional group */

    /* 6e. Subfunction 0x55: reportWWHOBDDTCByMaskRecord (3 bytes: 19 55 00) */
    uint8_t req_55[] = {0x19U, 0x55U, 0x00U};
    assert(backend->report(NULL, 0x55U, req_55, sizeof(req_55), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(response[0] == 0x55U);
    assert(response[1] == 0x00U); /* Echoes functional group */

    /* 7. Subfunction 0x14: reportDTCFaultDetectionCounter */
    uint8_t req_14[] = {0x19U, 0x14U};
    assert(backend->report(NULL, 0x14U, req_14, sizeof(req_14), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(response[0] == 0x14U);

    /* 8. Clear DTCs via service 0x14 - verify AUTOSAR Dem status byte 0x50 */
    assert(uds_dtc_app_clear(NULL, 0xFFFFFFUL) == UDS_RESULT_OK);

    /* Verify count for active faults is now 0 */
    assert(backend->report(NULL, 0x01U, req_01, sizeof(req_01), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    uint16_t cleared_count = (uint16_t)(((uint16_t)response[3] << 8U) | (uint16_t)response[4]);
    assert(cleared_count == 0U);

    /* 9. AUTOSAR Dem Debouncing: reportEvent increments fault counter */
    /* Initially fault counter is 0; report 8 consecutive failures to cross +127 threshold */
    for (uint8_t f = 0U; f < 8U; f++) {
        assert(uds_dtc_app_report_event(0x010000UL, true));
    }
    /* Event is now qualified & confirmed */
    assert(backend->report(NULL, 0x01U, req_01, sizeof(req_01), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    uint16_t debounced_count = (uint16_t)(((uint16_t)response[3] << 8U) | (uint16_t)response[4]);
    assert(debounced_count == 1U);

    /* 10. Add a new fault dynamically at runtime */
    assert(uds_dtc_app_set_fault(0x020000UL, 0x24U, 0x20U, 15));
    assert(backend->report(NULL, 0x01U, req_01, sizeof(req_01), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    uint16_t updated_count = (uint16_t)(((uint16_t)response[3] << 8U) | (uint16_t)response[4]);
    assert(updated_count == 2U);
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

    /* 2. TransferData (0x36) and RequestTransferExit (0x37) */
    uint8_t chunk[64] = {0xAAU};

    /* Sequence error check before download starts */
    assert(uds_bootloader_transfer_data(NULL, 1U, chunk, sizeof(chunk)) ==
           UDS_RESULT_SEQUENCE_ERROR);

    /* Valid target in Slot B -> must succeed */
    assert(uds_bootloader_request_download(NULL, UDS_BL_APP_SLOT_B_START, sizeof(chunk),
                                           &max_block) == UDS_RESULT_OK);
    assert(max_block == 256U);

    /* Sequence error check (expected block is 1, block 2 must fail) */
    assert(uds_bootloader_transfer_data(NULL, 2U, chunk, sizeof(chunk)) ==
           UDS_RESULT_SEQUENCE_ERROR);

    /* 2. TransferData (0x36) */
    assert(uds_bootloader_transfer_data(NULL, 1U, chunk, sizeof(chunk)) == UDS_RESULT_OK);

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

    /* Missing / invalid signature must be rejected with 0x04 / SECURITY_DENIED */
    assert(uds_bootloader_routine_control(
               NULL, 0x01U, UDS_BL_ROUTINE_CHECK_MEMORY, (const uint8_t *)&meta, sizeof(meta),
               routine_out, &routine_out_len, sizeof(routine_out)) == UDS_RESULT_SECURITY_DENIED);
    assert(routine_out[0] == 0x04U); /* 0x04: Signature verification failed */

    /* Compute valid cryptographic signature */
    uds_bootloader_calculate_manifest_signature(meta.sha256, meta.signature);

    assert(uds_bootloader_routine_control(NULL, 0x01U, UDS_BL_ROUTINE_CHECK_MEMORY,
                                          (const uint8_t *)&meta, sizeof(meta), routine_out,
                                          &routine_out_len, sizeof(routine_out)) == UDS_RESULT_OK);
    assert(routine_out[0] == 0x00U); /* Verification Passed */
    assert(uds_bootloader_is_activation_pending());
    assert(uds_bootloader_get_slot_status() == UDS_BL_SLOT_CANDIDATE);

    /* 5. RoutineControl Subfunction 0x03: requestRoutineResults */
    assert(uds_bootloader_routine_control(NULL, UDS_ROUTINE_SUBFUNCTION_REQUEST_RESULTS,
                                          UDS_BL_ROUTINE_CHECK_MEMORY, NULL, 0U, routine_out,
                                          &routine_out_len, sizeof(routine_out)) == UDS_RESULT_OK);
    assert(routine_out[0] == 0x00U);

    /* 6. RoutineControl 0xFF01: CheckProgrammingDependencies */
    assert(uds_bootloader_routine_control(NULL, UDS_ROUTINE_SUBFUNCTION_START_ROUTINE,
                                          UDS_BL_ROUTINE_CHECK_DEPENDENCIES, NULL, 0U, routine_out,
                                          &routine_out_len, sizeof(routine_out)) == UDS_RESULT_OK);
    assert(routine_out_len == 1U);
    assert(routine_out[0] == 0x00U);

    assert(uds_bootloader_routine_control(NULL, UDS_ROUTINE_SUBFUNCTION_REQUEST_RESULTS,
                                          UDS_BL_ROUTINE_CHECK_DEPENDENCIES, NULL, 0U, routine_out,
                                          &routine_out_len, sizeof(routine_out)) == UDS_RESULT_OK);
    assert(routine_out_len == 1U);
    assert(routine_out[0] == 0x00U);

    /* 7. Slot activation, confirmation, and rollback state journal */
    assert(uds_bootloader_activate_candidate() == UDS_DOWNLOAD_OK);
    assert(uds_bootloader_get_slot_status() == UDS_BL_SLOT_ACTIVE);
    assert(uds_bootloader_confirm_active_image() == UDS_DOWNLOAD_OK);
    assert(uds_bootloader_get_slot_status() == UDS_BL_SLOT_CONFIRMED);
    assert(uds_bootloader_rollback_candidate() == UDS_DOWNLOAD_OK);
    assert(uds_bootloader_get_slot_status() == UDS_BL_SLOT_ROLLBACK);

    /* 8. Vector Table Sanity Validation Gate (S32K144 / OpenBLT specification) */
    assert(!uds_bootloader_is_application_valid(0x00000000UL)); /* NULL address */
    assert(!uds_bootloader_is_application_valid(0x20000000UL)); /* RAM, not Flash */
    assert(!uds_bootloader_is_application_valid(0x08300000UL)); /* Beyond Flash size */
}

static void test_iso14229_19_04_conformance(void) {
    uds_dtc_app_init();
    const UdsDtcBackend *backend = uds_dtc_app_get_backend();
    uint8_t response[256];
    uint16_t resp_len = 0U;

    /* 1. Issue #51 Conformance: 19 04 on supported DTCs that are NOT faulted (status 0x50 / cleared) */
    /* Must return positive response with 5 bytes [0x04, DTC_H, DTC_M, DTC_L, status] */
    /* MUST NOT return NRC 0x31 (UDS_RESULT_OUT_OF_RANGE) */
    uint8_t req_16[] = {0x19U, 0x04U, 0xD0U, 0x06U, 0x16U, 0x00U};
    assert(backend->report(NULL, 0x04U, req_16, sizeof(req_16), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len == 5U);
    assert(response[0] == 0x04U);
    assert(response[1] == 0xD0U && response[2] == 0x06U && response[3] == 0x16U);
    assert(response[4] == UDS_DTC_STATUS_CLEARED);

    uint8_t req_14[] = {0x19U, 0x04U, 0xF0U, 0x06U, 0x14U, 0x00U};
    assert(backend->report(NULL, 0x04U, req_14, sizeof(req_14), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len == 5U);
    assert(response[0] == 0x04U);
    assert(response[1] == 0xF0U && response[2] == 0x06U && response[3] == 0x14U);

    uint8_t req_15[] = {0x19U, 0x04U, 0xF0U, 0x06U, 0x15U, 0x00U};
    assert(backend->report(NULL, 0x04U, req_15, sizeof(req_15), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len == 5U);

    /* 2. 19 04 on an unsupported DTC -> must return UDS_RESULT_OUT_OF_RANGE (NRC 0x31) */
    uint8_t req_unsupported[] = {0x19U, 0x04U, 0xFFU, 0x00U, 0x11U, 0x00U};
    assert(backend->report(NULL, 0x04U, req_unsupported, sizeof(req_unsupported), response,
                           &resp_len, sizeof(response)) == UDS_RESULT_OUT_OF_RANGE);

    /* 3. 19 06 on supported unfaulted DTC -> positive response 5 bytes, no NRC 0x31 */
    uint8_t req_06_unfaulted[] = {0x19U, 0x06U, 0xD0U, 0x06U, 0x16U, 0x01U};
    assert(backend->report(NULL, 0x06U, req_06_unfaulted, sizeof(req_06_unfaulted), response,
                           &resp_len, sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len == 5U);

    /* 4. 19 09 on supported unfaulted DTC -> returns severity & functional unit */
    uint8_t req_09_unfaulted[] = {0x19U, 0x09U, 0xD0U, 0x06U, 0x16U};
    assert(backend->report(NULL, 0x09U, req_09_unfaulted, sizeof(req_09_unfaulted), response,
                           &resp_len, sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len >= 6U);

    /* 5. Set fault on 0xD00616 and verify snapshot reporting with record 0x00 */
    assert(uds_dtc_app_set_fault(0xD00616UL, 0x08U, 0x80U, 10));
    assert(backend->report(NULL, 0x04U, req_16, sizeof(req_16), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len > 5U);
    assert(response[0] == 0x04U);
    assert(response[5] == 0x00U); /* Echoes record 0x00 */
}

static void test_control_dtc_setting_service(void) {
    uds_dtc_app_init();
    assert(uds_dtc_app_is_setting_enabled());

    /* 1. Subfunction 0x02: off */
    assert(uds_dtc_app_control_setting(NULL, 0x02U) == UDS_RESULT_OK);
    assert(!uds_dtc_app_is_setting_enabled());

    /* While disabled, set_fault should not update DTC state */
    assert(uds_dtc_app_set_fault(0xD00617UL, 0x01U, 0x80U, 50));
    const UdsDtcBackend *backend = uds_dtc_app_get_backend();
    uint8_t response[256];
    uint16_t resp_len = 0U;
    uint8_t req_04[] = {0x19U, 0x04U, 0xD0U, 0x06U, 0x17U, 0x00U};
    assert(backend->report(NULL, 0x04U, req_04, sizeof(req_04), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    /* Should remain 5 bytes (no snapshot) because updates were suppressed */
    assert(resp_len == 5U);

    /* While disabled, report_event should not update DTC state */
    for (int i = 0; i < 10; ++i) {
        assert(uds_dtc_app_report_event(0xD00617UL, true));
    }
    assert(backend->report(NULL, 0x04U, req_04, sizeof(req_04), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len == 5U);

    /* 2. Subfunction 0x01: on */
    assert(uds_dtc_app_control_setting(NULL, 0x01U) == UDS_RESULT_OK);
    assert(uds_dtc_app_is_setting_enabled());

    /* Now set_fault works */
    assert(uds_dtc_app_set_fault(0xD00617UL, 0x09U, 0x80U, 50));
    assert(backend->report(NULL, 0x04U, req_04, sizeof(req_04), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len > 5U); /* Snapshot present */

    /* 3. Unsupported subfunction */
    assert(uds_dtc_app_control_setting(NULL, 0x03U) == UDS_RESULT_SUBFUNCTION_NOT_SUPPORTED);
}

static void test_dtc_wear_leveling_nvm(void) {
    (void)memset(s_mock_flash_storage, 0xFF, sizeof(s_mock_flash_storage));

    UdsParamStore store;
    assert(uds_param_init(&store, &s_mock_flash_port, 0x08000000U, MOCK_FLASH_SECTOR_COUNT,
                          (uint16_t)sizeof(UdsDtcNvBlock)) == UDS_PARAM_OK);

    uds_dtc_app_init();
    uds_dtc_app_attach_nvm(&store);

    /* Fault a DTC and attach custom snapshot payload */
    uint8_t snap[4] = {0x11U, 0x22U, 0x33U, 0x44U};
    assert(uds_dtc_app_set_fault(0xD00618UL, 0x29U, 0x80U, 120));
    assert(uds_dtc_app_set_snapshot(0xD00618UL, 0x01U, snap, sizeof(snap)));
    assert(uds_dtc_app_save_to_nvm());

    /* Simulate power cycle / reboot: wipe RAM */
    uds_dtc_app_init();

    /* Re-attach store: auto-loads from flash */
    uds_dtc_app_attach_nvm(&store);

    /* Verify 0xD00618 is restored with fault status and snapshot */
    const UdsDtcBackend *backend = uds_dtc_app_get_backend();
    uint8_t response[256];
    uint16_t resp_len = 0U;
    uint8_t req_04[] = {0x19U, 0x04U, 0xD0U, 0x06U, 0x18U, 0x01U};
    assert(backend->report(NULL, 0x04U, req_04, sizeof(req_04), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len > 5U);
    assert(response[0] == 0x04U);
    assert(response[1] == 0xD0U && response[2] == 0x06U && response[3] == 0x18U);
    assert(response[4] == 0x29U); /* Restored status */
    assert(response[5] == 0x01U); /* Snapshot record 1 */
    /* Check snapshot payload bytes */
    assert(response[9] == 0x11U && response[10] == 0x22U && response[11] == 0x33U &&
           response[12] == 0x44U);
}

static void test_extended_data_and_snapshot_structures(void) {
    uds_dtc_app_init();
    const UdsDtcBackend *backend = uds_dtc_app_get_backend();
    assert(backend != NULL);
    uint8_t response[512];
    uint16_t resp_len = 0U;

    /* 1. Test Issue #58: Global Snapshot Format & Getters/Setters */
    OBD_Global_Snapshot_Format snap_in = {
        .voltage = 135U,            /* 13.5 V */
        .global_power_mode = 0x02U, /* ACC */
        .st_global_snapshot_datatime =
            {
                .second = 30U,
                .minute = 45U,
                .hour = 14U,
                .day = 15U,
                .month = 8U,
                .year = 24U,
            },
    };
    assert(uds_dtc_app_set_global_snapshot(0xD00617UL, &snap_in));

    OBD_Global_Snapshot_Format snap_out;
    (void)memset(&snap_out, 0, sizeof(snap_out));
    assert(uds_dtc_app_get_global_snapshot(0xD00617UL, &snap_out));
    assert(snap_out.voltage == 135U);
    assert(snap_out.global_power_mode == 0x02U);
    assert(snap_out.st_global_snapshot_datatime.second == 30U);
    assert(snap_out.st_global_snapshot_datatime.minute == 45U);
    assert(snap_out.st_global_snapshot_datatime.hour == 14U);
    assert(snap_out.st_global_snapshot_datatime.day == 15U);
    assert(snap_out.st_global_snapshot_datatime.month == 8U);
    assert(snap_out.st_global_snapshot_datatime.year == 24U);

    /* 2. Test Issue #58: Extended Data Format & Getters/Setters */
    OBD_Extended_Data_Format ext_in = {
        .fault_occur_counter = 7U,
        .fault_pending_counter = 3U,
        .aged_counter = 1U,
        .ageing_counter = 12U,
    };
    assert(uds_dtc_app_set_extended_data(0xD00617UL, &ext_in));

    OBD_Extended_Data_Format ext_out;
    (void)memset(&ext_out, 0, sizeof(ext_out));
    assert(uds_dtc_app_get_extended_data(0xD00617UL, &ext_out));
    assert(ext_out.fault_occur_counter == 7U);
    assert(ext_out.fault_pending_counter == 3U);
    assert(ext_out.aged_counter == 1U);
    assert(ext_out.ageing_counter == 12U);

    /* 3. Test Issue #55: 19 04 and 19 06 return distinct responses for active DTC */
    /* Activate fault on 0xD00617 so it has records */
    assert(uds_dtc_app_set_fault(0xD00617UL, 0x2FU, 0x80U, 50));
    /* Re-apply custom snapshot and extended data */
    assert(uds_dtc_app_set_global_snapshot(0xD00617UL, &snap_in));
    assert(uds_dtc_app_set_extended_data(0xD00617UL, &ext_in));

    /* Query 19 04: Snapshot record 0x01 */
    uint8_t req_04[] = {0x19U, 0x04U, 0xD0U, 0x06U, 0x17U, 0x01U};
    assert(backend->report(NULL, 0x04U, req_04, sizeof(req_04), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    /* Response: 1 (subfunction) + 3 (DTC) + 1 (status) + 1 (record 0x01) + 1 (1 DID) + 2 (DID 0x0100) + 8 (data) = 17 bytes */
    assert(resp_len == 17U);
    assert(response[0] == 0x04U);
    assert(response[1] == 0xD0U && response[2] == 0x06U && response[3] == 0x17U);
    assert(response[5] == 0x01U);                         /* Snapshot record 1 */
    assert(response[6] == 0x01U);                         /* 1 DID */
    assert(response[7] == 0x01U && response[8] == 0x00U); /* DID 0x0100 */
    assert(response[9] == 135U);                          /* Voltage 13.5V */
    assert(response[10] == 0x02U);                        /* Power mode ACC */
    assert(response[11] == 30U && response[12] == 45U && response[13] == 14U);

    /* Query 19 06: Record 0x01 (occurrences) */
    uint8_t req_06_r1[] = {0x19U, 0x06U, 0xD0U, 0x06U, 0x17U, 0x01U};
    assert(backend->report(NULL, 0x06U, req_06_r1, sizeof(req_06_r1), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    /* Response: 1 (subfunction) + 3 (DTC) + 1 (status) + 1 (record 0x01) + 1 (counter) = 7 bytes */
    assert(resp_len == 7U);
    assert(response[0] == 0x06U);
    assert(response[1] == 0xD0U && response[2] == 0x06U && response[3] == 0x17U);
    assert(response[5] == 0x01U); /* Record 0x01 */
    assert(response[6] == 7U);    /* Occurrence counter = 7 */

    /* Query 19 06: Record 0x02 (pending counter) */
    uint8_t req_06_r2[] = {0x19U, 0x06U, 0xD0U, 0x06U, 0x17U, 0x02U};
    assert(backend->report(NULL, 0x06U, req_06_r2, sizeof(req_06_r2), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len == 7U);
    assert(response[5] == 0x02U); /* Record 0x02 */
    assert(response[6] == 3U);    /* Pending counter = 3 */

    /* Query 19 06: Record 0x03 (aging counter) */
    uint8_t req_06_r3[] = {0x19U, 0x06U, 0xD0U, 0x06U, 0x17U, 0x03U};
    assert(backend->report(NULL, 0x06U, req_06_r3, sizeof(req_06_r3), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len == 7U);
    assert(response[5] == 0x03U); /* Record 0x03 */
    assert(response[6] == 12U);   /* Aging counter = 12 */

    /* Query 19 06: Record 0x04 (aged counter) */
    uint8_t req_06_r4[] = {0x19U, 0x06U, 0xD0U, 0x06U, 0x17U, 0x04U};
    assert(backend->report(NULL, 0x06U, req_06_r4, sizeof(req_06_r4), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len == 7U);
    assert(response[5] == 0x04U); /* Record 0x04 */
    assert(response[6] == 1U);    /* Aged counter = 1 */

    /* Query 19 06: Record 0xFF (all records) */
    uint8_t req_06_all[] = {0x19U, 0x06U, 0xD0U, 0x06U, 0x17U, 0xFFU};
    assert(backend->report(NULL, 0x06U, req_06_all, sizeof(req_06_all), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    /* Response: 5 bytes header + 4 records * 2 bytes = 13 bytes */
    assert(resp_len == 13U);
    assert(response[5] == 0x01U && response[6] == 7U);
    assert(response[7] == 0x02U && response[8] == 3U);
    assert(response[9] == 0x03U && response[10] == 12U);
    assert(response[11] == 0x04U && response[12] == 1U);

    /* 4. Test Issue #56: 19 01 status mask filtering vs 19 0A supported DTCs */
    uds_dtc_app_init();
    /* 19 0A: All supported DTCs (69 count) */
    uint8_t req_0a[] = {0x19U, 0x0AU};
    assert(backend->report(NULL, 0x0AU, req_0a, sizeof(req_0a), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len == (2U + 69U * 4U)); /* 278 bytes */

    /* 19 01 FF: Active faulted DTCs at boot is exactly 3 (baseline demonstration faults) */
    uint8_t req_01_ff[] = {0x19U, 0x01U, 0xFFU};
    assert(backend->report(NULL, 0x01U, req_01_ff, sizeof(req_01_ff), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    uint16_t active_count = (uint16_t)(((uint16_t)response[3] << 8U) | (uint16_t)response[4]);
    assert(active_count == 3U);

    /* Reporting a fault on an OEM DTC increments the active fault count */
    assert(uds_dtc_app_report_event(0xD00617UL, true));
    /* Report enough events to qualify confirmed fault */
    for (uint8_t i = 0U; i < 8U; ++i) {
        (void)uds_dtc_app_report_event(0xD00617UL, true);
    }
    assert(backend->report(NULL, 0x01U, req_01_ff, sizeof(req_01_ff), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    active_count = (uint16_t)(((uint16_t)response[3] << 8U) | (uint16_t)response[4]);
    assert(active_count == 4U);

    /* Clear all DTCs -> active fault count becomes 0 */
    assert(uds_dtc_app_clear(NULL, 0xFFFFFFUL) == UDS_RESULT_OK);
    assert(backend->report(NULL, 0x01U, req_01_ff, sizeof(req_01_ff), response, &resp_len,
                           sizeof(response)) == UDS_RESULT_OK);
    active_count = (uint16_t)(((uint16_t)response[3] << 8U) | (uint16_t)response[4]);
    assert(active_count == 0U);
}

int main(void) {
    test_dtc_app_all_subfunctions();
    test_iso14229_19_04_conformance();
    test_control_dtc_setting_service();
    test_dtc_wear_leveling_nvm();
    test_bootloader_flow();
    test_extended_data_and_snapshot_structures();
    return 0;
}
