/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#include "uds_security_app.h"

#include "uds_platform.h"
#include "uds_security_cmac.h"

#include <string.h>

static const uint8_t s_sec_master_key[16] = {0x2BU, 0x7EU, 0x15U, 0x16U, 0x28U, 0xAEU,
                                             0xD2U, 0xA6U, 0xABU, 0xF7U, 0x15U, 0x88U,
                                             0x09U, 0xCFU, 0x4FU, 0x3CU};

static uint8_t s_level1_active_seed[UDS_SECURITY_APP_LEVEL1_SEED_LEN];
static bool s_level1_seed_valid = false;

static uint8_t s_level2_active_seed[UDS_SECURITY_APP_LEVEL2_SEED_LEN] = {
    0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U, 0x08U,
    0x09U, 0x0AU, 0x0BU, 0x0CU, 0x0DU, 0x0EU, 0x0FU, 0x10U};
static bool s_level2_seed_valid = false;

static uint32_t s_prng_state = 0x98765432UL;

static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13U;
    x ^= x >> 17U;
    x ^= x << 5U;
    *state = (x == 0U) ? 0x13579BDFUL : x;
    return *state;
}

static uint8_t constant_time_equal_4(const uint8_t left[4], const uint8_t right[4]) {
    uint8_t diff = 0U;
    for (uint8_t i = 0U; i < 4U; ++i) {
        diff |= (uint8_t)(left[i] ^ right[i]);
    }
    return (diff == 0U) ? 1U : 0U;
}

void uds_security_app_init(void) {
    s_prng_state = uds_platform_systick_val();
    if (s_prng_state == 0U) {
        s_prng_state = 0x2468ACE0UL;
    }
    s_level1_seed_valid = false;
    s_level2_seed_valid = false;
    (void)memset(s_level1_active_seed, 0, sizeof(s_level1_active_seed));
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
    return uds_security_cmac_derive_key(s_sec_master_key, seed, key);
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
        uint32_t systick = uds_platform_systick_val();
        s_prng_state ^= (systick != 0U) ? systick : 0x5A5AA5A5UL;
        uint32_t random_val = xorshift32(&s_prng_state);
        if (random_val == 0U) {
            random_val = 0x12345678UL;
        }
        s_level1_active_seed[0] = (uint8_t)(random_val >> 24U);
        s_level1_active_seed[1] = (uint8_t)(random_val >> 16U);
        s_level1_active_seed[2] = (uint8_t)(random_val >> 8U);
        s_level1_active_seed[3] = (uint8_t)(random_val);

        (void)memcpy(seed, s_level1_active_seed, UDS_SECURITY_APP_LEVEL1_SEED_LEN);
        *length = UDS_SECURITY_APP_LEVEL1_SEED_LEN;
        s_level1_seed_valid = true;
        return UDS_RESULT_OK;
    }

    if (level == UDS_SECURITY_LEVEL_2) {
        if (capacity < UDS_SECURITY_APP_LEVEL2_SEED_LEN) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        bool all_zero = true;
        for (uint8_t i = 0U; i < UDS_SECURITY_APP_LEVEL2_SEED_LEN; ++i) {
            uint32_t r = xorshift32(&s_prng_state);
            s_level2_active_seed[i] =
                (uint8_t)(s_level2_active_seed[i] + (uint8_t)(r ^ (uint8_t)(i * 3U + 0x21U)));
            if (s_level2_active_seed[i] != 0U) {
                all_zero = false;
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
