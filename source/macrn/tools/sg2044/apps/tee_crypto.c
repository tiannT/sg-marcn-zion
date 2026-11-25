/* SPDX-License-Identifier: BSD-3-Clause */
/* Acceptance-compatible CLI, authenticated encryption, constant-size token. */
#include "aead.h"
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <string.h>

#define PAYLOAD_BYTES 256U
#define ENVELOPE_BYTES (32U + PAYLOAD_BYTES + 16U)
#define PBKDF2_ROUNDS 100000

static int nibble(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

int main(int argc, char **argv)
{
    uint8_t envelope[ENVELOPE_BYTES] = {0}, plaintext[PAYLOAD_BYTES] = {0}, key[32] = {0};
    size_t password_length, length, i;
    bool decrypt;
    int status = 1;
    if (argc != 4 || (strcmp(argv[1], "enc") && strcmp(argv[1], "dec"))) {
        fprintf(stderr, "Usage: tee-crypto enc PASSWORD TEXT\n       tee-crypto dec PASSWORD TOKEN\n");
        return 2;
    }
    decrypt = !strcmp(argv[1], "dec");
    password_length = strlen(argv[2]);
    if (!password_length || password_length > 1024U) goto done;
    if (decrypt) {
        if (strlen(argv[3]) != ENVELOPE_BYTES * 2U) goto done;
        for (i = 0; i < ENVELOPE_BYTES; i++) {
            int high = nibble(argv[3][i * 2U]), low = nibble(argv[3][i * 2U + 1U]);
            if (high < 0 || low < 0) goto done;
            envelope[i] = (uint8_t)(high * 16 + low);
        }
        if (memcmp(envelope, "TC01", 4)) goto done;
    } else {
        length = strlen(argv[3]);
        if (length > PAYLOAD_BYTES - 4U) {
            fprintf(stderr, "tee-crypto: text exceeds 252 bytes\n");
            goto done;
        }
        memcpy(envelope, "TC01", 4);
        if (RAND_bytes(envelope + 4U, 28) != 1) goto done;
        plaintext[2] = (uint8_t)(length >> 8);
        plaintext[3] = (uint8_t)length;
        memcpy(plaintext + 4U, argv[3], length);
    }
    if (PKCS5_PBKDF2_HMAC(argv[2], (int)password_length, envelope + 4U, 16,
        PBKDF2_ROUNDS, EVP_sha256(), sizeof(key), key) != 1) goto done;
    if (decrypt) {
        if (!aead(key, envelope + 20U, envelope, 32, envelope + 32U, PAYLOAD_BYTES,
                  plaintext, envelope + 32U + PAYLOAD_BYTES, true)) goto done;
        length = ((size_t)plaintext[0] << 24) | ((size_t)plaintext[1] << 16) |
                 ((size_t)plaintext[2] << 8) | plaintext[3];
        if (length > PAYLOAD_BYTES - 4U) goto done;
        for (i = length + 4U; i < sizeof(plaintext); i++) if (plaintext[i]) goto done;
        if (fwrite(plaintext + 4U, 1, length, stdout) != length || putchar('\n') == EOF) goto done;
    } else {
        if (!aead(key, envelope + 20U, envelope, 32, plaintext, PAYLOAD_BYTES,
                  envelope + 32U, envelope + 32U + PAYLOAD_BYTES, false)) goto done;
        for (i = 0; i < ENVELOPE_BYTES; i++) if (printf("%02x", envelope[i]) != 2) goto done;
        if (putchar('\n') == EOF) goto done;
    }
    status = 0;
done:
    OPENSSL_cleanse(key, sizeof(key));
    OPENSSL_cleanse(plaintext, sizeof(plaintext));
    if (status) fprintf(stderr, "tee-crypto: invalid input or authentication failed\n");
    return status;
}
