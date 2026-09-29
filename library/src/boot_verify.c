#include "uds_iso_tp/boot_verify.h"
#include "uds_iso_tp/sha256.h"
#include <stddef.h>
#include <string.h>

static BootSignatureVerifierFn s_sig_verifier = NULL;
static const BootIntegrityPolicy *s_active_policy = NULL;

uint32_t boot_calc_crc32(const uint8_t *data, size_t len) {
    if ((data == NULL) || (len == 0U)) {
        return 0U;
    }
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0U; i < len; ++i) {
        crc ^= (uint32_t)data[i];
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1U) ^ 0xEDB88320UL;
            } else {
                crc >>= 1U;
            }
        }
    }
    return ~crc;
}

static bool verify_crc32_impl(const FirmwareMetadata_t *hdr, const uint8_t *image, size_t len) {
    if ((hdr == NULL) || (image == NULL) || (len == 0U)) {
        return false;
    }
    uint32_t computed_crc = boot_calc_crc32(image, len);
    return (computed_crc == hdr->crc32);
}

static bool constant_time_equal(const uint8_t *a, const uint8_t *b, size_t len) {
    uint8_t diff = 0U;
    for (size_t i = 0U; i < len; ++i) {
        diff |= (uint8_t)(a[i] ^ b[i]);
    }
    return (diff == 0U);
}

static bool verify_sha256_impl(const FirmwareMetadata_t *hdr, const uint8_t *image, size_t len) {
    if ((hdr == NULL) || (image == NULL) || (len == 0U)) {
        return false;
    }
    uint8_t digest[32];
    sha256_hash(image, len, digest);
    return constant_time_equal(digest, hdr->sha256, 32U);
}

static bool verify_signature_impl(const FirmwareMetadata_t *hdr, const uint8_t *image, size_t len) {
    if ((hdr == NULL) || (image == NULL) || (len == 0U)) {
        return false;
    }
    if (!verify_sha256_impl(hdr, image, len)) {
        return false;
    }
    if (s_sig_verifier == NULL) {
        /* Fail-closed: signature policy strictly requires a registered cryptographic verifier */
        return false;
    }
    return s_sig_verifier(hdr->sha256, hdr->signature);
}

static const BootIntegrityPolicy k_policy_crc32 = {
    "CRC32 (Accidental corruption detection only - no security)", verify_crc32_impl};

static const BootIntegrityPolicy k_policy_sha256 = {"SHA-256 (Cryptographic integrity check)",
                                                    verify_sha256_impl};

static const BootIntegrityPolicy k_policy_signature = {
    "Cryptographic Signature (Authenticity and integrity)", verify_signature_impl};

const BootIntegrityPolicy *boot_verify_policy_crc32(void) {
    return &k_policy_crc32;
}

const BootIntegrityPolicy *boot_verify_policy_sha256(void) {
    return &k_policy_sha256;
}

const BootIntegrityPolicy *boot_verify_policy_signature(void) {
    return &k_policy_signature;
}

void boot_verify_set_policy(const BootIntegrityPolicy *policy) {
    s_active_policy = policy;
}

const BootIntegrityPolicy *boot_verify_get_policy(void) {
    return (s_active_policy != NULL) ? s_active_policy : &k_policy_sha256;
}

void boot_verify_set_signature_verifier(BootSignatureVerifierFn verifier) {
    s_sig_verifier = verifier;
}

bool boot_verify_image(const FirmwareMetadata_t *hdr, const uint8_t *image, size_t len) {
    const BootIntegrityPolicy *policy = boot_verify_get_policy();
    if (policy->verify == NULL) {
        return false;
    }
    return policy->verify(hdr, image, len);
}

uint32_t boot_calc_metadata_crc(const FirmwareMetadata_t *hdr) {
    if (hdr == NULL) {
        return 0U;
    }
    return boot_calc_crc32((const uint8_t *)hdr, offsetof(FirmwareMetadata_t, header_crc32));
}

bool boot_validate_metadata(const FirmwareMetadata_t *hdr) {
    if (hdr == NULL) {
        return false;
    }
    if (hdr->magic != UDS_BL_METADATA_MAGIC) {
        return false;
    }
    if ((hdr->format_version != 0U) || (hdr->header_crc32 != 0U)) {
        if (hdr->format_version != UDS_BL_METADATA_FORMAT_VERSION) {
            return false;
        }
        uint32_t expected_crc = boot_calc_metadata_crc(hdr);
        if (hdr->header_crc32 != expected_crc) {
            return false;
        }
    }
    return true;
}
