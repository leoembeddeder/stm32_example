/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#include "uds_security_app.h"
#include "uds_iso_tp/uds.h"
#include "uds_iso_tp/uds_services.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static void test_direct_security_app_level1(void) {
    uds_security_app_init();

    uint8_t seed[UDS_SECURITY_APP_LEVEL1_SEED_LEN] = {0U};
    uint16_t seed_len = 0U;

    /* Capacity check */
    assert(uds_security_app_seed(NULL, UDS_SECURITY_LEVEL_1, seed, &seed_len, 3U) ==
           UDS_RESULT_RESPONSE_TOO_LONG);

    /* Generate Level 1 seed */
    assert(uds_security_app_seed(NULL, UDS_SECURITY_LEVEL_1, seed, &seed_len, sizeof(seed)) ==
           UDS_RESULT_OK);
    assert(seed_len == 4U);
    /* Seed must not be all zeros */
    assert((seed[0] | seed[1] | seed[2] | seed[3]) != 0U);

    /* Compute key */
    uint8_t key[UDS_SECURITY_APP_LEVEL1_KEY_LEN] = {0U};
    assert(uds_security_app_calculate_key_level1(seed, key));
    assert(key[0] == (uint8_t)(seed[0] ^ UDS_SECURITY_LEVEL1_MASK_BYTE0));
    assert(key[1] == (uint8_t)(seed[1] ^ UDS_SECURITY_LEVEL1_MASK_BYTE1));
    assert(key[2] == (uint8_t)(seed[2] ^ UDS_SECURITY_LEVEL1_MASK_BYTE2));
    assert(key[3] == (uint8_t)(seed[3] ^ UDS_SECURITY_LEVEL1_MASK_BYTE3));

    /* Verify with wrong key length */
    assert(uds_security_app_key(NULL, UDS_SECURITY_LEVEL_1, key, 3U) == UDS_RESULT_INVALID_KEY);

    /* Re-seed because failure invalidates */
    assert(uds_security_app_seed(NULL, UDS_SECURITY_LEVEL_1, seed, &seed_len, sizeof(seed)) ==
           UDS_RESULT_OK);
    assert(uds_security_app_calculate_key_level1(seed, key));

    /* Verify with corrupted key */
    uint8_t bad_key[4];
    (void)memcpy(bad_key, key, 4U);
    bad_key[0] ^= 0x01U;
    assert(uds_security_app_key(NULL, UDS_SECURITY_LEVEL_1, bad_key, sizeof(bad_key)) ==
           UDS_RESULT_INVALID_KEY);

    /* Second attempt without re-seed must fail (one-time seed requirement) */
    assert(uds_security_app_key(NULL, UDS_SECURITY_LEVEL_1, key, sizeof(key)) ==
           UDS_RESULT_INVALID_KEY);

    /* Re-seed and verify valid key */
    assert(uds_security_app_seed(NULL, UDS_SECURITY_LEVEL_1, seed, &seed_len, sizeof(seed)) ==
           UDS_RESULT_OK);
    assert(uds_security_app_calculate_key_level1(seed, key));
    assert(uds_security_app_key(NULL, UDS_SECURITY_LEVEL_1, key, sizeof(key)) == UDS_RESULT_OK);

    /* Subsequent verification without new seed must fail */
    assert(uds_security_app_key(NULL, UDS_SECURITY_LEVEL_1, key, sizeof(key)) ==
           UDS_RESULT_INVALID_KEY);
}

static void test_direct_security_app_level2(void) {
    uds_security_app_init();

    uint8_t seed[UDS_SECURITY_APP_LEVEL2_SEED_LEN] = {0U};
    uint16_t seed_len = 0U;

    /* Capacity check */
    assert(uds_security_app_seed(NULL, UDS_SECURITY_LEVEL_2, seed, &seed_len, 15U) ==
           UDS_RESULT_RESPONSE_TOO_LONG);

    /* Generate Level 2 seed */
    assert(uds_security_app_seed(NULL, UDS_SECURITY_LEVEL_2, seed, &seed_len, sizeof(seed)) ==
           UDS_RESULT_OK);
    assert(seed_len == 16U);

    /* Compute Level 2 key (AES-CMAC-128) */
    uint8_t key[UDS_SECURITY_APP_LEVEL2_KEY_LEN] = {0U};
    assert(uds_security_app_calculate_key_level2(seed, key));

    /* Verify with wrong key length */
    assert(uds_security_app_key(NULL, UDS_SECURITY_LEVEL_2, key, 8U) == UDS_RESULT_INVALID_KEY);

    /* Re-seed and verify with wrong key */
    assert(uds_security_app_seed(NULL, UDS_SECURITY_LEVEL_2, seed, &seed_len, sizeof(seed)) ==
           UDS_RESULT_OK);
    assert(uds_security_app_calculate_key_level2(seed, key));

    uint8_t bad_key[16];
    (void)memcpy(bad_key, key, 16U);
    bad_key[15] ^= 0x42U;
    assert(uds_security_app_key(NULL, UDS_SECURITY_LEVEL_2, bad_key, sizeof(bad_key)) ==
           UDS_RESULT_INVALID_KEY);

    /* Re-seed and verify valid key */
    assert(uds_security_app_seed(NULL, UDS_SECURITY_LEVEL_2, seed, &seed_len, sizeof(seed)) ==
           UDS_RESULT_OK);
    assert(uds_security_app_calculate_key_level2(seed, key));
    assert(uds_security_app_key(NULL, UDS_SECURITY_LEVEL_2, key, sizeof(key)) == UDS_RESULT_OK);

    /* Subsequent verification without new seed must fail */
    assert(uds_security_app_key(NULL, UDS_SECURITY_LEVEL_2, key, sizeof(key)) ==
           UDS_RESULT_INVALID_KEY);
}

static void test_direct_security_app_invalid_levels(void) {
    uint8_t buf[32];
    uint16_t len = 0U;

    /* Unsupported levels */
    assert(uds_security_app_seed(NULL, 0U, buf, &len, sizeof(buf)) == UDS_RESULT_OUT_OF_RANGE);
    assert(uds_security_app_seed(NULL, 3U, buf, &len, sizeof(buf)) == UDS_RESULT_OUT_OF_RANGE);
    assert(uds_security_app_seed(NULL, 5U, buf, &len, sizeof(buf)) == UDS_RESULT_OUT_OF_RANGE);

    assert(uds_security_app_key(NULL, 0U, buf, 4U) == UDS_RESULT_OUT_OF_RANGE);
    assert(uds_security_app_key(NULL, 3U, buf, 4U) == UDS_RESULT_OUT_OF_RANGE);
    assert(uds_security_app_key(NULL, 5U, buf, 4U) == UDS_RESULT_OUT_OF_RANGE);
}

static void test_server_multilevel_security_access(void) {
    uds_security_app_init();

    UdsServer server;
    UdsCallbacks callbacks = {0};
    callbacks.security_seed = uds_security_app_seed;
    callbacks.security_key = uds_security_app_key;
    uds_server_init(&server, &callbacks, NULL, 0U);

    uint8_t response[256];
    uint16_t resp_len = 0U;

    /* 1. In Default Session (0x01), SecurityAccess (0x27) is not allowed */
    uint8_t req_seed_l1[] = {0x27U, 0x01U};
    assert(uds_server_handle(&server, req_seed_l1, sizeof(req_seed_l1), response, &resp_len,
                             sizeof(response), 10U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x27U &&
           response[2] == UDS_NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);

    /* 2. Switch to Extended Diagnostic Session (0x10 0x03) */
    uint8_t req_ext_session[] = {0x10U, 0x03U};
    assert(uds_server_handle(&server, req_ext_session, sizeof(req_ext_session), response, &resp_len,
                             sizeof(response), 20U) == UDS_RESULT_OK);
    assert(response[0] == 0x50U && response[1] == 0x03U);

    /* 3. Level 1 Flow: Request Seed (0x27 0x01) */
    assert(uds_server_handle(&server, req_seed_l1, sizeof(req_seed_l1), response, &resp_len,
                             sizeof(response), 30U) == UDS_RESULT_OK);
    assert(resp_len == (2U + UDS_SECURITY_APP_LEVEL1_SEED_LEN));
    assert(response[0] == 0x67U && response[1] == 0x01U);

    uint8_t seed_l1[4];
    (void)memcpy(seed_l1, &response[2], 4U);

    /* Calculate Level 1 key */
    uint8_t key_l1[4];
    assert(uds_security_app_calculate_key_level1(seed_l1, key_l1));

    /* Level 1 Flow: Send Key (0x27 0x02) */
    uint8_t req_key_l1[6];
    req_key_l1[0] = 0x27U;
    req_key_l1[1] = 0x02U;
    (void)memcpy(&req_key_l1[2], key_l1, 4U);
    assert(uds_server_handle(&server, req_key_l1, sizeof(req_key_l1), response, &resp_len,
                             sizeof(response), 40U) == UDS_RESULT_OK);
    assert(resp_len == 2U);
    assert(response[0] == 0x67U && response[1] == 0x02U);
    assert(uds_server_security_level(&server) == 1U);

    /* 4. ISO 14229-1 Zero Seed test: Request Seed for Level 1 when already unlocked */
    assert(uds_server_handle(&server, req_seed_l1, sizeof(req_seed_l1), response, &resp_len,
                             sizeof(response), 50U) == UDS_RESULT_OK);
    assert(resp_len == (2U + UDS_SECURITY_APP_LEVEL1_SEED_LEN));
    assert(response[0] == 0x67U && response[1] == 0x01U);
    assert(response[2] == 0x00U && response[3] == 0x00U && response[4] == 0x00U &&
           response[5] == 0x00U);

    /* 5. Level 2 Flow: Request Seed (0x27 0x03) */
    uint8_t req_seed_l2[] = {0x27U, 0x03U};
    assert(uds_server_handle(&server, req_seed_l2, sizeof(req_seed_l2), response, &resp_len,
                             sizeof(response), 60U) == UDS_RESULT_OK);
    assert(resp_len == (2U + UDS_SECURITY_APP_LEVEL2_SEED_LEN));
    assert(response[0] == 0x67U && response[1] == 0x03U);

    uint8_t seed_l2[16];
    (void)memcpy(seed_l2, &response[2], 16U);

    /* Calculate Level 2 key (AES-CMAC) */
    uint8_t key_l2[16];
    assert(uds_security_app_calculate_key_level2(seed_l2, key_l2));

    /* Level 2 Flow: Send Key (0x27 0x04) */
    uint8_t req_key_l2[18];
    req_key_l2[0] = 0x27U;
    req_key_l2[1] = 0x04U;
    (void)memcpy(&req_key_l2[2], key_l2, 16U);
    assert(uds_server_handle(&server, req_key_l2, sizeof(req_key_l2), response, &resp_len,
                             sizeof(response), 70U) == UDS_RESULT_OK);
    assert(resp_len == 2U);
    assert(response[0] == 0x67U && response[1] == 0x04U);
    assert(uds_server_security_level(&server) == 2U);

    /* 6. ISO 14229-1 Zero Seed test: Request Seed for Level 2 when already unlocked */
    assert(uds_server_handle(&server, req_seed_l2, sizeof(req_seed_l2), response, &resp_len,
                             sizeof(response), 80U) == UDS_RESULT_OK);
    assert(resp_len == (2U + UDS_SECURITY_APP_LEVEL2_SEED_LEN));
    assert(response[0] == 0x67U && response[1] == 0x03U);
    for (uint8_t idx = 0U; idx < 16U; ++idx) {
        assert(response[2U + idx] == 0x00U);
    }
}

static void test_server_security_error_handling(void) {
    uds_security_app_init();

    UdsServer server;
    UdsCallbacks callbacks = {0};
    callbacks.security_seed = uds_security_app_seed;
    callbacks.security_key = uds_security_app_key;
    uds_server_init(&server, &callbacks, NULL, 0U);

    uint8_t response[256];
    uint16_t resp_len = 0U;

    uint8_t req_ext_session[] = {0x10U, 0x03U};
    assert(uds_server_handle(&server, req_ext_session, sizeof(req_ext_session), response, &resp_len,
                             sizeof(response), 0U) == UDS_RESULT_OK);

    /* Sequence error: SendKey without RequestSeed */
    uint8_t req_key_l1[6] = {0x27U, 0x02U, 0x11U, 0x22U, 0x33U, 0x44U};
    assert(uds_server_handle(&server, req_key_l1, sizeof(req_key_l1), response, &resp_len,
                             sizeof(response), 10U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x27U &&
           response[2] == UDS_NRC_REQUEST_SEQUENCE_ERROR);

    /* Cross-level sequence error: Request Level 1 seed, send Level 2 key */
    uint8_t req_seed_l1[] = {0x27U, 0x01U};
    assert(uds_server_handle(&server, req_seed_l1, sizeof(req_seed_l1), response, &resp_len,
                             sizeof(response), 20U) == UDS_RESULT_OK);

    uint8_t req_key_l2[18] = {0x27U, 0x04U};
    assert(uds_server_handle(&server, req_key_l2, sizeof(req_key_l2), response, &resp_len,
                             sizeof(response), 30U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x27U &&
           response[2] == UDS_NRC_REQUEST_SEQUENCE_ERROR);

    /* Invalid key for Level 1 */
    assert(uds_server_handle(&server, req_seed_l1, sizeof(req_seed_l1), response, &resp_len,
                             sizeof(response), 40U) == UDS_RESULT_OK);
    req_key_l1[2] = 0xDEU;
    req_key_l1[3] = 0xADU;
    req_key_l1[4] = 0xBEU;
    req_key_l1[5] = 0xEFU;
    assert(uds_server_handle(&server, req_key_l1, sizeof(req_key_l1), response, &resp_len,
                             sizeof(response), 50U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x27U && response[2] == UDS_NRC_INVALID_KEY);

    /* Lockout test: 3 consecutive failed attempts triggers 0x36 */
    /* Attempt 1 was above (at 50ms) */
    /* Attempt 2 */
    assert(uds_server_handle(&server, req_seed_l1, sizeof(req_seed_l1), response, &resp_len,
                             sizeof(response), 60U) == UDS_RESULT_OK);
    assert(uds_server_handle(&server, req_key_l1, sizeof(req_key_l1), response, &resp_len,
                             sizeof(response), 70U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x27U && response[2] == UDS_NRC_INVALID_KEY);

    /* Attempt 3 */
    assert(uds_server_handle(&server, req_seed_l1, sizeof(req_seed_l1), response, &resp_len,
                             sizeof(response), 80U) == UDS_RESULT_OK);
    assert(uds_server_handle(&server, req_key_l1, sizeof(req_key_l1), response, &resp_len,
                             sizeof(response), 90U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x27U &&
           response[2] == UDS_NRC_EXCEEDED_NUMBER_OF_ATTEMPTS);

    /* Cooldown delay not expired (0x37) */
    assert(uds_server_handle(&server, req_seed_l1, sizeof(req_seed_l1), response, &resp_len,
                             sizeof(response), 100U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x27U &&
           response[2] == UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED);
}

static bool mock_entropy_source(uint8_t *buffer, size_t length) {
    for (size_t i = 0U; i < length; ++i) {
        buffer[i] = (uint8_t)(0x40U + i);
    }
    return true;
}

static bool mock_key_provider(uint8_t level, const uint8_t *seed, uint8_t *key_out) {
    if ((level != 2U) || (seed == NULL) || (key_out == NULL)) {
        return false;
    }
    for (size_t i = 0U; i < 16U; ++i) {
        key_out[i] = (uint8_t)(seed[i] ^ 0xEEU);
    }
    return true;
}

static void test_key_provisioning_and_entropy(void) {
    uds_security_app_init();
    assert(!uds_security_app_has_provisioned_key());

    /* 1. Test entropy source injection */
    uds_security_app_set_entropy_source(mock_entropy_source);
    uint8_t seed[UDS_SECURITY_APP_LEVEL2_SEED_LEN] = {0U};
    uint16_t seed_len = 0U;
    assert(uds_security_app_seed(NULL, UDS_SECURITY_LEVEL_2, seed, &seed_len, sizeof(seed)) ==
           UDS_RESULT_OK);
    assert(seed_len == 16U);
    assert(seed[0] == 0x40U && seed[1] == 0x41U);

    /* Reset entropy source back to default CSPRNG */
    uds_security_app_set_entropy_source(NULL);

    /* 2. Test master key provisioning */
    const uint8_t custom_master_key[16] = {0x01U, 0x12U, 0x23U, 0x34U, 0x45U, 0x56U, 0x67U, 0x78U,
                                           0x89U, 0x9AU, 0xABU, 0xBCU, 0xCDU, 0xDEU, 0xEFU, 0xF0U};
    assert(uds_security_app_provision_master_key(custom_master_key));
    assert(uds_security_app_has_provisioned_key());

    uint8_t key_provisioned[16] = {0U};
    assert(uds_security_app_calculate_key_level2(seed, key_provisioned));

    /* Key must verify against the provisioned master key */
    assert(uds_security_app_key(NULL, UDS_SECURITY_LEVEL_2, key_provisioned,
                                sizeof(key_provisioned)) == UDS_RESULT_OK);

    /* 3. Test custom key provider (e.g. HSM / secure element) */
    uds_security_app_set_key_provider(mock_key_provider);
    assert(uds_security_app_seed(NULL, UDS_SECURITY_LEVEL_2, seed, &seed_len, sizeof(seed)) ==
           UDS_RESULT_OK);
    uint8_t key_hsm[16] = {0U};
    assert(uds_security_app_calculate_key_level2(seed, key_hsm));
    assert(key_hsm[0] == (uint8_t)(seed[0] ^ 0xEEU));
    assert(uds_security_app_key(NULL, UDS_SECURITY_LEVEL_2, key_hsm, sizeof(key_hsm)) ==
           UDS_RESULT_OK);

    /* Clean up key provider */
    uds_security_app_set_key_provider(NULL);
}

int main(void) {
    test_direct_security_app_level1();
    test_direct_security_app_level2();
    test_direct_security_app_invalid_levels();
    test_server_multilevel_security_access();
    test_server_security_error_handling();
    test_key_provisioning_and_entropy();
    return 0;
}
