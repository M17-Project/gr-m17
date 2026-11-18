/*
 * M17 stream signature hash (M17 Specification 2.1.0, section "Digital Signatures")
 *
 * Copyright 2026 Wojciech Kaczmarski
 * SPDX-License-Identifier: MIT
 *
 * Uses the SHA-256 implementation from https://github.com/amosnier/sha-2 (sha-256.c/.h,
 * Unlicense or 0BSD), included as a git submodule. The same two m17sig files are used by
 * gr-m17, M17_Implementations, the SDR# plugin and AtoM17; keep the copies identical.
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

void m17sig_init(m17sig_ctx_t *ctx)
{
    sha_256_init(&ctx->sha, ctx->out);
    sha_256_write(&ctx->sha, "M17-SIG-v2", 10);
}

void m17sig_frame(m17sig_ctx_t *ctx, uint16_t fn, const uint8_t payload[M17SIG_PAYLOAD_LEN])
{
    uint8_t fn_be[2] = { (uint8_t)(fn >> 8), (uint8_t)fn };

    sha_256_write(&ctx->sha, fn_be, 2);
    sha_256_write(&ctx->sha, payload, M17SIG_PAYLOAD_LEN);
}

void m17sig_final(m17sig_ctx_t *ctx, uint8_t hash[M17SIG_HASH_LEN])
{
    sha_256_close(&ctx->sha);
    memcpy(hash, ctx->out, M17SIG_HASH_LEN);
    memset(ctx, 0, sizeof(*ctx));
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

    calc_sha_256(h, "abc", 3);
    fails += memcmp(h, abc, 32) != 0;

    calc_sha_256(h, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56);
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
