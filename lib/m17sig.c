/*
 * M17 stream signature hash (M17 Specification 2.1.0, section "Digital Signatures")
 *
 * Copyright 2026 Wojciech Kaczmarski
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "m17sig.h"
#include <string.h>

/* ------------------------------------------------------------- SHA-256 */

static const uint32_t K[64] =
{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void compress(uint32_t state[8], const uint8_t block[64])
{
    uint32_t w[16]; /* rolling 16-word message schedule */
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; i++)
    {
        uint32_t wi;

        if (i < 16)
        {
            wi = ((uint32_t)block[4 * i] << 24) | ((uint32_t)block[4 * i + 1] << 16) |
                 ((uint32_t)block[4 * i + 2] << 8) | (uint32_t)block[4 * i + 3];
        }
        else
        {
            uint32_t w15 = w[(i - 15) & 15], w2 = w[(i - 2) & 15];
            uint32_t s0 = ROR(w15, 7) ^ ROR(w15, 18) ^ (w15 >> 3);
            uint32_t s1 = ROR(w2, 17) ^ ROR(w2, 19) ^ (w2 >> 10);
            wi = w[i & 15] + s0 + w[(i - 7) & 15] + s1;
        }
        w[i & 15] = wi;

        uint32_t t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + wi;
        uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));

        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

static void sha_init(m17sig_ctx_t *ctx)
{
    static const uint32_t iv[8] =
    {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    memcpy(ctx->state, iv, sizeof(iv));
    ctx->length = 0;
}

static void sha_update(m17sig_ctx_t *ctx, const uint8_t *data, size_t len)
{
    size_t fill = (size_t)(ctx->length & 63);

    ctx->length += len;

    while (len > 0)
    {
        size_t n = 64 - fill;
        if (n > len)
            n = len;

        memcpy(&ctx->buf[fill], data, n);
        fill += n;
        data += n;
        len  -= n;

        if (fill == 64)
        {
            compress(ctx->state, ctx->buf);
            fill = 0;
        }
    }
}

static void sha_final(m17sig_ctx_t *ctx, uint8_t out[32])
{
    uint64_t bits = ctx->length * 8;
    size_t fill = (size_t)(ctx->length & 63);

    ctx->buf[fill++] = 0x80;
    if (fill > 56)
    {
        memset(&ctx->buf[fill], 0, 64 - fill);
        compress(ctx->state, ctx->buf);
        fill = 0;
    }
    memset(&ctx->buf[fill], 0, 56 - fill);
    for (int i = 0; i < 8; i++)
        ctx->buf[63 - i] = (uint8_t)(bits >> (8 * i));
    compress(ctx->state, ctx->buf);

    for (int i = 0; i < 8; i++)
    {
        out[4 * i]     = (uint8_t)(ctx->state[i] >> 24);
        out[4 * i + 1] = (uint8_t)(ctx->state[i] >> 16);
        out[4 * i + 2] = (uint8_t)(ctx->state[i] >> 8);
        out[4 * i + 3] = (uint8_t)(ctx->state[i]);
    }

    memset(ctx, 0, sizeof(*ctx));
}

void m17sig_sha256(const uint8_t *data, size_t len, uint8_t out[32])
{
    m17sig_ctx_t ctx;

    sha_init(&ctx);
    sha_update(&ctx, data, len);
    sha_final(&ctx, out);
}

/* ------------------------------------------------------ M17 stream hash */

void m17sig_init(m17sig_ctx_t *ctx)
{
    sha_init(ctx);
    sha_update(ctx, (const uint8_t *)"M17-SIG-v2", 10);
}

void m17sig_frame(m17sig_ctx_t *ctx, uint16_t fn, const uint8_t payload[M17SIG_PAYLOAD_LEN])
{
    uint8_t fn_be[2] = { (uint8_t)(fn >> 8), (uint8_t)fn };

    sha_update(ctx, fn_be, 2);
    sha_update(ctx, payload, M17SIG_PAYLOAD_LEN);
}

void m17sig_final(m17sig_ctx_t *ctx, uint8_t hash[M17SIG_HASH_LEN])
{
    sha_final(ctx, hash);
}

void m17sig_auth_build(uint8_t payload[M17SIG_PAYLOAD_LEN], const uint8_t signer[6],
                       const uint8_t dst[6], uint32_t timestamp)
{
    memcpy(&payload[0], signer, 6);
    memcpy(&payload[6], dst, 6);
    payload[12] = (uint8_t)(timestamp >> 24);
    payload[13] = (uint8_t)(timestamp >> 16);
    payload[14] = (uint8_t)(timestamp >> 8);
    payload[15] = (uint8_t)timestamp;
}

void m17sig_auth_parse(const uint8_t payload[M17SIG_PAYLOAD_LEN], uint8_t signer[6],
                       uint8_t dst[6], uint32_t *timestamp)
{
    memcpy(signer, &payload[0], 6);
    memcpy(dst, &payload[6], 6);
    *timestamp = ((uint32_t)payload[12] << 24) | ((uint32_t)payload[13] << 16) |
                 ((uint32_t)payload[14] << 8) | (uint32_t)payload[15];
}

/* ----------------------------------------------------------- self-test */

int m17sig_selftest(void)
{
    static const uint8_t abc[32] =
    {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    static const uint8_t two_blocks[32] = /* "abcdbcde...nopq", 448 bits */
    {
        0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8, 0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39,
        0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67, 0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1
    };
    /* spec 2.1.0 test vector: frames 0..2 (payload bytes 0x00..0x2F),
       Authentication Frame N0CALL -> W1AW, 2026-11-18T00:00:00Z */
    static const uint8_t n0call[6] = { 0x00, 0x00, 0x4b, 0x13, 0xd1, 0x06 };
    static const uint8_t w1aw[6]   = { 0x00, 0x00, 0x00, 0x16, 0x80, 0xb7 };
    static const uint8_t spec_h[32] =
    {
        0x2d, 0xd1, 0xbc, 0x7b, 0x5c, 0x01, 0x7f, 0x10, 0x0f, 0x93, 0x79, 0xf4, 0x2e, 0xaa, 0x20, 0x5c,
        0xe9, 0x0b, 0x8c, 0x7e, 0xa2, 0x63, 0xf2, 0xf8, 0x0e, 0x37, 0xba, 0x6e, 0x6d, 0x6b, 0x58, 0x8d
    };
    uint8_t h[32], p[16], s[6], d[6];
    uint32_t ts;
    m17sig_ctx_t ctx;
    int fails = 0;

    m17sig_sha256((const uint8_t *)"abc", 3, h);
    fails += memcmp(h, abc, 32) != 0;

    m17sig_sha256((const uint8_t *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, h);
    fails += memcmp(h, two_blocks, 32) != 0;

    m17sig_init(&ctx);
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 16; j++)
            p[j] = (uint8_t)(16 * i + j);
        m17sig_frame(&ctx, (uint16_t)i, p);
    }
    m17sig_auth_build(p, n0call, w1aw, 217123200UL);
    m17sig_frame(&ctx, M17SIG_FN_AUTH, p);
    m17sig_final(&ctx, h);
    fails += memcmp(h, spec_h, 32) != 0;

    m17sig_auth_parse(p, s, d, &ts);
    fails += memcmp(s, n0call, 6) != 0 || memcmp(d, w1aw, 6) != 0 || ts != 217123200UL;

    return fails;
}
