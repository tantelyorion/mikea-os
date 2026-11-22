#include "net.h"
#include "../kernel/drivers/net/e1000.h"
#include "../kernel/drivers/net/rtl8139.h"
#include "../kernel/drivers/pci/pci.h"
#include "../libc/string.h"

void console_write(const char* text);
unsigned long timer_ticks();

/* ------------------------------------------------------------
   Configuration (reseau utilisateur par defaut de QEMU : "-nic user")
   ------------------------------------------------------------ */

/* Configuration IPv4 (valeurs par defaut = reseau utilisateur de QEMU ; modifiables par DHCP). */
u8 net_ip[4]      = { 10, 0, 2, 15 };
u8 net_mask[4]    = { 255, 255, 255, 0 };
u8 net_gateway[4] = { 10, 0, 2, 2 };
u8 net_dns[4]     = { 10, 0, 2, 3 };

/*
    Abstraction de carte reseau : la pile (ARP/IP/TCP...) ne connait que
    ces quatre fonctions. Chaque pilote (e1000, rtl8139, et plus tard
    WiFi / USB) s'y branche, ce qui permet d'ajouter du materiel sans
    toucher au protocole.
*/
static int (*nic_send_fn)(const u8*, u16) = (int (*)(const u8*, u16))0;
static int (*nic_recv_fn)(u8*, u16) = (int (*)(u8*, u16))0;
static const u8* (*nic_mac_fn)(void) = (const u8* (*)(void))0;
static int (*nic_link_fn)(void) = (int (*)(void))0;
static const char* nic_name = "aucune";
static u8 broadcast_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static int net_up = 0;
static int stealth_mode = 1;

static u32 stat_rx = 0;
static u32 stat_tx = 0;
static u32 stat_blocked = 0;

/* ------------------------------------------------------------
   Affichage
   ------------------------------------------------------------ */

static void put_u32(u32 value)
{
    char tmp[12];
    int n = 0;

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

    char out[12];
    int i = 0;
    while (n > 0)
    {
        out[i++] = tmp[--n];
    }
    out[i] = 0;
    console_write(out);
}

static void ip_to_str(const u8* ip, char* out)
{
    int pos = 0;

    for (int i = 0; i < 4; i++)
    {
        u8 v = ip[i];
        char digits[3];
        int d = 0;

        if (v == 0)
        {
            digits[d++] = '0';
        }
        while (v > 0)
        {
            digits[d++] = (char)('0' + (v % 10));
            v /= 10;
        }
        while (d > 0)
        {
            out[pos++] = digits[--d];
        }
        if (i < 3)
        {
            out[pos++] = '.';
        }
    }
    out[pos] = 0;
}

static void mac_to_str(const u8* mac, char* out)
{
    static const char hex[] = "0123456789ABCDEF";
    int pos = 0;

    for (int i = 0; i < 6; i++)
    {
        out[pos++] = hex[mac[i] >> 4];
        out[pos++] = hex[mac[i] & 0xF];
        if (i < 5)
        {
            out[pos++] = ':';
        }
    }
    out[pos] = 0;
}

static void put_ip(const u8* ip)
{
    char buf[20];
    ip_to_str(ip, buf);
    console_write(buf);
}

static void put_mac(const u8* mac)
{
    char buf[20];
    mac_to_str(mac, buf);
    console_write(buf);
}

/* ------------------------------------------------------------
   Journal de securite
   ------------------------------------------------------------ */

#define SECLOG_ENTRIES 16
#define SECLOG_LENGTH  72

static char seclog_lines[SECLOG_ENTRIES][SECLOG_LENGTH];
static unsigned long seclog_ticks[SECLOG_ENTRIES];
static u32 seclog_count = 0;

void sec_log(const char* message)
{
    u32 slot = seclog_count % SECLOG_ENTRIES;
    u32 i = 0;

    while (message[i] != 0 && i < SECLOG_LENGTH - 1)
    {
        seclog_lines[slot][i] = message[i];
        i++;
    }
    seclog_lines[slot][i] = 0;
    seclog_ticks[slot] = timer_ticks();
    seclog_count++;
}

static unsigned long last_block_log = 0;

/* Evite de saturer le journal si on est inonde de paquets. */
static void sec_log_ratelimited(const char* message)
{
    unsigned long now = timer_ticks();

    if (seclog_count == 0 || now - last_block_log >= 100)
    {
        last_block_log = now;
        sec_log(message);
    }
}

void net_print_seclog()
{
    if (seclog_count == 0)
    {
        console_write("Journal de securite vide : aucun evenement suspect.\n");
        return;
    }

    u32 total = (seclog_count < SECLOG_ENTRIES) ? seclog_count : SECLOG_ENTRIES;
    u32 first = (seclog_count < SECLOG_ENTRIES) ? 0 : seclog_count % SECLOG_ENTRIES;

    console_write("Journal de securite (du plus ancien au plus recent) :\n");

    for (u32 i = 0; i < total; i++)
    {
        u32 slot = (first + i) % SECLOG_ENTRIES;
        console_write("  [t=");
        put_u32((u32)(seclog_ticks[slot] / 100));
        console_write("s] ");
        console_write(seclog_lines[slot]);
        console_write("\n");
    }
}

/* ------------------------------------------------------------
   Utilitaires protocole
   ------------------------------------------------------------ */

static u16 rd16(const u8* p)
{
    return (u16)(((u16)p[0] << 8) | p[1]);
}

static void wr16(u8* p, u16 v)
{
    p[0] = (u8)(v >> 8);
    p[1] = (u8)(v & 0xFF);
}

static u16 checksum(const u8* data, u32 length)
{
    u32 sum = 0;

    for (u32 i = 0; i + 1 < length; i += 2)
    {
        sum += ((u32)data[i] << 8) | data[i + 1];
    }

    if (length & 1u)
    {
        sum += (u32)data[length - 1] << 8;
    }

    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (u16)(~sum);
}

static int same_ip(const u8* a, const u8* b)
{
    return mk_memcmp(a, b, 4) == 0;
}

static int on_my_subnet(const u8* ip)
{
    for (int i = 0; i < 4; i++)
    {
        if ((ip[i] & net_mask[i]) != (net_ip[i] & net_mask[i]))
        {
            return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------
   ARP (avec detection d'usurpation)
   ------------------------------------------------------------ */

#define ARP_ENTRIES 8

typedef struct
{
    int used;
    u8 ip[4];
    u8 mac[6];
} arp_entry_t;

static arp_entry_t arp_cache[ARP_ENTRIES];

static arp_entry_t* arp_find(const u8* ip)
{
    for (int i = 0; i < ARP_ENTRIES; i++)
    {
        if (arp_cache[i].used && same_ip(arp_cache[i].ip, ip))
        {
            return &arp_cache[i];
        }
    }
    return (arp_entry_t*)0;
}

static void arp_learn(const u8* ip, const u8* mac)
{
    arp_entry_t* existing = arp_find(ip);

    if (existing != (arp_entry_t*)0)
    {
        if (mk_memcmp(existing->mac, mac, 6) != 0)
        {
            /*
                Defense : l'adresse MAC associee a cette IP vient de
                CHANGER. C'est exactement la signature d'une attaque
                d'usurpation ARP (man-in-the-middle). On refuse la
                mise a jour et on alerte.
            */
            char msg[SECLOG_LENGTH];
            char ipbuf[20];
            char macbuf[20];
            ip_to_str(ip, ipbuf);
            mac_to_str(mac, macbuf);

            const char* prefix = "ALERTE ARP spoofing ? ";
            u32 n = 0;
            for (u32 i = 0; prefix[i] && n < SECLOG_LENGTH - 1; i++) { msg[n++] = prefix[i]; }
            for (u32 i = 0; ipbuf[i] && n < SECLOG_LENGTH - 1; i++) { msg[n++] = ipbuf[i]; }
            if (n < SECLOG_LENGTH - 1) { msg[n++] = ' '; }
            for (u32 i = 0; macbuf[i] && n < SECLOG_LENGTH - 1; i++) { msg[n++] = macbuf[i]; }
            msg[n] = 0;

            sec_log(msg);
            stat_blocked++;
        }
        return;
    }

    for (int i = 0; i < ARP_ENTRIES; i++)
    {
        if (!arp_cache[i].used)
        {
            arp_cache[i].used = 1;
            mk_memcpy(arp_cache[i].ip, ip, 4);
            mk_memcpy(arp_cache[i].mac, mac, 6);
            return;
        }
    }

    /* Cache plein : on recycle la premiere entree. */
    arp_cache[0].used = 1;
    mk_memcpy(arp_cache[0].ip, ip, 4);
    mk_memcpy(arp_cache[0].mac, mac, 6);
}

void net_arp_flush()
{
    for (int i = 0; i < ARP_ENTRIES; i++)
    {
        arp_cache[i].used = 0;
    }
}

void net_print_arp()
{
    int shown = 0;

    console_write("Table ARP :\n");

    for (int i = 0; i < ARP_ENTRIES; i++)
    {
        if (arp_cache[i].used)
        {
            console_write("  ");
            put_ip(arp_cache[i].ip);
            console_write("  ->  ");
            put_mac(arp_cache[i].mac);
            console_write("\n");
            shown++;
        }
    }

    if (shown == 0)
    {
        console_write("  (vide)\n");
    }
}

/* ------------------------------------------------------------
   Envoi
   ------------------------------------------------------------ */

static u8 tx_frame[E1000_MAX_FRAME];
static u8 rx_frame[E1000_MAX_FRAME];

static void send_frame(u16 length)
{
    if (nic_send_fn(tx_frame, length) == 0)
    {
        stat_tx++;
    }
}

static void eth_header(const u8* dst_mac, u16 ethertype)
{
    mk_memcpy(tx_frame, dst_mac, 6);
    mk_memcpy(tx_frame + 6, nic_mac_fn(), 6);
    wr16(tx_frame + 12, ethertype);
}

static void send_arp(u16 operation, const u8* target_mac, const u8* target_ip, const u8* eth_dst)
{
    eth_header(eth_dst, 0x0806);

    u8* arp = tx_frame + 14;
    wr16(arp + 0, 1);
    wr16(arp + 2, 0x0800);
    arp[4] = 6;
    arp[5] = 4;
    wr16(arp + 6, operation);
    mk_memcpy(arp + 8, nic_mac_fn(), 6);
    mk_memcpy(arp + 14, net_ip, 4);
    mk_memcpy(arp + 18, target_mac, 6);
    mk_memcpy(arp + 24, target_ip, 4);

    send_frame(42);
}

static u16 ip_id_counter = 1;

static void send_ip(const u8* dst_ip, u8 protocol, const u8* payload, u16 payload_length, const u8* dst_mac)
{
    eth_header(dst_mac, 0x0800);

    u8* ip = tx_frame + 14;
    u16 total = (u16)(20 + payload_length);

    ip[0] = 0x45;
    ip[1] = 0;
    wr16(ip + 2, total);
    wr16(ip + 4, ip_id_counter++);
    wr16(ip + 6, 0x4000);
    ip[8] = 64;
    ip[9] = protocol;
    wr16(ip + 10, 0);
    mk_memcpy(ip + 12, net_ip, 4);
    mk_memcpy(ip + 16, dst_ip, 4);
    wr16(ip + 10, checksum(ip, 20));

    mk_memcpy(ip + 20, payload, payload_length);

    send_frame((u16)(14 + total));
}

/* ------------------------------------------------------------
   Reception
   ------------------------------------------------------------ */

static volatile int ping_waiting = 0;
static volatile int ping_got_reply = 0;
static u16 ping_id = 0x4D4B;
static u16 ping_seq = 0;

static void handle_arp(const u8* frame, int length)
{
    if (length < 42)
    {
        return;
    }

    const u8* arp = frame + 14;
    u16 operation = rd16(arp + 6);
    const u8* sender_mac = arp + 8;
    const u8* sender_ip = arp + 14;
    const u8* target_ip = arp + 24;

    arp_learn(sender_ip, sender_mac);

    if (operation == 1 && same_ip(target_ip, net_ip))
    {
        send_arp(2, sender_mac, sender_ip, sender_mac);
    }
}

static void handle_icmp(const u8* ip, const u8* eth_src, int length)
{
    int ihl = (ip[0] & 0x0F) * 4;
    int total = (int)rd16(ip + 2);

    if (ihl < 20 || total < ihl + 8 || total > length)
    {
        return;
    }

    const u8* icmp = ip + ihl;
    int icmp_length = total - ihl;

    if (checksum(icmp, (u32)icmp_length) != 0)
    {
        stat_blocked++;
        sec_log_ratelimited("Paquet ICMP corrompu (checksum) rejete");
        return;
    }

    u8 type = icmp[0];

    if (type == 0)
    {
        if (ping_waiting && rd16(icmp + 4) == ping_id && rd16(icmp + 6) == ping_seq)
        {
            ping_got_reply = 1;
        }
        return;
    }

    if (type == 8)
    {
        if (stealth_mode)
        {
            stat_blocked++;

            char msg[SECLOG_LENGTH];
            char ipbuf[20];
            ip_to_str(ip + 12, ipbuf);
            const char* prefix = "Ping entrant ignore (mode furtif) de ";
            u32 n = 0;
            for (u32 i = 0; prefix[i] && n < SECLOG_LENGTH - 1; i++) { msg[n++] = prefix[i]; }
            for (u32 i = 0; ipbuf[i] && n < SECLOG_LENGTH - 1; i++) { msg[n++] = ipbuf[i]; }
            msg[n] = 0;
            sec_log_ratelimited(msg);
            return;
        }

        static u8 reply[1500];
        if (icmp_length > (int)sizeof(reply))
        {
            return;
        }

        mk_memcpy(reply, icmp, (u32)icmp_length);
        reply[0] = 0;
        wr16(reply + 2, 0);
        wr16(reply + 2, checksum(reply, (u32)icmp_length));
        send_ip(ip + 12, 1, reply, (u16)icmp_length, eth_src);
    }
}

static void handle_ipv4(const u8* frame, int length)
{
    if (length < 34)
    {
        return;
    }

    const u8* ip = frame + 14;

    if ((ip[0] >> 4) != 4)
    {
        return;
    }

    /* On ne traite que ce qui nous est destine (ou diffusion). */
    int for_me = same_ip(ip + 16, net_ip);
    int is_broadcast = (ip[16] == 255 && ip[17] == 255 && ip[18] == 255 && ip[19] == 255);

    u8 protocol = ip[9];

    if (!for_me && !is_broadcast && !(protocol == 17 && tcpip_accepts_any_ip()))
    {
        return;
    }

    if (protocol == 1)
    {
        handle_icmp(ip, frame + 6, length - 14);
        return;
    }

    /*
        TCP/UDP : traites UNIQUEMENT s'ils correspondent a une connexion ou une
        requete que NOUS avons initiee (tcpip.c). Sinon : refus silencieux.
    */
    int ihl_bytes = (ip[0] & 0x0F) * 4;

    if ((protocol == 6 || protocol == 17) && ihl_bytes >= 20
        && tcpip_input(protocol, ip, ihl_bytes, (int)rd16(ip + 2), frame + 6))
    {
        return;
    }

    stat_blocked++;

    if (protocol == 6)
    {
        sec_log_ratelimited("Connexion TCP entrante non sollicitee bloquee");
    }
    else if (protocol == 17)
    {
        /* Le trafic UDP de diffusion (DHCP, mDNS...) est banal : pas de journal. */
        if (!is_broadcast)
        {
            sec_log_ratelimited("Paquet UDP entrant non sollicite bloque");
        }
    }
    else
    {
        sec_log_ratelimited("Paquet IP de protocole inconnu bloque");
    }
}

void net_poll()
{
    if (!net_up)
    {
        return;
    }

    for (int budget = 0; budget < 16; budget++)
    {
        int length = nic_recv_fn(rx_frame, sizeof(rx_frame));

        if (length <= 0)
        {
            break;
        }

        stat_rx++;

        if (length < 14)
        {
            continue;
        }

        u16 ethertype = rd16(rx_frame + 12);

        if (ethertype == 0x0806)
        {
            handle_arp(rx_frame, length);
        }
        else if (ethertype == 0x0800)
        {
            handle_ipv4(rx_frame, length);
        }
    }
}

/* ------------------------------------------------------------
   Resolution ARP + ping
   ------------------------------------------------------------ */

static void wait_ticks_polling(u32 ticks, volatile int* done)
{
    unsigned long start = timer_ticks();
    u64 spins = 0;
    u64 limit = (u64)ticks * 400000ULL + 1000ULL;

    while (!(*done))
    {
        net_poll();

        if (timer_ticks() - start >= ticks)
        {
            break;
        }

        if (++spins > limit)
        {
            break;
        }

        asm volatile("pause");
    }
}

static int resolve_mac(const u8* next_hop, u8* mac_out)
{
    for (int attempt = 0; attempt < 3; attempt++)
    {
        arp_entry_t* entry = arp_find(next_hop);

        if (entry != (arp_entry_t*)0)
        {
            mk_memcpy(mac_out, entry->mac, 6);
            return 1;
        }

        static const u8 zero_mac[6] = { 0, 0, 0, 0, 0, 0 };
        send_arp(1, zero_mac, next_hop, broadcast_mac);

        unsigned long start = timer_ticks();
        u64 spins = 0;

        while (timer_ticks() - start < 100 && spins < 40000000ULL)
        {
            net_poll();

            if (arp_find(next_hop) != (arp_entry_t*)0)
            {
                break;
            }

            spins++;
            asm volatile("pause");
        }
    }

    arp_entry_t* entry = arp_find(next_hop);

    if (entry != (arp_entry_t*)0)
    {
        mk_memcpy(mac_out, entry->mac, 6);
        return 1;
    }

    return 0;
}


/* ------------------------------------------------------------
   Interface pour tcpip.c (UDP / DHCP / DNS / TCP)
   ------------------------------------------------------------ */

u16 net_checksum(const u8* data, u32 length)
{
    return checksum(data, length);
}

const u8* net_local_mac()
{
    return nic_mac_fn();
}

int net_send_frame(const u8* frame, u16 length)
{
    if (!net_up)
    {
        return -1;
    }

    int result = nic_send_fn(frame, length);
    if (result == 0)
    {
        stat_tx++;
    }
    return result;
}

void net_send_ip_packet(const u8* dst_ip, u8 protocol, const u8* payload, u16 payload_length, const u8* dst_mac)
{
    send_ip(dst_ip, protocol, payload, payload_length, dst_mac);
}

/* MAC du prochain saut (passerelle si hors sous-reseau). 1 si resolue. */
int net_next_hop_mac(const u8* destination, u8* mac_out)
{
    const u8* next_hop = on_my_subnet(destination) ? destination : net_gateway;
    return resolve_mac(next_hop, mac_out);
}

int net_ping(const u8 destination[4])
{
    if (!net_up)
    {
        console_write("Reseau indisponible (aucune carte reseau initialisee).\n");
        return -1;
    }

    const u8* next_hop = on_my_subnet(destination) ? destination : net_gateway;
    u8 mac[6];

    if (!resolve_mac(next_hop, mac))
    {
        console_write("Echec : impossible de resoudre l'adresse MAC (ARP sans reponse).\n");
        return -2;
    }

    u8 icmp[40];
    mk_memset(icmp, 0, sizeof(icmp));

    icmp[0] = 8;
    icmp[1] = 0;
    ping_seq++;
    wr16(icmp + 4, ping_id);
    wr16(icmp + 6, ping_seq);

    for (int i = 8; i < 40; i++)
    {
        icmp[i] = (u8)('a' + (i % 23));
    }

    wr16(icmp + 2, checksum(icmp, 40));

    ping_got_reply = 0;
    ping_waiting = 1;

    unsigned long start = timer_ticks();
    send_ip(destination, 1, icmp, 40, mac);
    wait_ticks_polling(200, &ping_got_reply);

    ping_waiting = 0;

    if (ping_got_reply)
    {
        unsigned long elapsed = (timer_ticks() - start) * 10;

        console_write("Reponse de ");
        put_ip(destination);
        console_write(" : 40 octets, temps ~");
        put_u32((u32)elapsed);
        console_write(" ms (resolution 10 ms)\n");
        return 0;
    }

    console_write("Delai depasse : aucune reponse de ");
    put_ip(destination);
    console_write("\n");
    return -3;
}

int net_parse_ip(const char* text, u8 out[4])
{
    int part = 0;
    u32 value = 0;
    int digits = 0;

    for (u32 i = 0;; i++)
    {
        char c = text[i];

        if (c >= '0' && c <= '9')
        {
            value = value * 10 + (u32)(c - '0');
            digits++;

            if (value > 255 || digits > 3)
            {
                return 0;
            }
        }
        else if (c == '.' || c == 0)
        {
            if (digits == 0 || part > 3)
            {
                return 0;
            }

            out[part++] = (u8)value;
            value = 0;
            digits = 0;

            if (c == 0)
            {
                break;
            }
        }
        else
        {
            return 0;
        }
    }

    return part == 4;
}

/* ------------------------------------------------------------
   Initialisation et etat
   ------------------------------------------------------------ */

void net_init()
{
    net_up = 0;
    net_arp_flush();

    /* 1) Intel e1000 : 8086:100E (QEMU), 100F, 1004, 10D3 (82574L, souvent compatible). */
    static const u16 e1000_ids[] = { 0x100E, 0x100F, 0x1004, 0x10D3 };
    pci_device_t* nic = (pci_device_t*)0;

    for (u32 i = 0; i < sizeof(e1000_ids) / sizeof(e1000_ids[0]) && nic == (pci_device_t*)0; i++)
    {
        nic = pci_find_id(0x8086, e1000_ids[i]);
    }

    if (nic != (pci_device_t*)0 && e1000_init(nic) == 0)
    {
        nic_send_fn = e1000_send;
        nic_recv_fn = e1000_recv;
        nic_mac_fn = e1000_mac;
        nic_link_fn = e1000_link_up;
        nic_name = "Intel e1000";
        net_up = 1;
    }
    else
    {
        /* 2) Realtek RTL8139 : 10EC:8139. */
        pci_device_t* rtl = pci_find_id(0x10EC, 0x8139);

        if (rtl != (pci_device_t*)0 && rtl8139_init(rtl) == 0)
        {
            nic_send_fn = rtl8139_send;
            nic_recv_fn = rtl8139_recv;
            nic_mac_fn = rtl8139_mac;
            nic_link_fn = rtl8139_link_up;
            nic_name = "Realtek RTL8139";
            net_up = 1;
        }
    }

    if (!net_up)
    {
        console_write("[net] Aucune carte Ethernet prise en charge (e1000, rtl8139)\n");
        return;
    }

    console_write("[net] Carte ");
    console_write(nic_name);
    console_write(" initialisee, pare-feu actif (mode furtif)\n");
}

int net_is_up()
{
    return net_up;
}

void net_set_stealth(int enabled)
{
    stealth_mode = enabled ? 1 : 0;
}

void net_print_status()
{
    if (!net_up)
    {
        console_write("Reseau : AUCUNE interface active.\n");
        console_write("Pilotes disponibles : Intel e1000, Realtek RTL8139 (QEMU: -nic user,model=e1000|rtl8139).\n");
        return;
    }

    console_write("Interface eth0 (");
    console_write(nic_name);
    console_write(")\n");
    console_write("  Lien      : ");
    console_write(nic_link_fn() ? "ACTIF\n" : "INACTIF\n");
    console_write("  MAC       : ");
    put_mac(nic_mac_fn());
    console_write("\n  IPv4      : ");
    put_ip(net_ip);
    console_write("\n  Masque    : ");
    put_ip(net_mask);
    console_write("\n  Passerelle: ");
    put_ip(net_gateway);
    console_write("\n  Trames    : recues ");
    put_u32(stat_rx);
    console_write(", envoyees ");
    put_u32(stat_tx);
    console_write(", bloquees ");
    put_u32(stat_blocked);
    console_write("\n");
}

void net_print_firewall()
{
    console_write("Pare-feu MikeaOS\n");
    console_write("  Politique entrante : REFUSER (aucun service expose)\n");
    console_write("  Mode furtif (ping entrant ignore) : ");
    console_write(stealth_mode ? "OUI\n" : "NON\n");
    console_write("  Detection usurpation ARP : ACTIVE\n");
    console_write("  Paquets bloques : ");
    put_u32(stat_blocked);
    console_write("\n");
}
