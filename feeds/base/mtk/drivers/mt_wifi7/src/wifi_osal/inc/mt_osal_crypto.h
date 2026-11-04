/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */

#ifndef __MT_OSAL_CRYTO_H_
#define __MT_OSAL_CRYTO_H_

/* deprecated */
struct crypto_aead *mt_aead_key_setup_encrypt(const char *alg, const u8 key[], size_t key_len, size_t mic_len);

/* deprecated */
void mt_aead_key_free(struct crypto_aead *tfm);

/* deprecated */
int mt_aead_encrypt(struct crypto_aead *tfm, u8 *b_0, u8 *aad, size_t aad_len, u8 *data, size_t data_len, u8 *mic);

/* deprecated */
int mt_aead_decrypt(struct crypto_aead *tfm, u8 *b_0, u8 *aad, size_t aad_len, u8 *data, size_t data_len, u8 *mic);

/* deprecated */
size_t mt_crypto_aead_authsize(struct crypto_aead *tfm);


void *mt_os_crypto_aead_key_setup_encrypt(const char *alg, const u8 key[], size_t key_len, size_t mic_len);

void mt_os_crypto_aead_key_free(void *ctx);

int mt_os_crypto_aead_encrypt(void *ctx, u8 *b_0, u8 *aad, size_t aad_len, u8 *data, size_t data_len, u8 *mic);

int mt_os_crypto_aead_decrypt(void *ctx, u8 *b_0, u8 *aad, size_t aad_len, u8 *data, size_t data_len, u8 *mic);

size_t mt_os_crypto_aead_authsize(void *ctx);

#endif /* __MT_OSAL_CRYTO_H_ */
