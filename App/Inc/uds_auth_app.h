#ifndef STM32_UDS_ISO_TP_UDS_AUTH_APP_H
#define STM32_UDS_ISO_TP_UDS_AUTH_APP_H

#include "uds_iso_tp/uds.h"
#include "uds_iso_tp/uds_services.h"

#include <stdbool.h>
#include <stdint.h>

#define UDS_AUTH_SUBFUNCTION_DEAUTHENTICATE 0x00U
#define UDS_AUTH_SUBFUNCTION_VERIFY_CERT_UNIDIRECTIONAL 0x01U
#define UDS_AUTH_SUBFUNCTION_VERIFY_CERT_BIDIRECTIONAL 0x02U
#define UDS_AUTH_SUBFUNCTION_PROOF_OF_OWNERSHIP 0x03U
#define UDS_AUTH_SUBFUNCTION_TRANSMIT_CERTIFICATE 0x04U
#define UDS_AUTH_SUBFUNCTION_REQUEST_CHALLENGE 0x05U
#define UDS_AUTH_SUBFUNCTION_VERIFY_PROOF_UNIDIRECTIONAL 0x06U
#define UDS_AUTH_SUBFUNCTION_VERIFY_PROOF_BIDIRECTIONAL 0x07U
#define UDS_AUTH_SUBFUNCTION_AUTHENTICATION_CONFIG 0x08U

void uds_auth_app_init(void);
bool uds_auth_app_is_authenticated(void);
void uds_auth_app_deauthenticate(void);
const UdsAuthenticationServiceBackend *uds_auth_app_get_backend(void);

UdsCallbackResult uds_auth_app_service_handler(void *context, const uint8_t *request,
                                               uint16_t request_length, uint8_t *response,
                                               uint16_t *response_length,
                                               uint16_t response_capacity);

#endif /* STM32_UDS_ISO_TP_UDS_AUTH_APP_H */