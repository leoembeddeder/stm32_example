/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#ifndef UDS_ISO_TP_SHA256_H
#define UDS_ISO_TP_SHA256_H

#include <stddef.h>
#include <stdint.h>

#ifndef SHA256_H
#define SHA256_H

#define SHA256_DIGEST_SIZE 32U
#define SHA256_BLOCK_SIZE  64U

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
} Sha256Ctx;

void sha256_init(Sha256Ctx *ctx);
void sha256_update(Sha256Ctx *ctx, const uint8_t *data, size_t len);
void sha256_final(Sha256Ctx *ctx, uint8_t *digest);
void sha256_hash(const uint8_t *data, size_t len, uint8_t *digest);

void hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len,
                 uint8_t *out);

#endif /* SHA256_H */

#endif /* UDS_ISO_TP_SHA256_H */
