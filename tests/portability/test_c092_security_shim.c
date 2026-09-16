/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#include "uds_security_app.h"
#include "uds_platform_fdcan.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Mock HAL state */
static uint32_t s_mock_tick = 5000U;

uint32_t HAL_GetTick(void) {
    s_mock_tick += 137U;
    return s_mock_tick;
}

void NVIC_SystemReset(void) {}

static void test_c092_security_level1(void) {
    uds_security_app_init();

    uint8_t seed[UDS_SECURITY_APP_LEVEL1_SEED_LEN] = {0U};
    uint16_t seed_len = 0U;

    assert(uds_security_app_seed(NULL, UDS_SECURITY_LEVEL_1, seed, &seed_len, sizeof(seed)) ==
           UDS_RESULT_OK);
    assert(seed_len == 4U);
    assert((seed[0] | seed[1] | seed[2] | seed[3]) != 0U);

    uint8_t key[UDS_SECURITY_APP_LEVEL1_KEY_LEN] = {0U};
    assert(uds_security_app_calculate_key_level1(seed, key));
    assert(uds_security_app_key(NULL, UDS_SECURITY_LEVEL_1, key, sizeof(key)) == UDS_RESULT_OK);
}

static void test_c092_security_level2_cmac(void) {
    uds_security_app_init();

    uint8_t seed[UDS_SECURITY_APP_LEVEL2_SEED_LEN] = {0U};
    uint16_t seed_len = 0U;

    assert(uds_security_app_seed(NULL, UDS_SECURITY_LEVEL_2, seed, &seed_len, sizeof(seed)) ==
           UDS_RESULT_OK);
    assert(seed_len == 16U);

    uint8_t key[UDS_SECURITY_APP_LEVEL2_KEY_LEN] = {0U};
    assert(uds_security_app_calculate_key_level2(seed, key));
    assert(uds_security_app_key(NULL, UDS_SECURITY_LEVEL_2, key, sizeof(key)) == UDS_RESULT_OK);
}

static void test_c092_platform_fallback_bindings(void) {
    /* Verify standard platform bindings mapped to C092 equivalents */
    uint32_t now = uds_platform_now_ms();
    assert(now == s_mock_tick);
    assert(uds_platform_systick_val() != 0U);
    uint8_t rand_buf[8] = {0U};
    assert(!uds_platform_trng_get_random(rand_buf, sizeof(rand_buf)));
    uds_platform_error();
    uds_platform_system_reset(UDS_RESET_TYPE_HARD);
}

int main(void) {
    test_c092_security_level1();
    test_c092_security_level2_cmac();
    test_c092_platform_fallback_bindings();
    return 0;
}
