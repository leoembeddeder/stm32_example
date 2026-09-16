/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#include "uds_security_app.h"

#include "uds_platform.h"
#include "uds_security_cmac.h"

#include <string.h>

static uint8_t s_provisioned_master_key[16];
static bool s_has_provisioned_key = false;
static UdsSecurityKeyProviderFn s_custom_key_provider = NULL;
static UdsEntropySourceFn s_custom_entropy_source = NULL;

/* Reference fallback key for regression testing when unprovisioned (NIST AES key) */
static const uint8_t s_default_reference_key[16] = {0x2BU, 0x7EU, 0x15U, 0x16U, 0x28U, 0xAEU,
                                                    0xD2U, 0xA6U, 0xABU, 0xF7U, 0x15U, 0x88U,
                                                    0x09U, 0xCFU, 0x4FU, 0x3CU};

static uint8_t s_level1_active_seed[UDS_SECURITY_APP_LEVEL1_SEED_LEN];
static bool s_level1_seed_valid = false;

static uint8_t s_level2_active_seed[UDS_SECURITY_APP_LEVEL2_SEED_LEN];
static bool s_level2_seed_valid = false;

static uint8_t s_csprng_pool[16] = {0x98U, 0x76U, 0x54U, 0x32U, 0x10U, 0xFEU, 0xDCU, 0xBAU,
                                    0x01U, 0x23U, 0x45U, 0x67U, 0x89U, 0xABU, 0xCDU, 0xEFU};
static uint32_t s_csprng_counter = 0U;

static void csprng_accumulate_entropy(uint32_t sample) {
    s_csprng_counter++;
    uint8_t sample_bytes[8];
    sample_bytes[0] = (uint8_t)(sample >> 24U);
    sample_bytes[1] = (uint8_t)(sample >> 16U);
    sample_bytes[2] = (uint8_t)(sample >> 8U);
    sample_bytes[3] = (uint8_t)(sample);
    sample_bytes[4] = (uint8_t)(s_csprng_counter >> 24U);
    sample_bytes[5] = (uint8_t)(s_csprng_counter >> 16U);
    sample_bytes[6] = (uint8_t)(s_csprng_counter >> 8U);
    sample_bytes[7] = (uint8_t)(s_csprng_counter);

    uint8_t mac[16];
    if (uds_security_cmac_derive_key(s_csprng_pool, sample_bytes, mac)) {
        (void)memcpy(s_csprng_pool, mac, 16U);
    }
}

static bool csprng_generate_bytes(uint8_t *output, size_t length) {
    if ((output == NULL) || (length == 0U)) {
        return false;
    }
    if (s_custom_entropy_source != NULL) {
        if (s_custom_entropy_source(output, length)) {
            return true;
        }
    }
    if (uds_platform_trng_get_random(output, length)) {
        return true;
    }
    uint32_t systick = uds_platform_systick_val();
    csprng_accumulate_entropy(systick);

    size_t generated = 0U;
    while (generated < length) {
        csprng_accumulate_entropy((uint32_t)generated);
        size_t chunk = ((length - generated) < 16U) ? (length - generated) : 16U;
        (void)memcpy(&output[generated], s_csprng_pool, chunk);
        generated += chunk;
    }
    return true;
}

bool uds_security_app_provision_master_key(const uint8_t key[16]) {
    if (key == NULL) {
        return false;
    }
    (void)memcpy(s_provisioned_master_key, key, 16U);
    s_has_provisioned_key = true;
    return true;
}

void uds_security_app_set_key_provider(UdsSecurityKeyProviderFn provider) {
    s_custom_key_provider = provider;
}

bool uds_security_app_has_provisioned_key(void) {
    return s_has_provisioned_key;
}

void uds_security_app_set_entropy_source(UdsEntropySourceFn source) {
    s_custom_entropy_source = source;
}

static uint8_t constant_time_equal_4(const uint8_t left[4], const uint8_t right[4]) {
    uint8_t diff = 0U;
    for (uint8_t i = 0U; i < 4U; ++i) {
        diff |= (uint8_t)(left[i] ^ right[i]);
    }
    return (diff == 0U) ? 1U : 0U;
}

void uds_security_app_init(void) {
    s_level1_seed_valid = false;
    s_level2_seed_valid = false;
    (void)memset(s_level1_active_seed, 0, sizeof(s_level1_active_seed));
    (void)memset(s_level2_active_seed, 0, sizeof(s_level2_active_seed));
    uint32_t initial_entropy = uds_platform_systick_val();
    csprng_accumulate_entropy(initial_entropy);
}

bool uds_security_app_calculate_key_level1(const uint8_t seed[4], uint8_t key[4]) {
    if ((seed == NULL) || (key == NULL)) {
        return false;
    }
    key[0] = (uint8_t)(seed[0] ^ UDS_SECURITY_LEVEL1_MASK_BYTE0);
    key[1] = (uint8_t)(seed[1] ^ UDS_SECURITY_LEVEL1_MASK_BYTE1);
    key[2] = (uint8_t)(seed[2] ^ UDS_SECURITY_LEVEL1_MASK_BYTE2);
    key[3] = (uint8_t)(seed[3] ^ UDS_SECURITY_LEVEL1_MASK_BYTE3);
    return true;
}

bool uds_security_app_calculate_key_level2(const uint8_t seed[16], uint8_t key[16]) {
    if ((seed == NULL) || (key == NULL)) {
        return false;
    }
    if (s_custom_key_provider != NULL) {
        return s_custom_key_provider(2U, seed, key);
    }
    const uint8_t *master_key =
        s_has_provisioned_key ? s_provisioned_master_key : s_default_reference_key;
    return uds_security_cmac_derive_key(master_key, seed, key);
}

UdsCallbackResult uds_security_app_seed(void *context, uint8_t level, uint8_t *seed,
                                        uint16_t *length, uint16_t capacity) {
    (void)context;
    if ((seed == NULL) || (length == NULL)) {
        return UDS_RESULT_ERROR;
    }

    if (level == UDS_SECURITY_LEVEL_1) {
        if (capacity < UDS_SECURITY_APP_LEVEL1_SEED_LEN) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        (void)csprng_generate_bytes(s_level1_active_seed, UDS_SECURITY_APP_LEVEL1_SEED_LEN);
        if ((s_level1_active_seed[0] | s_level1_active_seed[1] | s_level1_active_seed[2] |
             s_level1_active_seed[3]) == 0U) {
            s_level1_active_seed[0] = 0xA5U;
            s_level1_active_seed[1] = 0x5AU;
        }

        (void)memcpy(seed, s_level1_active_seed, UDS_SECURITY_APP_LEVEL1_SEED_LEN);
        *length = UDS_SECURITY_APP_LEVEL1_SEED_LEN;
        s_level1_seed_valid = true;
        return UDS_RESULT_OK;
    }

    if (level == UDS_SECURITY_LEVEL_2) {
        if (capacity < UDS_SECURITY_APP_LEVEL2_SEED_LEN) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        (void)csprng_generate_bytes(s_level2_active_seed, UDS_SECURITY_APP_LEVEL2_SEED_LEN);
        bool all_zero = true;
        for (uint8_t i = 0U; i < UDS_SECURITY_APP_LEVEL2_SEED_LEN; ++i) {
            if (s_level2_active_seed[i] != 0U) {
                all_zero = false;
                break;
            }
        }
        if (all_zero) {
            s_level2_active_seed[0] = 0xA5U;
        }
        (void)memcpy(seed, s_level2_active_seed, UDS_SECURITY_APP_LEVEL2_SEED_LEN);
        *length = UDS_SECURITY_APP_LEVEL2_SEED_LEN;
        s_level2_seed_valid = true;
        return UDS_RESULT_OK;
    }

    return UDS_RESULT_OUT_OF_RANGE;
}

UdsCallbackResult uds_security_app_key(void *context, uint8_t level, const uint8_t *key,
                                       uint16_t length) {
    (void)context;
    if (key == NULL) {
        return UDS_RESULT_ERROR;
    }

    if (level == UDS_SECURITY_LEVEL_1) {
        if ((length != UDS_SECURITY_APP_LEVEL1_KEY_LEN) || !s_level1_seed_valid) {
            s_level1_seed_valid = false;
            return UDS_RESULT_INVALID_KEY;
        }
        uint8_t expected_key[UDS_SECURITY_APP_LEVEL1_KEY_LEN];
        if (!uds_security_app_calculate_key_level1(s_level1_active_seed, expected_key)) {
            s_level1_seed_valid = false;
            return UDS_RESULT_ERROR;
        }
        s_level1_seed_valid = false;
        if (constant_time_equal_4(key, expected_key) == 0U) {
            return UDS_RESULT_INVALID_KEY;
        }
        return UDS_RESULT_OK;
    }

    if (level == UDS_SECURITY_LEVEL_2) {
        if ((length != UDS_SECURITY_APP_LEVEL2_KEY_LEN) || !s_level2_seed_valid) {
            s_level2_seed_valid = false;
            return UDS_RESULT_INVALID_KEY;
        }
        uint8_t expected_key[UDS_SECURITY_APP_LEVEL2_KEY_LEN];
        if (!uds_security_app_calculate_key_level2(s_level2_active_seed, expected_key)) {
            s_level2_seed_valid = false;
            return UDS_RESULT_ERROR;
        }
        s_level2_seed_valid = false;
        if (!uds_security_cmac_constant_time_equal(key, expected_key)) {
            return UDS_RESULT_INVALID_KEY;
        }
        return UDS_RESULT_OK;
    }

    return UDS_RESULT_OUT_OF_RANGE;
}
