#include "crypto.h"
#include "../libc/string.h"

/*
    Generateur pseudo-aleatoire cryptographique (HMAC-SHA256 en mode compteur,
    reamorce a chaque appel avec l'entropie materielle disponible).

    Sources d'entropie :
      1. RDRAND (instruction du processeur) -- FORTE. Sous QEMU : "-cpu max".
      2. Gigue du compteur d'horloge TSC autour d'instructions serialisantes (CPUID) --
         plus FAIBLE, utilisee seulement en complement/secours.
    random_bytes() renvoie 1 si RDRAND a contribue, 0 sinon (l'appelant peut alors avertir).
*/

static u8 pool[32];
static u64 counter = 0;
static int seeded = 0;

static u64 read_tsc()
{
    u32 lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}

static int have_rdrand()
{
    u32 eax = 1, ebx, ecx = 0, edx;
    asm volatile("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx));
    return (ecx >> 30) & 1;
}

static int rdrand64(u64* out)
{
    for (int attempt = 0; attempt < 10; attempt++)
    {
        u64 value;
        unsigned char ok;
        asm volatile("rdrand %0; setc %1" : "=r"(value), "=qm"(ok));

        if (ok)
        {
            *out = value;
            return 1;
        }
    }
    return 0;
}

static void mix(const u8* data, u32 length)
{
    u8 buffer[32 + 64];
    u32 n = length > 64 ? 64 : length;

    mk_memcpy(buffer, pool, 32);
    mk_memcpy(buffer + 32, data, n);
    sha256(buffer, 32 + n, pool);
}

int random_bytes(u8* out, u32 length)
{
    int strong = 0;

    if (have_rdrand())
    {
        u8 entropy[32];
        int ok = 1;

        for (int i = 0; i < 4; i++)
        {
            u64 v;
            if (!rdrand64(&v)) { ok = 0; break; }
            mk_memcpy(entropy + i * 8, &v, 8);
        }

        if (ok)
        {
            mix(entropy, 32);
            strong = 1;
        }
    }

    /* Gigue TSC : on mesure la duree d'instructions serialisantes, dont les variations sont imprevisibles. */
    u8 jitter[64];
    u64 previous = read_tsc();

    for (int i = 0; i < 64; i++)
    {
        u8 acc = 0;

        for (int k = 0; k < 8; k++)
        {
            /* CPUID : instruction serialisante dont la duree varie (surtout en machine virtuelle). */
            u32 ca = (u32)k, cb, cc = 0, cd;
            asm volatile("cpuid" : "+a"(ca), "=b"(cb), "=c"(cc), "=d"(cd));
            u64 now = read_tsc();
            acc = (u8)(acc * 3 + (u8)(now - previous) + (u8)(now >> 7));
            previous = now;
        }
        jitter[i] = acc;
    }

    mix(jitter, 64);

    u64 stamp[2] = { read_tsc(), ++counter };
    mix((const u8*)stamp, sizeof(stamp));
    seeded = 1;

    u32 produced = 0;

    while (produced < length)
    {
        u8 block[32];
        u64 c = ++counter;

        hmac_sha256(pool, 32, (const u8*)&c, sizeof(c), block);

        for (u32 i = 0; i < 32 && produced < length; i++)
        {
            out[produced++] = block[i];
        }
    }

    /* Evolution irreversible du pool : un vol ulterieur de l'etat ne revele pas les sorties passees. */
    u8 one = 1;
    mix(&one, 1);

    (void)seeded;
    return strong;
}
