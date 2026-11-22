#include "tls.h"
#include "../crypto/crypto.h"
#include "../libc/string.h"

#define REC_MAX 16640 /* 16384 + marge pour l'en-tete, l'etiquette et le type interne */

typedef struct
{
    u8 key[16];
    u8 iv[12];
    u64 seq;
} traffic_t;

static const tls_transport_t* transport = (const tls_transport_t*)0;
static traffic_t rx_traffic, tx_traffic;
static int rx_encrypted = 0;
static int tx_encrypted = 0;
static int connected = 0;
static int stream_ended = 0;

static u8 rec_buffer[REC_MAX + 5];
static u8 rec_plain[REC_MAX];
static u8 send_buffer[REC_MAX + 5];
static u8 hs_buffer[24576];
static u32 hs_length = 0;

static u8 app_buffer[REC_MAX];
static u32 app_length = 0;
static u32 app_position = 0;

static sha256_ctx transcript;

/* ------------------------------------------------------------
   HKDF / calendrier de cles TLS 1.3 (RFC 8446 section 7.1)
   ------------------------------------------------------------ */

static void hkdf_extract(const u8* salt, u32 salt_length, const u8* ikm, u32 ikm_length, u8 out[32])
{
    static const u8 zero = 0;
    hmac_sha256(salt_length ? salt : &zero, salt_length, ikm, ikm_length, out);
}

static void expand_label(const u8 secret[32], const char* label, const u8* context, u32 context_length,
                         u8* out, u32 out_length)
{
    u8 info[2 + 1 + 6 + 12 + 1 + 32 + 1];
    u32 n = 0;
    u32 label_length = mk_strlen(label);

    info[n++] = (u8)(out_length >> 8);
    info[n++] = (u8)out_length;
    info[n++] = (u8)(6 + label_length);
    mk_memcpy(info + n, "tls13 ", 6);
    n += 6;
    mk_memcpy(info + n, label, label_length);
    n += label_length;
    info[n++] = (u8)context_length;
    mk_memcpy(info + n, context, context_length);
    n += context_length;
    info[n++] = 1; /* compteur HKDF-Expand : un seul bloc (sortie <= 32 octets) */

    u8 block[32];
    hmac_sha256(secret, 32, info, n, block);
    mk_memcpy(out, block, out_length);
}

static void derive_secret(const u8 secret[32], const char* label, const u8 hash[32], u8 out[32])
{
    expand_label(secret, label, hash, 32, out, 32);
}

static void traffic_keys(const u8 secret[32], traffic_t* t)
{
    expand_label(secret, "key", (const u8*)"", 0, t->key, 16);
    expand_label(secret, "iv", (const u8*)"", 0, t->iv, 12);
    t->seq = 0;
}

static void transcript_hash(u8 out[32])
{
    sha256_ctx copy = transcript;
    sha256_final(&copy, out);
}

/* ------------------------------------------------------------
   Couche enregistrement
   ------------------------------------------------------------ */

static void make_nonce(const traffic_t* t, u8 nonce[12])
{
    mk_memcpy(nonce, t->iv, 12);

    for (int i = 0; i < 8; i++)
    {
        nonce[11 - i] ^= (u8)(t->seq >> (8 * i));
    }
}

static int read_exact(u8* buffer, u32 length, u32 timeout)
{
    u32 got = 0;

    while (got < length)
    {
        int r = transport->recv(buffer + got, length - got, timeout);

        if (r <= 0)
        {
            return -1;
        }
        got += (u32)r;
    }
    return 0;
}

/* Envoie un enregistrement ; chiffre si les cles d'emission sont actives. */
static int send_record(u8 type, const u8* data, u32 length)
{
    if (length > 16384)
    {
        return -1;
    }

    if (!tx_encrypted)
    {
        send_buffer[0] = type;
        send_buffer[1] = 0x03;
        send_buffer[2] = 0x01;
        send_buffer[3] = (u8)(length >> 8);
        send_buffer[4] = (u8)length;
        mk_memcpy(send_buffer + 5, data, length);
        return transport->send(send_buffer, 5 + length) == 0 ? 0 : -1;
    }

    /* Contenu interne = donnees || type reel. */
    static u8 inner[REC_MAX];
    mk_memcpy(inner, data, length);
    inner[length] = type;

    u32 cipher_length = length + 1 + 16;

    send_buffer[0] = 23;
    send_buffer[1] = 0x03;
    send_buffer[2] = 0x03;
    send_buffer[3] = (u8)(cipher_length >> 8);
    send_buffer[4] = (u8)cipher_length;

    u8 nonce[12];
    make_nonce(&tx_traffic, nonce);

    aes128_gcm_encrypt(tx_traffic.key, nonce, send_buffer, 5, inner, length + 1,
                       send_buffer + 5, send_buffer + 5 + length + 1);
    tx_traffic.seq++;

    return transport->send(send_buffer, 5 + cipher_length) == 0 ? 0 : -1;
}

/*
    Lit un enregistrement. Pour les enregistrements chiffres, dechiffre et
    renvoie le VRAI type et le contenu dans rec_plain. Renvoie TLS_OK ou une erreur.
*/
static int read_record(u8* type_out, u32* length_out, u32 timeout)
{
    u8 header[5];

    if (read_exact(header, 5, timeout) != 0)
    {
        return TLS_ERR_TRANSPORT;
    }

    u32 length = ((u32)header[3] << 8) | header[4];

    if (length > REC_MAX)
    {
        return TLS_ERR_PROTOCOL;
    }

    if (read_exact(rec_buffer, length, timeout) != 0)
    {
        return TLS_ERR_TRANSPORT;
    }

    if (header[0] == 20)
    {
        /* ChangeCipherSpec de compatibilite : ignore. */
        *type_out = 20;
        *length_out = 0;
        return TLS_OK;
    }

    if (!rx_encrypted || header[0] != 23)
    {
        mk_memcpy(rec_plain, rec_buffer, length);
        *type_out = header[0];
        *length_out = length;
        return TLS_OK;
    }

    if (length < 17)
    {
        return TLS_ERR_PROTOCOL;
    }

    u8 nonce[12];
    make_nonce(&rx_traffic, nonce);

    u32 plain_length = length - 16;

    if (!aes128_gcm_decrypt(rx_traffic.key, nonce, header, 5, rec_buffer, plain_length,
                            rec_buffer + plain_length, rec_plain))
    {
        return TLS_ERR_PROTOCOL; /* etiquette invalide : message falsifie */
    }

    rx_traffic.seq++;

    /* Retire le remplissage : le dernier octet non nul est le type reel. */
    while (plain_length > 0 && rec_plain[plain_length - 1] == 0)
    {
        plain_length--;
    }

    if (plain_length == 0)
    {
        return TLS_ERR_PROTOCOL;
    }

    *type_out = rec_plain[plain_length - 1];
    *length_out = plain_length - 1;
    return TLS_OK;
}

/* ------------------------------------------------------------
   Messages de poignee de main
   ------------------------------------------------------------ */

static int send_handshake_plain(const u8* message, u32 length)
{
    sha256_update(&transcript, message, length);
    return send_record(22, message, length);
}

/* Extrait le prochain message complet de hs_buffer. 1 si disponible. */
static int take_handshake(u8* type, const u8** body, u32* body_length, u32* total, u32 timeout, int* error)
{
    while (1)
    {
        if (hs_length >= 4)
        {
            u32 blen = ((u32)hs_buffer[1] << 16) | ((u32)hs_buffer[2] << 8) | hs_buffer[3];

            if (blen + 4 > sizeof(hs_buffer))
            {
                *error = TLS_ERR_PROTOCOL;
                return 0;
            }

            if (hs_length >= blen + 4)
            {
                *type = hs_buffer[0];
                *body = hs_buffer + 4;
                *body_length = blen;
                *total = blen + 4;
                return 1;
            }
        }

        u8 rtype;
        u32 rlen;
        int r = read_record(&rtype, &rlen, timeout);

        if (r != TLS_OK)
        {
            *error = r;
            return 0;
        }

        if (rtype == 20)
        {
            continue;
        }

        if (rtype == 21)
        {
            /* Alerte du serveur : handshake_failure (40), protocol_version (70), insufficient_security (71)
               signifient en pratique "je n'accepte pas vos algorithmes". */
            u8 description = (rlen >= 2) ? rec_plain[1] : 0;
            *error = (description == 40 || description == 70 || description == 71 || description == 120)
                   ? TLS_ERR_UNSUPPORTED : TLS_ERR_PROTOCOL;
            return 0;
        }

        if (rtype != 22)
        {
            *error = TLS_ERR_PROTOCOL;
            return 0;
        }

        if (hs_length + rlen > sizeof(hs_buffer))
        {
            *error = TLS_ERR_PROTOCOL;
            return 0;
        }

        mk_memcpy(hs_buffer + hs_length, rec_plain, rlen);
        hs_length += rlen;
    }
}

static void consume_handshake(u32 total)
{
    for (u32 i = 0; i + total < hs_length; i++)
    {
        hs_buffer[i] = hs_buffer[i + total];
    }
    hs_length -= total;
}

/* ------------------------------------------------------------
   Lecture DER minimale (certificat X.509)
   ------------------------------------------------------------ */

/* Lit un element TLV. Renvoie 1 si ok. */
static int der_next(const u8* data, u32 length, u8* tag, const u8** content, u32* content_length, u32* total)
{
    if (length < 2)
    {
        return 0;
    }

    *tag = data[0];
    u32 pos = 2;
    u32 len = data[1];

    if (len & 0x80)
    {
        u32 count = len & 0x7F;

        if (count == 0 || count > 3 || pos + count > length)
        {
            return 0;
        }

        len = 0;
        for (u32 i = 0; i < count; i++)
        {
            len = (len << 8) | data[pos++];
        }
    }

    if (pos + len > length)
    {
        return 0;
    }

    *content = data + pos;
    *content_length = len;
    *total = pos + len;
    return 1;
}

/*
    Extrait du certificat : l'element SubjectPublicKeyInfo complet (pour l'empreinte)
    et le module/exposant RSA.
*/
#define KEY_RSA 1
#define KEY_EC_P256 2

static int parse_certificate(const u8* cert, u32 cert_length,
                             const u8** spki, u32* spki_length, int* key_type,
                             const u8** modulus, u32* modulus_length,
                             const u8** exponent, u32* exponent_length,
                             const u8** ec_point)
{
    u8 tag;
    const u8* content;
    u32 clen, total;

    if (!der_next(cert, cert_length, &tag, &content, &clen, &total) || tag != 0x30)
    {
        return 0;
    }

    /* tbsCertificate */
    const u8* tbs;
    u32 tbs_length;

    if (!der_next(content, clen, &tag, &tbs, &tbs_length, &total) || tag != 0x30)
    {
        return 0;
    }

    u32 pos = 0;
    int index = 0;

    /* Version optionnelle [0]. */
    if (tbs_length > 0 && tbs[0] == 0xA0)
    {
        if (!der_next(tbs, tbs_length, &tag, &content, &clen, &total))
        {
            return 0;
        }
        pos += total;
    }

    /* serial, signature, issuer, validity, subject : on saute 5 elements. */
    while (index < 5)
    {
        if (!der_next(tbs + pos, tbs_length - pos, &tag, &content, &clen, &total))
        {
            return 0;
        }
        pos += total;
        index++;
    }

    /* subjectPublicKeyInfo */
    if (!der_next(tbs + pos, tbs_length - pos, &tag, &content, &clen, &total) || tag != 0x30)
    {
        return 0;
    }

    *spki = tbs + pos;
    *spki_length = total;

    const u8* spki_content = content;
    u32 spki_content_length = clen;

    /* AlgorithmIdentifier : doit etre rsaEncryption (1.2.840.113549.1.1.1). */
    const u8* algo;
    u32 algo_length;

    if (!der_next(spki_content, spki_content_length, &tag, &algo, &algo_length, &total) || tag != 0x30)
    {
        return 0;
    }

    static const u8 rsa_oid[] = { 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01 };
    static const u8 ec_oid[] = { 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
                                 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07 };

    if (algo_length >= sizeof(rsa_oid) && mk_memcmp(algo, rsa_oid, sizeof(rsa_oid)) == 0)
    {
        *key_type = KEY_RSA;
    }
    else if (algo_length >= sizeof(ec_oid) && mk_memcmp(algo, ec_oid, sizeof(ec_oid)) == 0)
    {
        *key_type = KEY_EC_P256; /* id-ecPublicKey + prime256v1 */
    }
    else
    {
        return 0; /* autre algorithme de cle (ex. Ed25519, P-384) */
    }

    u32 after_algo = total;

    /* BIT STRING contenant SEQUENCE { INTEGER n, INTEGER e }. */
    const u8* bits;
    u32 bits_length;

    if (!der_next(spki_content + after_algo, spki_content_length - after_algo, &tag, &bits, &bits_length, &total)
        || tag != 0x03 || bits_length < 1 || bits[0] != 0)
    {
        return 0;
    }

    if (*key_type == KEY_EC_P256)
    {
        if (bits_length != 66 || bits[1] != 0x04)
        {
            return 0;
        }
        *ec_point = bits + 1;
        return 1;
    }

    const u8* seq;
    u32 seq_length;

    if (!der_next(bits + 1, bits_length - 1, &tag, &seq, &seq_length, &total) || tag != 0x30)
    {
        return 0;
    }

    const u8* n;
    u32 n_length;

    if (!der_next(seq, seq_length, &tag, &n, &n_length, &total) || tag != 0x02)
    {
        return 0;
    }

    u32 after_n = total;
    const u8* e;
    u32 e_length;

    if (!der_next(seq + after_n, seq_length - after_n, &tag, &e, &e_length, &total) || tag != 0x02)
    {
        return 0;
    }

    *modulus = n;
    *modulus_length = n_length;
    *exponent = e;
    *exponent_length = e_length;
    return 1;
}

/* ------------------------------------------------------------
   Poignee de main
   ------------------------------------------------------------ */

static u32 put16(u8* p, u16 v)
{
    p[0] = (u8)(v >> 8);
    p[1] = (u8)v;
    return 2;
}

int tls_connect(const tls_transport_t* t, const char* host,
                const u8* expected_pin, u8* pin_out, int* entropy_is_strong)
{
    transport = t;
    connected = 0;
    stream_ended = 0;
    rx_encrypted = 0;
    tx_encrypted = 0;
    hs_length = 0;
    app_length = 0;
    app_position = 0;
    sha256_init(&transcript);

    u32 host_length = mk_strlen(host);

    if (host_length == 0 || host_length > 200)
    {
        return TLS_ERR_UNSUPPORTED;
    }

    /* --- Cle ephemere X25519 et nombres aleatoires --- */
    u8 client_private[32], client_public[32], client_random[32], session_id[32];

    int strong = random_bytes(client_private, 32);
    random_bytes(client_random, 32);
    random_bytes(session_id, 32);
    x25519_base(client_public, client_private);

    if (entropy_is_strong != (int*)0)
    {
        *entropy_is_strong = strong;
    }

    /* --- ClientHello --- */
    static u8 hello[600];
    u32 n = 0;

    hello[n++] = 1; /* ClientHello */
    n += 3;         /* longueur : remplie a la fin */

    n += put16(hello + n, 0x0303);
    mk_memcpy(hello + n, client_random, 32); n += 32;
    hello[n++] = 32;
    mk_memcpy(hello + n, session_id, 32); n += 32;
    n += put16(hello + n, 2);
    n += put16(hello + n, 0x1301);
    hello[n++] = 1;
    hello[n++] = 0;

    u32 ext_length_pos = n;
    n += 2;
    u32 ext_start = n;

    /* server_name */
    n += put16(hello + n, 0x0000);
    n += put16(hello + n, (u16)(host_length + 5));
    n += put16(hello + n, (u16)(host_length + 3));
    hello[n++] = 0;
    n += put16(hello + n, (u16)host_length);
    mk_memcpy(hello + n, host, host_length); n += host_length;

    /* supported_groups : x25519 */
    n += put16(hello + n, 0x000A);
    n += put16(hello + n, 4);
    n += put16(hello + n, 2);
    n += put16(hello + n, 0x001D);

    /* signature_algorithms : rsa_pss_rsae_sha256, ecdsa_secp256r1_sha256 */
    n += put16(hello + n, 0x000D);
    n += put16(hello + n, 6);
    n += put16(hello + n, 4);
    n += put16(hello + n, 0x0804);
    n += put16(hello + n, 0x0403);

    /* supported_versions : TLS 1.3 */
    n += put16(hello + n, 0x002B);
    n += put16(hello + n, 3);
    hello[n++] = 2;
    n += put16(hello + n, 0x0304);

    /* key_share : x25519 */
    n += put16(hello + n, 0x0033);
    n += put16(hello + n, 38);
    n += put16(hello + n, 36);
    n += put16(hello + n, 0x001D);
    n += put16(hello + n, 32);
    mk_memcpy(hello + n, client_public, 32); n += 32;

    put16(hello + ext_length_pos, (u16)(n - ext_start));

    u32 body = n - 4;
    hello[1] = (u8)(body >> 16);
    hello[2] = (u8)(body >> 8);
    hello[3] = (u8)body;

    if (send_handshake_plain(hello, n) != 0)
    {
        return TLS_ERR_TRANSPORT;
    }

    /* --- ServerHello --- */
    int error = TLS_OK;
    u8 type;
    const u8* msg;
    u32 msg_length, total;

    if (!take_handshake(&type, &msg, &msg_length, &total, 1500, &error))
    {
        return error ? error : TLS_ERR_PROTOCOL;
    }

    if (type != 2 || msg_length < 38)
    {
        return TLS_ERR_PROTOCOL;
    }

    sha256_update(&transcript, hs_buffer, total);

    {
        static const u8 hello_retry[32] = {
            0xCF, 0x21, 0xAD, 0x74, 0xE5, 0x9A, 0x61, 0x11, 0xBE, 0x1D, 0x8C, 0x02, 0x1E, 0x65, 0xB8, 0x91,
            0xC2, 0xA2, 0x11, 0x16, 0x7A, 0xBB, 0x8C, 0x5E, 0x07, 0x9E, 0x09, 0xE2, 0xC8, 0xA8, 0x33, 0x9C
        };

        if (mk_memcmp(msg + 2, hello_retry, 32) == 0)
        {
            return TLS_ERR_UNSUPPORTED; /* HelloRetryRequest non gere */
        }
    }

    u32 p = 2 + 32;
    u32 sid_length = msg[p++];
    p += sid_length;

    if (p + 3 + 2 > msg_length)
    {
        return TLS_ERR_PROTOCOL;
    }

    u16 suite = (u16)((msg[p] << 8) | msg[p + 1]);
    p += 2;
    p += 1; /* compression */

    if (suite != 0x1301)
    {
        return TLS_ERR_UNSUPPORTED;
    }

    u32 ext_total = ((u32)msg[p] << 8) | msg[p + 1];
    p += 2;

    if (p + ext_total > msg_length)
    {
        return TLS_ERR_PROTOCOL;
    }

    const u8* server_key = (const u8*)0;
    int version_ok = 0;
    u32 end = p + ext_total;

    while (p + 4 <= end)
    {
        u16 etype = (u16)((msg[p] << 8) | msg[p + 1]);
        u32 elen = ((u32)msg[p + 2] << 8) | msg[p + 3];
        p += 4;

        if (p + elen > end)
        {
            return TLS_ERR_PROTOCOL;
        }

        if (etype == 43 && elen == 2 && msg[p] == 0x03 && msg[p + 1] == 0x04)
        {
            version_ok = 1;
        }
        else if (etype == 51 && elen == 36 && msg[p] == 0x00 && msg[p + 1] == 0x1D
                 && msg[p + 2] == 0x00 && msg[p + 3] == 0x20)
        {
            server_key = msg + p + 4;
        }

        p += elen;
    }

    if (!version_ok || server_key == (const u8*)0)
    {
        return TLS_ERR_UNSUPPORTED;
    }

    u8 shared[32];
    x25519(shared, client_private, server_key);

    {
        u8 any = 0;
        for (int i = 0; i < 32; i++) { any |= shared[i]; }

        if (any == 0)
        {
            return TLS_ERR_PROTOCOL; /* point d'ordre faible : echange refuse */
        }
    }

    mk_memset(client_private, 0, 32);
    consume_handshake(total);

    /* --- Calendrier de cles --- */
    u8 zeros[32], empty_hash[32], early[32], derived[32], hs_secret[32];
    u8 th[32], c_hs[32], s_hs[32];

    mk_memset(zeros, 0, 32);
    sha256((const u8*)"", 0, empty_hash);

    hkdf_extract((const u8*)0, 0, zeros, 32, early);
    derive_secret(early, "derived", empty_hash, derived);
    hkdf_extract(derived, 32, shared, 32, hs_secret);
    mk_memset(shared, 0, 32);

    transcript_hash(th);
    derive_secret(hs_secret, "c hs traffic", th, c_hs);
    derive_secret(hs_secret, "s hs traffic", th, s_hs);

    traffic_keys(s_hs, &rx_traffic);
    rx_encrypted = 1;

    /* --- EncryptedExtensions, Certificate, CertificateVerify, Finished --- */
    u8 th_cert[32], th_cv[32];
    const u8* leaf = (const u8*)0;
    u32 leaf_length = 0;

    if (!take_handshake(&type, &msg, &msg_length, &total, 1500, &error))
    {
        return error ? error : TLS_ERR_PROTOCOL;
    }
    if (type != 8)
    {
        return TLS_ERR_PROTOCOL;
    }
    sha256_update(&transcript, hs_buffer, total);
    consume_handshake(total);

    if (!take_handshake(&type, &msg, &msg_length, &total, 1500, &error))
    {
        return error ? error : TLS_ERR_PROTOCOL;
    }
    if (type == 13)
    {
        return TLS_ERR_UNSUPPORTED; /* le serveur exige un certificat client */
    }
    if (type != 11)
    {
        return TLS_ERR_PROTOCOL;
    }

    /* Certificat : on garde une copie du premier (le tampon va etre decale). */
    static u8 leaf_copy[8192];
    {
        u32 q = 0;

        if (msg_length < 1 + 3 || msg[0] != 0)
        {
            return TLS_ERR_PROTOCOL;
        }
        q = 1;
        u32 list_length = ((u32)msg[q] << 16) | ((u32)msg[q + 1] << 8) | msg[q + 2];
        q += 3;

        if (q + list_length > msg_length || list_length < 3)
        {
            return TLS_ERR_PROTOCOL;
        }

        leaf_length = ((u32)msg[q] << 16) | ((u32)msg[q + 1] << 8) | msg[q + 2];
        q += 3;

        if (leaf_length == 0 || leaf_length > sizeof(leaf_copy) || q + leaf_length > msg_length)
        {
            return TLS_ERR_CERT;
        }

        mk_memcpy(leaf_copy, msg + q, leaf_length);
        leaf = leaf_copy;
    }

    sha256_update(&transcript, hs_buffer, total);
    consume_handshake(total);
    transcript_hash(th_cert);

    const u8 *spki, *modulus = (const u8*)0, *exponent = (const u8*)0, *ec_point = (const u8*)0;
    u32 spki_length, modulus_length = 0, exponent_length = 0;
    int key_type = 0;

    if (!parse_certificate(leaf, leaf_length, &spki, &spki_length, &key_type, &modulus, &modulus_length,
                           &exponent, &exponent_length, &ec_point))
    {
        return TLS_ERR_CERT; /* certificat illisible ou type de cle non pris en charge */
    }

    u8 pin[32];
    sha256(spki, spki_length, pin);

    if (pin_out != (u8*)0)
    {
        mk_memcpy(pin_out, pin, 32);
    }

    if (expected_pin != (const u8*)0 && !crypto_equal(pin, expected_pin, 32))
    {
        return TLS_ERR_PIN;
    }

    if (!take_handshake(&type, &msg, &msg_length, &total, 1500, &error))
    {
        return error ? error : TLS_ERR_PROTOCOL;
    }
    if (type != 15 || msg_length < 4)
    {
        return TLS_ERR_PROTOCOL;
    }

    {
        u16 scheme = (u16)((msg[0] << 8) | msg[1]);
        u32 sig_length = ((u32)msg[2] << 8) | msg[3];

        if (4 + sig_length != msg_length
            || (key_type == KEY_RSA && scheme != 0x0804)
            || (key_type == KEY_EC_P256 && scheme != 0x0403))
        {
            return TLS_ERR_UNSUPPORTED; /* schema de signature incoherent avec la cle du certificat */
        }

        u8 signed_content[64 + 33 + 1 + 32];
        mk_memset(signed_content, 0x20, 64);
        mk_memcpy(signed_content + 64, "TLS 1.3, server CertificateVerify", 33);
        signed_content[97] = 0;
        mk_memcpy(signed_content + 98, th_cert, 32);

        int valid = 0;

        if (key_type == KEY_RSA)
        {
            valid = rsa_pss_sha256_verify(modulus, modulus_length, exponent, exponent_length,
                                          signed_content, sizeof(signed_content), msg + 4, sig_length);
        }
        else
        {
            /* Signature ECDSA au format DER : SEQUENCE { INTEGER r, INTEGER s }. */
            u8 tag;
            const u8 *seq, *rv, *sv;
            u32 seq_len, rlen, slen, tot;
            const u8* der = msg + 4;

            if (der_next(der, sig_length, &tag, &seq, &seq_len, &tot) && tag == 0x30 && tot == sig_length
                && der_next(seq, seq_len, &tag, &rv, &rlen, &tot) && tag == 0x02)
            {
                u32 first = tot;

                if (der_next(seq + first, seq_len - first, &tag, &sv, &slen, &tot) && tag == 0x02
                    && first + tot == seq_len)
                {
                    valid = ecdsa_p256_sha256_verify(ec_point, signed_content, sizeof(signed_content),
                                                     rv, rlen, sv, slen);
                }
            }
        }

        if (!valid)
        {
            return TLS_ERR_SIGNATURE;
        }
    }

    sha256_update(&transcript, hs_buffer, total);
    consume_handshake(total);
    transcript_hash(th_cv);

    if (!take_handshake(&type, &msg, &msg_length, &total, 1500, &error))
    {
        return error ? error : TLS_ERR_PROTOCOL;
    }
    if (type != 20 || msg_length != 32)
    {
        return TLS_ERR_PROTOCOL;
    }

    {
        u8 finished_key[32], expected[32];
        expand_label(s_hs, "finished", (const u8*)"", 0, finished_key, 32);
        hmac_sha256(finished_key, 32, th_cv, 32, expected);

        if (!crypto_equal(expected, msg, 32))
        {
            return TLS_ERR_FINISHED;
        }
    }

    sha256_update(&transcript, hs_buffer, total);
    consume_handshake(total);

    u8 th_sf[32];
    transcript_hash(th_sf);

    /* --- Cles applicatives --- */
    u8 derived2[32], master[32], c_ap[32], s_ap[32];

    derive_secret(hs_secret, "derived", empty_hash, derived2);
    hkdf_extract(derived2, 32, zeros, 32, master);
    derive_secret(master, "c ap traffic", th_sf, c_ap);
    derive_secret(master, "s ap traffic", th_sf, s_ap);

    /* --- Finished du client (chiffre avec les cles de poignee de main) --- */
    traffic_keys(c_hs, &tx_traffic);
    tx_encrypted = 1;

    {
        u8 finished_key[32], verify[32], message[4 + 32];

        expand_label(c_hs, "finished", (const u8*)"", 0, finished_key, 32);
        hmac_sha256(finished_key, 32, th_sf, 32, verify);

        message[0] = 20;
        message[1] = 0;
        message[2] = 0;
        message[3] = 32;
        mk_memcpy(message + 4, verify, 32);

        if (send_record(22, message, sizeof(message)) != 0)
        {
            return TLS_ERR_TRANSPORT;
        }
    }

    traffic_keys(c_ap, &tx_traffic);
    traffic_keys(s_ap, &rx_traffic);

    mk_memset(hs_secret, 0, 32);
    mk_memset(master, 0, 32);

    connected = 1;
    return TLS_OK;
}

int tls_write(const u8* data, u32 length)
{
    if (!connected)
    {
        return -1;
    }

    u32 offset = 0;

    while (offset < length)
    {
        u32 chunk = length - offset;

        if (chunk > 8192)
        {
            chunk = 8192;
        }

        if (send_record(23, data + offset, chunk) != 0)
        {
            return -1;
        }
        offset += chunk;
    }
    return 0;
}

int tls_read(u8* buffer, u32 max, u32 timeout_ticks)
{
    if (!connected)
    {
        return -2;
    }

    while (app_position >= app_length)
    {
        if (stream_ended)
        {
            return 0;
        }

        u8 type;
        u32 length;
        int r = read_record(&type, &length, timeout_ticks);

        if (r == TLS_ERR_TRANSPORT)
        {
            return -1;
        }
        if (r != TLS_OK)
        {
            return -2;
        }

        if (type == 23)
        {
            mk_memcpy(app_buffer, rec_plain, length);
            app_length = length;
            app_position = 0;
        }
        else if (type == 21)
        {
            /* Alerte : close_notify (0) = fin normale ; toute autre = erreur. */
            stream_ended = 1;
            if (length >= 2 && rec_plain[1] != 0)
            {
                return -2;
            }
            return 0;
        }
        /* type 22 (NewSessionTicket...) et 20 : ignores. */
    }

    u32 n = app_length - app_position;

    if (n > max)
    {
        n = max;
    }

    mk_memcpy(buffer, app_buffer + app_position, n);
    app_position += n;
    return (int)n;
}

void tls_close()
{
    if (connected)
    {
        static const u8 close_notify[2] = { 1, 0 };
        send_record(21, close_notify, 2);
    }

    connected = 0;
    mk_memset(&rx_traffic, 0, sizeof(rx_traffic));
    mk_memset(&tx_traffic, 0, sizeof(tx_traffic));
}

const char* tls_error_text(int code)
{
    switch (code)
    {
        case TLS_OK: return "ok";
        case TLS_ERR_TRANSPORT: return "erreur reseau ou delai depasse pendant la poignee de main";
        case TLS_ERR_PROTOCOL: return "reponse TLS invalide (ou message falsifie)";
        case TLS_ERR_CERT: return "certificat illisible ou type de cle non pris en charge (RSA et ECDSA P-256 seulement)";
        case TLS_ERR_SIGNATURE: return "SIGNATURE DU SERVEUR INVALIDE : connexion refusee";
        case TLS_ERR_PIN: return "EMPREINTE DE CLE DIFFERENTE de celle attendue : connexion refusee (possible attaque)";
        case TLS_ERR_FINISHED: return "verification Finished echouee : echange altere";
        case TLS_ERR_UNSUPPORTED: return "le serveur n'accepte pas nos algorithmes (TLS 1.3, AES-128-GCM, X25519, RSA-PSS ou ECDSA P-256 requis)";
        default: return "erreur TLS inconnue";
    }
}
