#ifndef BOOT_TEST_UTIL_H
#define BOOT_TEST_UTIL_H

#include "uds_bootloader.h"
#include "uds_iso_tp/boot_verify.h"
#include "uds_iso_tp/sha256.h"

#include <assert.h>
#include <string.h>

/* Payload used by the bootloader tests (64 bytes, so 128 + 64 = 192 is 8-byte aligned). */
#define BT_PAYLOAD_LEN 64U

/*
 * Download a real image (128-byte header placeholder + payload) into the staging slot,
 * then build a finalized (format version + header CRC) metadata block that describes it.
 * The signature field is left zero; call bt_sign() to add a valid test signature.
 */
static void bt_stage_image(FirmwareMetadata_t *meta, uint32_t version) {
    uint8_t image[sizeof(FirmwareMetadata_t) + BT_PAYLOAD_LEN];
    uint16_t max_block = 0U;
    uint8_t exit_resp[4];
    uint16_t exit_len = 0U;
    const UdsDownloadMemoryMap mmap = uds_bootloader_get_memory_map();

    (void)memset(image, 0, sizeof(image));
    (void)memset(&image[sizeof(FirmwareMetadata_t)], 0xA5, BT_PAYLOAD_LEN);

    assert(uds_bootloader_request_download(NULL, mmap.staging_image.start, (uint32_t)sizeof(image),
                                           &max_block) == UDS_RESULT_OK);
    assert(uds_bootloader_transfer_data(NULL, 1U, image, (uint16_t)sizeof(image)) == UDS_RESULT_OK);
    assert(uds_bootloader_transfer_exit(NULL, NULL, 0U, exit_resp, &exit_len, sizeof(exit_resp)) ==
           UDS_RESULT_OK);

    (void)memset(meta, 0, sizeof(*meta));
    meta->magic = UDS_BL_METADATA_MAGIC;
    meta->version = version;
    meta->image_size = (uint32_t)sizeof(image);
    sha256_hash(&image[sizeof(FirmwareMetadata_t)], BT_PAYLOAD_LEN, meta->sha256);
    meta->crc32 = boot_calc_crc32(&image[sizeof(FirmwareMetadata_t)], BT_PAYLOAD_LEN);
    boot_finalize_metadata(meta);
}

/* Sign the manifest (covers version + size + flags + payload hash) with the test-only key. */
static void bt_sign(FirmwareMetadata_t *meta) {
    uint8_t manifest[32];
    boot_manifest_digest(meta, manifest);
    uds_bootloader_calculate_manifest_signature(manifest, meta->signature);
    /* header_crc32 covers the signature field, so it must be recomputed LAST. */
    boot_finalize_metadata(meta);
}

#endif /* BOOT_TEST_UTIL_H */
