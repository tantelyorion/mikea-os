#include "crypto.h"

/*
    X25519 (Curve25519, RFC 7748) : arithmetique en 16 limbes de 16 bits,
    echelle de Montgomery a temps constant (sans branchement sur le secret).
*/

typedef long long i64;
typedef i64 gf[16];

static const gf C121665 = { 0xDB41, 1 };

static void car25519(gf o)
{
    i64 c;

    for (int i = 0; i < 16; i++)
    {
        o[i] += (1LL << 16);
        c = o[i] >> 16;
        o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
        o[i] -= c << 16;
    }
}

static void sel25519(gf p, gf q, int b)
{
    i64 t;
    i64 c = ~((i64)b - 1);

    for (int i = 0; i < 16; i++)
    {
        t = c & (p[i] ^ q[i]);
        p[i] ^= t;
        q[i] ^= t;
    }
}

static void pack25519(u8* o, const gf n)
{
    int b;
    gf m, t;

    for (int i = 0; i < 16; i++)
    {
        t[i] = n[i];
    }

    car25519(t);
    car25519(t);
    car25519(t);

    for (int j = 0; j < 2; j++)
    {
        m[0] = t[0] - 0xffed;

        for (int i = 1; i < 15; i++)
        {
            m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
            m[i - 1] &= 0xffff;
        }

        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
        b = (int)((m[15] >> 16) & 1);
        m[14] &= 0xffff;
        sel25519(t, m, 1 - b);
    }

    for (int i = 0; i < 16; i++)
    {
        o[2 * i] = (u8)(t[i] & 0xff);
        o[2 * i + 1] = (u8)(t[i] >> 8);
    }
}

static void unpack25519(gf o, const u8* n)
{
    for (int i = 0; i < 16; i++)
    {
        o[i] = n[2 * i] + ((i64)n[2 * i + 1] << 8);
    }
    o[15] &= 0x7fff;
}

static void fe_add(gf o, const gf a, const gf b)
{
    for (int i = 0; i < 16; i++) { o[i] = a[i] + b[i]; }
}

static void fe_sub(gf o, const gf a, const gf b)
{
    for (int i = 0; i < 16; i++) { o[i] = a[i] - b[i]; }
}

static void fe_mul(gf o, const gf a, const gf b)
{
    i64 t[31];

    for (int i = 0; i < 31; i++) { t[i] = 0; }

    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            t[i + j] += a[i] * b[j];
        }
    }

    for (int i = 0; i < 15; i++)
    {
        t[i] += 38 * t[i + 16];
    }

    for (int i = 0; i < 16; i++) { o[i] = t[i]; }

    car25519(o);
    car25519(o);
}

static void fe_sq(gf o, const gf a)
{
    fe_mul(o, a, a);
}

static void fe_inv(gf o, const gf in)
{
    gf c;

    for (int a = 0; a < 16; a++) { c[a] = in[a]; }

    for (int a = 253; a >= 0; a--)
    {
        fe_sq(c, c);
        if (a != 2 && a != 4)
        {
            fe_mul(c, c, in);
        }
    }

    for (int a = 0; a < 16; a++) { o[a] = c[a]; }
}

void x25519(u8 out[32], const u8 scalar[32], const u8 point[32])
{
    u8 z[32];
    i64 x[80];
    i64 r;
    gf a, b, c, d, e, f;

    for (int i = 0; i < 31; i++) { z[i] = scalar[i]; }
    z[31] = (u8)((scalar[31] & 127) | 64);
    z[0] &= 248;

    unpack25519(x, point);

    for (int i = 0; i < 16; i++)
    {
        b[i] = x[i];
        d[i] = a[i] = c[i] = 0;
    }
    a[0] = d[0] = 1;

    for (int i = 254; i >= 0; --i)
    {
        r = (z[i >> 3] >> (i & 7)) & 1;

        sel25519(a, b, (int)r);
        sel25519(c, d, (int)r);

        fe_add(e, a, c);
        fe_sub(a, a, c);
        fe_add(c, b, d);
        fe_sub(b, b, d);
        fe_sq(d, e);
        fe_sq(f, a);
        fe_mul(a, c, a);
        fe_mul(c, b, e);
        fe_add(e, a, c);
        fe_sub(a, a, c);
        fe_sq(b, a);
        fe_sub(c, d, f);
        fe_mul(a, c, C121665);
        fe_add(a, a, d);
        fe_mul(c, c, a);
        fe_mul(a, d, f);
        fe_mul(d, b, x);
        fe_sq(b, e);

        sel25519(a, b, (int)r);
        sel25519(c, d, (int)r);
    }

    for (int i = 0; i < 16; i++)
    {
        x[i + 16] = a[i];
        x[i + 32] = c[i];
        x[i + 48] = b[i];
        x[i + 64] = d[i];
    }

    fe_inv(x + 32, x + 32);
    fe_mul(x + 16, x + 16, x + 32);
    pack25519(out, x + 16);
}

void x25519_base(u8 out[32], const u8 scalar[32])
{
    u8 base[32] = { 9 };
    x25519(out, scalar, base);
}
