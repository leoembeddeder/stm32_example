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
    uint8_t manifest[32];
    boot_manifest_digest(hdr, manifest);
    return s_sig_verifier(manifest, hdr->signature);
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

static void put_u32_le(uint8_t *dst, uint32_t v) {
    dst[0] = (uint8_t)(v & 0xFFU);
    dst[1] = (uint8_t)((v >> 8U) & 0xFFU);
    dst[2] = (uint8_t)((v >> 16U) & 0xFFU);
    dst[3] = (uint8_t)((v >> 24U) & 0xFFU);
}

void boot_manifest_digest(const FirmwareMetadata_t *hdr, uint8_t out[32]) {
    static const uint8_t tag[16] = {'U', 'D', 'S', '-', 'M', 'A', 'N', 'I',
                                    'F', 'E', 'S', 'T', '-', 'V', '1', 0U};
    uint8_t buf[16 + 4 + 2 + 2 + 4 + 4 + 32];
    size_t n = 0U;
    if ((hdr == NULL) || (out == NULL)) {
        return;
    }
    (void)memcpy(&buf[n], tag, sizeof(tag));
    n += sizeof(tag);
    put_u32_le(&buf[n], hdr->magic);
    n += 4U;
    buf[n++] = (uint8_t)(hdr->format_version & 0xFFU);
    buf[n++] = (uint8_t)(hdr->format_version >> 8U);
    buf[n++] = (uint8_t)(hdr->flags & 0xFFU);
    buf[n++] = (uint8_t)(hdr->flags >> 8U);
    put_u32_le(&buf[n], hdr->version);
    n += 4U;
    put_u32_le(&buf[n], hdr->image_size);
    n += 4U;
    (void)memcpy(&buf[n], hdr->sha256, 32U);
    n += 32U;
    sha256_hash(buf, n, out);
}

void boot_finalize_metadata(FirmwareMetadata_t *hdr) {
    if (hdr == NULL) {
        return;
    }
    hdr->format_version = UDS_BL_METADATA_FORMAT_VERSION;
    hdr->header_crc32 = boot_calc_metadata_crc(hdr);
}

bool boot_validate_metadata(const FirmwareMetadata_t *hdr) {
    if (hdr == NULL) {
        return false;
    }
    if (hdr->magic != UDS_BL_METADATA_MAGIC) {
        return false;
    }
    /* No legacy bypass: every header must carry a supported format and a valid CRC. */
    if (hdr->format_version != UDS_BL_METADATA_FORMAT_VERSION) {
        return false;
    }
    return (hdr->header_crc32 == boot_calc_metadata_crc(hdr));
}
