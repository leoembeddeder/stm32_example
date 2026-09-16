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

#if !defined(HAL_FLASH_MODULE_ENABLED)
#define UDS_BL_MOCK_FLASH_SIZE 4096U
static uint8_t s_mock_flash_slot_a[UDS_BL_MOCK_FLASH_SIZE];
static uint8_t s_mock_flash_slot_b[UDS_BL_MOCK_FLASH_SIZE];
static uint32_t s_mock_flash_slot_a_len = 0U;
static uint32_t s_mock_flash_slot_b_len = 0U;
#endif

static uint32_t bootloader_calc_crc32(const uint8_t *data, uint32_t length) {
    uint32_t value = 0xFFFFFFFFUL;
    if (data == NULL) {
        return 0U;
    }
    for (uint32_t index = 0U; index < length; ++index) {
        value ^= data[index];
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            value = ((value & 1U) != 0U) ? ((value >> 1U) ^ 0xEDB88320UL) : (value >> 1U);
        }
    }
    return value ^ 0xFFFFFFFFUL;
}

static const uint8_t s_oem_root_pubkey[16] = {0xD4U, 0x51U, 0x86U, 0x93U, 0xB6U, 0xA2U,
                                              0x54U, 0x07U, 0x38U, 0x8BU, 0x22U, 0xF6U,
                                              0x1BU, 0x8CU, 0x0DU, 0x48U};

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
    (void)memcpy(s_mock_nvm_storage, meta, sizeof(FirmwareMetadata_t));
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
    return (meta->magic == UDS_BL_METADATA_MAGIC);
#endif
}

static void hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len,
                        uint8_t *out) {
    uint8_t k_pad[64];
    uint8_t tk[32];
    if (key_len > 64U) {
        Sha256Ctx kctx;
        sha256_init(&kctx);
        sha256_update(&kctx, key, key_len);
        sha256_final(&kctx, tk);
        key = tk;
        key_len = 32U;
    }
    (void)memset(k_pad, 0x36, sizeof(k_pad));
    for (size_t i = 0U; i < key_len; ++i) {
        k_pad[i] = (uint8_t)(k_pad[i] ^ key[i]);
    }
    Sha256Ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, k_pad, sizeof(k_pad));
    sha256_update(&ctx, msg, msg_len);
    uint8_t inner[32];
    sha256_final(&ctx, inner);

    (void)memset(k_pad, 0x5cU, sizeof(k_pad));
    for (size_t i = 0U; i < key_len; ++i) {
        k_pad[i] = (uint8_t)(k_pad[i] ^ key[i]);
    }
    sha256_init(&ctx);
    sha256_update(&ctx, k_pad, sizeof(k_pad));
    sha256_update(&ctx, inner, sizeof(inner));
    sha256_final(&ctx, out);
}

void uds_bootloader_set_signature_verifier(UdsBootloaderSignatureVerifierFn verifier) {
    s_signature_verifier = verifier;
}

void uds_bootloader_set_signature_required(bool required) {
    s_signature_required = required;
}

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

bool uds_bootloader_verify_signature(const uint8_t digest32[32], const uint8_t signature64[64]) {
    if ((digest32 == NULL) || (signature64 == NULL)) {
        return false;
    }
    if (s_signature_verifier != NULL) {
        return s_signature_verifier(digest32, signature64);
    }
    uint8_t expected_sig[64];
    uds_bootloader_calculate_manifest_signature(digest32, expected_sig);
    uint8_t diff = 0U;
    for (uint8_t i = 0U; i < 64U; ++i) {
        diff |= (uint8_t)(signature64[i] ^ expected_sig[i]);
    }
    return (diff == 0U);
}

UdsBootloaderSlotStatus uds_bootloader_get_slot_status(void) {
    return (UdsBootloaderSlotStatus)s_bl_ctx.staging_metadata.status;
}

UdsDownloadResult uds_bootloader_confirm_active_image(void) {
    if (s_bl_ctx.staging_metadata.status != (uint8_t)UDS_BL_SLOT_ACTIVE) {
        return UDS_DOWNLOAD_SEQUENCE_ERROR;
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
    if ((data != NULL) && (length > 0U)) {
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
#endif
    return UDS_DOWNLOAD_OK;
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

UdsDownloadResult uds_bootloader_activate_candidate(void) {
    if (!s_bl_ctx.candidate_verified) {
        return UDS_DOWNLOAD_SEQUENCE_ERROR;
    }

    if (s_bl_ctx.target == UDS_BL_TARGET_STM32C092) {
        uint32_t image_size = s_bl_ctx.staging_metadata.image_size;
        if ((image_size == 0U) || (image_size > s_bl_ctx.active_slot_size)) {
            return UDS_DOWNLOAD_OUT_OF_RANGE;
        }

        uint32_t slot_a_addr = s_bl_ctx.active_slot_addr;

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
            uint16_t chunk_len = ((image_size - bytes_written) > 256U)
                                     ? 256U
                                     : (uint16_t)(image_size - bytes_written);
            UdsDownloadResult prog_res = bootloader_flash_program(NULL, slot_a_addr + bytes_written,
                                                                  &src[bytes_written], chunk_len);
            if (prog_res != UDS_DOWNLOAD_OK) {
                return prog_res;
            }
            bytes_written += chunk_len;
        }

        /* 3. Read back from Slot A and verify CRC32 */
        const uint8_t *dest = (const uint8_t *)(uintptr_t)slot_a_addr;
        uint32_t flash_crc = bootloader_calc_crc32(dest, image_size);
        if ((s_bl_ctx.staging_metadata.crc32 != 0U) &&
            (flash_crc != s_bl_ctx.staging_metadata.crc32)) {
            return UDS_DOWNLOAD_VERIFY_ERROR;
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

        uint32_t flash_crc = bootloader_calc_crc32(s_mock_flash_slot_a, copy_len);
        if ((s_bl_ctx.staging_metadata.crc32 != 0U) &&
            (flash_crc != s_bl_ctx.staging_metadata.crc32)) {
            return UDS_DOWNLOAD_VERIFY_ERROR;
        }
#endif
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

        /* 5. Cryptographic signature verification */
        if (s_signature_required) {
            if (!uds_bootloader_verify_signature(meta->sha256, meta->signature)) {
                s_bl_ctx.last_check_memory_result = 0x04U;
                out[0] = 0x04U; /* 0x04: Signature verification failed */
                *out_len = 1U;
                return UDS_RESULT_SECURITY_DENIED;
            }
        }

        /* All checks passed: mark candidate verified and ready for activation */
        s_bl_ctx.staging_metadata = *meta;
        s_bl_ctx.staging_metadata.status = (uint8_t)UDS_BL_SLOT_CANDIDATE;
        bootloader_nvm_save_metadata(&s_bl_ctx.staging_metadata);
        s_bl_ctx.candidate_verified = true;
        s_bl_ctx.last_check_memory_result = 0x00U;
        s_bl_ctx.last_check_dependencies_result = 0x00U;
        out[0] = 0x00U; /* 0x00 = Verification Passed */
        *out_len = 1U;
        return UDS_RESULT_OK;
    }

    if (routine_id == UDS_BL_ROUTINE_CHECK_DEPENDENCIES) {
        if (subfunction == UDS_ROUTINE_SUBFUNCTION_REQUEST_RESULTS) {
            out[0] = s_bl_ctx.last_check_dependencies_result;
            *out_len = 1U;
            return UDS_RESULT_OK;
        }

        /* Routine 0xFF01: Check Programming Dependencies (ISO 14229-1 / OEM Flashing) */
        bool dependencies_ok = s_bl_ctx.candidate_verified && (!s_bl_ctx.download_in_progress) &&
                               (s_bl_ctx.staging_metadata.magic == UDS_BL_METADATA_MAGIC);
#if defined(HAL_FLASH_MODULE_ENABLED)
        dependencies_ok =
            dependencies_ok && uds_bootloader_is_application_valid(s_bl_ctx.target_slot_addr);
#endif

        uint8_t status =
            (uint8_t)(dependencies_ok ? 0x00U : 0x01U); /* 0x00: dependencies satisfied */
        s_bl_ctx.last_check_dependencies_result = status;
        out[0] = status;
        *out_len = 1U;
        return dependencies_ok ? UDS_RESULT_OK : UDS_RESULT_DENIED;
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
