#include "net.h"
#include "../kernel/drivers/pci/pci.h"
#include "../kernel/ai/jarvis.h"
#include "../libc/string.h"
#include "../crypto/crypto.h"

void console_write(const char* text);

static void put_hex(u32 value, int digits)
{
    static const char hex[] = "0123456789ABCDEF";
    char out[9];
    int n = 0;

    for (int i = digits - 1; i >= 0; i--)
    {
        out[n++] = hex[(value >> (i * 4)) & 0xF];
    }
    out[n] = 0;
    console_write(out);
}

static void put_dec(u32 value)
{
    char tmp[12];
    char out[12];
    int n = 0;
    int i = 0;

    if (value == 0)
    {
        console_write("0");
        return;
    }

    while (value > 0 && n < 11)
    {
        tmp[n++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (n > 0)
    {
        out[i++] = tmp[--n];
    }
    out[i] = 0;
    console_write(out);
}

static void cmd_lspci()
{
    int count = pci_device_count();

    console_write("Peripheriques PCI detectes : ");
    put_dec((u32)count);
    console_write("\n");

    for (int i = 0; i < count; i++)
    {
        const pci_device_t* d = pci_device_at(i);

        console_write("  ");
        put_hex(d->bus, 2);
        console_write(":");
        put_hex(d->dev, 2);
        console_write(".");
        put_dec(d->fn);
        console_write("  ");
        put_hex(d->vendor, 4);
        console_write(":");
        put_hex(d->device, 4);
        console_write("  ");
        console_write(pci_class_name(d->class_code, d->subclass));
        console_write("\n");
    }
}

/*
    Etat honnete des radios : on dit clairement ce qui est
    detecte et ce qui manque, au lieu de pretendre.
*/
static void cmd_radio()
{
    int wifi = 0;
    int wireless_other = 0;
    int usb = 0;

    for (int i = 0; i < pci_device_count(); i++)
    {
        const pci_device_t* d = pci_device_at(i);

        if (d->class_code == 0x02 && d->subclass == 0x80)
        {
            wifi++;
        }
        if (d->class_code == 0x0D)
        {
            wireless_other++;
        }
        if (d->class_code == 0x0C && d->subclass == 0x03)
        {
            usb++;
        }
    }

    console_write("Radios et localisation\n");

    console_write("  Ethernet filaire : ");
    console_write(net_is_up() ? "pilote e1000 ACTIF\n" : "aucune carte prise en charge\n");

    console_write("  WiFi / WLAN      : ");
    if (wifi > 0)
    {
        console_write("carte detectee, PILOTE ABSENT (firmware + pile 802.11 requis)\n");
    }
    else
    {
        console_write("aucune carte PCI detectee (les cartes USB demandent une pile USB)\n");
    }

    console_write("  Bluetooth        : ");
    if (wireless_other > 0)
    {
        console_write("controleur detecte, PILOTE ABSENT\n");
    }
    else if (usb > 0)
    {
        console_write("controleur USB present ; pile USB + HCI Bluetooth non implementees\n");
    }
    else
    {
        console_write("non detecte\n");
    }

    console_write("  Localisation GPS : aucun recepteur GNSS ; position par IP impossible sans TCP/TLS\n");
}

/* Copie un mot (jusqu'a l'espace) dans out. Renvoie le reste de la ligne. */
static const char* take_word(const char* line, char* out, u32 max)
{
    u32 n = 0;

    while (*line != 0 && *line != ' ')
    {
        if (n < max - 1)
        {
            out[n++] = *line;
        }
        line++;
    }
    out[n] = 0;

    while (*line == ' ')
    {
        line++;
    }
    return line;
}

static void cmd_nslookup(const char* host)
{
    u8 ip[4];

    if (host[0] == 0)
    {
        console_write("Usage : nslookup <nom>   (ex. nslookup example.com)\n");
        return;
    }

    if (dns_resolve(host, ip) == 0)
    {
        console_write(host);
        console_write(" -> ");
        for (int i = 0; i < 4; i++)
        {
            put_dec(ip[i]);
            if (i < 3)
            {
                console_write(".");
            }
        }
        console_write("\n");
    }
    else
    {
        console_write("Resolution DNS echouee.\n");
    }
}

static void cmd_http(const char* args)
{
    char host[96];
    char path[160];

    const char* rest = take_word(args, host, sizeof(host));

    if (host[0] == 0)
    {
        console_write("Usage : http <hote> [/chemin]   (ex. http example.com)\n");
        console_write("HTTP uniquement (pas de HTTPS/TLS).\n");
        return;
    }

    take_word(rest, path, sizeof(path));

    if (path[0] == 0)
    {
        path[0] = '/';
        path[1] = 0;
    }

    http_get(host, path);
}

static void cmd_https(const char* args)
{
    char host[96];
    char word[160];
    char path[160];
    char pin[80];

    path[0] = '/';
    path[1] = 0;
    pin[0] = 0;

    const char* rest = take_word(args, host, sizeof(host));

    if (host[0] == 0)
    {
        console_write("Usage : https <hote> [/chemin] [pin=<empreinte SHA-256 hex>]\n");
        console_write("TLS 1.3 ; identite par empreinte de cle (pas de magasin d'autorites).\n");
        return;
    }

    while (*rest != 0)
    {
        rest = take_word(rest, word, sizeof(word));

        if (mk_strncmp(word, "pin=", 4) == 0)
        {
            mk_strcpy(pin, word + 4, sizeof(pin));
        }
        else if (word[0] == '/')
        {
            mk_strcpy(path, word, sizeof(path));
        }
    }

    https_get(host, path, pin);
}

static int starts_with(const char* line, const char* word)
{
    u32 n = mk_strlen(word);

    if (mk_strncmp(line, word, n) != 0)
    {
        return 0;
    }
    return line[n] == 0 || line[n] == ' ';
}

static const char* skip_word(const char* line)
{
    while (*line != 0 && *line != ' ')
    {
        line++;
    }
    while (*line == ' ')
    {
        line++;
    }
    return line;
}

int mikea_extra_command(char* line)
{
    if (starts_with(line, "lspci") || starts_with(line, "hwscan"))
    {
        cmd_lspci();
        return 1;
    }

    if (starts_with(line, "netinfo") || starts_with(line, "ifconfig") || starts_with(line, "ip"))
    {
        net_poll();
        net_print_status();
        return 1;
    }

    if (starts_with(line, "ping"))
    {
        const char* arg = skip_word(line);
        u8 ip[4];

        if (!net_parse_ip(arg, ip))
        {
            console_write("Usage : ping <a.b.c.d>   (ex. ping 10.0.2.2)\n");
            return 1;
        }

        net_ping(ip);
        return 1;
    }

    if (starts_with(line, "arp"))
    {
        const char* arg = skip_word(line);

        net_poll();

        if (mk_strcmp(arg, "flush") == 0)
        {
            net_arp_flush();
            console_write("Table ARP videe.\n");
            return 1;
        }

        net_print_arp();
        return 1;
    }

    if (starts_with(line, "firewall"))
    {
        const char* arg = skip_word(line);

        if (mk_strcmp(arg, "stealth on") == 0)
        {
            net_set_stealth(1);
            console_write("Mode furtif active : les ping entrants sont ignores.\n");
            return 1;
        }

        if (mk_strcmp(arg, "stealth off") == 0)
        {
            net_set_stealth(0);
            console_write("Mode furtif desactive : la machine repond aux ping.\n");
            return 1;
        }

        net_poll();
        net_print_firewall();
        console_write("  Usage : firewall stealth on|off\n");
        return 1;
    }

    if (starts_with(line, "seclog"))
    {
        net_poll();
        net_print_seclog();
        return 1;
    }

    if (starts_with(line, "radio") || starts_with(line, "wifi") || starts_with(line, "bluetooth"))
    {
        cmd_radio();
        return 1;
    }

    if (starts_with(line, "dhcp"))
    {
        dhcp_run();
        return 1;
    }

    if (starts_with(line, "nslookup") || starts_with(line, "dns"))
    {
        cmd_nslookup(skip_word(line));
        return 1;
    }

    if (starts_with(line, "https"))
    {
        cmd_https(skip_word(line));
        return 1;
    }

    if (starts_with(line, "http") || starts_with(line, "wget") || starts_with(line, "curl"))
    {
        cmd_http(skip_word(line));
        return 1;
    }

    if (starts_with(line, "selftest") || starts_with(line, "cryptotest"))
    {
        crypto_selftest();
        return 1;
    }

    if (starts_with(line, "sha256"))
    {
        const char* text = skip_word(line);
        u8 digest[32];
        char hex[65];
        static const char digits[] = "0123456789abcdef";

        sha256((const u8*)text, mk_strlen(text), digest);

        for (int i = 0; i < 32; i++)
        {
            hex[i * 2] = digits[digest[i] >> 4];
            hex[i * 2 + 1] = digits[digest[i] & 0xF];
        }
        hex[64] = 0;

        console_write(hex);
        console_write("\n");
        return 1;
    }

    if (starts_with(line, "jarvis"))
    {
        return jarvis_command(line);
    }

    return 0;
}
