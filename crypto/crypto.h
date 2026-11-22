#ifndef MIKEA_CRYPTO_H
#define MIKEA_CRYPTO_H

#include "../include/types.h"

/*
    Primitives cryptographiques de MikeaOS (implementations autonomes,
    sans dependance), socle du futur TLS :

      - SHA-256 et HMAC-SHA256              (FIPS 180-4, RFC 2104)
      - AES-128-GCM chiffrement authentifie (NIST SP 800-38D)
      - X25519 echange de cles              (RFC 7748)

    Chaque primitive est verifiee par des vecteurs de test officiels
    (commande "selftest"). Les comparaisons de secrets se font en temps
    constant. Ce code est volontairement simple (lisible, auditable) et
    pas optimise pour la vitesse.
*/

void sha256(const u8* data, u32 length, u8 out[32]);

/* SHA-256 en flux (pour hacher une transcription progressivement : copier la struct = "photo" intermediaire). */
typedef struct
{
    u32 state[8];
    u8 buffer[64];
    u32 buffered;
    u64 total;
} sha256_ctx;

void sha256_init(sha256_ctx* ctx);
void sha256_update(sha256_ctx* ctx, const u8* data, u32 length);
void sha256_final(sha256_ctx* ctx, u8 out[32]);

void hmac_sha256(const u8* key, u32 key_length, const u8* data, u32 length, u8 out[32]);

/* Chiffre "length" octets. IV de 12 octets. tag = 16 octets. */
void aes128_gcm_encrypt(const u8 key[16], const u8 iv[12],
                        const u8* aad, u32 aad_length,
                        const u8* plaintext, u32 length,
                        u8* ciphertext, u8 tag[16]);

/* Dechiffre ; renvoie 1 si l'etiquette est valide, 0 sinon (alors rien n'est ecrit). */
int aes128_gcm_decrypt(const u8 key[16], const u8 iv[12],
                       const u8* aad, u32 aad_length,
                       const u8* ciphertext, u32 length,
                       const u8 tag[16], u8* plaintext);

void x25519(u8 out[32], const u8 scalar[32], const u8 point[32]);
void x25519_base(u8 out[32], const u8 scalar[32]);

/*
    Aleatoire cryptographique : RDRAND si le processeur l'offre (QEMU : "-cpu max"),
    sinon gigue de l'horloge materielle (TSC) -- plus faible : a n'utiliser qu'en secours.
    Renvoie 1 si une source materielle forte (RDRAND) a ete utilisee.
*/
int random_bytes(u8* out, u32 length);

/*
    Verifie une signature RSASSA-PSS (SHA-256, sel de 32 octets, MGF1-SHA-256), celle de
    TLS 1.3 "rsa_pss_rsae_sha256". n, e, signature : entiers grands-boutistes.
    Renvoie 1 si valide.
*/
int rsa_pss_sha256_verify(const u8* n, u32 n_length, const u8* e, u32 e_length,
                          const u8* message, u32 message_length,
                          const u8* signature, u32 signature_length);

/*
    Verifie une signature ECDSA P-256 / SHA-256 (TLS 1.3 "ecdsa_secp256r1_sha256").
    public_key : point non compresse 0x04 || X || Y (65 octets). r, s : entiers grands-boutistes.
    Renvoie 1 si valide.
*/
int ecdsa_p256_sha256_verify(const u8 public_key[65],
                             const u8* message, u32 message_length,
                             const u8* r, u32 r_length,
                             const u8* s, u32 s_length);

/* Comparaison en temps constant. 1 si egaux. */
int crypto_equal(const u8* a, const u8* b, u32 length);

/* Execute les tests officiels et les affiche. Renvoie le nombre d'echecs. */
int crypto_selftest();

#endif
