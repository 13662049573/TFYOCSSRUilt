/*
 * aead.c - Manage AEAD ciphers
 *
 * Copyright (C) 2013 - 2017, Max Lv <max.c.lv@gmail.com>
 *
 * This file is part of the shadowsocks-libev.
 *
 * shadowsocks-libev is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * shadowsocks-libev is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with shadowsocks-libev; see the file COPYING. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "libev_config.h"
#endif

#include <mbedtls/version.h>
#define CIPHER_UNSUPPORTED "unsupported"
#include <time.h>
#include <stdio.h>
#include <assert.h>

#include <sodium.h>
#include <arpa/inet.h>

// remove aead nonce check :T3003
//#include "ppbloom.h"
#include "aead.h"
#include "utils.h"
#include "crypto.h"

#define NONE                    (-1)
#define AES128GCM               0
#define AES192GCM               1
#define AES256GCM               2
/*
 * methods above requires gcm context
 * methods below doesn't require it,
 * then we need to fake one
 */
#define CHACHA20POLY1305IETF    3

#ifdef FS_HAVE_XCHACHA20IETF
#define XCHACHA20POLY1305IETF   4
#endif

#define CHUNK_SIZE_LEN          2
#define CHUNK_SIZE_MASK         0x3FFF

#define CHUNK_PADDING_SIZE_LEN  2
#define CHUNK_PADDING_SIZE_LIMIT  512
#define CHUNK_PADDING_SIZE_MASK 0x7FFF

/*
 * This is SIP004 proposed by @Mygod, the design of TCP chunk is from @breakwa11 and
 * @Noisyfox. This first version of this file is written by @wongsyrone.
 *
 * The first nonce is either from client or server side, it is generated randomly, and
 * the sequent nonces are increased by 1.
 *
 * Data.Len is used to separate general ciphertext and Auth tag. We can start decryption
 * if and only if the verification is passed.
 * Firstly, we do length check, then decrypt it, separate ciphertext and attached data tag
 * based on the verified length, verify data tag and decrypt the corresponding data.
 * Finally, do what you supposed to do, e.g. forward user data.
 *
 * For UDP, nonces are generated randomly without the incrementation.
 *
 * TCP request (before encryption)
 * +------+---------------------+------------------+
 * | ATYP | Destination Address | Destination Port |
 * +------+---------------------+------------------+
 * |  1   |       Variable      |         2        |
 * +------+---------------------+------------------+
 *
 * TCP request (after encryption, *ciphertext*)
 * +--------+--------------+-------------+---------------+-----------+----------+---------------+
 * | NONCE  | *PaddingLen* | *HeaderLen* | HeaderLen_TAG | *Padding* | *Header* |  Header_TAG   |
 * +--------+--------------+-------------+---------------+-----------+----------+---------------+
 * | Fixed  |       2      |      2      |       Fixed   | Variable  | Variable |     Fixed     |
 * +--------+--------------+-------------+---------------+-----------+----------+---------------+
 *
 * Header input: atyp + dst.addr + dst.port
 * HeaderLen is length(atyp + dst.addr + dst.port)
 * HeaderLen_TAG and Header_TAG are in plaintext
 * PaddingLen 1 ~ 0x7FFF, 1 ~ 512 if HeaderLen < 512 else 513 ~ 0x7FFF
 * Padding 0 ~ 512 bytes, if Padding > 512, padding is empty
 *
 * TCP Chunk (before encryption)
 * +----------+
 * |  DATA    |
 * +----------+
 * | Variable |
 * +----------+
 *
 * Data.Len is a 16-bit big-endian integer indicating the length of DATA.
 *
 * TCP Chunk (after encryption, *ciphertext*)
 * +--------------+--------------+---------------+------------+----------+------------+
 * | *PaddingLen* |  *DataLen*   |  DataLen_TAG  |  Padding   |  *Data*  |  Data_TAG  |
 * +--------------+--------------+---------------+------------+----------+------------+
 * |      2       |      2       |     Fixed     |  Variable  | Variable |   Fixed    |
 * +--------------+--------------+---------------+------------+----------+------------+
 *
 * Len_TAG and DATA_TAG have the same length, they are in plaintext.
 * After encryption, DATA -> DATA*
 *
 * UDP (before encryption)
 * +------+---------------------+------------------+----------+
 * | ATYP | Destination Address | Destination Port |   DATA   |
 * +------+---------------------+------------------+----------+
 * |  1   |       Variable      |         2        | Variable |
 * +------+---------------------+------------------+----------+
 *
 * UDP (after encryption, *ciphertext*)
 * +--------+-----------+-----------+
 * | NONCE  |  *Data*   |  Data_TAG |
 * +--------+-----------+-----------+
 * | Fixed  | Variable  |   Fixed   |
 * +--------+-----------+-----------+
 *
 * *Data* is Encrypt(atyp + dst.addr + dst.port + payload)
 * RSV and FRAG are dropped
 * Since UDP packet is either received completely or missed,
 * we don't have to keep a field to track its length.
 *
 */

const char *supported_aead_ciphers[AEAD_CIPHER_NUM] = {
    "aes-128-gcm",
    "aes-192-gcm",
    "aes-256-gcm",
    "chacha20-ietf-poly1305",
#ifdef FS_HAVE_XCHACHA20IETF
    "xchacha20-ietf-poly1305"
#endif
};

/*
 * use mbed TLS cipher wrapper to unify handling
 */
static const char *supported_aead_ciphers_mbedtls[AEAD_CIPHER_NUM] = {
    "AES-128-GCM",
    "AES-192-GCM",
    "AES-256-GCM",
    CIPHER_UNSUPPORTED,
#ifdef FS_HAVE_XCHACHA20IETF
    CIPHER_UNSUPPORTED
#endif
};

static const size_t supported_aead_ciphers_nonce_size[AEAD_CIPHER_NUM] = {
    12, 12, 12, 12,
#ifdef FS_HAVE_XCHACHA20IETF
    24
#endif
};

static const size_t supported_aead_ciphers_key_size[AEAD_CIPHER_NUM] = {
    16, 24, 32, 32,
#ifdef FS_HAVE_XCHACHA20IETF
    32
#endif
};

static const size_t supported_aead_ciphers_tag_size[AEAD_CIPHER_NUM] = {
    16, 16, 16, 16,
#ifdef FS_HAVE_XCHACHA20IETF
    16
#endif
};

static int
aead_cipher_encrypt(cipher_ctx_t *cipher_ctx,
                    uint8_t *c,
                    size_t *clen,
                    uint8_t *m,
                    size_t mlen,
                    uint8_t *ad,
                    size_t adlen,
                    uint8_t *n,
                    uint8_t *k)
{
    int err                      = CRYPTO_OK;
    unsigned long long long_clen = 0;

    size_t nlen = cipher_ctx->cipher->nonce_len;
    size_t tlen = cipher_ctx->cipher->tag_len;

    switch (cipher_ctx->cipher->method) {
    case AES128GCM:
    case AES192GCM:
    case AES256GCM:
        err = mbedtls_cipher_auth_encrypt(cipher_ctx->evp, n, nlen, ad, adlen,
                                          m, mlen, c, clen, c + mlen, tlen);
        *clen += tlen;
        break;
    case CHACHA20POLY1305IETF:
        err = crypto_aead_chacha20poly1305_ietf_encrypt(c, &long_clen, m, mlen,
                                                        ad, adlen, NULL, n, k);
        *clen = (size_t)long_clen;
        break;
#ifdef FS_HAVE_XCHACHA20IETF
    case XCHACHA20POLY1305IETF:
        err = crypto_aead_xchacha20poly1305_ietf_encrypt(c, &long_clen, m, mlen,
                                                         ad, adlen, NULL, n, k);
        *clen = (size_t)long_clen;
        break;
#endif
    default:
        return CRYPTO_ERROR;
    }

    return err;
}

static int
aead_cipher_decrypt(cipher_ctx_t *cipher_ctx,
                    uint8_t *p, size_t *plen,
                    uint8_t *m, size_t mlen,
                    uint8_t *ad, size_t adlen,
                    uint8_t *n, uint8_t *k)
{
    int err                      = CRYPTO_ERROR;
    unsigned long long long_plen = 0;

    size_t nlen = cipher_ctx->cipher->nonce_len;
    size_t tlen = cipher_ctx->cipher->tag_len;

    switch (cipher_ctx->cipher->method) {
    case AES128GCM:
    case AES192GCM:
    case AES256GCM:
        err = mbedtls_cipher_auth_decrypt(cipher_ctx->evp, n, nlen, ad, adlen,
                                          m, mlen - tlen, p, plen, m + mlen - tlen, tlen);
        break;
    case CHACHA20POLY1305IETF:
        err = crypto_aead_chacha20poly1305_ietf_decrypt(p, &long_plen, NULL, m, mlen,
                                                        ad, adlen, n, k);
        *plen = (size_t)long_plen; // it's safe to cast 64bit to 32bit length here
        break;
#ifdef FS_HAVE_XCHACHA20IETF
    case XCHACHA20POLY1305IETF:
        err = crypto_aead_xchacha20poly1305_ietf_decrypt(p, &long_plen, NULL, m, mlen,
                                                         ad, adlen, n, k);
        *plen = (size_t)long_plen; // it's safe to cast 64bit to 32bit length here
        break;
#endif
    default:
        return CRYPTO_ERROR;
    }

    return err;
}

/*
 * get basic cipher info structure
 * it's a wrapper offered by crypto library
 */
const cipher_kt_t *
aead_get_cipher_type(int method)
{
    if (method < AES128GCM || method >= AEAD_CIPHER_NUM) {
        LOGE("aead_get_cipher_type(): Illegal method");
        return NULL;
    }

    /* cipher that don't use mbed TLS, just return */
    if (method >= CHACHA20POLY1305IETF) {
        return NULL;
    }

    const char *ciphername  = supported_aead_ciphers[method];
    const char *mbedtlsname = supported_aead_ciphers_mbedtls[method];
    if (strcmp(mbedtlsname, CIPHER_UNSUPPORTED) == 0) {
        LOGE("Cipher %s currently is not supported by mbed TLS library",
             ciphername);
        return NULL;
    }
    return mbedtls_cipher_info_from_string(mbedtlsname);
}

static void
aead_cipher_ctx_set_key(cipher_ctx_t *cipher_ctx, int enc)
{
    const digest_type_t *md = mbedtls_md_info_from_string("SHA1");
    if (md == NULL) {
        FATAL("SHA1 Digest not found in crypto library");
    }
    uint8_t *key;
    uint8_t *skey;
    if (enc) {
        key = cipher_ctx->kx.tx;
        skey = cipher_ctx->s_tx;
    } else {
        key = cipher_ctx->kx.rx;
        skey = cipher_ctx->s_rx;
    }

    int err = crypto_hkdf(md,
            cipher_ctx->salt, cipher_ctx->cipher->key_len,
            key, cipher_ctx->cipher->key_len,
            (uint8_t *)SUBKEY_INFO, strlen(SUBKEY_INFO),
            skey, cipher_ctx->cipher->key_len);
    if (err) {
        FATAL("Unable to generate subkey");
    }

    memset(cipher_ctx->nonce, 0, cipher_ctx->cipher->nonce_len);

    /* cipher that don't use mbed TLS, just return */
    if (cipher_ctx->cipher->method >= CHACHA20POLY1305IETF) {
        return;
    }

    if (mbedtls_cipher_setkey(cipher_ctx->evp, skey,
            (int)cipher_ctx->cipher->key_len * 8, (mbedtls_operation_t)enc) != 0) {
        FATAL("Cannot set mbed TLS cipher key");
    }
    if (mbedtls_cipher_reset(cipher_ctx->evp) != 0) {
        FATAL("Cannot finish preparation of mbed TLS cipher context");
    }
}

static void
aead_cipher_ctx_init(cipher_ctx_t *cipher_ctx, int method, int enc)
{
    if (method < AES128GCM || method >= AEAD_CIPHER_NUM) {
        LOGE("cipher_context_init(): Illegal method");
        return;
    }

    if (method >= CHACHA20POLY1305IETF) {
        return;
    }

    const char *ciphername = supported_aead_ciphers[method];

    const cipher_kt_t *cipher = aead_get_cipher_type(method);

    cipher_ctx->evp = ss_malloc(sizeof(cipher_evp_t));
    memset(cipher_ctx->evp, 0, sizeof(cipher_evp_t));
    cipher_evp_t *evp = cipher_ctx->evp;

    if (cipher == NULL) {
        LOGE("Cipher %s not found in mbed TLS library", ciphername);
        FATAL("Cannot initialize mbed TLS cipher");
    }
    mbedtls_cipher_init(evp);
    if (mbedtls_cipher_setup(evp, cipher) != 0) {
        FATAL("Cannot initialize mbed TLS cipher context");
    }

#ifdef SS_DEBUG
    dump("PK", (char *)cipher_ctx->cipher->pk, cipher_ctx->cipher->key_len);
    dump("SK", (char *)cipher_ctx->cipher->sk, cipher_ctx->cipher->key_len);
#endif
}

void
aead_ctx_init(cipher_t *cipher, kx_ctx_t *kx, cipher_ctx_t *cipher_ctx, int enc)
{
    sodium_memzero(cipher_ctx, sizeof(cipher_ctx_t));
    cipher_ctx->cipher = cipher;
    
    if (kx != NULL) {
        memcpy(&cipher_ctx->kx, kx, sizeof(kx_ctx_t));
    }
    aead_cipher_ctx_init(cipher_ctx, cipher->method, enc);

    if (enc) {
        rand_bytes(cipher_ctx->salt, (int)cipher->key_len);
    }
}

void
aead_ctx_release(cipher_ctx_t *cipher_ctx)
{
    if (cipher_ctx->chunk != NULL) {
        bfree(cipher_ctx->chunk);
        ss_free(cipher_ctx->chunk);
        cipher_ctx->chunk = NULL;
    }

    if (cipher_ctx->cipher->method >= CHACHA20POLY1305IETF) {
        return;
    }

    mbedtls_cipher_free(cipher_ctx->evp);
    ss_free(cipher_ctx->evp);
}

int
aead_encrypt_all(buffer_t *plaintext, cipher_t *cipher, kx_ctx_t *kx, size_t capacity)
{
    cipher_ctx_t cipher_ctx;
    aead_ctx_init(cipher, kx, &cipher_ctx, 1);

    size_t salt_len  = cipher->key_len;
    size_t tag_len   = cipher->tag_len;
    int err          = CRYPTO_OK;

    static buffer_t tmp = { 0, 0, 0, NULL };
    size_t pk_len = 0;
    if (! kx->pk_sent) {
        pk_len = sizeof(kx->pk);
    }
    brealloc(&tmp, pk_len + salt_len + tag_len + plaintext->len, capacity);
    buffer_t *ciphertext = &tmp;
    ciphertext->len = tag_len + plaintext->len;

    /* copy salt to first pos */
    if (pk_len) {
        memcpy(ciphertext->data, kx->pk, pk_len);
        kx->pk_sent = 1;
    }
    memcpy(ciphertext->data + pk_len, cipher_ctx.salt, salt_len);

    aead_cipher_ctx_set_key(&cipher_ctx, 1);

    size_t clen = ciphertext->len;
    err = aead_cipher_encrypt(&cipher_ctx,
                              (uint8_t *)ciphertext->data + pk_len + salt_len, &clen,
                              (uint8_t *)plaintext->data, plaintext->len,
                              NULL, 0, cipher_ctx.nonce, cipher_ctx.s_tx);

    aead_ctx_release(&cipher_ctx);

    if (err)
        return CRYPTO_ERROR;

    assert(ciphertext->len == clen);

    brealloc(plaintext, pk_len + salt_len + ciphertext->len, capacity);
    memcpy(plaintext->data, ciphertext->data, pk_len + salt_len + ciphertext->len);
    plaintext->len = pk_len + salt_len + ciphertext->len;

    return CRYPTO_OK;
}

int
aead_decrypt_all(buffer_t *ciphertext, cipher_t *cipher, kx_ctx_t *kx, size_t capacity)
{
    size_t salt_len  = cipher->key_len;
    size_t tag_len   = cipher->tag_len;
    int err          = CRYPTO_OK;

    int rpk_len = 0;
    if (! kx->rpk_received) {
        rpk_len = sizeof(kx->rpk);
    }
    if (ciphertext->len <= rpk_len + salt_len + tag_len) {
        return CRYPTO_ERROR;
    }

    cipher_ctx_t cipher_ctx;
    if (! kx->rpk_received) {
        // init server kx ctx
        crypto_kx_ctx_init(kx, 0, ciphertext->data);
        kx->rpk_received = 1;
    }
    aead_ctx_init(cipher, kx, &cipher_ctx, 0);

    static buffer_t tmp = { 0, 0, 0, NULL };
    brealloc(&tmp, ciphertext->len, capacity);
    buffer_t *plaintext = &tmp;
    plaintext->len = ciphertext->len - rpk_len - salt_len - tag_len;

    /* get salt */
    uint8_t *salt = cipher_ctx.salt;
    memcpy(salt, ciphertext->data  + rpk_len, salt_len);

    aead_cipher_ctx_set_key(&cipher_ctx, 0);

    size_t plen = plaintext->len;
    err = aead_cipher_decrypt(&cipher_ctx,
                              (uint8_t *)plaintext->data, &plen,
                              (uint8_t *)ciphertext->data + rpk_len + salt_len,
                              ciphertext->len - rpk_len - salt_len, NULL, 0,
                              cipher_ctx.nonce, cipher_ctx.s_rx);

    aead_ctx_release(&cipher_ctx);

    if (err)
        return CRYPTO_ERROR;

    brealloc(ciphertext, plaintext->len, capacity);
    memcpy(ciphertext->data, plaintext->data, plaintext->len);
    ciphertext->len = plaintext->len;

    return CRYPTO_OK;
}

static int
aead_chunk_encrypt(cipher_ctx_t *ctx, uint8_t *p, uint8_t *c,
                   uint8_t *n, uint16_t plen, size_t *outlen)
{
    size_t nlen = ctx->cipher->nonce_len;
    size_t tlen = ctx->cipher->tag_len;

    assert(plen <= CHUNK_SIZE_MASK);

    int err;
    size_t clen;
    uint8_t len_buf[CHUNK_SIZE_LEN + CHUNK_PADDING_SIZE_LEN];
    uint16_t pdlen = 0;
    uint8_t pdbuf[CHUNK_PADDING_SIZE_LIMIT];
    uint32_t t = plen & CHUNK_SIZE_MASK;
    
    if (plen < CHUNK_PADDING_SIZE_LIMIT) {
        pdlen = random() % CHUNK_PADDING_SIZE_LIMIT + 1;
        rand_bytes(pdbuf, pdlen);
    } else {
        pdlen = random() % (CHUNK_PADDING_SIZE_MASK - CHUNK_PADDING_SIZE_LIMIT) \
            + CHUNK_PADDING_SIZE_LIMIT + 1;
    }
    t = htonl(t | (pdlen << 16));
    memcpy(len_buf, &t, CHUNK_SIZE_LEN + CHUNK_PADDING_SIZE_LEN);

    clen = CHUNK_SIZE_LEN + CHUNK_PADDING_SIZE_LEN + tlen;
    err  = aead_cipher_encrypt(ctx, c, &clen, len_buf,
                               CHUNK_SIZE_LEN + CHUNK_PADDING_SIZE_LEN,
                               NULL, 0, n, ctx->s_tx);
    if (err)
        return CRYPTO_ERROR;

    assert(clen == CHUNK_SIZE_LEN + CHUNK_PADDING_SIZE_LEN + tlen);

    sodium_increment(n, nlen);

    // copy padding random bytes
    if (pdlen <= CHUNK_PADDING_SIZE_LIMIT) {
        memcpy(c + clen, pdbuf, pdlen);
    } else {
        pdlen = 0;
    }
    
    clen = plen + tlen;
    err  = aead_cipher_encrypt(ctx, c + CHUNK_SIZE_LEN + CHUNK_PADDING_SIZE_LEN \
                               + tlen + pdlen,
                               &clen, p, plen,
                               NULL, 0, n, ctx->s_tx);
    if (err)
        return CRYPTO_ERROR;

    assert(clen == plen + tlen);

    sodium_increment(n, nlen);

    *outlen = CHUNK_SIZE_LEN + CHUNK_PADDING_SIZE_LEN \
        + tlen + pdlen + plen + tlen;
    
    return CRYPTO_OK;
}

/* TCP */
int
aead_encrypt(buffer_t *plaintext, cipher_ctx_t *cipher_ctx, size_t capacity)
{
    if (cipher_ctx == NULL)
        return CRYPTO_ERROR;

    if (plaintext->len == 0) {
        return CRYPTO_OK;
    }

    static buffer_t tmp = { 0, 0, 0, NULL };
    buffer_t *ciphertext;

    cipher_t *cipher  = cipher_ctx->cipher;
    int err           = CRYPTO_ERROR;
    size_t salt_offset  = 0;
    size_t pk_offset  = 0;
    size_t salt_len   = cipher->key_len;
    size_t tag_len    = cipher->tag_len;

    if (!cipher_ctx->init) {
        salt_offset = salt_len;
        if (cipher_ctx->is_local) {
            pk_offset = sizeof(cipher_ctx->kx.pk);
        }
    }

    size_t out_len = pk_offset + salt_offset + 2 * tag_len + plaintext->len \
        + CHUNK_SIZE_LEN + CHUNK_PADDING_SIZE_LEN + CHUNK_PADDING_SIZE_LIMIT;
    brealloc(&tmp, out_len, capacity);
    ciphertext      = &tmp;
    ciphertext->len = out_len;

    if (!cipher_ctx->init) {
        if (cipher_ctx->is_local) {
            memcpy(ciphertext->data, cipher_ctx->kx.pk, pk_offset);
            cipher_ctx->kx.pk_sent = 1;
        }
        memcpy(ciphertext->data + pk_offset, cipher_ctx->salt, salt_len);
        aead_cipher_ctx_set_key(cipher_ctx, 1);
        cipher_ctx->init = 1;
    }

    err = aead_chunk_encrypt(cipher_ctx,
                             (uint8_t *)plaintext->data,
                             (uint8_t *)ciphertext->data + pk_offset + salt_offset,
                             cipher_ctx->nonce, plaintext->len,
                             &out_len);
    if (err)
        return err;

    brealloc(plaintext, out_len + pk_offset + salt_offset, capacity);
    memcpy(plaintext->data, ciphertext->data, out_len + pk_offset + salt_offset);
    plaintext->len = out_len + pk_offset + salt_offset;

    return 0;
}

static int
aead_chunk_decrypt(cipher_ctx_t *ctx, uint8_t *p, uint8_t *c, uint8_t *n,
                   size_t *plen, size_t *clen)
{
    int err;
    size_t mlen;
    size_t padlen;
    size_t nlen = ctx->cipher->nonce_len;
    size_t tlen = ctx->cipher->tag_len;

    if (*clen <= 2 * tlen + CHUNK_SIZE_LEN + CHUNK_PADDING_SIZE_LEN)
        return CRYPTO_NEED_MORE;

    uint8_t len_buf[CHUNK_SIZE_LEN + CHUNK_PADDING_SIZE_LEN];
    err = aead_cipher_decrypt(ctx, len_buf, plen,
                              c, CHUNK_SIZE_LEN + CHUNK_PADDING_SIZE_LEN + tlen,
                              NULL, 0, n, ctx->s_rx);
    if (err)
        return CRYPTO_ERROR;
    assert(*plen == CHUNK_SIZE_LEN + CHUNK_PADDING_SIZE_LEN);

    mlen = ntohl(*(uint32_t *)len_buf);
    padlen = (mlen >> 16) & CHUNK_PADDING_SIZE_MASK;
    mlen = mlen & CHUNK_SIZE_MASK;

    if (mlen == 0)
        return CRYPTO_ERROR;

    if (padlen > CHUNK_PADDING_SIZE_LIMIT) {
        padlen = 0;
    }
    size_t chunk_len = 2 * tlen + CHUNK_SIZE_LEN \
        + CHUNK_PADDING_SIZE_LEN + padlen + mlen;

    if (*clen < chunk_len)
        return CRYPTO_NEED_MORE;

    sodium_increment(n, nlen);

    err = aead_cipher_decrypt(ctx, p, plen,
                              c + CHUNK_SIZE_LEN + CHUNK_PADDING_SIZE_LEN \
                                + tlen + padlen,
                              mlen + tlen,
                              NULL, 0, n, ctx->s_rx);
    if (err)
        return CRYPTO_ERROR;
    assert(*plen == mlen);

    sodium_increment(n, nlen);

    if (*clen > chunk_len)
        memmove(c, c + chunk_len, *clen - chunk_len);

    *clen = *clen - chunk_len;

    return CRYPTO_OK;
}

int
aead_decrypt(buffer_t *ciphertext, cipher_ctx_t *cipher_ctx, size_t capacity)
{
    int err             = CRYPTO_OK;
    static buffer_t tmp = { 0, 0, 0, NULL };

    cipher_t *cipher = cipher_ctx->cipher;

    size_t salt_len  = cipher->key_len;
    size_t rpk_len   = 0;

    if (cipher_ctx->chunk == NULL) {
        cipher_ctx->chunk = (buffer_t *)ss_malloc(sizeof(buffer_t));
        memset(cipher_ctx->chunk, 0, sizeof(buffer_t));
        balloc(cipher_ctx->chunk, capacity);
    }

    brealloc(cipher_ctx->chunk,
             cipher_ctx->chunk->len + ciphertext->len, capacity);
    memcpy(cipher_ctx->chunk->data + cipher_ctx->chunk->len,
           ciphertext->data, ciphertext->len);
    cipher_ctx->chunk->len += ciphertext->len;

    brealloc(&tmp, cipher_ctx->chunk->len, capacity);
    buffer_t *plaintext = &tmp;

    if (!cipher_ctx->init) {
        if (! cipher_ctx->is_local) {
            rpk_len = sizeof(cipher_ctx->kx.rpk);
        }
        if (cipher_ctx->chunk->len <= rpk_len + salt_len) {
            return CRYPTO_NEED_MORE;
        }
        if (! cipher_ctx->is_local) {
            memcpy(cipher_ctx->kx.pk, cipher->pk, sizeof(cipher->pk));
            memcpy(cipher_ctx->kx.sk, cipher->sk, sizeof(cipher->sk));
            crypto_kx_ctx_init(&cipher_ctx->kx, cipher_ctx->is_local,
                    (unsigned char *) cipher_ctx->chunk->data);
        }
        memcpy(cipher_ctx->salt, cipher_ctx->chunk->data + rpk_len, salt_len);

        aead_cipher_ctx_set_key(cipher_ctx, 0);

        memmove(cipher_ctx->chunk->data,
                cipher_ctx->chunk->data + rpk_len + salt_len,
                cipher_ctx->chunk->len - rpk_len - salt_len);
        cipher_ctx->chunk->len -= rpk_len + salt_len;

        cipher_ctx->init = 1;

    } else if (cipher_ctx->init == 1) {
        cipher_ctx->init = 2;
    }

    size_t plen = 0;
    while (cipher_ctx->chunk->len > 0) {
        size_t chunk_clen = cipher_ctx->chunk->len;
        size_t chunk_plen = 0;
        err = aead_chunk_decrypt(cipher_ctx,
                                 (uint8_t *)plaintext->data + plen,
                                 (uint8_t *)cipher_ctx->chunk->data,
                                 cipher_ctx->nonce, &chunk_plen, &chunk_clen);
        if (err == CRYPTO_ERROR) {
            return err;
        } else if (err == CRYPTO_NEED_MORE) {
            if (plen == 0)
                return err;
            else
                break;
        }
        cipher_ctx->chunk->len = chunk_clen;
        plen                  += chunk_plen;
    }
    plaintext->len = plen;

    brealloc(ciphertext, plaintext->len, capacity);
    memcpy(ciphertext->data, plaintext->data, plaintext->len);
    ciphertext->len = plaintext->len;

    return CRYPTO_OK;
}

cipher_t *
aead_key_init(int method, const char *pk, const char *sk)
{
    if (method < AES128GCM || method >= AEAD_CIPHER_NUM) {
        LOGE("aead_key_init(): Illegal method");
        return NULL;
    }

    cipher_t *cipher = (cipher_t *)ss_malloc(sizeof(cipher_t));
    memset(cipher, 0, sizeof(cipher_t));

    if (method >= CHACHA20POLY1305IETF) {
        cipher_kt_t *cipher_info = (cipher_kt_t *)ss_malloc(sizeof(cipher_kt_t));
        cipher->info             = cipher_info;
        cipher->info->base       = NULL;
        cipher->info->key_bitlen = (unsigned int)supported_aead_ciphers_key_size[method] * 8;
        cipher->info->iv_size    = (int)supported_aead_ciphers_nonce_size[method];
    } else {
        cipher->info = (cipher_kt_t *)aead_get_cipher_type(method);
    }

    if (cipher->info == NULL && cipher->key_len == 0) {
        LOGE("Cipher %s not found in crypto library", supported_aead_ciphers[method]);
        FATAL("Cannot initialize cipher");
    }

    cipher->key_len = supported_aead_ciphers_key_size[method];
    if (pk) {
        memcpy(cipher->pk, pk, sizeof(cipher->pk));
    }
    if (sk) {
        memcpy(cipher->sk, sk, sizeof(cipher->sk));
    }

    if (cipher->key_len == 0) {
        FATAL("Cannot generate key and nonce");
    }

    cipher->nonce_len = supported_aead_ciphers_nonce_size[method];
    cipher->tag_len   = supported_aead_ciphers_tag_size[method];
    cipher->method    = method;

    return cipher;
}

cipher_t *
aead_init(const char *pk, const char *sk, const char *method)
{
    int m = AES128GCM;
    if (method != NULL) {
        /* check method validity */
        for (m = AES128GCM; m < AEAD_CIPHER_NUM; m++)
            if (strcmp(method, supported_aead_ciphers[m]) == 0) {
                break;
            }
        if (m >= AEAD_CIPHER_NUM) {
            LOGE("Invalid cipher name: %s, use aes-256-gcm instead", method);
            m = AES256GCM;
        }
    }
    return aead_key_init(m, pk, sk);
}

