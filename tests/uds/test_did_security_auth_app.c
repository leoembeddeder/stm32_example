#include "uds_auth_app.h"
#include "uds_did_app.h"
#include "uds_dtc_app.h"
#include "uds_iso_tp/uds.h"
#include "uds_iso_tp/uds_services.h"
#include "uds_security_cmac.h"

#include <assert.h>
#include <string.h>

static const uint8_t s_test_master_key[16] = {0x2BU, 0x7EU, 0x15U, 0x16U, 0x28U, 0xAEU,
                                              0xD2U, 0xA6U, 0xABU, 0xF7U, 0x15U, 0x88U,
                                              0x09U, 0xCFU, 0x4FU, 0x3CU};

static uint8_t s_test_seed[16] = {0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U, 0x88U,
                                  0x99U, 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU, 0x00U};

static UdsCallbackResult mock_sec_seed(void *context, uint8_t level, uint8_t *seed,
                                       uint16_t *length, uint16_t capacity) {
    (void)context;
    (void)level;
    if (capacity < 16U) {
        return UDS_RESULT_OUT_OF_RANGE;
    }
    (void)memcpy(seed, s_test_seed, 16U);
    *length = 16U;
    return UDS_RESULT_OK;
}

static UdsCallbackResult mock_sec_key(void *context, uint8_t level, const uint8_t *key,
                                      uint16_t length) {
    (void)context;
    (void)level;
    if (length != 16U) {
        return UDS_RESULT_INVALID_KEY;
    }
    uint8_t expected[16];
    if (!uds_security_cmac_derive_key(s_test_master_key, s_test_seed, expected)) {
        return UDS_RESULT_ERROR;
    }
    if (!uds_security_cmac_constant_time_equal(key, expected)) {
        return UDS_RESULT_INVALID_KEY;
    }
    return UDS_RESULT_OK;
}

static void test_multi_did_operations(void) {
    uds_did_app_init();

    UdsServer server;
    UdsCallbacks callbacks = {0};
    callbacks.read_did = uds_did_app_read;
    callbacks.write_did = uds_did_app_write;
    uds_server_init(&server, &callbacks, NULL, 1000U);

    uint8_t response[256];
    uint16_t resp_len = 0U;

    /* 1. Read single DID: 0xF186 ActiveDiagnosticSession */
    uint8_t req_f186[] = {0x22U, 0xF1U, 0x86U};
    assert(uds_server_handle(&server, req_f186, sizeof(req_f186), response, &resp_len,
                             sizeof(response), 1000U) == UDS_RESULT_OK);
    assert(resp_len == 4U);
    assert(response[0] == 0x62U && response[1] == 0xF1U && response[2] == 0x86U &&
           response[3] == 0x01U);

    /* 2. Read single DID: 0xF181 HIBManufacturerSparePartNumber */
    uint8_t req_f181[] = {0x22U, 0xF1U, 0x81U};
    assert(uds_server_handle(&server, req_f181, sizeof(req_f181), response, &resp_len,
                             sizeof(response), 1000U) == UDS_RESULT_OK);
    assert(resp_len == (3U + 16U));
    assert(memcmp(&response[3], "HIB-HCU-SPARE-01", 16) == 0);

    /* 3. Multi-DID Read in a single request: 0xF186 + 0xF190 */
    uint8_t req_multi[] = {0x22U, 0xF1U, 0x86U, 0xF1U, 0x90U};
    assert(uds_server_handle(&server, req_multi, sizeof(req_multi), response, &resp_len,
                             sizeof(response), 1000U) == UDS_RESULT_OK);
    assert(resp_len == (1U + (2U + 1U) + (2U + 17U)));
    assert(response[0] == 0x62U);
    assert(response[1] == 0xF1U && response[2] == 0x86U && response[3] == 0x01U);
    assert(response[4] == 0xF1U && response[5] == 0x90U);
    assert(memcmp(&response[6], "stm32f767-uds-vin", 17) == 0);

    /* 4. Write to writable DID: 0xF190 VIN */
    uint8_t req_write_vin[] = {0x2EU, 0xF1U, 0x90U, 'M', 'Y', 'N', 'E', 'W', 'V', 'I',
                               'N',   '1',   '2',   '3', '4', '5', '6', '7', '8', '9'};
    assert(uds_server_handle(&server, req_write_vin, sizeof(req_write_vin), response, &resp_len,
                             sizeof(response), 1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x6EU && response[1] == 0xF1U && response[2] == 0x90U);

    /* Verify updated VIN via read */
    assert(uds_server_handle(&server, req_multi, sizeof(req_multi), response, &resp_len,
                             sizeof(response), 1000U) == UDS_RESULT_OK);
    assert(memcmp(&response[6], "MYNEWVIN123456789", 17) == 0);

    /* 5. Attempt write to read-only DID: 0xF181 must be rejected */
    uint8_t req_write_ro[] = {0x2EU, 0xF1U, 0x81U, 0xAAU, 0xBBU};
    assert(uds_server_handle(&server, req_write_ro, sizeof(req_write_ro), response, &resp_len,
                             sizeof(response), 1000U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x2EU &&
           response[2] == UDS_NRC_CONDITIONS_NOT_CORRECT);
}

static void test_aes_cmac_security_access_flow(void) {
    UdsServer server;
    UdsCallbacks callbacks = {0};
    callbacks.security_seed = mock_sec_seed;
    callbacks.security_key = mock_sec_key;
    uds_server_init(&server, &callbacks, NULL, 0U);

    uint8_t response[256];
    uint16_t resp_len = 0U;

    /* Transition to Extended Session for security access */
    uint8_t req_ext_session[] = {0x10U, 0x03U};
    assert(uds_server_handle(&server, req_ext_session, sizeof(req_ext_session), response, &resp_len,
                             sizeof(response), 0U) == UDS_RESULT_OK);
    assert(response[0] == 0x50U && response[1] == 0x03U);

    /* 1. Request Seed (0x27 0x01) */
    uint8_t req_seed[] = {0x27U, 0x01U};
    assert(uds_server_handle(&server, req_seed, sizeof(req_seed), response, &resp_len,
                             sizeof(response), 0U) == UDS_RESULT_OK);
    assert(resp_len == (2U + 16U));
    assert(response[0] == 0x67U && response[1] == 0x01U);
    assert(memcmp(&response[2], s_test_seed, 16) == 0);

    /* 2. Compute correct AES-CMAC-128 key */
    uint8_t correct_key[16];
    assert(uds_security_cmac_derive_key(s_test_master_key, s_test_seed, correct_key));

    /* 3. Send Key (0x27 0x02) - valid key */
    uint8_t req_key[18];
    req_key[0] = 0x27U;
    req_key[1] = 0x02U;
    (void)memcpy(&req_key[2], correct_key, 16U);
    assert(uds_server_handle(&server, req_key, sizeof(req_key), response, &resp_len,
                             sizeof(response), 10U) == UDS_RESULT_OK);
    assert(response[0] == 0x67U && response[1] == 0x02U);

    /* 4. Test Lockout: Reset and fail 3 times */
    uds_server_init(&server, &callbacks, NULL, 0U);
    assert(uds_server_handle(&server, req_ext_session, sizeof(req_ext_session), response, &resp_len,
                             sizeof(response), 0U) == UDS_RESULT_OK);

    uint8_t bad_key[18] = {0x27U, 0x02U, 0xDEU, 0xADU, 0xBEU, 0xEFU};

    /* Attempt 1 */
    assert(uds_server_handle(&server, req_seed, sizeof(req_seed), response, &resp_len,
                             sizeof(response), 100U) == UDS_RESULT_OK);
    assert(uds_server_handle(&server, bad_key, sizeof(bad_key), response, &resp_len,
                             sizeof(response), 200U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x27U && response[2] == UDS_NRC_INVALID_KEY);

    /* Attempt 2 */
    assert(uds_server_handle(&server, req_seed, sizeof(req_seed), response, &resp_len,
                             sizeof(response), 300U) == UDS_RESULT_OK);
    assert(uds_server_handle(&server, bad_key, sizeof(bad_key), response, &resp_len,
                             sizeof(response), 400U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x27U && response[2] == UDS_NRC_INVALID_KEY);

    /* Attempt 3 -> triggers exceeded number of attempts (0x36) */
    assert(uds_server_handle(&server, req_seed, sizeof(req_seed), response, &resp_len,
                             sizeof(response), 500U) == UDS_RESULT_OK);
    assert(uds_server_handle(&server, bad_key, sizeof(bad_key), response, &resp_len,
                             sizeof(response), 600U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x27U &&
           response[2] == UDS_NRC_EXCEEDED_NUMBER_OF_ATTEMPTS);

    /* 5. During 10s cooldown, Request Seed must be rejected with 0x37 */
    assert(uds_server_handle(&server, req_seed, sizeof(req_seed), response, &resp_len,
                             sizeof(response), 700U) == UDS_RESULT_OK);
    assert(response[0] == 0x7FU && response[1] == 0x27U &&
           response[2] == UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED);

    /* 6. After 10s cooldown (e.g. at 10700 ms), Request Seed is accepted again */
    assert(uds_server_handle(&server, req_seed, sizeof(req_seed), response, &resp_len,
                             sizeof(response), 10700U) == UDS_RESULT_OK);
    assert(response[0] == 0x67U && response[1] == 0x01U);
}

static void test_service_0x14_group_clearing(void) {
    uds_dtc_app_init();

    /* Clear Powertrain group (0x000000) */
    assert(uds_dtc_app_clear(NULL, 0x000000UL) == UDS_RESULT_OK);

    /* Clear Chassis group (0x400000) */
    assert(uds_dtc_app_set_fault(0x401000UL, 0x2FU, 0x20U, 10));
    assert(uds_dtc_app_clear(NULL, 0x400000UL) == UDS_RESULT_OK);

    /* Clear Body group (0x800000) */
    assert(uds_dtc_app_set_fault(0x800100UL, 0x2FU, 0x20U, 10));
    assert(uds_dtc_app_clear(NULL, 0x800000UL) == UDS_RESULT_OK);

    /* Clear Network group (0xC00000) */
    assert(uds_dtc_app_set_fault(0xC10000UL, 0x2FU, 0x20U, 10));
    assert(uds_dtc_app_clear(NULL, 0xC00000UL) == UDS_RESULT_OK);

    /* Clear All DTCs (0xFFFFFF) */
    assert(uds_dtc_app_clear(NULL, 0xFFFFFFUL) == UDS_RESULT_OK);
}

static void test_service_0x29_authentication(void) {
    uds_auth_app_init();
    const UdsAuthenticationServiceBackend *backend = uds_auth_app_get_backend();
    assert(backend != NULL && backend->authentication != NULL);

    uint8_t response[256];
    uint16_t resp_len = 0U;

    /* 1. Subfunction 0x08: authenticationConfiguration */
    uint8_t req_cfg[] = {0x29U, 0x08U};
    assert(backend->authentication(NULL, req_cfg, sizeof(req_cfg), response, &resp_len,
                                   sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len == 4U && response[0] == 0x69U && response[1] == 0x08U);

    /* 2. Subfunction 0x05: requestChallengeForAuthentication */
    uint8_t req_chal[] = {0x29U, 0x05U};
    assert(backend->authentication(NULL, req_chal, sizeof(req_chal), response, &resp_len,
                                   sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len == 18U && response[0] == 0x69U && response[1] == 0x05U);

    /* Extract challenge */
    uint8_t challenge[16];
    (void)memcpy(challenge, &response[2], 16U);

    /* 3. Compute proof using AES-CMAC-128 */
    uint8_t proof[16];
    assert(uds_security_cmac_derive_key(s_test_master_key, challenge, proof));

    /* 4. Subfunction 0x06: verifyProofOfOwnershipUnidirectional with valid proof */
    uint8_t req_proof[18];
    req_proof[0] = 0x29U;
    req_proof[1] = 0x06U;
    (void)memcpy(&req_proof[2], proof, 16U);
    assert(backend->authentication(NULL, req_proof, sizeof(req_proof), response, &resp_len,
                                   sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len == 3U && response[0] == 0x69U && response[1] == 0x06U && response[2] == 0x00U);
    assert(uds_auth_app_is_authenticated());

    /* 5. Subfunction 0x00: deAuthenticate */
    uint8_t req_deauth[] = {0x29U, 0x00U};
    assert(backend->authentication(NULL, req_deauth, sizeof(req_deauth), response, &resp_len,
                                   sizeof(response)) == UDS_RESULT_OK);
    assert(resp_len == 3U && response[0] == 0x69U && response[1] == 0x00U);
    assert(!uds_auth_app_is_authenticated());
}

int main(void) {
    test_multi_did_operations();
    test_aes_cmac_security_access_flow();
    test_service_0x14_group_clearing();
    test_service_0x29_authentication();
    return 0;
}