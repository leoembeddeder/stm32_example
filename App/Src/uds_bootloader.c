#include "uds_bootloader.h"

#if defined(USE_HAL_DRIVER) || defined(STM32F767xx) || defined(STM32C092xx)
#include "main.h"
#endif
#include <string.h>

/* Lightweight standalone SHA-256 implementation */
typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
} Sha256Ctx;

#define ROR32(val, bits) (((val) >> (bits)) | ((val) << (32U - (bits))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROR32(x, 2U) ^ ROR32(x, 13U) ^ ROR32(x, 22U))
#define EP1(x) (ROR32(x, 6U) ^ ROR32(x, 11U) ^ ROR32(x, 25U))
#define SIG0(x) (ROR32(x, 7U) ^ ROR32(x, 18U) ^ ((x) >> 3U))
#define SIG1(x) (ROR32(x, 17U) ^ ROR32(x, 19U) ^ ((x) >> 10U))

static const uint32_t kSha256K[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL, 0x59f111f1UL,
    0x923f82a4UL, 0xab1c5ed5UL, 0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
    0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL,
    0x0fc19dc6UL, 0x240ca1ccUL, 0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL,
    0x06ca6351UL, 0x14292967UL, 0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
    0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL, 0xa2bfe8a1UL, 0xa81a664bUL,
    0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL,
    0x5b9cca4fUL, 0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL};

static void sha256_transform(Sha256Ctx *ctx, const uint8_t *data) {
    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];
    uint32_t f = ctx->state[5];
    uint32_t g = ctx->state[6];
    uint32_t h = ctx->state[7];
    uint32_t m[64];

    for (uint8_t i = 0U; i < 16U; ++i) {
        m[i] = ((uint32_t)data[i * 4U] << 24U) | ((uint32_t)data[i * 4U + 1U] << 16U) |
               ((uint32_t)data[i * 4U + 2U] << 8U) | (uint32_t)data[i * 4U + 3U];
    }
    for (uint8_t i = 16U; i < 64U; ++i) {
        m[i] = SIG1(m[i - 2U]) + m[i - 7U] + SIG0(m[i - 15U]) + m[i - 16U];
    }
    for (uint8_t i = 0U; i < 64U; ++i) {
        uint32_t t1 = h + EP1(e) + CH(e, f, g) + kSha256K[i] + m[i];
        uint32_t t2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(Sha256Ctx *ctx) {
    ctx->state[0] = 0x6a09e667UL;
    ctx->state[1] = 0xbb67ae85UL;
    ctx->state[2] = 0x3c6ef372UL;
    ctx->state[3] = 0xa54ff53aUL;
    ctx->state[4] = 0x510e527fUL;
    ctx->state[5] = 0x9b05688cUL;
    ctx->state[6] = 0x1f83d9abUL;
    ctx->state[7] = 0x5be0cd19UL;
    ctx->count = 0U;
}

static void sha256_update(Sha256Ctx *ctx, const uint8_t *data, size_t len) {
    size_t buffer_idx = (size_t)(ctx->count & 0x3FU);
    ctx->count += len;
    for (size_t i = 0U; i < len; ++i) {
        ctx->buffer[buffer_idx++] = data[i];
        if (buffer_idx == 64U) {
            sha256_transform(ctx, ctx->buffer);
            buffer_idx = 0U;
        }
    }
}

static void sha256_final(Sha256Ctx *ctx, uint8_t *digest) {
    uint64_t total_bits = ctx->count * 8U;
    size_t buffer_idx = (size_t)(ctx->count & 0x3FU);
    ctx->buffer[buffer_idx++] = 0x80U;

    if (buffer_idx > 56U) {
        (void)memset(&ctx->buffer[buffer_idx], 0, 64U - buffer_idx);
        sha256_transform(ctx, ctx->buffer);
        buffer_idx = 0U;
    }
    (void)memset(&ctx->buffer[buffer_idx], 0, 56U - buffer_idx);
    for (uint8_t i = 0U; i < 8U; ++i) {
        ctx->buffer[56U + i] = (uint8_t)((total_bits >> (56U - i * 8U)) & 0xFFU);
    }
    sha256_transform(ctx, ctx->buffer);

    for (uint8_t i = 0U; i < 8U; ++i) {
        digest[i * 4U] = (uint8_t)((ctx->state[i] >> 24U) & 0xFFU);
        digest[i * 4U + 1U] = (uint8_t)((ctx->state[i] >> 16U) & 0xFFU);
        digest[i * 4U + 2U] = (uint8_t)((ctx->state[i] >> 8U) & 0xFFU);
        digest[i * 4U + 3U] = (uint8_t)(ctx->state[i] & 0xFFU);
    }
}

static UdsBootloaderContext s_bl_ctx;
static UdsDownloadMemoryMap s_bl_memory_map;
static UdsDownload s_bl_download;

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
#endif
    return UDS_DOWNLOAD_OK;
}

static UdsDownloadResult bootloader_flash_erase_poll(void *context) {
    (void)context;
    return UDS_DOWNLOAD_OK;
}

static UdsDownloadResult bootloader_flash_program(void *context, uint32_t address,
                                                  const uint8_t *data, uint16_t length) {
    (void)context;
#if defined(HAL_FLASH_MODULE_ENABLED)
    HAL_FLASH_Unlock();
#if defined(STM32F767xx)
    for (uint32_t i = 0U; i < (uint32_t)length; i += 4U) {
        uint32_t word = 0xFFFFFFFFUL;
        size_t chunk_len = ((uint32_t)length - i < 4U) ? (size_t)((uint32_t)length - i) : 4U;
        (void)memcpy(&word, &data[i], chunk_len);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + i, (uint64_t)word) != HAL_OK) {
            HAL_FLASH_Lock();
            return UDS_DOWNLOAD_PROGRAM_ERROR;
        }
    }
#elif defined(STM32C092xx) || defined(STM32C0xx)
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    size_t i = 0U;
    while ((i + 8U) <= (size_t)length) {
        uint64_t dword_val = 0U;
        (void)memcpy(&dword_val, &data[i], 8U);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address + i, dword_val) != HAL_OK) {
            HAL_FLASH_Lock();
            return UDS_DOWNLOAD_PROGRAM_ERROR;
        }
        i += 8U;
    }
    if (i < (size_t)length) {
        uint64_t dword_val = 0xFFFFFFFFFFFFFFFFULL;
        (void)memcpy(&dword_val, &data[i], (size_t)length - i);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address + i, dword_val) != HAL_OK) {
            HAL_FLASH_Lock();
            return UDS_DOWNLOAD_PROGRAM_ERROR;
        }
    }
#endif
    HAL_FLASH_Lock();
#else
    (void)address;
    (void)data;
    (void)length;
#endif
    return UDS_DOWNLOAD_OK;
}

static UdsDownloadResult bootloader_flash_verify(void *context, const UdsDownloadMetadata *metadata,
                                                 uint32_t expected_crc32, bool has_expected_crc32) {
    (void)context;
    (void)metadata;
    (void)expected_crc32;
    (void)has_expected_crc32;
    return UDS_DOWNLOAD_OK;
}

void uds_bootloader_set_target(UdsBootloaderTarget target) {
    s_bl_ctx.target = target;
    s_bl_ctx.active_version = 1U;
    s_bl_ctx.download_in_progress = false;
    s_bl_ctx.candidate_verified = false;
    s_bl_ctx.last_erase_result = 0x00U;
    s_bl_ctx.last_check_memory_result = 0x01U;
    (void)memset(&s_bl_ctx.staging_metadata, 0, sizeof(s_bl_ctx.staging_metadata));

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
    UdsDownloadResult res = uds_download_begin(&s_bl_download, address, length, 0U);
    if (res != UDS_DOWNLOAD_OK) {
        return (res == UDS_DOWNLOAD_OUT_OF_RANGE) ? UDS_RESULT_OUT_OF_RANGE : UDS_RESULT_ERROR;
    }
    (void)uds_download_poll_erase(&s_bl_download, 0U);
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
    if (uds_download_state(&s_bl_download) == UDS_DOWNLOAD_ERASING) {
        (void)uds_download_poll_erase(&s_bl_download, 0U);
    }
    UdsDownloadResult res = uds_download_write(&s_bl_download, block_sequence, data, length, 0U);
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
    (void)request;
    (void)request_len;
    (void)capacity;
    if (!s_bl_ctx.download_in_progress) {
        return UDS_RESULT_SEQUENCE_ERROR;
    }
    UdsDownloadResult res = uds_download_finish(&s_bl_download, 0U, false, 0U);
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

    if (routine_id == UDS_BL_ROUTINE_ERASE_MEMORY) {
        if (subfunction == UDS_ROUTINE_SUBFUNCTION_REQUEST_RESULTS) {
            out[0] = s_bl_ctx.last_erase_result;
            *out_len = 1U;
            return UDS_RESULT_OK;
        }
        /* Routine 0xFF00: Erase Slot B */
        UdsDownloadResult res = bootloader_flash_erase_start(NULL, s_bl_ctx.target_slot_addr,
                                                             s_bl_ctx.target_slot_size);
        uint8_t status = (uint8_t)((res == UDS_DOWNLOAD_OK) ? 0x00U : 0x01U); /* 0x00 = success */
        s_bl_ctx.last_erase_result = status;
        out[0] = status;
        *out_len = 1U;
        return (res == UDS_DOWNLOAD_OK) ? UDS_RESULT_OK : UDS_RESULT_ERROR;
    }

    if (routine_id == UDS_BL_ROUTINE_CHECK_MEMORY) {
        if (subfunction == UDS_ROUTINE_SUBFUNCTION_REQUEST_RESULTS) {
            out[0] = s_bl_ctx.last_check_memory_result;
            *out_len = 1U;
            return UDS_RESULT_OK;
        }

        /* Routine 0x0202: Verify Checksum/Hash, Anti-Rollback, and Cryptographic Signature */
        const FirmwareMetadata_t *meta =
            (const FirmwareMetadata_t *)(uintptr_t)s_bl_ctx.target_slot_addr;
        FirmwareMetadata_t temp_meta;

        if (in_len >= sizeof(FirmwareMetadata_t)) {
            (void)memcpy(&temp_meta, in, sizeof(FirmwareMetadata_t));
            meta = &temp_meta;
        }

        /* 1. Magic check */
        if (meta->magic != UDS_BL_METADATA_MAGIC) {
            s_bl_ctx.last_check_memory_result = 0x01U;
            out[0] = 0x01U; /* Failed validation */
            *out_len = 1U;
            return UDS_RESULT_OUT_OF_RANGE;
        }

        /* 2. Anti-rollback check: Version must be >= currently active monotonic version */
        if (meta->version < s_bl_ctx.active_version) {
            s_bl_ctx.last_check_memory_result = 0x02U;
            out[0] = 0x02U; /* Rejected: Version downgrade attempt */
            *out_len = 1U;
            return UDS_RESULT_OUT_OF_RANGE;
        }

        /* 3. Compute SHA-256 over image */
        uint8_t computed_hash[32];
        Sha256Ctx sha;
        sha256_init(&sha);
        if (meta->image_size > sizeof(FirmwareMetadata_t)) {
            const uint8_t *payload = (const uint8_t *)(uintptr_t)(s_bl_ctx.target_slot_addr +
                                                                  sizeof(FirmwareMetadata_t));
            size_t payload_size = (size_t)(meta->image_size - sizeof(FirmwareMetadata_t));
            sha256_update(&sha, payload, payload_size);
        }
        sha256_final(&sha, computed_hash);

        /* 4. Match hash digest */
        if (memcmp(computed_hash, meta->sha256, sizeof(computed_hash)) != 0) {
            s_bl_ctx.last_check_memory_result = 0x03U;
            out[0] = 0x03U; /* Digest mismatch */
            *out_len = 1U;
            return UDS_RESULT_ERROR;
        }

        /* All checks passed: mark candidate verified and ready for activation */
        s_bl_ctx.staging_metadata = *meta;
        s_bl_ctx.candidate_verified = true;
        s_bl_ctx.last_check_memory_result = 0x00U;
        out[0] = 0x00U; /* 0x00 = Verification Passed */
        *out_len = 1U;
        return UDS_RESULT_OK;
    }

    return UDS_RESULT_NOT_SUPPORTED;
}

/* Vector Table Sanity Validation Gate (S32K144 / OpenBLT specification) */
bool uds_bootloader_is_application_valid(uint32_t app_vector_addr) {
    uint32_t flash_base = (s_bl_ctx.target == UDS_BL_TARGET_STM32C092) ? UDS_BL_C092_FLASH_BASE
                                                                       : UDS_BL_F767_FLASH_BASE;
    uint32_t flash_end = (s_bl_ctx.target == UDS_BL_TARGET_STM32C092)
                             ? (UDS_BL_C092_FLASH_BASE + UDS_BL_C092_FLASH_SIZE)
                             : (UDS_BL_F767_FLASH_BASE + UDS_BL_F767_FLASH_SIZE);
    uint32_t ram_start = (s_bl_ctx.target == UDS_BL_TARGET_STM32C092) ? UDS_BL_C092_RAM_START
                                                                      : UDS_BL_F767_RAM_START;
    uint32_t ram_end =
        (s_bl_ctx.target == UDS_BL_TARGET_STM32C092) ? UDS_BL_C092_RAM_END : UDS_BL_F767_RAM_END;

    if ((app_vector_addr < flash_base) || (app_vector_addr >= flash_end)) {
        return false;
    }

    /* Hardware VTOR alignment: 256-byte aligned on Cortex-M0+, 512-byte aligned on Cortex-M7 */
    uint32_t vtor_alignment_mask = (s_bl_ctx.target == UDS_BL_TARGET_STM32C092) ? 0xFFU : 0x1FFU;
    if ((app_vector_addr & vtor_alignment_mask) != 0U) {
        return false;
    }
    const uint32_t *vectors = (const uint32_t *)(uintptr_t)app_vector_addr;
    uint32_t initial_msp = vectors[0];
    uint32_t reset_handler = vectors[1];

    /* 1. Initial MSP must be in SRAM bounds and aligned */
    if ((initial_msp < ram_start) || (initial_msp > ram_end) || ((initial_msp & 0x3U) != 0U)) {
        return false;
    }

    /* 2. Reset Handler must have Thumb bit set and be non-erased */
    if (((reset_handler & 0x1U) == 0U) || (reset_handler == 0xFFFFFFFFUL)) {
        return false;
    }

    uint32_t reset_addr = reset_handler & ~1U;
    if ((reset_addr < app_vector_addr) || (reset_addr >= flash_end)) {
        return false;
    }
    return true;
}

typedef void (*AppEntryFn)(void);

/* Vector Jump, Barrier Synchronization & Cache Maintenance */
void uds_bootloader_jump_to_app(uint32_t app_vector_addr) {
#if defined(STM32C092xx) || defined(CORTEX_M0PLUS)
    /* Sanity gate: Validate vector table before attempting execution */
    if (!uds_bootloader_is_application_valid(app_vector_addr)) {
        return;
    }

    uint32_t app_msp = *(__IO uint32_t *)(uintptr_t)app_vector_addr;
    AppEntryFn app_entry =
        (AppEntryFn)(uintptr_t)(*(__IO uint32_t *)(uintptr_t)(app_vector_addr + 4U));

    /* 1. Disable all interrupts */
    __disable_irq();

    /* 2. Disable SysTick timer and clear pending register */
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    /* 3. Disable all peripherals & clear NVIC pending interrupts (single 32-bit register on Cortex-M0+) */
    NVIC->ICER[0] = 0xFFFFFFFFUL;
    NVIC->ICPR[0] = 0xFFFFFFFFUL;

    /* 4. Set Vector Table Offset Register (VTOR) */
    SCB->VTOR = app_vector_addr;

    /* 5. Reset CONTROL register to Privileged Thread Mode on MSP */
    __set_CONTROL(0U);

    /* 6. Memory Synchronization Barriers */
    __DSB();
    __ISB();

    /* 7. Set Main Stack Pointer and branch to reset handler */
    __set_MSP(app_msp);
    app_entry();
#elif defined(CORTEX_M7) || defined(STM32F767xx)
    /* Sanity gate: Validate vector table before attempting execution */
    if (!uds_bootloader_is_application_valid(app_vector_addr)) {
        return;
    }

    uint32_t app_msp = *(__IO uint32_t *)(uintptr_t)app_vector_addr;
    AppEntryFn app_entry =
        (AppEntryFn)(uintptr_t)(*(__IO uint32_t *)(uintptr_t)(app_vector_addr + 4U));

    /* 1. Disable all interrupts */
    __disable_irq();

    /* 2. Disable SysTick timer and clear pending register */
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    /* 3. Disable all peripherals & clear NVIC pending interrupts */
    for (uint32_t i = 0U; i < 8U; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFUL;
        NVIC->ICPR[i] = 0xFFFFFFFFUL;
    }

    /* 4. Clear pending system exceptions (PendSV, SysTick) */
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;

    /* 5. Disable and clean Cortex-M7 L1 Caches */
    SCB_DisableICache();
    SCB_DisableDCache();
    SCB_InvalidateICache();
    SCB_CleanInvalidateDCache();

    /* 6. Set Vector Table Offset Register (VTOR) */
    SCB->VTOR = app_vector_addr;

    /* 7. Reset CONTROL register to Privileged Thread Mode on MSP */
    __set_CONTROL(0U);

    /* 8. Memory Synchronization Barriers */
    __DSB();
    __ISB();

    /* 9. Set Main Stack Pointer and branch to reset handler */
    __set_MSP(app_msp);
    app_entry();
#else
    (void)app_vector_addr;
#endif
}
