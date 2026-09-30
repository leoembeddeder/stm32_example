#include "uds_iso_tp/boot_verify.h"
#include "sha256.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_crc32_policy(void) {
    const uint8_t image[64] = "STM32 Application Firmware Payload Example Data 1234567890";
    FirmwareMetadata_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = UDS_BL_METADATA_MAGIC;
    hdr.image_size = (uint32_t)sizeof(image);
    hdr.crc32 = boot_calc_crc32(image, sizeof(image));

    boot_verify_set_policy(boot_verify_policy_crc32());
    assert(boot_verify_image(&hdr, image, sizeof(image)) == true);

    /* Bit flip corruption must be detected */
    uint8_t corrupted[64];
    memcpy(corrupted, image, sizeof(image));
    corrupted[10] ^= 0x01U;
    assert(boot_verify_image(&hdr, corrupted, sizeof(corrupted)) == false);
}

static void test_sha256_policy(void) {
    const uint8_t image[64] = "Secure Firmware Payload with SHA-256 Digest Verification";
    FirmwareMetadata_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = UDS_BL_METADATA_MAGIC;
    hdr.image_size = (uint32_t)sizeof(image);
    sha256_hash(image, sizeof(image), hdr.sha256);

    boot_verify_set_policy(boot_verify_policy_sha256());
    assert(boot_verify_image(&hdr, image, sizeof(image)) == true);

    /* Corrupted image must be rejected */
    uint8_t corrupted[64];
    memcpy(corrupted, image, sizeof(image));
    corrupted[0] ^= 0xFFU;
    assert(boot_verify_image(&hdr, corrupted, sizeof(corrupted)) == false);
}

static bool custom_mock_verifier(const uint8_t *digest32, const uint8_t *signature64) {
    /* Custom verifier accepting signature where first byte matches digest */
    return (signature64[0] == digest32[0]);
}

static uint8_t s_expected_manifest[32];
static bool exact_manifest_verifier(const uint8_t *digest32, const uint8_t *signature64) {
    (void)signature64;
    return (memcmp(digest32, s_expected_manifest, 32U) == 0);
}

/* Relabelling an old signed image with a higher version must invalidate the signature. */
static void test_signature_covers_version_size_flags(void) {
    const uint8_t image[32] = "signed image bytes for manifest";
    FirmwareMetadata_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = UDS_BL_METADATA_MAGIC;
    hdr.version = 5U;
    hdr.image_size = (uint32_t)sizeof(image);
    sha256_hash(image, sizeof(image), hdr.sha256);
    boot_finalize_metadata(&hdr);
    boot_manifest_digest(&hdr, s_expected_manifest); /* what the OEM signed */

    boot_verify_set_policy(boot_verify_policy_signature());
    boot_verify_set_signature_verifier(exact_manifest_verifier);
    assert(boot_verify_image(&hdr, image, sizeof(image)) == true);

    FirmwareMetadata_t forged = hdr;
    forged.version = 99U; /* attacker relabels the same signed bytes as a newer release */
    boot_finalize_metadata(&forged);
    assert(boot_verify_image(&forged, image, sizeof(image)) == false);

    forged = hdr;
    forged.image_size += 1U;
    assert(boot_verify_image(&forged, image, sizeof(image)) == false);

    forged = hdr;
    forged.flags = 0x0001U;
    assert(boot_verify_image(&forged, image, sizeof(image)) == false);

    /* Runtime-mutable fields are NOT part of the manifest, so slot bookkeeping still works. */
    forged = hdr;
    forged.status = 3U;
    forged.boot_attempts = 2U;
    forged.active_slot = 1U;
    assert(boot_verify_image(&forged, image, sizeof(image)) == true);

    boot_verify_set_signature_verifier(NULL);
    boot_verify_set_policy(NULL);
}

static void test_signature_policy(void) {
    const uint8_t image[64] = "Cryptographically Signed Bootloader Application Image";
    FirmwareMetadata_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = UDS_BL_METADATA_MAGIC;
    hdr.image_size = (uint32_t)sizeof(image);
    sha256_hash(image, sizeof(image), hdr.sha256);

    /* Generate old mock key HMAC signature to verify that forged mock signatures fail */
    const uint8_t mock_key[16] = {0x53, 0x45, 0x43, 0x55, 0x52, 0x45, 0x5f, 0x42,
                                  0x4f, 0x4f, 0x54, 0x5f, 0x4b, 0x45, 0x59, 0x31};
    hmac_sha256(mock_key, sizeof(mock_key), hdr.sha256, 32U, hdr.signature);

    boot_verify_set_policy(boot_verify_policy_signature());

    /* P0-1 Acceptance: When no verifier is set, signature policy MUST fail closed and reject image */
    boot_verify_set_signature_verifier(NULL);
    assert(boot_verify_image(&hdr, image, sizeof(image)) == false);

    /* Test custom signature verifier callback: it now receives the manifest digest */
    boot_verify_set_signature_verifier(custom_mock_verifier);
    uint8_t manifest[32];
    boot_manifest_digest(&hdr, manifest);
    hdr.signature[0] = manifest[0];
    assert(boot_verify_image(&hdr, image, sizeof(image)) == true);

    /* Invalid signature rejected */
    hdr.signature[0] = (uint8_t)(manifest[0] ^ 0xFFU);
    assert(boot_verify_image(&hdr, image, sizeof(image)) == false);

    /* Signature verification when SHA256 digest fails */
    uint8_t corrupted[64];
    memcpy(corrupted, image, sizeof(image));
    corrupted[0] ^= 0x55U;
    assert(boot_verify_image(&hdr, corrupted, sizeof(corrupted)) == false);

    boot_verify_set_signature_verifier(NULL);
}

static void test_metadata_validation(void) {
    FirmwareMetadata_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = UDS_BL_METADATA_MAGIC;
    hdr.version = 10U;
    hdr.image_size = 4096U;
    hdr.crc32 = 0x12345678U;
    hdr.format_version = UDS_BL_METADATA_FORMAT_VERSION;
    hdr.header_crc32 = boot_calc_metadata_crc(&hdr);

    /* Valid formatted metadata */
    assert(boot_validate_metadata(&hdr) == true);

    /* NULL check */
    assert(boot_validate_metadata(NULL) == false);

    /* Corrupted magic */
    hdr.magic = 0xDEADBEEFUL;
    assert(boot_validate_metadata(&hdr) == false);
    hdr.magic = UDS_BL_METADATA_MAGIC;

    /* Unsupported format version */
    hdr.format_version = 99U;
    assert(boot_validate_metadata(&hdr) == false);
    hdr.format_version = UDS_BL_METADATA_FORMAT_VERSION;

    /* Corrupted metadata CRC */
    hdr.header_crc32 ^= 0x01U;
    assert(boot_validate_metadata(&hdr) == false);
    hdr.header_crc32 = boot_calc_metadata_crc(&hdr);
    assert(boot_validate_metadata(&hdr) == true);

    /* Unformatted/legacy metadata (format_version == 0 and header_crc32 == 0) */
    FirmwareMetadata_t legacy_hdr;
    memset(&legacy_hdr, 0, sizeof(legacy_hdr));
    legacy_hdr.magic = UDS_BL_METADATA_MAGIC;
    assert(boot_validate_metadata(&legacy_hdr) == false); /* no legacy bypass any more */
    boot_finalize_metadata(&legacy_hdr);
    assert(boot_validate_metadata(&legacy_hdr) == true);

    assert(boot_calc_metadata_crc(NULL) == 0U);
}

static void test_null_guards_and_edge_cases(void) {
    FirmwareMetadata_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    const uint8_t image[16] = {0};

    /* CRC calc with NULL */
    assert(boot_calc_crc32(NULL, 0U) == 0U);
    assert(boot_calc_crc32(image, 0U) == 0U);

    /* Verify with NULL header or NULL image or 0 len across policies */
    boot_verify_set_policy(boot_verify_policy_crc32());
    assert(boot_verify_image(NULL, image, sizeof(image)) == false);
    assert(boot_verify_image(&hdr, NULL, sizeof(image)) == false);
    assert(boot_verify_image(&hdr, image, 0U) == false);

    boot_verify_set_policy(boot_verify_policy_sha256());
    assert(boot_verify_image(NULL, image, sizeof(image)) == false);
    assert(boot_verify_image(&hdr, NULL, sizeof(image)) == false);
    assert(boot_verify_image(&hdr, image, 0U) == false);

    boot_verify_set_policy(boot_verify_policy_signature());
    assert(boot_verify_image(NULL, image, sizeof(image)) == false);
    assert(boot_verify_image(&hdr, NULL, sizeof(image)) == false);
    assert(boot_verify_image(&hdr, image, 0U) == false);

    /* Null verify function in policy */
    BootIntegrityPolicy dummy_policy = {"Empty", NULL};
    boot_verify_set_policy(&dummy_policy);
    assert(boot_verify_image(&hdr, image, sizeof(image)) == false);

    /* Set policy back to default */
    boot_verify_set_policy(NULL);
    assert(boot_verify_get_policy() == boot_verify_policy_sha256());
}

int main(void) {
    test_crc32_policy();
    test_sha256_policy();
    test_signature_policy();
    test_signature_covers_version_size_flags();
    test_metadata_validation();
    test_null_guards_and_edge_cases();
    printf("Pluggable BootIntegrityPolicy tests passed successfully.\n");
    return 0;
}
