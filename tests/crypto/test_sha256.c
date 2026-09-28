#include "sha256.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_sha256_nist_vectors(void) {
    uint8_t digest[32];

    /* Test vector 1: Empty string */
    /* SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 */
    const uint8_t expected_empty[32] = {0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
                                        0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
                                        0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
                                        0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55};
    sha256_hash((const uint8_t *)"", 0U, digest);
    assert(memcmp(digest, expected_empty, 32) == 0);

    /* Test vector 2: "abc" */
    /* SHA-256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad */
    const uint8_t expected_abc[32] = {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
                                      0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
                                      0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
                                      0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
    sha256_hash((const uint8_t *)"abc", 3U, digest);
    assert(memcmp(digest, expected_abc, 32) == 0);

    /* Test vector 3: "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" */
    /* SHA-256 = 248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1 */
    const char *vector3 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    const uint8_t expected_v3[32] = {0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8,
                                     0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39,
                                     0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67,
                                     0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1};
    sha256_hash((const uint8_t *)vector3, strlen(vector3), digest);
    assert(memcmp(digest, expected_v3, 32) == 0);
}

static void test_hmac_sha256_rfc4231(void) {
    uint8_t out[32];
    /* RFC 4231 Test Case 2: key = "Jefe", data = "what do ya want for nothing?" */
    /* HMAC = 5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843 */
    const char *key = "Jefe";
    const char *data = "what do ya want for nothing?";
    const uint8_t expected_hmac[32] = {0x5b, 0xdc, 0xc1, 0x46, 0xbf, 0x60, 0x75, 0x4e,
                                       0x6a, 0x04, 0x24, 0x26, 0x08, 0x95, 0x75, 0xc7,
                                       0x5a, 0x00, 0x3f, 0x08, 0x9d, 0x27, 0x39, 0x83,
                                       0x9d, 0xec, 0x58, 0xb9, 0x64, 0xec, 0x38, 0x43};
    hmac_sha256((const uint8_t *)key, strlen(key), (const uint8_t *)data, strlen(data), out);
    assert(memcmp(out, expected_hmac, 32) == 0);
}

int main(void) {
    test_sha256_nist_vectors();
    test_hmac_sha256_rfc4231();
    printf("SHA-256 and HMAC-SHA256 test suite passed successfully.\n");
    return 0;
}
