/* SHA-1 (FIPS 180-4), enough to identify a ROM against the known list. */
#include "sha1.h"

#include <stdio.h>
#include <string.h>

static uint32_t rol(uint32_t value, unsigned bits)
{
    return (value << bits) | (value >> (32u - bits));
}

static void process_block(uint32_t state[5], const uint8_t block[64])
{
    uint32_t w[80];
    for (unsigned i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16)
             | ((uint32_t)block[i * 4 + 2] << 8) | block[i * 4 + 3];
    }
    for (unsigned i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    for (unsigned i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) { f = (b & c) | (~b & d); k = 0x5a827999u; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ed9eba1u; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdcu; }
        else { f = b ^ c ^ d; k = 0xca62c1d6u; }
        const uint32_t t = rol(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rol(b, 30); b = a; a = t;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

void oracles_sha1_hex(const uint8_t *data, size_t size, char out[41])
{
    uint32_t state[5] = { 0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u, 0xc3d2e1f0u };
    size_t offset = 0;
    for (; offset + 64 <= size; offset += 64) process_block(state, data + offset);
    uint8_t tail[128];
    const size_t rest = size - offset;
    memset(tail, 0, sizeof tail);
    memcpy(tail, data + offset, rest);
    tail[rest] = 0x80;
    const size_t total = rest + 1 + 8 > 64 ? 128 : 64;
    const uint64_t bits = (uint64_t)size * 8u;
    for (unsigned i = 0; i < 8; i++) tail[total - 1 - i] = (uint8_t)(bits >> (8u * i));
    process_block(state, tail);
    if (total == 128) process_block(state, tail + 64);
    for (unsigned i = 0; i < 5; i++) snprintf(out + i * 8, 9, "%08x", state[i]);
}
