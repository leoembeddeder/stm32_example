#include "uds_bootloader.h"
#if defined(STM32C092xx)
#include "uds_platform_fdcan.h"
#else
#include "uds_platform.h"
#endif

#if defined(USE_HAL_DRIVER) || defined(STM32F767xx) || defined(STM32C092xx)
#include "main.h"
#endif
#include <string.h>

#include "sha256.h"
#include "boot_jump.h"
#include "uds_iso_tp/boot_verify.h"

static UdsBootloaderContext s_bl_ctx;
static UdsDownloadMemoryMap s_bl_memory_map;
static UdsDownload s_bl_download;

#if !defined(HAL_FLASH_MODULE_ENABLED)
#define UDS_BL_MOCK_FLASH_SIZE 4096U
static uint8_t s_mock_flash_slot_a[UDS_BL_MOCK_FLASH_SIZE];
static uint8_t s_mock_flash_slot_b[UDS_BL_MOCK_FLASH_SIZE];
static uint32_t s_mock_flash_slot_a_len = 0U;
static uint32_t s_mock_flash_slot_b_len = 0U;
#endif

static uint32_t bootloader_calc_crc32(const uint8_t *data, uint32_t length) {
    return boot_calc_crc32(data, (size_t)length);
}

static const uint8_t *bootloader_get_slot_ptr(uint32_t address, size_t *avail_len) {
#if defined(HAL_FLASH_MODULE_ENABLED)
    if (avail_len != NULL) {
        *avail_len = (size_t)s_bl_ctx.target_slot_size;
    }
    return (const uint8_t *)(uintptr_t)address;
#else
    if (address >= s_bl_ctx.target_slot_addr) {
        uint32_t offset = address - s_bl_ctx.target_slot_addr;
        if (offset < sizeof(s_mock_flash_slot_b)) {
            if (avail_len != NULL) {
                *avail_len = sizeof(s_mock_flash_slot_b) - offset;
            }
            return &s_mock_flash_slot_b[offset];
        }
    } else {
        uint32_t offset =
            (address >= s_bl_ctx.active_slot_addr) ? (address - s_bl_ctx.active_slot_addr) : 0U;
        if (offset < sizeof(s_mock_flash_slot_a)) {
            if (avail_len != NULL) {
                *avail_len = sizeof(s_mock_flash_slot_a) - offset;
            }
            return &s_mock_flash_slot_a[offset];
        }
    }
    if (avail_len != NULL) {
        *avail_len = 0U;
    }
    return NULL;
#endif
}

#if defined(UDS_ISO_TP_TESTING)
/* Test-only symmetric key. Production builds MUST register a real verifier
 * (asymmetric, key in protected memory) with uds_bootloader_set_signature_verifier(). */
static const uint8_t s_oem_root_pubkey[16] = {0xD4U, 0x51U, 0x86U, 0x93U, 0xB6U, 0xA2U,
                                              0x54U, 0x07U, 0x38U, 0x8BU, 0x22U, 0xF6U,
                                              0x1BU, 0x8CU, 0x0DU, 0x48U};
#endif

static UdsBootFloor *s_floor_store = NULL;
static UdsBootloaderSignatureVerifierFn s_signature_verifier = NULL;
static bool s_signature_required = true;

#if !defined(HAL_FLASH_MODULE_ENABLED)
static uint8_t s_mock_nvm_storage[sizeof(FirmwareMetadata_t)];
static bool s_mock_nvm_valid = false;
#endif

static void bootloader_nvm_save_metadata(const FirmwareMetadata_t *meta) {
    if (meta == NULL) {
        return;
    }
#if defined(HAL_FLASH_MODULE_ENABLED)
    (void)meta;
#else
    FirmwareMetadata_t copy = *meta;
    copy.format_version = UDS_BL_METADATA_FORMAT_VERSION;
    copy.header_crc32 = boot_calc_metadata_crc(&copy);
    (void)memcpy(s_mock_nvm_storage, &copy, sizeof(FirmwareMetadata_t));
    s_mock_nvm_valid = true;
#endif
}

static bool bootloader_nvm_load_metadata(FirmwareMetadata_t *meta) {
    if (meta == NULL) {
        return false;
    }
#if defined(HAL_FLASH_MODULE_ENABLED)
    return false;
#else
    if (!s_mock_nvm_valid) {
        return false;
    }
    (void)memcpy(meta, s_mock_nvm_storage, sizeof(FirmwareMetadata_t));
    return boot_validate_metadata(meta);
#endif
}

void uds_bootloader_set_floor_store(UdsBootFloor *floor_store) {
    s_floor_store = floor_store;
}

uint32_t uds_bootloader_get_version_floor(void) {
    uint32_t floor = s_bl_ctx.active_version;
    if (s_floor_store != NULL) {
        const uint32_t persisted = boot_floor_get(s_floor_store);
        floor = (persisted > floor) ? persisted : floor;
    }
    return floor;
}

void uds_bootloader_set_signature_verifier(UdsBootloaderSignatureVerifierFn verifier) {
    s_signature_verifier = verifier;
}

void uds_bootloader_set_signature_required(bool required) {
    s_signature_required = required;
}

void uds_bootloader_set_verification_mode(UdsBootloaderVerifyMode mode) {
    s_bl_ctx.verify_mode = mode;
}

UdsBootloaderVerifyMode uds_bootloader_get_verification_mode(void) {
    return s_bl_ctx.verify_mode;
}

#if defined(UDS_ISO_TP_TESTING)
void uds_bootloader_calculate_manifest_signature(const uint8_t digest32[32],
                                                 uint8_t signature64[64]) {
    if ((digest32 == NULL) || (signature64 == NULL)) {
        return;
    }
    (void)memset(signature64, 0, 64U);
    hmac_sha256(s_oem_root_pubkey, sizeof(s_oem_root_pubkey), digest32, 32U, &signature64[0]);
    uint8_t tag_key[16];
    for (size_t i = 0U; i < 16U; ++i) {
        tag_key[i] = (uint8_t)(s_oem_root_pubkey[i] ^ 0xAAU);
    }
    hmac_sha256(tag_key, sizeof(tag_key), &signature64[0], 32U, &signature64[32]);
}

#endif

bool uds_bootloader_verify_signature(const uint8_t digest32[32], const uint8_t signature64[64]) {
    if ((digest32 == NULL) || (signature64 == NULL)) {
        return false;
    }
    if (s_signature_verifier != NULL) {
        return s_signature_verifier(digest32, signature64);
    }
#if !defined(UDS_ISO_TP_TESTING)
    /* Fail closed: no verifier registered means no image is authentic. */
    return false;
#else
    uint8_t expected_sig[64];
    uds_bootloader_calculate_manifest_signature(digest32, expected_sig);
    uint8_t diff = 0U;
    for (uint8_t i = 0U; i < 64U; ++i) {
        diff |= (uint8_t)(signature64[i] ^ expected_sig[i]);
    }
    return (diff == 0U);
#endif
}

UdsBootloaderSlotStatus uds_bootloader_get_slot_status(void) {
    return (UdsBootloaderSlotStatus)s_bl_ctx.staging_metadata.status;
}

UdsDownloadResult uds_bootloader_confirm_active_image(void) {
    if (s_bl_ctx.staging_metadata.status != (uint8_t)UDS_BL_SLOT_ACTIVE) {
        return UDS_DOWNLOAD_SEQUENCE_ERROR;
    }
    /* Raise the persistent floor FIRST: if it cannot be stored we do not confirm. */
    if ((s_floor_store != NULL) &&
        !boot_floor_raise(s_floor_store, s_bl_ctx.staging_metadata.version)) {
        return UDS_DOWNLOAD_PROGRAM_ERROR;
    }
    s_bl_ctx.staging_metadata.status = (uint8_t)UDS_BL_SLOT_CONFIRMED;
    s_bl_ctx.staging_metadata.boot_attempts = 0U;
    bootloader_nvm_save_metadata(&s_bl_ctx.staging_metadata);
    return UDS_DOWNLOAD_OK;
}

UdsDownloadResult uds_bootloader_rollback_candidate(void) {
    s_bl_ctx.staging_metadata.status = (uint8_t)UDS_BL_SLOT_ROLLBACK;
    s_bl_ctx.active_slot_addr = (s_bl_ctx.target == UDS_BL_TARGET_STM32C092)
                                    ? UDS_BL_C092_APP_SLOT_A_START
                                    : UDS_BL_F767_APP_SLOT_A_START;
    s_bl_ctx.active_version = 1U;
    s_bl_ctx.candidate_verified = false;
    s_bl_ctx.staging_metadata.active_slot = 0U;
    bootloader_nvm_save_metadata(&s_bl_ctx.staging_metadata);
    return UDS_DOWNLOAD_OK;
}

static UdsDownloadResult bootloader_flash_erase_start(void *context, uint32_t address,
                                                      uint32_t length) {
    (void)context;
    (void)address;
    (void)length;
#if defined(HAL_FLASH_MODULE_ENABLED)
#if defined(STM32F767xx)
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase_init;
    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.Sector = FLASH_SECTOR_8; /* Start of Bank 2 Slot B */
    erase_init.NbSectors = 4U;          /* Sectors 8 to 11 (1024 KB) */
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    uint32_t sector_error = 0U;
    if (HAL_FLASHEx_Erase(&erase_init, &sector_error) != HAL_OK) {
        HAL_FLASH_Lock();
        return UDS_DOWNLOAD_ERASE_ERROR;
    }
    HAL_FLASH_Lock();
#elif defined(STM32C092xx) || defined(STM32C0xx)
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    uint32_t page_index = (address - 0x08000000U) / 2048U;
    uint32_t nb_pages = (length + 2047U) / 2048U;
    FLASH_EraseInitTypeDef erase_init;
    (void)memset(&erase_init, 0, sizeof(erase_init));
    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.Page = page_index;
    erase_init.NbPages = nb_pages;
    uint32_t page_error = 0U;
    if (HAL_FLASHEx_Erase(&erase_init, &page_error) != HAL_OK) {
        HAL_FLASH_Lock();
        return UDS_DOWNLOAD_ERASE_ERROR;
    }
    HAL_FLASH_Lock();
#endif
#else
    if (address >= s_bl_ctx.target_slot_addr) {
        s_mock_flash_slot_b_len = 0U;
        (void)memset(s_mock_flash_slot_b, 0xFF, sizeof(s_mock_flash_slot_b));
    } else {
        s_mock_flash_slot_a_len = 0U;
        (void)memset(s_mock_flash_slot_a, 0xFF, sizeof(s_mock_flash_slot_a));
    }
#endif
    return UDS_DOWNLOAD_OK;
}

static UdsDownloadResult bootloader_flash_erase_poll(void *context) {
    (void)context;
#if defined(HAL_FLASH_MODULE_ENABLED)
#if defined(STM32F767xx)
    if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY)) {
        return UDS_DOWNLOAD_BUSY;
    }
    if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                             FLASH_FLAG_PGPERR | FLASH_FLAG_ERSERR)) {
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                               FLASH_FLAG_PGPERR | FLASH_FLAG_ERSERR);
        HAL_FLASH_Lock();
        return UDS_DOWNLOAD_ERASE_ERROR;
    }
    HAL_FLASH_Lock();
#elif defined(STM32C092xx) || defined(STM32C0xx)
    if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY)) {
        return UDS_DOWNLOAD_BUSY;
    }
#if defined(FLASH_FLAG_ALL_ERRORS)
    if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_ALL_ERRORS)) {
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
        HAL_FLASH_Lock();
        return UDS_DOWNLOAD_ERASE_ERROR;
    }
#else
    uint32_t err_flags = FLASH_FLAG_OPERR | FLASH_FLAG_PROGERR | FLASH_FLAG_WRPERR |
                         FLASH_FLAG_PGAERR | FLASH_FLAG_SIZERR | FLASH_FLAG_PGSERR |
                         FLASH_FLAG_FASTERR;
#if defined(FLASH_FLAG_MISSERR)
    err_flags |= FLASH_FLAG_MISSERR;
#endif
    if (__HAL_FLASH_GET_FLAG(err_flags)) {
        __HAL_FLASH_CLEAR_FLAG(err_flags);
        HAL_FLASH_Lock();
        return UDS_DOWNLOAD_ERASE_ERROR;
    }
#endif
    HAL_FLASH_Lock();
#endif
#endif
    return (s_bl_ctx.last_erase_result == 0x00U) ? UDS_DOWNLOAD_OK : UDS_DOWNLOAD_ERASE_ERROR;
}

#if !defined(HAL_FLASH_MODULE_ENABLED)
static void mock_flash_program(uint32_t address, const uint8_t *data, uint16_t length) {
    if ((data == NULL) || (length == 0U)) {
        return;
    }
    if (address >= s_bl_ctx.target_slot_addr) {
        uint32_t offset = address - s_bl_ctx.target_slot_addr;
        if ((offset + (uint32_t)length) <= sizeof(s_mock_flash_slot_b)) {
            (void)memcpy(&s_mock_flash_slot_b[offset], data, (size_t)length);
            if ((offset + (uint32_t)length) > s_mock_flash_slot_b_len) {
                s_mock_flash_slot_b_len = offset + (uint32_t)length;
            }
        }
    } else {
        uint32_t offset =
            (address >= s_bl_ctx.active_slot_addr) ? (address - s_bl_ctx.active_slot_addr) : 0U;
        if ((offset + (uint32_t)length) <= sizeof(s_mock_flash_slot_a)) {
            (void)memcpy(&s_mock_flash_slot_a[offset], data, (size_t)length);
            if ((offset + (uint32_t)length) > s_mock_flash_slot_a_len) {
                s_mock_flash_slot_a_len = offset + (uint32_t)length;
            }
        }
    }
}
#elif defined(STM32F767xx)
static bool hal_flash_program_f767(uint32_t address, const uint8_t *data, uint16_t length) {
    for (uint32_t i = 0U; i < (uint32_t)length; i += 4U) {
        uint32_t word = 0xFFFFFFFFUL;
        size_t chunk_len = ((uint32_t)length - i < 4U) ? (size_t)((uint32_t)length - i) : 4U;
        (void)memcpy(&word, &data[i], chunk_len);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + i, (uint64_t)word) != HAL_OK) {
            return false;
        }
    }
    return true;
}
#elif defined(STM32C092xx) || defined(STM32C0xx)
static bool hal_flash_program_c092(uint32_t address, const uint8_t *data, uint16_t length) {
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    size_t i = 0U;
    while ((i + 8U) <= (size_t)length) {
        uint64_t dword_val = 0U;
        (void)memcpy(&dword_val, &data[i], 8U);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address + i, dword_val) != HAL_OK) {
            return false;
        }
        i += 8U;
    }
    if (i < (size_t)length) {
        uint64_t dword_val = 0xFFFFFFFFFFFFFFFFULL;
        (void)memcpy(&dword_val, &data[i], (size_t)length - i);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address + i, dword_val) != HAL_OK) {
            return false;
        }
    }
    return true;
}
#endif

static UdsDownloadResult bootloader_flash_program(void *context, uint32_t address,
                                                  const uint8_t *data, uint16_t length) {
    (void)context;
#if defined(HAL_FLASH_MODULE_ENABLED)
    HAL_FLASH_Unlock();
#if defined(STM32F767xx)
    bool ok = hal_flash_program_f767(address, data, length);
#elif defined(STM32C092xx) || defined(STM32C0xx)
    bool ok = hal_flash_program_c092(address, data, length);
#else
    bool ok = true;
    (void)address;
    (void)data;
    (void)length;
#endif
    HAL_FLASH_Lock();
    return ok ? UDS_DOWNLOAD_OK : UDS_DOWNLOAD_PROGRAM_ERROR;
#else
    mock_flash_program(address, data, length);
    return UDS_DOWNLOAD_OK;
#endif
}

static UdsDownloadResult bootloader_flash_verify(void *context, const UdsDownloadMetadata *metadata,
                                                 uint32_t expected_crc32, bool has_expected_crc32) {
    (void)context;
    if ((metadata == NULL) || (metadata->image_length == 0U)) {
        return UDS_DOWNLOAD_INVALID_ARGUMENT;
    }

    uint32_t computed_flash_crc = 0U;
#if defined(HAL_FLASH_MODULE_ENABLED)
    const uint8_t *flash_ptr = (const uint8_t *)(uintptr_t)metadata->image_address;
    computed_flash_crc = bootloader_calc_crc32(flash_ptr, metadata->image_length);
#else
    const uint8_t *mock_buf = (metadata->image_address >= s_bl_ctx.target_slot_addr)
                                  ? s_mock_flash_slot_b
                                  : s_mock_flash_slot_a;
    uint32_t mock_len = (metadata->image_address >= s_bl_ctx.target_slot_addr)
                            ? s_mock_flash_slot_b_len
                            : s_mock_flash_slot_a_len;
    if ((mock_len == 0U) || (mock_len < metadata->image_length)) {
        return UDS_DOWNLOAD_VERIFY_ERROR;
    }
    computed_flash_crc = bootloader_calc_crc32(mock_buf, metadata->image_length);
#endif

    if (has_expected_crc32) {
        if (computed_flash_crc != expected_crc32) {
            return UDS_DOWNLOAD_VERIFY_ERROR;
        }
    } else {
        if ((metadata->crc32 != 0xFFFFFFFFUL) &&
            (computed_flash_crc != (metadata->crc32 ^ 0xFFFFFFFFUL))) {
            return UDS_DOWNLOAD_VERIFY_ERROR;
        }
    }

    return UDS_DOWNLOAD_OK;
}

UdsDownloadResult uds_bootloader_flash_verify(const UdsDownloadMetadata *metadata,
                                              uint32_t expected_crc32, bool has_expected_crc32) {
    return bootloader_flash_verify(NULL, metadata, expected_crc32, has_expected_crc32);
}

UdsDownloadResult uds_bootloader_flash_erase_poll(void) {
    return bootloader_flash_erase_poll(NULL);
}

/* Verify what was actually copied into slot A, using the same range (payload after the 128-byte
 * header) and the same primitive that CheckMemory used. Fail closed: no hash/CRC, no activation. */
static UdsDownloadResult c092_verify_copied_image(const uint8_t *copied, uint32_t image_size) {
    if (image_size <= sizeof(FirmwareMetadata_t)) {
        return UDS_DOWNLOAD_VERIFY_ERROR;
    }
    const uint8_t *payload = &copied[sizeof(FirmwareMetadata_t)];
    const size_t payload_len = (size_t)(image_size - sizeof(FirmwareMetadata_t));
    if (s_bl_ctx.verify_mode == UDS_BL_VERIFY_MODE_CRC32) {
        if ((s_bl_ctx.staging_metadata.crc32 == 0U) ||
            (bootloader_calc_crc32(payload, (uint32_t)payload_len) !=
             s_bl_ctx.staging_metadata.crc32)) {
            return UDS_DOWNLOAD_VERIFY_ERROR;
        }
        return UDS_DOWNLOAD_OK;
    }
    uint8_t digest[32];
    sha256_hash(payload, payload_len, digest);
    uint8_t diff = 0U;
    for (size_t i = 0U; i < sizeof(digest); ++i) {
        diff |= (uint8_t)(digest[i] ^ s_bl_ctx.staging_metadata.sha256[i]);
    }
    return (diff == 0U) ? UDS_DOWNLOAD_OK : UDS_DOWNLOAD_VERIFY_ERROR;
}

static UdsDownloadResult c092_copy_candidate_to_slot_a(uint32_t image_size, uint32_t slot_a_addr) {
    /* 1. Erase Slot A for the candidate image size */
    UdsDownloadResult erase_res = bootloader_flash_erase_start(NULL, slot_a_addr, image_size);
    if (erase_res != UDS_DOWNLOAD_OK) {
        return erase_res;
    }
    UdsDownloadResult poll_res = bootloader_flash_erase_poll(NULL);
    if (poll_res != UDS_DOWNLOAD_OK) {
        return poll_res;
    }

    /* 2. Copy candidate image from Slot B to Slot A */
#if defined(HAL_FLASH_MODULE_ENABLED)
    uint32_t slot_b_addr = s_bl_ctx.target_slot_addr;
    const uint8_t *src = (const uint8_t *)(uintptr_t)slot_b_addr;
    uint32_t bytes_written = 0U;
    while (bytes_written < image_size) {
        uint16_t chunk_len =
            ((image_size - bytes_written) > 256U) ? 256U : (uint16_t)(image_size - bytes_written);
        UdsDownloadResult prog_res = bootloader_flash_program(NULL, slot_a_addr + bytes_written,
                                                              &src[bytes_written], chunk_len);
        if (prog_res != UDS_DOWNLOAD_OK) {
            return prog_res;
        }
        bytes_written += chunk_len;
    }

    /* 3. Read back from Slot A and verify CRC32 */
    const uint8_t *dest = (const uint8_t *)(uintptr_t)slot_a_addr;
    UdsDownloadResult verify_res = c092_verify_copied_image(dest, image_size);
    if (verify_res != UDS_DOWNLOAD_OK) {
        return verify_res;
    }
#else
    uint32_t copy_len = (image_size > sizeof(s_mock_flash_slot_b))
                            ? (uint32_t)sizeof(s_mock_flash_slot_b)
                            : image_size;
    UdsDownloadResult prog_res =
        bootloader_flash_program(NULL, slot_a_addr, s_mock_flash_slot_b, (uint16_t)copy_len);
    if (prog_res != UDS_DOWNLOAD_OK) {
        return prog_res;
    }

    UdsDownloadResult verify_res = c092_verify_copied_image(s_mock_flash_slot_a, copy_len);
    if (verify_res != UDS_DOWNLOAD_OK) {
        return verify_res;
    }
#endif
    return UDS_DOWNLOAD_OK;
}

UdsDownloadResult uds_bootloader_activate_candidate(void) {
    if (!s_bl_ctx.candidate_verified) {
        return UDS_DOWNLOAD_SEQUENCE_ERROR;
    }

    if (s_bl_ctx.target == UDS_BL_TARGET_STM32C092) {
        uint32_t image_size = s_bl_ctx.staging_metadata.image_size;
        if ((image_size == 0U) || (image_size > s_bl_ctx.active_slot_size)) {
            return UDS_DOWNLOAD_OUT_OF_RANGE;
        }

        UdsDownloadResult copy_res =
            c092_copy_candidate_to_slot_a(image_size, s_bl_ctx.active_slot_addr);
        if (copy_res != UDS_DOWNLOAD_OK) {
            return copy_res;
        }
    }

    /* 4. Update active version and slot status */
    s_bl_ctx.active_version = s_bl_ctx.staging_metadata.version;
    s_bl_ctx.staging_metadata.status = (uint8_t)UDS_BL_SLOT_ACTIVE;
    s_bl_ctx.staging_metadata.boot_attempts = 1U;
    s_bl_ctx.staging_metadata.max_attempts = 3U;
    s_bl_ctx.staging_metadata.active_slot = 0U;
    s_bl_ctx.candidate_verified = false;
    bootloader_nvm_save_metadata(&s_bl_ctx.staging_metadata);
    return UDS_DOWNLOAD_OK;
}

void uds_bootloader_set_target(UdsBootloaderTarget target) {
    s_bl_ctx.target = target;
    s_bl_ctx.active_version = 1U;
    s_bl_ctx.download_in_progress = false;
    s_bl_ctx.candidate_verified = false;
    s_bl_ctx.last_erase_result = 0x00U;
    s_bl_ctx.last_check_memory_result = 0x01U;
    s_bl_ctx.last_check_dependencies_result = 0x01U;
    (void)memset(&s_bl_ctx.staging_metadata, 0, sizeof(s_bl_ctx.staging_metadata));
    s_bl_ctx.staging_metadata.status = (uint8_t)UDS_BL_SLOT_CONFIRMED;
    s_bl_ctx.staging_metadata.active_slot = 0U;
    s_signature_required = true;
    s_bl_ctx.verify_mode = UDS_BL_VERIFY_MODE_SHA256_SECURE;

    if (target == UDS_BL_TARGET_STM32C092) {
        s_bl_ctx.active_slot_addr = UDS_BL_C092_APP_SLOT_A_START;
        s_bl_ctx.active_slot_size = UDS_BL_C092_APP_SLOT_A_SIZE;
        s_bl_ctx.target_slot_addr = UDS_BL_C092_APP_SLOT_B_START;
        s_bl_ctx.target_slot_size = UDS_BL_C092_APP_SLOT_B_SIZE;

        s_bl_memory_map.staging_image.start = UDS_BL_C092_APP_SLOT_B_START;
        s_bl_memory_map.staging_image.end_exclusive =
            UDS_BL_C092_APP_SLOT_B_START + UDS_BL_C092_APP_SLOT_B_SIZE;
        s_bl_memory_map.bootloader.start = UDS_BL_C092_BOOTLOADER_START;
        s_bl_memory_map.bootloader.end_exclusive =
            UDS_BL_C092_BOOTLOADER_START + UDS_BL_C092_BOOTLOADER_SIZE;
        s_bl_memory_map.active_application.start = UDS_BL_C092_APP_SLOT_A_START;
        s_bl_memory_map.active_application.end_exclusive =
            UDS_BL_C092_APP_SLOT_A_START + UDS_BL_C092_APP_SLOT_A_SIZE;
        s_bl_memory_map.persistent_storage.start = UDS_BL_C092_NVM_METADATA_START;
        s_bl_memory_map.persistent_storage.end_exclusive =
            UDS_BL_C092_NVM_METADATA_START + UDS_BL_C092_NVM_METADATA_SIZE;
        s_bl_memory_map.diagnostic_storage.start = UDS_BL_C092_NVM_METADATA_START;
        s_bl_memory_map.diagnostic_storage.end_exclusive =
            UDS_BL_C092_NVM_METADATA_START + UDS_BL_C092_NVM_METADATA_SIZE;
        s_bl_memory_map.erase_alignment = 8U;
        s_bl_memory_map.program_alignment = 8U;
        s_bl_memory_map.max_block_length = 256U;
        s_bl_memory_map.activation_supported = true;
    } else {
        s_bl_ctx.active_slot_addr = UDS_BL_F767_APP_SLOT_A_START;
        s_bl_ctx.active_slot_size = UDS_BL_F767_APP_SLOT_A_SIZE;
        s_bl_ctx.target_slot_addr = UDS_BL_F767_APP_SLOT_B_START;
        s_bl_ctx.target_slot_size = UDS_BL_F767_APP_SLOT_B_SIZE;

        s_bl_memory_map.staging_image.start = UDS_BL_F767_APP_SLOT_B_START;
        s_bl_memory_map.staging_image.end_exclusive =
            UDS_BL_F767_APP_SLOT_B_START + UDS_BL_F767_APP_SLOT_B_SIZE;
        s_bl_memory_map.bootloader.start = UDS_BL_F767_BOOTLOADER_START;
        s_bl_memory_map.bootloader.end_exclusive =
            UDS_BL_F767_BOOTLOADER_START + UDS_BL_F767_BOOTLOADER_SIZE;
        s_bl_memory_map.active_application.start = UDS_BL_F767_APP_SLOT_A_START;
        s_bl_memory_map.active_application.end_exclusive =
            UDS_BL_F767_APP_SLOT_A_START + UDS_BL_F767_APP_SLOT_A_SIZE;
        s_bl_memory_map.persistent_storage.start = UDS_BL_F767_NVM_METADATA_START;
        s_bl_memory_map.persistent_storage.end_exclusive =
            UDS_BL_F767_NVM_METADATA_START + 0x8000UL;
        s_bl_memory_map.diagnostic_storage.start = UDS_BL_F767_NVM_METADATA_START + 0x8000UL;
        s_bl_memory_map.diagnostic_storage.end_exclusive = UDS_BL_F767_APP_SLOT_A_START;
        s_bl_memory_map.erase_alignment = 4U;
        s_bl_memory_map.program_alignment = 4U;
        s_bl_memory_map.max_block_length = 256U;
        s_bl_memory_map.activation_supported = true;
    }

    UdsDownloadCallbacks dl_callbacks = {
        .erase_start = bootloader_flash_erase_start,
        .erase_poll = bootloader_flash_erase_poll,
        .program = bootloader_flash_program,
        .verify_image = bootloader_flash_verify,
        .abort = NULL,
        .watchdog_kick = NULL,
    };
    uds_download_init(&s_bl_download, &s_bl_memory_map, &dl_callbacks, &s_bl_ctx);
}

void uds_bootloader_init(void) {
#if defined(STM32C092xx) || defined(TARGET_STM32C092)
    uds_bootloader_set_target(UDS_BL_TARGET_STM32C092);
#else
    uds_bootloader_set_target(UDS_BL_TARGET_STM32F767);
#endif
    FirmwareMetadata_t loaded_meta;
    if (bootloader_nvm_load_metadata(&loaded_meta)) {
        s_bl_ctx.staging_metadata = loaded_meta;
        s_bl_ctx.active_version = loaded_meta.version;
        if (loaded_meta.status == (uint8_t)UDS_BL_SLOT_ACTIVE) {
            s_bl_ctx.staging_metadata.boot_attempts =
                (uint8_t)(s_bl_ctx.staging_metadata.boot_attempts + 1U);
            if (s_bl_ctx.staging_metadata.boot_attempts > s_bl_ctx.staging_metadata.max_attempts) {
                (void)uds_bootloader_rollback_candidate();
            } else {
                bootloader_nvm_save_metadata(&s_bl_ctx.staging_metadata);
            }
        }
    }
}

UdsBootloaderTarget uds_bootloader_get_target(void) {
    return s_bl_ctx.target;
}

UdsDownloadMemoryMap uds_bootloader_get_memory_map(void) {
    return s_bl_memory_map;
}

uint32_t uds_bootloader_get_active_version(void) {
    return s_bl_ctx.active_version;
}

uint32_t uds_bootloader_get_active_slot(void) {
    return s_bl_ctx.active_slot_addr;
}

bool uds_bootloader_is_activation_pending(void) {
    return s_bl_ctx.candidate_verified;
}

/* Service 0x34: RequestDownload */
UdsCallbackResult uds_bootloader_request_download(void *context, uint32_t address, uint32_t length,
                                                  uint16_t *max_block_length) {
    (void)context;
    if (max_block_length == NULL) {
        return UDS_RESULT_ERROR;
    }
    /* Enforce boundary check: Address must target Slot B */
    if ((address < s_bl_ctx.target_slot_addr) ||
        ((address + length) > (s_bl_ctx.target_slot_addr + s_bl_ctx.target_slot_size))) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    uint32_t now_ms = uds_platform_now_ms();
    UdsDownloadResult res = uds_download_begin(&s_bl_download, address, length, now_ms);
    if (res != UDS_DOWNLOAD_OK) {
        return (res == UDS_DOWNLOAD_OUT_OF_RANGE) ? UDS_RESULT_OUT_OF_RANGE : UDS_RESULT_ERROR;
    }
    (void)uds_download_poll_erase(&s_bl_download, now_ms);
    *max_block_length = s_bl_memory_map.max_block_length;
    s_bl_ctx.download_in_progress = true;
    s_bl_ctx.candidate_verified = false;
    return UDS_RESULT_OK;
}

/* Service 0x36: TransferData */
UdsCallbackResult uds_bootloader_transfer_data(void *context, uint8_t block_sequence,
                                               const uint8_t *data, uint16_t length) {
    (void)context;
    if (!s_bl_ctx.download_in_progress) {
        return UDS_RESULT_SEQUENCE_ERROR;
    }
    uint32_t now_ms = uds_platform_now_ms();
    if (uds_download_state(&s_bl_download) == UDS_DOWNLOAD_ERASING) {
        (void)uds_download_poll_erase(&s_bl_download, now_ms);
    }
    UdsDownloadResult res =
        uds_download_write(&s_bl_download, block_sequence, data, length, now_ms);
    if (res != UDS_DOWNLOAD_OK) {
        return (res == UDS_DOWNLOAD_SEQUENCE_ERROR) ? UDS_RESULT_SEQUENCE_ERROR : UDS_RESULT_ERROR;
    }
    return UDS_RESULT_OK;
}

/* Service 0x37: RequestTransferExit */
UdsCallbackResult uds_bootloader_transfer_exit(void *context, const uint8_t *request,
                                               uint16_t request_len, uint8_t *response,
                                               uint16_t *response_len, uint16_t capacity) {
    (void)context;
    (void)capacity;
    if (!s_bl_ctx.download_in_progress) {
        return UDS_RESULT_SEQUENCE_ERROR;
    }
    uint32_t expected_crc = 0U;
    bool has_expected_crc = false;
    if ((request != NULL) && (request_len >= 4U)) {
        expected_crc = ((uint32_t)request[0] << 24U) | ((uint32_t)request[1] << 16U) |
                       ((uint32_t)request[2] << 8U) | (uint32_t)request[3];
        has_expected_crc = true;
    }
    uint32_t now_ms = uds_platform_now_ms();
    UdsDownloadResult res =
        uds_download_finish(&s_bl_download, expected_crc, has_expected_crc, now_ms);
    s_bl_ctx.download_in_progress = false;
    if (res != UDS_DOWNLOAD_OK) {
        uds_download_abort(&s_bl_download);
        return UDS_RESULT_ERROR;
    }
    if (response != NULL) {
        response[0] = 0x00U; /* Success status parameter */
    }
    if (response_len != NULL) {
        *response_len = 1U;
    }
    return UDS_RESULT_OK;
}

static UdsCallbackResult routine_erase_memory(uint8_t subfunction, const uint8_t *in,
                                              uint16_t in_len, uint8_t *out, uint16_t *out_len,
                                              uint16_t capacity) {
    (void)in;
    (void)in_len;
    (void)capacity;
    if (subfunction == UDS_ROUTINE_SUBFUNCTION_REQUEST_RESULTS) {
        out[0] = s_bl_ctx.last_erase_result;
        *out_len = 1U;
        return UDS_RESULT_OK;
    }
    /* Routine 0xFF00: Erase Slot B */
    UdsDownloadResult res =
        bootloader_flash_erase_start(NULL, s_bl_ctx.target_slot_addr, s_bl_ctx.target_slot_size);
    uint8_t status = (uint8_t)((res == UDS_DOWNLOAD_OK) ? 0x00U : 0x01U);
    s_bl_ctx.last_erase_result = status;
    out[0] = status;
    *out_len = 1U;
    return (res == UDS_DOWNLOAD_OK) ? UDS_RESULT_OK : UDS_RESULT_ERROR;
}

static void parse_check_memory_metadata(const uint8_t *in, uint16_t in_len,
                                        FirmwareMetadata_t *temp_meta,
                                        const FirmwareMetadata_t **meta_out) {
    if (in_len >= sizeof(FirmwareMetadata_t)) {
        (void)memcpy(temp_meta, in, sizeof(FirmwareMetadata_t));
        *meta_out = temp_meta;
    } else if ((in_len == 4U) && (s_bl_ctx.verify_mode == UDS_BL_VERIFY_MODE_CRC32)) {
        /* 4-byte CRC-32 form. Only honoured when the INTEGRATOR selected CRC mode with
         * uds_bootloader_set_verification_mode(); a tester can never downgrade the policy. */
        *temp_meta = s_bl_ctx.staging_metadata;
        temp_meta->magic = UDS_BL_METADATA_MAGIC;
        temp_meta->version = s_bl_ctx.active_version;
        temp_meta->crc32 = ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
                           ((uint32_t)in[2] << 8) | (uint32_t)in[3];
        boot_finalize_metadata(temp_meta);
        *meta_out = temp_meta;
    }
}

static bool verify_candidate_crc32(const FirmwareMetadata_t *meta) {
    if (meta->crc32 == 0U) {
        return false; /* a zero CRC used to mean "skip the check" */
    }
    uint32_t computed_crc = 0U;
    size_t avail = 0U;
    bool payload_present = false;
    if (meta->image_size > sizeof(FirmwareMetadata_t)) {
        size_t payload_size = (size_t)(meta->image_size - sizeof(FirmwareMetadata_t));
        const uint8_t *payload = bootloader_get_slot_ptr(
            s_bl_ctx.target_slot_addr + (uint32_t)sizeof(FirmwareMetadata_t), &avail);
        if ((payload != NULL) && (avail >= payload_size)) {
            computed_crc = bootloader_calc_crc32(payload, (uint32_t)payload_size);
            payload_present = true;
        }
    } else if (meta->image_size > 0U) {
        const uint8_t *payload = bootloader_get_slot_ptr(s_bl_ctx.target_slot_addr, &avail);
        if ((payload != NULL) && (avail >= meta->image_size)) {
            computed_crc = bootloader_calc_crc32(payload, meta->image_size);
            payload_present = true;
        }
    }
    return (payload_present && (computed_crc == meta->crc32));
}

static UdsCallbackResult verify_candidate_crypto(const FirmwareMetadata_t *meta,
                                                 uint8_t *out_status) {
    uint8_t computed_hash[32];
    Sha256Ctx sha;
    sha256_init(&sha);
    if (meta->image_size <= sizeof(FirmwareMetadata_t)) {
        *out_status = 0x03U; /* an image with no payload is never valid */
        return UDS_RESULT_ERROR;
    }
    {
        size_t payload_size = (size_t)(meta->image_size - sizeof(FirmwareMetadata_t));
        size_t avail = 0U;
        const uint8_t *payload = bootloader_get_slot_ptr(
            s_bl_ctx.target_slot_addr + (uint32_t)sizeof(FirmwareMetadata_t), &avail);
        if ((payload == NULL) || (avail < payload_size)) {
            *out_status = 0x03U;
            return UDS_RESULT_ERROR;
        }
        sha256_update(&sha, payload, payload_size);
    }
    sha256_final(&sha, computed_hash);

    if (memcmp(computed_hash, meta->sha256, sizeof(computed_hash)) != 0) {
        *out_status = 0x03U;
        return UDS_RESULT_ERROR;
    }

    if (s_signature_required) {
        uint8_t manifest[32];
        boot_manifest_digest(meta, manifest); /* covers version + size + flags + payload hash */
        if (!uds_bootloader_verify_signature(manifest, meta->signature)) {
            *out_status = 0x04U;
            return UDS_RESULT_SECURITY_DENIED;
        }
    }
    return UDS_RESULT_OK;
}

static UdsCallbackResult routine_check_memory(uint8_t subfunction, const uint8_t *in,
                                              uint16_t in_len, uint8_t *out, uint16_t *out_len,
                                              uint16_t capacity) {
    (void)capacity;
    if (subfunction == UDS_ROUTINE_SUBFUNCTION_REQUEST_RESULTS) {
        out[0] = s_bl_ctx.last_check_memory_result;
        *out_len = 1U;
        return UDS_RESULT_OK;
    }

    const FirmwareMetadata_t *meta = NULL;
    FirmwareMetadata_t temp_meta;
    parse_check_memory_metadata(in, in_len, &temp_meta, &meta);
    if (meta == NULL) {
        /* No usable metadata in the request: fall back to the header stored at the start of
         * the staging slot. Copy it out (packed struct, possibly unaligned) after a bounds
         * check instead of dereferencing the raw slot address. */
        size_t slot_avail = 0U;
        const uint8_t *slot_hdr = bootloader_get_slot_ptr(s_bl_ctx.target_slot_addr, &slot_avail);
        if ((slot_hdr == NULL) || (slot_avail < sizeof(temp_meta))) {
            s_bl_ctx.last_check_memory_result = 0x01U;
            out[0] = 0x01U;
            *out_len = 1U;
            return UDS_RESULT_OUT_OF_RANGE;
        }
        (void)memcpy(&temp_meta, slot_hdr, sizeof(temp_meta));
        meta = &temp_meta;
    }

    if (!boot_validate_metadata(meta)) {
        s_bl_ctx.last_check_memory_result = 0x01U;
        out[0] = 0x01U;
        *out_len = 1U;
        return UDS_RESULT_OUT_OF_RANGE;
    }

    if (meta->version < uds_bootloader_get_version_floor()) {
        s_bl_ctx.last_check_memory_result = 0x02U;
        out[0] = 0x02U;
        *out_len = 1U;
        return UDS_RESULT_OUT_OF_RANGE;
    }

    if (s_bl_ctx.verify_mode == UDS_BL_VERIFY_MODE_CRC32) {
        if (!verify_candidate_crc32(meta)) {
            s_bl_ctx.last_check_memory_result = 0x03U;
            out[0] = 0x03U;
            *out_len = 1U;
            return UDS_RESULT_ERROR;
        }
    } else {
        uint8_t status = 0U;
        UdsCallbackResult crypto_res = verify_candidate_crypto(meta, &status);
        if (crypto_res != UDS_RESULT_OK) {
            s_bl_ctx.last_check_memory_result = status;
            out[0] = status;
            *out_len = 1U;
            return crypto_res;
        }
    }

    s_bl_ctx.staging_metadata = *meta;
    s_bl_ctx.staging_metadata.status = (uint8_t)UDS_BL_SLOT_CANDIDATE;
    bootloader_nvm_save_metadata(&s_bl_ctx.staging_metadata);
    s_bl_ctx.candidate_verified = true;
    s_bl_ctx.last_check_memory_result = 0x00U;
    s_bl_ctx.last_check_dependencies_result = 0x00U;
    out[0] = 0x00U;
    *out_len = 1U;
    return UDS_RESULT_OK;
}

static UdsCallbackResult routine_check_dependencies(uint8_t subfunction, const uint8_t *in,
                                                    uint16_t in_len, uint8_t *out,
                                                    uint16_t *out_len, uint16_t capacity) {
    (void)in;
    (void)in_len;
    (void)capacity;
    if (subfunction == UDS_ROUTINE_SUBFUNCTION_REQUEST_RESULTS) {
        out[0] = s_bl_ctx.last_check_dependencies_result;
        *out_len = 1U;
        return UDS_RESULT_OK;
    }

    bool dependencies_ok = s_bl_ctx.candidate_verified && (!s_bl_ctx.download_in_progress) &&
                           (s_bl_ctx.staging_metadata.magic == UDS_BL_METADATA_MAGIC);
#if defined(HAL_FLASH_MODULE_ENABLED)
    dependencies_ok =
        dependencies_ok && uds_bootloader_is_application_valid(s_bl_ctx.target_slot_addr);
#endif

    uint8_t status = (uint8_t)(dependencies_ok ? 0x00U : 0x01U);
    s_bl_ctx.last_check_dependencies_result = status;
    out[0] = status;
    *out_len = 1U;
    return dependencies_ok ? UDS_RESULT_OK : UDS_RESULT_DENIED;
}

typedef UdsCallbackResult (*BootRoutineHandler)(uint8_t subfunction, const uint8_t *in,
                                                uint16_t in_len, uint8_t *out, uint16_t *out_len,
                                                uint16_t capacity);

typedef struct {
    uint16_t routine_id;
    BootRoutineHandler handler;
} BootRoutineEntry;

static const BootRoutineEntry k_boot_routines[] = {
    {UDS_BL_ROUTINE_ERASE_MEMORY, routine_erase_memory},
    {UDS_BL_ROUTINE_CHECK_MEMORY, routine_check_memory},
    {UDS_BL_ROUTINE_CHECK_DEPENDENCIES, routine_check_dependencies}};

/* Service 0x31: RoutineControl (0xFF00 Erase, 0x0202 CheckMemory & Anti-Rollback) */
UdsCallbackResult uds_bootloader_routine_control(void *context, uint8_t subfunction,
                                                 uint16_t routine_id, const uint8_t *in,
                                                 uint16_t in_len, uint8_t *out, uint16_t *out_len,
                                                 uint16_t capacity) {
    (void)context;
    if (((subfunction != UDS_ROUTINE_SUBFUNCTION_START_ROUTINE) &&
         (subfunction != UDS_ROUTINE_SUBFUNCTION_REQUEST_RESULTS)) ||
        (out == NULL) || (out_len == NULL) || (capacity < 1U)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }

    for (size_t i = 0U; i < (sizeof(k_boot_routines) / sizeof(k_boot_routines[0])); i++) {
        if (k_boot_routines[i].routine_id == routine_id) {
            return k_boot_routines[i].handler(subfunction, in, in_len, out, out_len, capacity);
        }
    }

    return UDS_RESULT_NOT_SUPPORTED;
}

static bool is_flash_addr_valid(UdsBootloaderTarget target, uint32_t addr, uint32_t *out_end) {
    uint32_t base =
        (target == UDS_BL_TARGET_STM32C092) ? UDS_BL_C092_FLASH_BASE : UDS_BL_F767_FLASH_BASE;
    uint32_t end = (target == UDS_BL_TARGET_STM32C092)
                       ? (UDS_BL_C092_FLASH_BASE + UDS_BL_C092_FLASH_SIZE)
                       : (UDS_BL_F767_FLASH_BASE + UDS_BL_F767_FLASH_SIZE);
    if (out_end != NULL) {
        *out_end = end;
    }
    return (addr >= base) && (addr < end);
}

static bool is_msp_valid(UdsBootloaderTarget target, uint32_t msp) {
    uint32_t ram_start =
        (target == UDS_BL_TARGET_STM32C092) ? UDS_BL_C092_RAM_START : UDS_BL_F767_RAM_START;
    uint32_t ram_end =
        (target == UDS_BL_TARGET_STM32C092) ? UDS_BL_C092_RAM_END : UDS_BL_F767_RAM_END;
    if ((msp & 0x3U) != 0U) {
        return false;
    }
    return (msp >= ram_start) && (msp <= ram_end);
}

static bool is_reset_handler_valid(uint32_t reset_handler, uint32_t app_vector_addr,
                                   uint32_t flash_end) {
    if (((reset_handler & 0x1U) == 0U) || (reset_handler == 0xFFFFFFFFUL)) {
        return false;
    }
    uint32_t reset_addr = reset_handler & ~1U;
    return (reset_addr >= app_vector_addr) && (reset_addr < flash_end);
}

/* Vector Table Sanity Validation Gate (S32K144 / OpenBLT specification) */
bool uds_bootloader_is_application_valid(uint32_t app_vector_addr) {
    uint32_t flash_end = 0U;
    if (!is_flash_addr_valid(s_bl_ctx.target, app_vector_addr, &flash_end)) {
        return false;
    }

    /* Hardware VTOR alignment: 256-byte aligned on Cortex-M0+, 512-byte aligned on Cortex-M7 */
    uint32_t vtor_alignment_mask = (s_bl_ctx.target == UDS_BL_TARGET_STM32C092) ? 0xFFU : 0x1FFU;
    if ((app_vector_addr & vtor_alignment_mask) != 0U) {
        return false;
    }
    const uint32_t *vectors = (const uint32_t *)(uintptr_t)app_vector_addr;
    if (!is_msp_valid(s_bl_ctx.target, vectors[0])) {
        return false;
    }
    return is_reset_handler_valid(vectors[1], app_vector_addr, flash_end);
}
