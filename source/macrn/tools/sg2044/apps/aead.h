/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef PLANB_APP_AEAD_H
#define PLANB_APP_AEAD_H
#include <openssl/evp.h>
#include <stdbool.h>
#include <stdint.h>

/* Plaintext is never released until GCM authentication has succeeded. */
static int aead(const uint8_t key[32], const uint8_t nonce[12],
    const void *associated, int associated_length, const uint8_t *input, int length,
    uint8_t *output, uint8_t tag[16], bool decrypt)
{
    EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
    int count = 0, final = 0, ok = 0;
    if (!context) return 0;
    if (EVP_CipherInit_ex(context, EVP_aes_256_gcm(), NULL, NULL, NULL, !decrypt) != 1 ||
        EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) != 1 ||
        EVP_CipherInit_ex(context, NULL, NULL, key, nonce, !decrypt) != 1 ||
        EVP_CipherUpdate(context, NULL, &count, associated, associated_length) != 1 ||
        EVP_CipherUpdate(context, output, &count, input, length) != 1 || count != length)
        goto done;
    if (decrypt && EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_TAG, 16, tag) != 1)
        goto done;
    if (EVP_CipherFinal_ex(context, output + count, &final) != 1 || final != 0)
        goto done;
    if (!decrypt && EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_GET_TAG, 16, tag) != 1)
        goto done;
    ok = 1;
done:
    EVP_CIPHER_CTX_free(context);
    return ok;
}
#endif
