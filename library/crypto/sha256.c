#include "sha256.h"
#include <string.h>

#define ROTRIGHT(a, b) (((a) >> (b)) | ((a) << (32U - (b))))
#define CH(x, y, z)    (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z)   (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x)         (ROTRIGHT(x, 2U) ^ ROTRIGHT(x, 13U) ^ ROTRIGHT(x, 22U))
#define EP1(x)         (ROTRIGHT(x, 6U) ^ ROTRIGHT(x, 11U) ^ ROTRIGHT(x, 25U))
#define SIG0(x)        (ROTRIGHT(x, 7U) ^ ROTRIGHT(x, 18U) ^ ((x) >> 3U))
#define SIG1(x)        (ROTRIGHT(x, 17U) ^ ROTRIGHT(x, 19U) ^ ((x) >> 10U))

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

void sha256_init(Sha256Ctx *ctx) {
    if (ctx == NULL) {
        return;
    }
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

void sha256_update(Sha256Ctx *ctx, const uint8_t *data, size_t len) {
    if ((ctx == NULL) || (data == NULL) || (len == 0U)) {
        return;
    }
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

void sha256_final(Sha256Ctx *ctx, uint8_t *digest) {
    if ((ctx == NULL) || (digest == NULL)) {
        return;
    }
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
        ctx->buffer[56U + i] = (uint8_t)((total_bits >> (56U - ((uint32_t)i * 8U))) & 0xFFU);
    }
    sha256_transform(ctx, ctx->buffer);

    for (uint8_t i = 0U; i < 8U; ++i) {
        digest[i * 4U] = (uint8_t)((ctx->state[i] >> 24U) & 0xFFU);
        digest[i * 4U + 1U] = (uint8_t)((ctx->state[i] >> 16U) & 0xFFU);
        digest[i * 4U + 2U] = (uint8_t)((ctx->state[i] >> 8U) & 0xFFU);
        digest[i * 4U + 3U] = (uint8_t)(ctx->state[i] & 0xFFU);
    }
}

void sha256_hash(const uint8_t *data, size_t len, uint8_t *digest) {
    Sha256Ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
}

void hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len,
                 uint8_t *out) {
    if ((out == NULL) || ((key == NULL) && (key_len > 0U)) || ((msg == NULL) && (msg_len > 0U))) {
        return;
    }
    uint8_t k_pad[64];
    uint8_t tk[32];
    if (key_len > 64U) {
        sha256_hash(key, key_len, tk);
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
