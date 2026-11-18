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
 *   H = SHA-256( "M17-SIG-v2" || FN_0 || P_0 || ... || FN_{N-1} || P_{N-1} || FN_A || P_A )
 *
 *   FN_i - 16-bit Frame Number as transmitted, most significant byte first
 *   P_i  - 16-byte payload as transmitted (after encryption, if any)
 *   FN_A - 0x7FFB, P_A - Authentication Frame payload:
 *          signer callsign (6) || destination (6) || timestamp (4, s since 2020-01-01 UTC, MSB first)
 *
 * The signature frames (FN 0x7FFC..0x7FFE, 0xFFFF) are not hashed. H is signed with ECDSA over
 * secp256r1, used directly as the message hash: signature = r || s (32 + 32 bytes, MSB first).
 *
 * NOTE: an m17sig_ctx_t holds pointers into itself - do not copy it (or move it in memory)
 * between m17sig_init() and m17sig_final().
 */
#ifndef M17SIG_H
#define M17SIG_H

#include <stdint.h>
#include <stddef.h>
#include "sha-256.h"

#ifdef __cplusplus
extern "C" {
#endif

#define M17SIG_PAYLOAD_LEN 16
#define M17SIG_HASH_LEN    32
#define M17SIG_SIG_LEN     64
#define M17SIG_FN_AUTH     0x7FFBU     /* Authentication Frame */
#define M17SIG_FN_SIG      0x7FFCU     /* first of the 4 signature frames (0x7FFC..0x7FFE, 0xFFFF) */
#define M17SIG_EPOCH       1577836800L /* 2020-01-01T00:00:00Z as a Unix time */

typedef struct
{
    struct Sha_256 sha;
    uint8_t out[SIZE_OF_SHA_256_HASH];
} m17sig_ctx_t;

/* stream hash: init at FN 0, one m17sig_frame() per stream frame and for the Authentication Frame */
void m17sig_init(m17sig_ctx_t *ctx);
void m17sig_frame(m17sig_ctx_t *ctx, uint16_t fn, const uint8_t payload[M17SIG_PAYLOAD_LEN]);
void m17sig_final(m17sig_ctx_t *ctx, uint8_t hash[M17SIG_HASH_LEN]);

/* Authentication Frame payload */
void m17sig_auth_build(uint8_t payload[M17SIG_PAYLOAD_LEN], const uint8_t signer[6],
                       const uint8_t dst[6], uint32_t timestamp);
void m17sig_auth_parse(const uint8_t payload[M17SIG_PAYLOAD_LEN], uint8_t signer[6],
                       uint8_t dst[6], uint32_t *timestamp);

/* known answer tests (FIPS 180-4 SHA-256, M17 spec 2.1.0 test vector hash): 0 = pass */
int m17sig_selftest(void);

#ifdef __cplusplus
}
#endif

#endif
