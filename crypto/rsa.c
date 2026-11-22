#include "crypto.h"
#include "../libc/string.h"

/*
    Verification RSA par grands entiers a limbes de 32 bits (4096 bits max).
    Seule l'operation PUBLIQUE est implementee (s^e mod n, e petit) : aucune
    donnee secrete n'est manipulee, donc pas de contrainte de temps constant.
    Multiplication modulaire "par decalage et addition" : lente mais courte et
    facile a verifier.
*/

#define MAX_LIMBS 128

static int big_cmp(const u32* a, const u32* b, u32 n)
{
    for (int i = (int)n - 1; i >= 0; i--)
    {
        if (a[i] != b[i])
        {
            return a[i] > b[i] ? 1 : -1;
        }
    }
    return 0;
}

static u32 big_add(u32* a, const u32* b, u32 n)
{
    u64 carry = 0;

    for (u32 i = 0; i < n; i++)
    {
        u64 sum = (u64)a[i] + b[i] + carry;
        a[i] = (u32)sum;
        carry = sum >> 32;
    }
    return (u32)carry;
}

static u32 big_sub(u32* a, const u32* b, u32 n)
{
    u64 borrow = 0;

    for (u32 i = 0; i < n; i++)
    {
        u64 diff = (u64)a[i] - b[i] - borrow;
        a[i] = (u32)diff;
        borrow = (diff >> 32) & 1;
    }
    return (u32)borrow;
}

static u32 big_shl1(u32* a, u32 n)
{
    u32 carry = 0;

    for (u32 i = 0; i < n; i++)
    {
        u32 next = a[i] >> 31;
        a[i] = (a[i] << 1) | carry;
        carry = next;
    }
    return carry;
}

/* r = (a * b) mod n, avec a, b < n. */
static void mul_mod(u32* r, const u32* a, const u32* b, const u32* n, u32 limbs)
{
    u32 result[MAX_LIMBS];

    mk_memset(result, 0, limbs * 4);

    for (int bit = (int)limbs * 32 - 1; bit >= 0; bit--)
    {
        u32 carry = big_shl1(result, limbs);

        if (carry || big_cmp(result, n, limbs) >= 0)
        {
            big_sub(result, n, limbs);
        }

        if ((b[bit / 32] >> (bit % 32)) & 1)
        {
            carry = big_add(result, a, limbs);

            if (carry || big_cmp(result, n, limbs) >= 0)
            {
                big_sub(result, n, limbs);
            }
        }
    }

    mk_memcpy(r, result, limbs * 4);
}

static void big_from_bytes(u32* out, u32 limbs, const u8* bytes, u32 length)
{
    mk_memset(out, 0, limbs * 4);

    for (u32 i = 0; i < length; i++)
    {
        u32 pos = length - 1 - i; /* poids faible d'abord */
        out[i / 4] |= (u32)bytes[pos] << (8 * (i % 4));
    }
}

static void big_to_bytes(u8* out, u32 length, const u32* in)
{
    for (u32 i = 0; i < length; i++)
    {
        out[length - 1 - i] = (u8)(in[i / 4] >> (8 * (i % 4)));
    }
}

/* MGF1 avec SHA-256. */
static void mgf1(const u8* seed, u32 seed_length, u8* out, u32 out_length)
{
    u8 block[64 + 4];
    u8 digest[32];
    u32 produced = 0;

    for (u32 counter = 0; produced < out_length; counter++)
    {
        mk_memcpy(block, seed, seed_length);
        block[seed_length] = (u8)(counter >> 24);
        block[seed_length + 1] = (u8)(counter >> 16);
        block[seed_length + 2] = (u8)(counter >> 8);
        block[seed_length + 3] = (u8)counter;

        sha256(block, seed_length + 4, digest);

        for (u32 i = 0; i < 32 && produced < out_length; i++)
        {
            out[produced++] = digest[i];
        }
    }
}

int rsa_pss_sha256_verify(const u8* n_bytes, u32 n_length, const u8* e_bytes, u32 e_length,
                          const u8* message, u32 message_length,
                          const u8* signature, u32 signature_length)
{
    /* Retire les zeros de tete. */
    while (n_length > 0 && n_bytes[0] == 0) { n_bytes++; n_length--; }
    while (e_length > 0 && e_bytes[0] == 0) { e_bytes++; e_length--; }

    if (n_length < 128 || n_length > MAX_LIMBS * 4 || e_length == 0 || e_length > 4)
    {
        return 0; /* module < 1024 bits refuse (trop faible) ou trop grand */
    }

    if (signature_length != n_length)
    {
        return 0;
    }

    u32 limbs = (n_length + 3) / 4;
    u32 n[MAX_LIMBS], s[MAX_LIMBS], m[MAX_LIMBS];

    big_from_bytes(n, limbs, n_bytes, n_length);
    big_from_bytes(s, limbs, signature, signature_length);

    if (big_cmp(s, n, limbs) >= 0)
    {
        return 0; /* representant hors intervalle */
    }

    u32 exponent = 0;
    for (u32 i = 0; i < e_length; i++)
    {
        exponent = (exponent << 8) | e_bytes[i];
    }

    if (exponent < 3 || (exponent & 1) == 0)
    {
        return 0;
    }

    /* m = s^e mod n (carre-et-multiplie, exposant public). */
    mk_memcpy(m, s, limbs * 4);

    int top = 31;
    while (top > 0 && !((exponent >> top) & 1)) { top--; }

    for (int bit = top - 1; bit >= 0; bit--)
    {
        mul_mod(m, m, m, n, limbs);

        if ((exponent >> bit) & 1)
        {
            mul_mod(m, m, s, n, limbs);
        }
    }

    u8 em_full[MAX_LIMBS * 4];
    big_to_bytes(em_full, n_length, m);

    /* Taille du module en bits. */
    u32 mod_bits = n_length * 8;
    u8 lead = n_bytes[0];
    for (int b = 7; b >= 0 && !((lead >> b) & 1); b--) { mod_bits--; }

    u32 em_bits = mod_bits - 1;
    u32 em_length = (em_bits + 7) / 8;
    const u8* em = em_full + (n_length - em_length);

    const u32 hash_length = 32;
    const u32 salt_length = 32;

    if (em_length < hash_length + salt_length + 2 || em[em_length - 1] != 0xBC)
    {
        return 0;
    }

    u32 db_length = em_length - hash_length - 1;
    const u8* masked_db = em;
    const u8* h = em + db_length;

    u32 unused_bits = 8 * em_length - em_bits;
    if (unused_bits > 0 && (masked_db[0] >> (8 - unused_bits)) != 0)
    {
        return 0;
    }

    u8 db[MAX_LIMBS * 4];
    mgf1(h, hash_length, db, db_length);

    for (u32 i = 0; i < db_length; i++)
    {
        db[i] ^= masked_db[i];
    }

    if (unused_bits > 0)
    {
        db[0] &= (u8)(0xFF >> unused_bits);
    }

    u32 pad_length = db_length - salt_length - 1;

    for (u32 i = 0; i < pad_length; i++)
    {
        if (db[i] != 0)
        {
            return 0;
        }
    }

    if (db[pad_length] != 0x01)
    {
        return 0;
    }

    const u8* salt = db + pad_length + 1;

    u8 m_hash[32];
    sha256(message, message_length, m_hash);

    u8 m_prime[8 + 32 + 32];
    mk_memset(m_prime, 0, 8);
    mk_memcpy(m_prime + 8, m_hash, 32);
    mk_memcpy(m_prime + 40, salt, salt_length);

    u8 h_prime[32];
    sha256(m_prime, sizeof(m_prime), h_prime);

    return crypto_equal(h, h_prime, 32);
}

/* ============================================================
   ECDSA P-256 / SHA-256 (verification uniquement)
   Coordonnees jacobiennes, arithmetique modulaire par grands entiers de 8 limbes.
   Toutes les donnees manipulees sont PUBLIQUES (cle publique, message, signature) :
   pas de contrainte de temps constant.
   ============================================================ */

#define EC_L 8

static const u8 P256_P[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};
static const u8 P256_N[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xBC, 0xE6, 0xFA, 0xAD, 0xA7, 0x17, 0x9E, 0x84, 0xF3, 0xB9, 0xCA, 0xC2, 0xFC, 0x63, 0x25, 0x51
};
static const u8 P256_B[32] = {
    0x5A, 0xC6, 0x35, 0xD8, 0xAA, 0x3A, 0x93, 0xE7, 0xB3, 0xEB, 0xBD, 0x55, 0x76, 0x98, 0x86, 0xBC,
    0x65, 0x1D, 0x06, 0xB0, 0xCC, 0x53, 0xB0, 0xF6, 0x3B, 0xCE, 0x3C, 0x3E, 0x27, 0xD2, 0x60, 0x4B
};
static const u8 P256_GX[32] = {
    0x6B, 0x17, 0xD1, 0xF2, 0xE1, 0x2C, 0x42, 0x47, 0xF8, 0xBC, 0xE6, 0xE5, 0x63, 0xA4, 0x40, 0xF2,
    0x77, 0x03, 0x7D, 0x81, 0x2D, 0xEB, 0x33, 0xA0, 0xF4, 0xA1, 0x39, 0x45, 0xD8, 0x98, 0xC2, 0x96
};
static const u8 P256_GY[32] = {
    0x4F, 0xE3, 0x42, 0xE2, 0xFE, 0x1A, 0x7F, 0x9B, 0x8E, 0xE7, 0xEB, 0x4A, 0x7C, 0x0F, 0x9E, 0x16,
    0x2B, 0xCE, 0x33, 0x57, 0x6B, 0x31, 0x5E, 0xCE, 0xCB, 0xB6, 0x40, 0x68, 0x37, 0xBF, 0x51, 0xF5
};

static u32 fp[EC_L];  /* p */
static u32 fn[EC_L];  /* n (ordre du groupe) */

typedef struct
{
    u32 x[EC_L];
    u32 y[EC_L];
    u32 z[EC_L]; /* z == 0 : point a l'infini */
} jpoint;

static int big_is_zero(const u32* a)
{
    u32 acc = 0;
    for (int i = 0; i < EC_L; i++) { acc |= a[i]; }
    return acc == 0;
}

static void fe_add_mod(u32* r, const u32* a, const u32* b)
{
    u32 t[EC_L];
    mk_memcpy(t, a, EC_L * 4);
    u32 carry = big_add(t, b, EC_L);

    if (carry || big_cmp(t, fp, EC_L) >= 0)
    {
        big_sub(t, fp, EC_L);
    }
    mk_memcpy(r, t, EC_L * 4);
}

static void fe_sub_mod(u32* r, const u32* a, const u32* b)
{
    u32 t[EC_L];
    mk_memcpy(t, a, EC_L * 4);

    if (big_sub(t, b, EC_L))
    {
        big_add(t, fp, EC_L);
    }
    mk_memcpy(r, t, EC_L * 4);
}

static void fe_mul_mod(u32* r, const u32* a, const u32* b)
{
    mul_mod(r, a, b, fp, EC_L);
}

/* r = base^exp mod m (exposant sur EC_L limbes). */
static void pow_mod(u32* r, const u32* base, const u32* exp, const u32* m)
{
    u32 result[EC_L];
    u32 b[EC_L];

    mk_memset(result, 0, sizeof(result));
    result[0] = 1;
    mk_memcpy(b, base, sizeof(b));

    for (int bit = EC_L * 32 - 1; bit >= 0; bit--)
    {
        mul_mod(result, result, result, m, EC_L);

        if ((exp[bit / 32] >> (bit % 32)) & 1)
        {
            mul_mod(result, result, b, m, EC_L);
        }
    }

    mk_memcpy(r, result, sizeof(result));
}

static void inverse_mod(u32* r, const u32* a, const u32* m)
{
    u32 e[EC_L];
    u32 two[EC_L];

    mk_memcpy(e, m, sizeof(e));
    mk_memset(two, 0, sizeof(two));
    two[0] = 2;
    big_sub(e, two, EC_L); /* m - 2 : inverse de Fermat (m premier) */
    pow_mod(r, a, e, m);
}

static void jp_set_infinity(jpoint* p)
{
    mk_memset(p, 0, sizeof(*p));
    p->x[0] = 1;
    p->y[0] = 1;
}

static void jp_double(jpoint* r, const jpoint* p)
{
    if (big_is_zero(p->z) || big_is_zero(p->y))
    {
        jp_set_infinity(r);
        return;
    }

    u32 delta[EC_L], gamma[EC_L], beta[EC_L], alpha[EC_L], t1[EC_L], t2[EC_L];
    u32 x3[EC_L], y3[EC_L], z3[EC_L];

    fe_mul_mod(delta, p->z, p->z);
    fe_mul_mod(gamma, p->y, p->y);
    fe_mul_mod(beta, p->x, gamma);

    fe_sub_mod(t1, p->x, delta);
    fe_add_mod(t2, p->x, delta);
    fe_mul_mod(alpha, t1, t2);
    fe_add_mod(t1, alpha, alpha);
    fe_add_mod(alpha, t1, alpha);           /* alpha = 3 (X - delta)(X + delta) */

    fe_mul_mod(x3, alpha, alpha);
    fe_add_mod(t1, beta, beta);
    fe_add_mod(t1, t1, t1);
    fe_add_mod(t2, t1, t1);                 /* t2 = 8 beta */
    fe_sub_mod(x3, x3, t2);                 /* X3 = alpha^2 - 8 beta */

    fe_add_mod(z3, p->y, p->z);
    fe_mul_mod(z3, z3, z3);
    fe_sub_mod(z3, z3, gamma);
    fe_sub_mod(z3, z3, delta);              /* Z3 = (Y+Z)^2 - gamma - delta */

    fe_sub_mod(t1, t1, x3);                 /* 4 beta - X3 */
    fe_mul_mod(y3, alpha, t1);
    fe_mul_mod(t2, gamma, gamma);
    fe_add_mod(t2, t2, t2);
    fe_add_mod(t2, t2, t2);
    fe_add_mod(t2, t2, t2);                 /* 8 gamma^2 */
    fe_sub_mod(y3, y3, t2);

    mk_memcpy(r->x, x3, sizeof(x3));
    mk_memcpy(r->y, y3, sizeof(y3));
    mk_memcpy(r->z, z3, sizeof(z3));
}

static void jp_add(jpoint* r, const jpoint* p, const jpoint* q)
{
    if (big_is_zero(p->z)) { *r = *q; return; }
    if (big_is_zero(q->z)) { *r = *p; return; }

    u32 z1z1[EC_L], z2z2[EC_L], u1[EC_L], u2[EC_L], s1[EC_L], s2[EC_L];
    u32 h[EC_L], i[EC_L], j[EC_L], rr[EC_L], v[EC_L], t[EC_L];
    u32 x3[EC_L], y3[EC_L], z3[EC_L];

    fe_mul_mod(z1z1, p->z, p->z);
    fe_mul_mod(z2z2, q->z, q->z);
    fe_mul_mod(u1, p->x, z2z2);
    fe_mul_mod(u2, q->x, z1z1);
    fe_mul_mod(t, q->z, z2z2);
    fe_mul_mod(s1, p->y, t);
    fe_mul_mod(t, p->z, z1z1);
    fe_mul_mod(s2, q->y, t);

    if (big_cmp(u1, u2, EC_L) == 0)
    {
        if (big_cmp(s1, s2, EC_L) == 0)
        {
            jp_double(r, p);
        }
        else
        {
            jp_set_infinity(r);
        }
        return;
    }

    fe_sub_mod(h, u2, u1);
    fe_add_mod(t, h, h);
    fe_mul_mod(i, t, t);
    fe_mul_mod(j, h, i);
    fe_sub_mod(t, s2, s1);
    fe_add_mod(rr, t, t);
    fe_mul_mod(v, u1, i);

    fe_mul_mod(x3, rr, rr);
    fe_sub_mod(x3, x3, j);
    fe_sub_mod(x3, x3, v);
    fe_sub_mod(x3, x3, v);

    fe_sub_mod(t, v, x3);
    fe_mul_mod(y3, rr, t);
    fe_mul_mod(t, s1, j);
    fe_add_mod(t, t, t);
    fe_sub_mod(y3, y3, t);

    fe_add_mod(t, p->z, q->z);
    fe_mul_mod(t, t, t);
    fe_sub_mod(t, t, z1z1);
    fe_sub_mod(t, t, z2z2);
    fe_mul_mod(z3, t, h);

    mk_memcpy(r->x, x3, sizeof(x3));
    mk_memcpy(r->y, y3, sizeof(y3));
    mk_memcpy(r->z, z3, sizeof(z3));
}

static void jp_mul(jpoint* r, const jpoint* base, const u32* scalar)
{
    jpoint acc;
    jp_set_infinity(&acc);

    for (int bit = EC_L * 32 - 1; bit >= 0; bit--)
    {
        jp_double(&acc, &acc);

        if ((scalar[bit / 32] >> (bit % 32)) & 1)
        {
            jp_add(&acc, &acc, base);
        }
    }

    *r = acc;
}

int ecdsa_p256_sha256_verify(const u8 public_key[65],
                             const u8* message, u32 message_length,
                             const u8* r_bytes, u32 r_length,
                             const u8* s_bytes, u32 s_length)
{
    if (public_key[0] != 0x04)
    {
        return 0;
    }

    while (r_length > 0 && r_bytes[0] == 0) { r_bytes++; r_length--; }
    while (s_length > 0 && s_bytes[0] == 0) { s_bytes++; s_length--; }

    if (r_length == 0 || r_length > 32 || s_length == 0 || s_length > 32)
    {
        return 0;
    }

    big_from_bytes(fp, EC_L, P256_P, 32);
    big_from_bytes(fn, EC_L, P256_N, 32);

    u32 r[EC_L], s[EC_L], qx[EC_L], qy[EC_L], gx[EC_L], gy[EC_L], b[EC_L];

    big_from_bytes(r, EC_L, r_bytes, r_length);
    big_from_bytes(s, EC_L, s_bytes, s_length);
    big_from_bytes(qx, EC_L, public_key + 1, 32);
    big_from_bytes(qy, EC_L, public_key + 33, 32);
    big_from_bytes(gx, EC_L, P256_GX, 32);
    big_from_bytes(gy, EC_L, P256_GY, 32);
    big_from_bytes(b, EC_L, P256_B, 32);

    if (big_is_zero(r) || big_cmp(r, fn, EC_L) >= 0 || big_is_zero(s) || big_cmp(s, fn, EC_L) >= 0)
    {
        return 0;
    }

    if (big_cmp(qx, fp, EC_L) >= 0 || big_cmp(qy, fp, EC_L) >= 0)
    {
        return 0;
    }

    /* Le point public doit appartenir a la courbe : y^2 = x^3 - 3x + b. */
    {
        u32 lhs[EC_L], rhs[EC_L], t[EC_L];

        fe_mul_mod(lhs, qy, qy);
        fe_mul_mod(t, qx, qx);
        fe_mul_mod(rhs, t, qx);
        fe_add_mod(t, qx, qx);
        fe_add_mod(t, t, qx);
        fe_sub_mod(rhs, rhs, t);
        fe_add_mod(rhs, rhs, b);

        if (big_cmp(lhs, rhs, EC_L) != 0)
        {
            return 0;
        }
    }

    u8 digest[32];
    u32 e[EC_L];
    sha256(message, message_length, digest);
    big_from_bytes(e, EC_L, digest, 32);

    if (big_cmp(e, fn, EC_L) >= 0)
    {
        big_sub(e, fn, EC_L);
    }

    u32 w[EC_L], u1[EC_L], u2[EC_L];

    inverse_mod(w, s, fn);
    mul_mod(u1, e, w, fn, EC_L);
    mul_mod(u2, r, w, fn, EC_L);

    jpoint g, q, p1, p2, sum;

    mk_memcpy(g.x, gx, sizeof(gx));
    mk_memcpy(g.y, gy, sizeof(gy));
    mk_memset(g.z, 0, sizeof(g.z));
    g.z[0] = 1;

    mk_memcpy(q.x, qx, sizeof(qx));
    mk_memcpy(q.y, qy, sizeof(qy));
    mk_memset(q.z, 0, sizeof(q.z));
    q.z[0] = 1;

    jp_mul(&p1, &g, u1);
    jp_mul(&p2, &q, u2);
    jp_add(&sum, &p1, &p2);

    if (big_is_zero(sum.z))
    {
        return 0;
    }

    /* x affine = X / Z^2 */
    u32 zinv[EC_L], zinv2[EC_L], x_affine[EC_L];

    inverse_mod(zinv, sum.z, fp);
    fe_mul_mod(zinv2, zinv, zinv);
    fe_mul_mod(x_affine, sum.x, zinv2);

    if (big_cmp(x_affine, fn, EC_L) >= 0)
    {
        big_sub(x_affine, fn, EC_L);
    }

    return big_cmp(x_affine, r, EC_L) == 0;
}
