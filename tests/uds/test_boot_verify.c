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

static void test_signature_policy(void) {
    const uint8_t image[64] = "Cryptographically Signed Bootloader Application Image";
    FirmwareMetadata_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = UDS_BL_METADATA_MAGIC;
    hdr.image_size = (uint32_t)sizeof(image);
    sha256_hash(image, sizeof(image), hdr.sha256);

    /* Generate standard HMAC signature */
    const uint8_t mock_key[16] = {0x53, 0x45, 0x43, 0x55, 0x52, 0x45, 0x5f, 0x42,
                                  0x4f, 0x4f, 0x54, 0x5f, 0x4b, 0x45, 0x59, 0x31};
    hmac_sha256(mock_key, sizeof(mock_key), hdr.sha256, 32U, hdr.signature);

    boot_verify_set_policy(boot_verify_policy_signature());
    assert(boot_verify_image(&hdr, image, sizeof(image)) == true);

    /* Test custom signature verifier callback */
    boot_verify_set_signature_verifier(custom_mock_verifier);
    hdr.signature[0] = hdr.sha256[0];
    assert(boot_verify_image(&hdr, image, sizeof(image)) == true);

    /* Invalid signature rejected */
    hdr.signature[0] = (uint8_t)(hdr.sha256[0] ^ 0xFFU);
    assert(boot_verify_image(&hdr, image, sizeof(image)) == false);

    boot_verify_set_signature_verifier(NULL);
}

int main(void) {
    test_crc32_policy();
    test_sha256_policy();
    test_signature_policy();
    printf("Pluggable BootIntegrityPolicy tests passed successfully.\n");
    return 0;
}
