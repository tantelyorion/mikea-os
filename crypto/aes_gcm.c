#include "crypto.h"
#include "../libc/string.h"

/* ------------------------------------------------------------
   AES-128 (chiffrement de bloc)
   ------------------------------------------------------------ */

static u8 sbox[256];
static int sbox_ready = 0;

static u8 rotl8(u8 x, int s)
{
    return (u8)((x << s) | (x >> (8 - s)));
}

/* Table de substitution calculee (algorithme classique), evite une longue table a saisir. */
static void build_sbox()
{
    u8 p = 1, q = 1;

    do
    {
        p = (u8)(p ^ (u8)(p << 1) ^ ((p & 0x80) ? 0x1B : 0));

        q ^= (u8)(q << 1);
        q ^= (u8)(q << 2);
        q ^= (u8)(q << 4);
        if (q & 0x80)
        {
            q ^= 0x09;
        }

        u8 x = (u8)(q ^ rotl8(q, 1) ^ rotl8(q, 2) ^ rotl8(q, 3) ^ rotl8(q, 4));
        sbox[p] = (u8)(x ^ 0x63);
    }
    while (p != 1);

    sbox[0] = 0x63;
    sbox_ready = 1;
}

static u8 xtime(u8 x)
{
    return (u8)((x << 1) ^ ((x & 0x80) ? 0x1B : 0));
}

static void expand_key(const u8 key[16], u8 round_keys[176])
{
    mk_memcpy(round_keys, key, 16);

    u8 rcon = 1;

    for (int i = 16; i < 176; i += 4)
    {
        u8 t[4];
        mk_memcpy(t, round_keys + i - 4, 4);

        if (i % 16 == 0)
        {
            u8 first = t[0];
            t[0] = (u8)(sbox[t[1]] ^ rcon);
            t[1] = sbox[t[2]];
            t[2] = sbox[t[3]];
            t[3] = sbox[first];
            rcon = xtime(rcon);
        }

        for (int j = 0; j < 4; j++)
        {
            round_keys[i + j] = (u8)(round_keys[i - 16 + j] ^ t[j]);
        }
    }
}

static void aes_encrypt_block(const u8 round_keys[176], const u8 in[16], u8 out[16])
{
    u8 s[16];

    for (int i = 0; i < 16; i++)
    {
        s[i] = (u8)(in[i] ^ round_keys[i]);
    }

    for (int round = 1; round <= 10; round++)
    {
        u8 t[16];

        /* SubBytes + ShiftRows */
        for (int c = 0; c < 4; c++)
        {
            for (int r = 0; r < 4; r++)
            {
                t[c * 4 + r] = sbox[s[((c + r) % 4) * 4 + r]];
            }
        }

        /* MixColumns (sauf dernier tour) */
        if (round < 10)
        {
            for (int c = 0; c < 4; c++)
            {
                u8* col = t + c * 4;
                u8 a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];

                s[c * 4 + 0] = (u8)(xtime(a0) ^ (xtime(a1) ^ a1) ^ a2 ^ a3);
                s[c * 4 + 1] = (u8)(a0 ^ xtime(a1) ^ (xtime(a2) ^ a2) ^ a3);
                s[c * 4 + 2] = (u8)(a0 ^ a1 ^ xtime(a2) ^ (xtime(a3) ^ a3));
                s[c * 4 + 3] = (u8)((xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3));
            }
        }
        else
        {
            mk_memcpy(s, t, 16);
        }

        for (int i = 0; i < 16; i++)
        {
            s[i] ^= round_keys[round * 16 + i];
        }
    }

    mk_memcpy(out, s, 16);
}

/* ------------------------------------------------------------
   GHASH (multiplication dans GF(2^128), version simple bit a bit)
   ------------------------------------------------------------ */

static void gf128_mul(const u8 x[16], const u8 y[16], u8 out[16])
{
    u8 z[16];
    u8 v[16];

    mk_memset(z, 0, 16);
    mk_memcpy(v, y, 16);

    for (int i = 0; i < 128; i++)
    {
        if ((x[i / 8] >> (7 - (i % 8))) & 1)
        {
            for (int j = 0; j < 16; j++)
            {
                z[j] ^= v[j];
            }
        }

        int lsb = v[15] & 1;

        for (int j = 15; j > 0; j--)
        {
            v[j] = (u8)((v[j] >> 1) | (v[j - 1] << 7));
        }
        v[0] >>= 1;

        if (lsb)
        {
            v[0] ^= 0xE1;
        }
    }

    mk_memcpy(out, z, 16);
}

static void ghash_update(u8 y[16], const u8 h[16], const u8* data, u32 length)
{
    u8 block[16];

    for (u32 offset = 0; offset < length; offset += 16)
    {
        u32 n = (length - offset < 16) ? (length - offset) : 16;

        mk_memset(block, 0, 16);
        mk_memcpy(block, data + offset, n);

        for (int i = 0; i < 16; i++)
        {
            y[i] ^= block[i];
        }

        gf128_mul(y, h, y);
    }
}

static void inc32(u8 counter[16])
{
    for (int i = 15; i >= 12; i--)
    {
        if (++counter[i] != 0)
        {
            break;
        }
    }
}

static void gcm_crypt(const u8 round_keys[176], const u8 iv[12], const u8* in, u32 length, u8* out)
{
    u8 counter[16];
    u8 keystream[16];

    mk_memcpy(counter, iv, 12);
    counter[12] = 0; counter[13] = 0; counter[14] = 0; counter[15] = 1;

    for (u32 offset = 0; offset < length; offset += 16)
    {
        inc32(counter);
        aes_encrypt_block(round_keys, counter, keystream);

        u32 n = (length - offset < 16) ? (length - offset) : 16;

        for (u32 i = 0; i < n; i++)
        {
            out[offset + i] = (u8)(in[offset + i] ^ keystream[i]);
        }
    }
}

static void gcm_tag(const u8 round_keys[176], const u8 h[16], const u8 iv[12],
                    const u8* aad, u32 aad_length, const u8* ciphertext, u32 length, u8 tag[16])
{
    u8 y[16];
    u8 lengths[16];
    u8 j0[16];
    u8 mask[16];

    mk_memset(y, 0, 16);
    ghash_update(y, h, aad, aad_length);
    ghash_update(y, h, ciphertext, length);

    u64 aad_bits = (u64)aad_length * 8;
    u64 data_bits = (u64)length * 8;

    for (int i = 0; i < 8; i++)
    {
        lengths[7 - i] = (u8)(aad_bits >> (8 * i));
        lengths[15 - i] = (u8)(data_bits >> (8 * i));
    }

    for (int i = 0; i < 16; i++)
    {
        y[i] ^= lengths[i];
    }
    gf128_mul(y, h, y);

    mk_memcpy(j0, iv, 12);
    j0[12] = 0; j0[13] = 0; j0[14] = 0; j0[15] = 1;
    aes_encrypt_block(round_keys, j0, mask);

    for (int i = 0; i < 16; i++)
    {
        tag[i] = (u8)(y[i] ^ mask[i]);
    }
}

void aes128_gcm_encrypt(const u8 key[16], const u8 iv[12],
                        const u8* aad, u32 aad_length,
                        const u8* plaintext, u32 length,
                        u8* ciphertext, u8 tag[16])
{
    if (!sbox_ready)
    {
        build_sbox();
    }

    u8 round_keys[176];
    u8 h[16];
    u8 zero[16];

    expand_key(key, round_keys);
    mk_memset(zero, 0, 16);
    aes_encrypt_block(round_keys, zero, h);

    gcm_crypt(round_keys, iv, plaintext, length, ciphertext);
    gcm_tag(round_keys, h, iv, aad, aad_length, ciphertext, length, tag);
}

int aes128_gcm_decrypt(const u8 key[16], const u8 iv[12],
                       const u8* aad, u32 aad_length,
                       const u8* ciphertext, u32 length,
                       const u8 tag[16], u8* plaintext)
{
    if (!sbox_ready)
    {
        build_sbox();
    }

    u8 round_keys[176];
    u8 h[16];
    u8 zero[16];
    u8 expected[16];

    expand_key(key, round_keys);
    mk_memset(zero, 0, 16);
    aes_encrypt_block(round_keys, zero, h);

    gcm_tag(round_keys, h, iv, aad, aad_length, ciphertext, length, expected);

    /* On authentifie AVANT de dechiffrer : un message falsifie ne produit aucune sortie. */
    if (!crypto_equal(expected, tag, 16))
    {
        return 0;
    }

    gcm_crypt(round_keys, iv, ciphertext, length, plaintext);
    return 1;
}
