#ifndef MIKEA_TLS_H
#define MIKEA_TLS_H

#include "../include/types.h"

/*
    Client TLS 1.3 de MikeaOS.

    Suite unique : TLS_AES_128_GCM_SHA256, echange X25519, signature serveur
    rsa_pss_rsae_sha256 (RSA 1024-4096 bits) ou ecdsa_secp256r1_sha256 (ECDSA P-256).

    IDENTITE DU SERVEUR : il n'y a pas (encore) de magasin d'autorites de
    certification. L'identite repose donc sur l'EMPREINTE de la cle publique
    du serveur (SHA-256 du SubjectPublicKeyInfo), a la maniere de SSH :
      - avec une empreinte attendue ("pin") : la connexion est REFUSEE si elle
        differe -> protege contre un attaquant actif ;
      - sans empreinte : premiere confiance (TOFU) -- l'appelant doit avertir
        l'utilisateur que l'identite n'est pas verifiee.
    La signature CertificateVerify et le message Finished sont TOUJOURS
    verifies (preuve que le serveur possede la cle et que l'echange n'a pas
    ete altere).

    Non pris en charge : autres courbes (P-384, Ed25519), verification de chaine/expiration/
    nom d'hote, reprise de session, authentification du client.
*/

typedef struct
{
    /* Envoi : renvoie 0 si ok. */
    int (*send)(const u8* data, u32 length);
    /* Reception : >0 octets, 0 fin de flux, -1 delai depasse. */
    int (*recv)(u8* buffer, u32 max, u32 timeout_ticks);
} tls_transport_t;

#define TLS_OK            0
#define TLS_ERR_TRANSPORT -1
#define TLS_ERR_PROTOCOL  -2
#define TLS_ERR_CERT      -3
#define TLS_ERR_SIGNATURE -4
#define TLS_ERR_PIN       -5
#define TLS_ERR_FINISHED  -6
#define TLS_ERR_UNSUPPORTED -7

/*
    Poignee de main. expected_pin : 32 octets ou 0. pin_out (32 octets) recoit
    l'empreinte de la cle du serveur. Renvoie TLS_OK ou un code d'erreur.
*/
int tls_connect(const tls_transport_t* transport, const char* host,
                const u8* expected_pin, u8* pin_out, int* entropy_is_strong);

int tls_write(const u8* data, u32 length);

/* >0 octets, 0 = fin (close_notify), -1 = delai depasse, -2 = erreur. */
int tls_read(u8* buffer, u32 max, u32 timeout_ticks);

void tls_close();

const char* tls_error_text(int code);

#endif
