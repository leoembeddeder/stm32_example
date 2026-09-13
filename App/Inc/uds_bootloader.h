#ifndef STM32_UDS_ISO_TP_UDS_BOOTLOADER_H
#define STM32_UDS_ISO_TP_UDS_BOOTLOADER_H

#include "uds_iso_tp/uds.h"
#include "uds_iso_tp/uds_download.h"

#include <stdbool.h>
#include <stdint.h>

/* STM32F767 2MB Dual-Bank Flash Memory Map */
#define UDS_BL_FLASH_BASE 0x08000000UL
#define UDS_BL_BOOTLOADER_START 0x08000000UL
#define UDS_BL_BOOTLOADER_SIZE 0x00040000UL    /* 256 KB (Sectors 0-1) */
#define UDS_BL_NVM_METADATA_START 0x08010000UL /* Sector 2 (32 KB) */
#define UDS_BL_APP_SLOT_A_START 0x08040000UL   /* Active App (Sectors 4-7, 768 KB) */
#define UDS_BL_APP_SLOT_A_SIZE 0x000C0000UL
#define UDS_BL_APP_SLOT_B_START 0x08100000UL /* Staging Slot B (Bank 2, 1024 KB) */
#define UDS_BL_APP_SLOT_B_SIZE 0x00100000UL

#define UDS_BL_METADATA_MAGIC 0x5544534DUL /* "UDSM" */

#define UDS_BL_ROUTINE_ERASE_MEMORY 0xFF00U
#define UDS_BL_ROUTINE_CHECK_MEMORY 0x0202U

typedef enum {
    UDS_BL_SLOT_INVALID = 0,
    UDS_BL_SLOT_CANDIDATE = 1,
    UDS_BL_SLOT_ACTIVE = 2
} UdsBootloaderSlotStatus;

typedef struct __attribute__((packed)) {
    uint32_t magic;        /* 0x5544534D */
    uint32_t version;      /* Monotonic firmware version counter for anti-rollback */
    uint32_t image_size;   /* Application binary size in bytes */
    uint32_t crc32;        /* Transmission CRC-32 */
    uint8_t sha256[32];    /* SHA-256 cryptographic digest */
    uint8_t signature[64]; /* ECDSA/Ed25519 signature */
    uint8_t status;        /* UdsBootloaderSlotStatus */
    uint8_t reserved[15];
} FirmwareMetadata_t;

typedef struct {
    uint32_t active_version;
    uint32_t active_slot_addr;
    uint32_t target_slot_addr;
    bool download_in_progress;
    bool candidate_verified;
    FirmwareMetadata_t staging_metadata;
} UdsBootloaderContext;

void uds_bootloader_init(void);
UdsDownloadMemoryMap uds_bootloader_get_memory_map(void);
uint32_t uds_bootloader_get_active_version(void);
uint32_t uds_bootloader_get_active_slot(void);
bool uds_bootloader_is_activation_pending(void);

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

/* Cortex-M7 Vector Table Relocation & Cache Maintenance Jump */
void uds_bootloader_jump_to_app(uint32_t app_vector_addr);

#endif /* STM32_UDS_ISO_TP_UDS_BOOTLOADER_H */
