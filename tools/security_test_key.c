/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#include "uds_security_cmac.h"
#include "uds_security_reference.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const uint8_t s_cmac_master_key[16] = {0x2BU, 0x7EU, 0x15U, 0x16U, 0x28U, 0xAEU,
                                              0xD2U, 0xA6U, 0xABU, 0xF7U, 0x15U, 0x88U,
                                              0x09U, 0xCFU, 0x4FU, 0x3CU};

static void usage(const char *program) {
    fprintf(stderr, "Usage: %s <1|2|5> <hex-seed>\n", program);
    fprintf(stderr, "       Level 1 / 5: 4-byte seed (8 hex digits)\n");
    fprintf(stderr, "       Level 2    : 16-byte seed (32 hex digits, AES-CMAC128)\n");
    fprintf(stderr, "TEST/REFERENCE ONLY: not production ECU security.\n");
}

static bool parse_level(const char *text, uint8_t *level) {
    if ((text == NULL) || (level == NULL)) {
        return false;
    }
    if (strcmp(text, "1") == 0) {
        *level = 1U;
        return true;
    }
    if (strcmp(text, "2") == 0) {
        *level = 2U;
        return true;
    }
    if (strcmp(text, "5") == 0) {
        *level = 5U;
        return true;
    }
    return false;
}

static bool parse_hex_nibble(char value, uint8_t *nibble) {
    if (nibble == NULL) {
        return false;
    }
    if ((value >= '0') && (value <= '9')) {
        *nibble = (uint8_t)(value - '0');
        return true;
    }
    if ((value >= 'A') && (value <= 'F')) {
        *nibble = (uint8_t)(value - 'A' + 10);
        return true;
    }
    if ((value >= 'a') && (value <= 'f')) {
        *nibble = (uint8_t)(value - 'a' + 10);
        return true;
    }
    return false;
}

static bool parse_seed_bytes(const char *text, uint8_t *seed, uint16_t expected_len) {
    size_t offset = 0U;
    if ((text == NULL) || (seed == NULL)) {
        return false;
    }
    if ((strlen(text) >= 2U) && (text[0] == '0') && ((text[1] == 'x') || (text[1] == 'X'))) {
        offset = 2U;
    }
    if (strlen(text) - offset != ((size_t)expected_len * 2U)) {
        return false;
    }
    for (size_t index = 0U; index < (size_t)expected_len; ++index) {
        uint8_t high = 0U;
        uint8_t low = 0U;
        if (!parse_hex_nibble(text[offset + (index * 2U)], &high) ||
            !parse_hex_nibble(text[offset + (index * 2U) + 1U], &low)) {
            return false;
        }
        seed[index] = (uint8_t)((high << 4U) | low);
    }
    return true;
}

static void print_bytes(const char *label, const uint8_t *data, uint16_t length) {
    printf("%s", label);
    for (uint16_t index = 0U; index < length; ++index) {
        printf("%s%02X", (index == 0U) ? "" : " ", data[index]);
    }
    putchar('\n');
}

int main(int argc, char **argv) {
    uint8_t level = 0U;
    uint8_t seed[16] = {0U};
    uint8_t key[16] = {0U};
    uint16_t seed_length = 0U;
    uint16_t key_length = 0U;

    if ((argc != 3) || !parse_level(argv[1], &level)) {
        usage(argv[0]);
        return 2;
    }

    if (level == 2U) {
        seed_length = 16U;
        if (!parse_seed_bytes(argv[2], seed, seed_length) ||
            !uds_security_cmac_derive_key(s_cmac_master_key, seed, key)) {
            usage(argv[0]);
            return 2;
        }
        key_length = 16U;
    } else {
        seed_length = 4U;
        if (!parse_seed_bytes(argv[2], seed, seed_length) ||
            !uds_security_reference_calculate_key(level, seed, seed_length, key, sizeof(key),
                                                  &key_length)) {
            usage(argv[0]);
            return 2;
        }
    }

    printf("level = %u\n", (unsigned int)level);
    print_bytes("seed = ", seed, seed_length);
    print_bytes("key  = ", key, key_length);
    puts("WARNING: TEST/REFERENCE ONLY; do not use as production security.");
    return 0;
}
