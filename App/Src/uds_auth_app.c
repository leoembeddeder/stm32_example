#include "uds_auth_app.h"
#include "uds_security_cmac.h"

#include <string.h>

#define UDS_AUTH_CHALLENGE_SIZE 16U

static const uint8_t s_auth_master_key[16] = {0x2BU, 0x7EU, 0x15U, 0x16U, 0x28U, 0xAEU,
                                              0xD2U, 0xA6U, 0xABU, 0xF7U, 0x15U, 0x88U,
                                              0x09U, 0xCFU, 0x4FU, 0x3CU};

static uint8_t s_active_challenge[UDS_AUTH_CHALLENGE_SIZE];
static bool s_challenge_valid = false;
static bool s_is_authenticated = false;

static const UdsAuthenticationServiceBackend s_auth_backend = {.authentication =
                                                                   uds_auth_app_service_handler};

void uds_auth_app_init(void) {
    (void)memset(s_active_challenge, 0x5AU, sizeof(s_active_challenge));
    s_challenge_valid = false;
    s_is_authenticated = false;
}

bool uds_auth_app_is_authenticated(void) {
    return s_is_authenticated;
}

void uds_auth_app_deauthenticate(void) {
    s_is_authenticated = false;
    s_challenge_valid = false;
}

const UdsAuthenticationServiceBackend *uds_auth_app_get_backend(void) {
    return &s_auth_backend;
}

UdsCallbackResult uds_auth_app_service_handler(void *context, const uint8_t *request,
                                               uint16_t request_length, uint8_t *response,
                                               uint16_t *response_length,
                                               uint16_t response_capacity) {
    (void)context;
    if ((request == NULL) || (request_length < 2U) || (response == NULL) ||
        (response_length == NULL)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }

    uint8_t subfunction = (uint8_t)(request[1] & 0x7FU);

    switch (subfunction) {
    case UDS_AUTH_SUBFUNCTION_DEAUTHENTICATE: {
        uds_auth_app_deauthenticate();
        if (response_capacity < 3U) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        response[0] = 0x69U;
        response[1] = subfunction;
        response[2] = 0x00U; /* Success status */
        *response_length = 3U;
        return UDS_RESULT_OK;
    }

    case UDS_AUTH_SUBFUNCTION_REQUEST_CHALLENGE: {
        /* Request format: 29 05 [communicationConfiguration (optional 1 byte)] */
        if (response_capacity < (2U + UDS_AUTH_CHALLENGE_SIZE)) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        /* Generate deterministic rolling pseudo-random challenge */
        for (uint8_t i = 0U; i < UDS_AUTH_CHALLENGE_SIZE; ++i) {
            s_active_challenge[i] = (uint8_t)(s_active_challenge[i] + (uint8_t)(i * 7U + 0x13U));
        }
        s_challenge_valid = true;

        response[0] = 0x69U;
        response[1] = subfunction;
        (void)memcpy(&response[2], s_active_challenge, UDS_AUTH_CHALLENGE_SIZE);
        *response_length = (uint16_t)(2U + UDS_AUTH_CHALLENGE_SIZE);
        return UDS_RESULT_OK;
    }

    case UDS_AUTH_SUBFUNCTION_VERIFY_PROOF_UNIDIRECTIONAL: {
        /* Request format: 29 06 <16-byte proof/MAC> */
        if ((request_length < (2U + UDS_AUTH_CHALLENGE_SIZE)) || !s_challenge_valid) {
            return UDS_RESULT_SEQUENCE_ERROR;
        }

        uint8_t expected_mac[16];
        if (!uds_security_cmac_derive_key(s_auth_master_key, s_active_challenge, expected_mac)) {
            return UDS_RESULT_ERROR;
        }

        if (!uds_security_cmac_constant_time_equal(&request[2], expected_mac)) {
            s_is_authenticated = false;
            return UDS_RESULT_INVALID_KEY;
        }

        s_is_authenticated = true;
        s_challenge_valid = false;

        if (response_capacity < 3U) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        response[0] = 0x69U;
        response[1] = subfunction;
        response[2] = 0x00U; /* Proof accepted */
        *response_length = 3U;
        return UDS_RESULT_OK;
    }

    case UDS_AUTH_SUBFUNCTION_AUTHENTICATION_CONFIG: {
        /* Response: 69 08 <authMode: 0x01 APCE> <algorithmId: 0x01 AES-CMAC-128> */
        if (response_capacity < 4U) {
            return UDS_RESULT_RESPONSE_TOO_LONG;
        }
        response[0] = 0x69U;
        response[1] = subfunction;
        response[2] = 0x01U; /* APCE Mode */
        response[3] = 0x01U; /* AES-CMAC-128 */
        *response_length = 4U;
        return UDS_RESULT_OK;
    }

    default:
        return UDS_RESULT_SUBFUNCTION_NOT_SUPPORTED;
    }
}