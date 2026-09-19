#ifndef STM32_UDS_ISO_TP_UDS_BOOTLOADER_H
#define STM32_UDS_ISO_TP_UDS_BOOTLOADER_H

#include "uds_iso_tp/uds.h"
#include "uds_iso_tp/uds_download.h"

#include <stdbool.h>
#include <stdint.h>

/* Target Profile Selection */
typedef enum { UDS_BL_TARGET_STM32F767 = 0, UDS_BL_TARGET_STM32C092 = 1 } UdsBootloaderTarget;

/* STM32F767 2MB Dual-Bank Flash Memory Map */
#define UDS_BL_F767_FLASH_BASE 0x08000000UL
#define UDS_BL_F767_FLASH_SIZE 0x00200000UL /* 2048 KB */
#define UDS_BL_F767_BOOTLOADER_START 0x08000000UL
#define UDS_BL_F767_BOOTLOADER_SIZE 0x00040000UL    /* 256 KB (Sectors 0-1) */
#define UDS_BL_F767_NVM_METADATA_START 0x08010000UL /* Sector 2 (32 KB) */
#define UDS_BL_F767_APP_SLOT_A_START 0x08040000UL   /* Active App (Sectors 4-7, 768 KB) */
#define UDS_BL_F767_APP_SLOT_A_SIZE 0x000C0000UL
#define UDS_BL_F767_APP_SLOT_B_START 0x08100000UL /* Staging Slot B (Bank 2, 1024 KB) */
#define UDS_BL_F767_APP_SLOT_B_SIZE 0x00100000UL
#define UDS_BL_F767_RAM_START 0x20000000UL
#define UDS_BL_F767_RAM_END 0x20080000UL /* 512 KB */

/* STM32C092RC 256KB Flash Memory Map (128 uniform 2 KB sectors/pages) */
#define UDS_BL_C092_FLASH_BASE 0x08000000UL
#define UDS_BL_C092_FLASH_SIZE 0x00040000UL /* 256 KB */
#define UDS_BL_C092_PAGE_SIZE 0x00000800UL  /* 2048 Bytes */
#define UDS_BL_C092_PAGE_COUNT 128U
#define UDS_BL_C092_BOOTLOADER_START 0x08000000UL /* Pages 0-15 (32 KB) */
#define UDS_BL_C092_BOOTLOADER_SIZE 0x00008000UL
#define UDS_BL_C092_NVM_METADATA_START 0x08008000UL /* Pages 16-19 (8 KB) */
#define UDS_BL_C092_NVM_METADATA_SIZE 0x00002000UL
#define UDS_BL_C092_APP_SLOT_A_START 0x0800A000UL /* Active App (Pages 20-73, 108 KB) */
#define UDS_BL_C092_APP_SLOT_A_SIZE 0x0001B000UL
#define UDS_BL_C092_APP_SLOT_B_START 0x08025000UL /* Staging Slot B (Pages 74-127, 108 KB) */
#define UDS_BL_C092_APP_SLOT_B_SIZE 0x0001B000UL
#define UDS_BL_C092_RAM_START 0x20000000UL
#define UDS_BL_C092_RAM_END 0x20006000UL /* 24 KB */

/* Default memory map selection based on compile target */
#if defined(STM32C092xx) || defined(TARGET_STM32C092)
#define UDS_BL_FLASH_BASE UDS_BL_C092_FLASH_BASE
#define UDS_BL_FLASH_SIZE UDS_BL_C092_FLASH_SIZE
#define UDS_BL_BOOTLOADER_START UDS_BL_C092_BOOTLOADER_START
#define UDS_BL_BOOTLOADER_SIZE UDS_BL_C092_BOOTLOADER_SIZE
#define UDS_BL_NVM_METADATA_START UDS_BL_C092_NVM_METADATA_START
#define UDS_BL_APP_SLOT_A_START UDS_BL_C092_APP_SLOT_A_START
#define UDS_BL_APP_SLOT_A_SIZE UDS_BL_C092_APP_SLOT_A_SIZE
#define UDS_BL_APP_SLOT_B_START UDS_BL_C092_APP_SLOT_B_START
#define UDS_BL_APP_SLOT_B_SIZE UDS_BL_C092_APP_SLOT_B_SIZE
#define UDS_BL_RAM_START UDS_BL_C092_RAM_START
#define UDS_BL_RAM_END UDS_BL_C092_RAM_END
#else
#define UDS_BL_FLASH_BASE UDS_BL_F767_FLASH_BASE
#define UDS_BL_FLASH_SIZE UDS_BL_F767_FLASH_SIZE
#define UDS_BL_BOOTLOADER_START UDS_BL_F767_BOOTLOADER_START
#define UDS_BL_BOOTLOADER_SIZE UDS_BL_F767_BOOTLOADER_SIZE
#define UDS_BL_NVM_METADATA_START UDS_BL_F767_NVM_METADATA_START
#define UDS_BL_APP_SLOT_A_START UDS_BL_F767_APP_SLOT_A_START
#define UDS_BL_APP_SLOT_A_SIZE UDS_BL_F767_APP_SLOT_A_SIZE
#define UDS_BL_APP_SLOT_B_START UDS_BL_F767_APP_SLOT_B_START
#define UDS_BL_APP_SLOT_B_SIZE UDS_BL_F767_APP_SLOT_B_SIZE
#define UDS_BL_RAM_START UDS_BL_F767_RAM_START
#define UDS_BL_RAM_END UDS_BL_F767_RAM_END
#endif

#define UDS_BL_METADATA_MAGIC 0x5544534DUL /* "UDSM" */

#define UDS_BL_ROUTINE_ERASE_MEMORY 0xFF00U
#define UDS_BL_ROUTINE_CHECK_MEMORY 0x0202U
#define UDS_BL_ROUTINE_CHECK_DEPENDENCIES 0xFF01U

#define UDS_ROUTINE_SUBFUNCTION_START_ROUTINE 0x01U
#define UDS_ROUTINE_SUBFUNCTION_STOP_ROUTINE 0x02U
#define UDS_ROUTINE_SUBFUNCTION_REQUEST_RESULTS 0x03U

typedef enum {
    UDS_BL_SLOT_INVALID = 0,
    UDS_BL_SLOT_CANDIDATE = 1,
    UDS_BL_SLOT_ACTIVE = 2,
    UDS_BL_SLOT_CONFIRMED = 3,
    UDS_BL_SLOT_ROLLBACK = 4
} UdsBootloaderSlotStatus;

typedef struct __attribute__((packed)) {
    uint32_t magic;        /* 0x5544534D */
    uint32_t version;      /* Monotonic firmware version counter for anti-rollback */
    uint32_t image_size;   /* Application binary size in bytes */
    uint32_t crc32;        /* Transmission CRC-32 */
    uint8_t sha256[32];    /* SHA-256 cryptographic digest */
    uint8_t signature[64]; /* ECDSA/Ed25519 signature */
    uint8_t status;        /* UdsBootloaderSlotStatus */
    uint8_t boot_attempts; /* Current boot attempt count for candidate verification */
    uint8_t max_attempts;  /* Max allowed boot attempts before automatic rollback */
    uint8_t active_slot;   /* 0 = Slot A, 1 = Slot B */
    uint8_t reserved[12];
} FirmwareMetadata_t;

typedef enum {
    UDS_BL_VERIFY_MODE_CRC32 = 0,        /* Non-cryptographic CRC-32 (ISO 14229 A/B swapping) */
    UDS_BL_VERIFY_MODE_SHA256_SECURE = 1 /* SHA-256 cryptographic digest + signature check */
} UdsBootloaderVerifyMode;

typedef struct {
    UdsBootloaderTarget target;
    uint32_t active_version;
    uint32_t active_slot_addr;
    uint32_t active_slot_size;
    uint32_t target_slot_addr;
    uint32_t target_slot_size;
    bool download_in_progress;
    bool candidate_verified;
    uint8_t last_erase_result;
    uint8_t last_check_memory_result;
    uint8_t last_check_dependencies_result;
    UdsBootloaderVerifyMode verify_mode;
    FirmwareMetadata_t staging_metadata;
} UdsBootloaderContext;

typedef bool (*UdsBootloaderSignatureVerifierFn)(const uint8_t *digest32,
                                                 const uint8_t *signature64);

void uds_bootloader_init(void);
void uds_bootloader_set_target(UdsBootloaderTarget target);
UdsBootloaderTarget uds_bootloader_get_target(void);

void uds_bootloader_set_verification_mode(UdsBootloaderVerifyMode mode);
UdsBootloaderVerifyMode uds_bootloader_get_verification_mode(void);

UdsDownloadMemoryMap uds_bootloader_get_memory_map(void);
uint32_t uds_bootloader_get_active_version(void);
uint32_t uds_bootloader_get_active_slot(void);
bool uds_bootloader_is_activation_pending(void);
bool uds_bootloader_is_application_valid(uint32_t app_vector_addr);
UdsDownloadResult uds_bootloader_activate_candidate(void);

/* Cryptographic Signature Verification & Manifest */
void uds_bootloader_set_signature_verifier(UdsBootloaderSignatureVerifierFn verifier);
void uds_bootloader_set_signature_required(bool required);
bool uds_bootloader_verify_signature(const uint8_t digest32[32], const uint8_t signature64[64]);
void uds_bootloader_calculate_manifest_signature(const uint8_t digest32[32],
                                                 uint8_t signature64[64]);

/* Power-fail-safe boot state journal & A/B slot descriptor */
UdsBootloaderSlotStatus uds_bootloader_get_slot_status(void);
UdsDownloadResult uds_bootloader_confirm_active_image(void);
UdsDownloadResult uds_bootloader_rollback_candidate(void);

/* Flash Operations & Image Integrity Verification */
UdsDownloadResult uds_bootloader_flash_verify(const UdsDownloadMetadata *metadata,
                                              uint32_t expected_crc32, bool has_expected_crc32);
UdsDownloadResult uds_bootloader_flash_erase_poll(void);

/* UDS Service Callbacks for Flashing Pipeline */
UdsCallbackResult uds_bootloader_request_download(void *context, uint32_t address, uint32_t length,
                                                  uint16_t *max_block_length);
UdsCallbackResult uds_bootloader_transfer_data(void *context, uint8_t block_sequence,
                                               const uint8_t *data, uint16_t length);
UdsCallbackResult uds_bootloader_transfer_exit(void *context, const uint8_t *request,
                                               uint16_t request_len, uint8_t *response,
                                               uint16_t *response_len, uint16_t capacity);
UdsCallbackResult uds_bootloader_routine_control(void *context, uint8_t subfunction,
                                                 uint16_t routine_id, const uint8_t *in,
                                                 uint16_t in_len, uint8_t *out, uint16_t *out_len,
                                                 uint16_t capacity);

/* Vector Table Relocation & Jump */
void uds_bootloader_jump_to_app(uint32_t app_vector_addr);

#endif /* STM32_UDS_ISO_TP_UDS_BOOTLOADER_H */
