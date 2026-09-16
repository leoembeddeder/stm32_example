/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#ifndef APP_UDS_SECURITY_APP_H
#define APP_UDS_SECURITY_APP_H

#include "uds_iso_tp/uds.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UDS_SECURITY_APP_LEVEL1_SEED_LEN 4U
#define UDS_SECURITY_APP_LEVEL1_KEY_LEN 4U
#define UDS_SECURITY_APP_LEVEL2_SEED_LEN 16U
#define UDS_SECURITY_APP_LEVEL2_KEY_LEN 16U

#define UDS_SECURITY_LEVEL1_MASK_BYTE0 0xA5U
#define UDS_SECURITY_LEVEL1_MASK_BYTE1 0x5AU
#define UDS_SECURITY_LEVEL1_MASK_BYTE2 0xC3U
#define UDS_SECURITY_LEVEL1_MASK_BYTE3 0x3CU

typedef bool (*UdsSecurityKeyProviderFn)(uint8_t level, const uint8_t *seed, uint8_t *key_out);
typedef bool (*UdsEntropySourceFn)(uint8_t *buffer, size_t length);

void uds_security_app_init(void);

/* Key Provisioning & Hardware Security Module (HSM) Interface */
bool uds_security_app_provision_master_key(const uint8_t key[16]);
void uds_security_app_set_key_provider(UdsSecurityKeyProviderFn provider);
bool uds_security_app_has_provisioned_key(void);

/* Hardware TRNG / Entropy Accumulator Interface */
void uds_security_app_set_entropy_source(UdsEntropySourceFn source);

UdsCallbackResult uds_security_app_seed(void *context, uint8_t level, uint8_t *seed,
                                        uint16_t *length, uint16_t capacity);

UdsCallbackResult uds_security_app_key(void *context, uint8_t level, const uint8_t *key,
                                       uint16_t length);

bool uds_security_app_calculate_key_level1(const uint8_t seed[4], uint8_t key[4]);
bool uds_security_app_calculate_key_level2(const uint8_t seed[16], uint8_t key[16]);

#ifdef __cplusplus
}
#endif

#endif /* APP_UDS_SECURITY_APP_H */
