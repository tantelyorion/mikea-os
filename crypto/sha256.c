#include "crypto.h"
#include "../libc/string.h"

static const u32 K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void compress(u32 state[8], const u8 block[64])
{
    u32 w[64];

    for (int i = 0; i < 16; i++)
    {
        w[i] = ((u32)block[i * 4] << 24) | ((u32)block[i * 4 + 1] << 16)
             | ((u32)block[i * 4 + 2] << 8) | block[i * 4 + 3];
    }

    for (int i = 16; i < 64; i++)
    {
        u32 s0 = ROTR(w[i - 15], 7) ^ ROTR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        u32 s1 = ROTR(w[i - 2], 17) ^ ROTR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    u32 a = state[0], b = state[1], c = state[2], d = state[3];
    u32 e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; i++)
    {
        u32 S1 = ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25);
        u32 ch = (e & f) ^ (~e & g);
        u32 t1 = h + S1 + ch + K[i] + w[i];
        u32 S0 = ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22);
        u32 maj = (a & b) ^ (a & c) ^ (b & c);
        u32 t2 = S0 + maj;

        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

/* ------------------------------------------------------------
   SHA-256 en flux (init / update / final) : aucune limite de taille.
   ------------------------------------------------------------ */

void sha256_init(sha256_ctx* c)
{
    static const u32 init[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    for (int i = 0; i < 8; i++)
    {
        c->state[i] = init[i];
    }
    c->buffered = 0;
    c->total = 0;
}

void sha256_update(sha256_ctx* c, const u8* data, u32 length)
{
    c->total += length;

    while (length > 0)
    {
        u32 room = 64 - c->buffered;
        u32 n = (length < room) ? length : room;

        mk_memcpy(c->buffer + c->buffered, data, n);
        c->buffered += n;
        data += n;
        length -= n;

        if (c->buffered == 64)
        {
            compress(c->state, c->buffer);
            c->buffered = 0;
        }
    }
}

void sha256_final(sha256_ctx* c, u8 out[32])
{
    u64 bits = c->total * 8;
    u8 pad[72];
    u32 pad_length = (c->buffered < 56) ? (56 - c->buffered) : (120 - c->buffered);

    mk_memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;

    for (int i = 0; i < 8; i++)
    {
        pad[pad_length + i] = (u8)(bits >> (56 - 8 * i));
    }

    u64 saved_total = c->total;
    sha256_update(c, pad, pad_length + 8);
    c->total = saved_total;

    for (int i = 0; i < 8; i++)
    {
        out[i * 4] = (u8)(c->state[i] >> 24);
        out[i * 4 + 1] = (u8)(c->state[i] >> 16);
        out[i * 4 + 2] = (u8)(c->state[i] >> 8);
        out[i * 4 + 3] = (u8)c->state[i];
    }
}

void sha256(const u8* data, u32 length, u8 out[32])
{
    sha256_ctx c;
    sha256_init(&c);
    sha256_update(&c, data, length);
    sha256_final(&c, out);
}

void hmac_sha256(const u8* key, u32 key_length, const u8* data, u32 length, u8 out[32])
{
    u8 k[64];
    u8 pad[64];
    u8 inner[32];
    sha256_ctx c;

    mk_memset(k, 0, sizeof(k));

    if (key_length > 64)
    {
        sha256(key, key_length, k);
    }
    else
    {
        mk_memcpy(k, key, key_length);
    }

    for (int i = 0; i < 64; i++) { pad[i] = (u8)(k[i] ^ 0x36); }
    sha256_init(&c);
    sha256_update(&c, pad, 64);
    sha256_update(&c, data, length);
    sha256_final(&c, inner);

    for (int i = 0; i < 64; i++) { pad[i] = (u8)(k[i] ^ 0x5c); }
    sha256_init(&c);
    sha256_update(&c, pad, 64);
    sha256_update(&c, inner, 32);
    sha256_final(&c, out);
}

int crypto_equal(const u8* a, const u8* b, u32 length)
{
    u8 difference = 0;

    for (u32 i = 0; i < length; i++)
    {
        difference |= (u8)(a[i] ^ b[i]);
    }

    return difference == 0;
}
