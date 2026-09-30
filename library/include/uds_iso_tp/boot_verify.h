#ifndef BOOT_VERIFY_H
#define BOOT_VERIFY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UDS_BL_METADATA_MAGIC 0x5544534DUL /* "UDSM" */
#define UDS_BL_METADATA_FORMAT_VERSION 1U

typedef struct __attribute__((packed)) {
    uint32_t magic;          /* 0x5544534D */
    uint32_t version;        /* Monotonic firmware version counter for anti-rollback */
    uint32_t image_size;     /* Application binary size in bytes */
    uint32_t crc32;          /* Transmission CRC-32 */
    uint8_t sha256[32];      /* SHA-256 cryptographic digest */
    uint8_t signature[64];   /* ECDSA/Ed25519 signature */
    uint8_t status;          /* UdsBootloaderSlotStatus */
    uint8_t boot_attempts;   /* Current boot attempt count for candidate verification */
    uint8_t max_attempts;    /* Max allowed boot attempts before automatic rollback */
    uint8_t active_slot;     /* 0 = Slot A, 1 = Slot B */
    uint16_t format_version; /* Metadata format version (default 1) */
    uint16_t flags;          /* Header flags */
    uint32_t header_crc32;   /* CRC-32 over metadata header fields preceding this field */
    uint8_t reserved[4];     /* Reserved padding to 128 bytes total */
} FirmwareMetadata_t;

typedef FirmwareMetadata_t BootImageHeader;

typedef bool (*BootSignatureVerifierFn)(const uint8_t *digest32, const uint8_t *signature64);

typedef struct {
    const char *name;
    bool (*verify)(const FirmwareMetadata_t *hdr, const uint8_t *image, size_t len);
} BootIntegrityPolicy;

/* Standard integrity policies */
const BootIntegrityPolicy *boot_verify_policy_crc32(void);
const BootIntegrityPolicy *boot_verify_policy_sha256(void);
const BootIntegrityPolicy *boot_verify_policy_signature(void);

/* Policy configuration and execution */
void boot_verify_set_policy(const BootIntegrityPolicy *policy);
const BootIntegrityPolicy *boot_verify_get_policy(void);
void boot_verify_set_signature_verifier(BootSignatureVerifierFn verifier);
bool boot_verify_image(const FirmwareMetadata_t *hdr, const uint8_t *image, size_t len);

/* Standalone CRC-32 calculation utility */
uint32_t boot_calc_crc32(const uint8_t *data, size_t len);

/* Metadata header CRC calculation and validation */
uint32_t boot_calc_metadata_crc(const FirmwareMetadata_t *hdr);
bool boot_validate_metadata(const FirmwareMetadata_t *hdr);

/**
 * @brief Digest that the image signature must cover.
 *
 * SHA-256 over a domain tag plus every field that decides *whether and how* an
 * image may be installed: magic, format version, firmware version (anti-rollback
 * floor), image size, flags and the payload SHA-256. Runtime-mutable fields
 * (status, boot_attempts, active_slot) are deliberately excluded.
 */
void boot_manifest_digest(const FirmwareMetadata_t *hdr, uint8_t out[32]);

/** @brief Stamp format_version and header_crc32 so boot_validate_metadata() accepts hdr. */
void boot_finalize_metadata(FirmwareMetadata_t *hdr);

#endif /* BOOT_VERIFY_H */
