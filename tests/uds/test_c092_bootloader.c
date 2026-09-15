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
    (void)memset(&meta, 0, sizeof(meta));
    meta.magic = UDS_BL_METADATA_MAGIC;
    meta.version = 0U; /* Downgrade attempt! Active version is 1 */
    meta.image_size = sizeof(FirmwareMetadata_t);

    /* Anti-rollback downgrade must be rejected */
    assert(uds_bootloader_routine_control(
               NULL, 0x01U, UDS_BL_ROUTINE_CHECK_MEMORY, (const uint8_t *)&meta, sizeof(meta),
               routine_out, &routine_out_len, sizeof(routine_out)) == UDS_RESULT_OUT_OF_RANGE);
    assert(routine_out[0] == 0x02U); /* 0x02: Rejected downgrade */

    /* Valid version 2 with correct SHA-256 digest */
    meta.version = 2U;
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

    /* 7. Vector Table Sanity Validation Gate for STM32C092 (256 KB Flash: 0x08000000 - 0x08040000) */
    assert(!uds_bootloader_is_application_valid(0x00000000UL)); /* NULL address */
    assert(!uds_bootloader_is_application_valid(0x20000000UL)); /* RAM, not Flash */
    assert(!uds_bootloader_is_application_valid(0x08040000UL)); /* Flash boundary */
    assert(!uds_bootloader_is_application_valid(0x08050000UL)); /* Beyond 256 KB Flash */
    assert(!uds_bootloader_is_application_valid(
        0x0800A004UL)); /* Unaligned VTOR (not 256-byte aligned) */
}

int main(void) {
    test_c092_bootloader_memory_map_and_flow();
    return 0;
}
