#include "crypto.h"
#include "../libc/string.h"

void console_write(const char* text);

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    return 0;
}

/* Decode une chaine hexadecimale. Renvoie le nombre d'octets. */
static u32 from_hex(const char* hex, u8* out)
{
    u32 n = 0;

    for (u32 i = 0; hex[i] != 0 && hex[i + 1] != 0; i += 2)
    {
        out[n++] = (u8)((hex_value(hex[i]) << 4) | hex_value(hex[i + 1]));
    }
    return n;
}

static int check(const char* name, const u8* got, const char* expected_hex)
{
    u8 expected[128];
    u32 n = from_hex(expected_hex, expected);
    int ok = crypto_equal(got, expected, n);

    console_write(ok ? "  [OK]    " : "  [ECHEC] ");
    console_write(name);
    console_write("\n");
    return ok ? 0 : 1;
}

int crypto_selftest()
{
    int failures = 0;
    u8 out[128];

    console_write("Tests cryptographiques (vecteurs officiels) :\n");

    /* SHA-256("abc") - FIPS 180-4 */
    sha256((const u8*)"abc", 3, out);
    failures += check("SHA-256 \"abc\"", out,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    /* SHA-256 d'un message de 56 octets (deux blocs de remplissage) */
    sha256((const u8*)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, out);
    failures += check("SHA-256 (message de 56 octets)", out,
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    /* HMAC-SHA256 - RFC 4231, cas 1 */
    u8 key[20];
    mk_memset(key, 0x0b, sizeof(key));
    hmac_sha256(key, 20, (const u8*)"Hi There", 8, out);
    failures += check("HMAC-SHA256 (RFC 4231 cas 1)", out,
        "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");

    /* AES-128-GCM - NIST, cas 2 (bloc nul) */
    u8 k0[16], iv0[12], p0[16], c0[16], t0[16];
    mk_memset(k0, 0, 16);
    mk_memset(iv0, 0, 12);
    mk_memset(p0, 0, 16);
    aes128_gcm_encrypt(k0, iv0, (const u8*)0, 0, p0, 16, c0, t0);
    failures += check("AES-128-GCM chiffrement (NIST cas 2)", c0, "0388dace60b6a392f328c2b971b2fe78");
    failures += check("AES-128-GCM etiquette (NIST cas 2)", t0, "ab6e47d42cec13bdf53a67b21257bddf");

    /* AES-128-GCM - NIST, cas 4 (avec donnees authentifiees, longueur non multiple de 16) */
    u8 k4[16], iv4[12], aad4[20], p4[60], c4[60], t4[16];
    from_hex("feffe9928665731c6d6a8f9467308308", k4);
    from_hex("cafebabefacedbaddecaf888", iv4);
    from_hex("feedfacedeadbeeffeedfacedeadbeefabaddad2", aad4);
    from_hex("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39", p4);
    aes128_gcm_encrypt(k4, iv4, aad4, 20, p4, 60, c4, t4);
    failures += check("AES-128-GCM chiffrement (NIST cas 4)", c4,
        "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091");
    failures += check("AES-128-GCM etiquette (NIST cas 4)", t4, "5bc94fbc3221a5db94fae95ae7121a47");

    /* Dechiffrement + detection de falsification */
    u8 back[60];
    int good = aes128_gcm_decrypt(k4, iv4, aad4, 20, c4, 60, t4, back);
    int round_trip = good && crypto_equal(back, p4, 60);
    console_write(round_trip ? "  [OK]    " : "  [ECHEC] ");
    console_write("AES-128-GCM dechiffrement\n");
    failures += round_trip ? 0 : 1;

    c4[5] ^= 1;
    int rejected = !aes128_gcm_decrypt(k4, iv4, aad4, 20, c4, 60, t4, back);
    console_write(rejected ? "  [OK]    " : "  [ECHEC] ");
    console_write("message falsifie rejete\n");
    failures += rejected ? 0 : 1;

    /* X25519 - RFC 7748, section 5.2 */
    u8 scalar[32], point[32];
    from_hex("a546e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449ac4", scalar);
    from_hex("e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c", point);
    x25519(out, scalar, point);
    failures += check("X25519 (RFC 7748 5.2)", out,
        "c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552");

    /* X25519 - echange de Diffie-Hellman, RFC 7748, section 6.1 */
    u8 alice_priv[32], bob_priv[32], alice_pub[32], bob_pub[32], shared_a[32], shared_b[32];
    from_hex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a", alice_priv);
    from_hex("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb", bob_priv);
    x25519_base(alice_pub, alice_priv);
    x25519_base(bob_pub, bob_priv);
    failures += check("X25519 cle publique d'Alice (RFC 7748 6.1)", alice_pub,
        "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a");
    x25519(shared_a, alice_priv, bob_pub);
    x25519(shared_b, bob_priv, alice_pub);
    failures += check("X25519 secret partage (RFC 7748 6.1)", shared_a,
        "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");
    failures += crypto_equal(shared_a, shared_b, 32) ? 0 : 1;

    console_write(failures == 0 ? "Tous les tests sont reussis.\n" : "ATTENTION : des tests ont echoue !\n");
    return failures;
}
