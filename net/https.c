/*
    HTTPS minimal par-dessus tcp.c + tls.c.

    Modele de confiance (voir tls.h) : empreinte de la cle publique du serveur.
      https <hote> [/chemin] pin=<64 hex>   -> identite VERIFIEE (refus si differente)
      https <hote> [/chemin]                -> premiere confiance, empreinte memorisee
                                               pour la session (refus si elle change)
*/

#include "net.h"
#include "tls.h"
#include "../libc/string.h"

void console_write(const char* text);

static int tcp_send_adapter(const u8* data, u32 length)
{
    return tcp_write(data, length);
}

static int tcp_recv_adapter(u8* buffer, u32 max, u32 timeout)
{
    return tcp_read(buffer, max, timeout);
}

/* Empreintes deja vues pendant cette session. */
#define KNOWN_MAX 8

static struct
{
    int used;
    char host[64];
    u8 pin[32];
} known[KNOWN_MAX];

static int known_find(const char* host)
{
    for (int i = 0; i < KNOWN_MAX; i++)
    {
        if (known[i].used && mk_strcmp(known[i].host, host) == 0)
        {
            return i;
        }
    }
    return -1;
}

static void known_store(const char* host, const u8* pin)
{
    int slot = -1;

    for (int i = 0; i < KNOWN_MAX; i++)
    {
        if (!known[i].used)
        {
            slot = i;
            break;
        }
    }

    if (slot < 0)
    {
        slot = 0;
    }

    known[slot].used = 1;
    mk_strcpy(known[slot].host, host, sizeof(known[slot].host));
    mk_memcpy(known[slot].pin, pin, 32);
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    return -1;
}

static int parse_pin(const char* text, u8 out[32])
{
    for (int i = 0; i < 32; i++)
    {
        int hi = hex_digit(text[i * 2]);
        int lo = (hi < 0) ? -1 : hex_digit(text[i * 2 + 1]);

        if (hi < 0 || lo < 0)
        {
            return 0;
        }
        out[i] = (u8)((hi << 4) | lo);
    }
    return text[64] == 0 || text[64] == ' ';
}

static void print_pin(const u8* pin)
{
    static const char digits[] = "0123456789abcdef";
    char line[33];

    for (int half = 0; half < 2; half++)
    {
        for (int i = 0; i < 16; i++)
        {
            u8 b = pin[half * 16 + i];
            line[i * 2] = digits[b >> 4];
            line[i * 2 + 1] = digits[b & 0xF];
        }
        line[32] = 0;
        console_write(half == 0 ? "    " : "");
        console_write(line);
    }
    console_write("\n");
}

int https_get(const char* host, const char* path, const char* pin_text)
{
    u8 ip[4];
    u8 explicit_pin[32];
    u8 seen_pin[32];
    int have_explicit = 0;

    if (pin_text != (const char*)0 && pin_text[0] != 0)
    {
        if (!parse_pin(pin_text, explicit_pin))
        {
            console_write("Empreinte invalide : 64 chiffres hexadecimaux attendus (pin=...).\n");
            return -1;
        }
        have_explicit = 1;
    }

    console_write("Resolution de ");
    console_write(host);
    console_write(" ...\n");

    if (dns_resolve(host, ip) != 0)
    {
        console_write("Echec : nom introuvable (DNS).\n");
        return -1;
    }

    console_write("Connexion TCP (port 443) ...\n");

    if (tcp_connect(ip, 443) != 0)
    {
        return -2;
    }

    int known_index = known_find(host);
    const u8* expected = (const u8*)0;

    if (have_explicit)
    {
        expected = explicit_pin;
    }
    else if (known_index >= 0)
    {
        expected = known[known_index].pin;
    }

    console_write("Poignee de main TLS 1.3 (calcul de signature, quelques secondes) ...\n");

    tls_transport_t transport = { tcp_send_adapter, tcp_recv_adapter };
    int strong = 0;
    int result = tls_connect(&transport, host, expected, seen_pin, &strong);

    if (result != TLS_OK)
    {
        console_write("TLS REFUSE : ");
        console_write(tls_error_text(result));
        console_write("\n");

        if (result == TLS_ERR_PIN)
        {
            console_write("  Empreinte recue :\n");
            print_pin(seen_pin);
        }

        tcp_close();
        return -3;
    }

    console_write("Connexion chiffree etablie (TLS 1.3, AES-128-GCM, X25519).\n");
    console_write("  Empreinte de la cle du serveur (SHA-256) :\n");
    print_pin(seen_pin);

    if (have_explicit)
    {
        console_write("  Identite VERIFIEE : correspond a l'empreinte fournie.\n");
    }
    else if (known_index >= 0)
    {
        console_write("  Identique a la connexion precedente de cette session.\n");
    }
    else
    {
        console_write("  ATTENTION : premiere connexion, identite NON verifiee (pas de magasin d'autorites).\n");
        console_write("  Pour securiser : https <hote> pin=<empreinte ci-dessus, obtenue par un canal sur>.\n");
        known_store(host, seen_pin);
    }

    if (!strong)
    {
        console_write("  Avertissement : pas de RDRAND, aleatoire de secours (QEMU : option -cpu max).\n");
    }

    static char request[400];
    u32 n = 0;
    const char* parts[] = { "GET ", path, " HTTP/1.1\r\nHost: ", host,
                            "\r\nUser-Agent: MikeaOS\r\nAccept: text/html,*/*\r\nConnection: close\r\n\r\n" };

    for (u32 p = 0; p < sizeof(parts) / sizeof(parts[0]); p++)
    {
        for (u32 i = 0; parts[p][i] != 0 && n < sizeof(request) - 1; i++)
        {
            request[n++] = parts[p][i];
        }
    }
    request[n] = 0;

    if (tls_write((const u8*)request, n) != 0)
    {
        console_write("Echec de l'envoi de la requete.\n");
        tls_close();
        tcp_close();
        return -4;
    }

    console_write("---- reponse ----\n");

    u32 printed = 0;
    u8 chunk[200];

    while (printed < 3000)
    {
        int got = tls_read(chunk, sizeof(chunk) - 1, 600);

        if (got <= 0)
        {
            break;
        }

        char text[201];
        int t = 0;

        for (int i = 0; i < got; i++)
        {
            u8 c = chunk[i];

            if (c == '\n' || (c >= 32 && c <= 126))
            {
                text[t++] = (char)c;
            }
            else if (c != '\r')
            {
                text[t++] = '.';
            }
        }
        text[t] = 0;

        console_write(text);
        printed += (u32)got;
    }

    console_write("\n---- fin ----\n");

    tls_close();
    tcp_close();
    return 0;
}
