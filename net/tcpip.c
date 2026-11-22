/*
    Couche transport / services internet de MikeaOS :
      UDP, client DHCP, resolveur DNS (enregistrements A), client TCP,
      et une commande HTTP/1.1 (texte clair).

    Politique de securite (coherente avec net.c) :
      - AUCUN port n'est ouvert : on n'accepte que les reponses a ce que
        NOUS avons demande (identifiant DHCP/DNS, ports, numeros de sequence) ;
      - une seule connexion TCP sortante a la fois, sequence verifiee
        strictement (pas d'injection de segments hors-fenetre) ;
      - port source TCP/DNS imprevisible a chaque usage.
    Pas de TLS : HTTPS n'est pas pris en charge (HTTP seulement).
*/

#include "net.h"
#include "../libc/string.h"

void console_write(const char* text);
unsigned long timer_ticks();

#define PROTO_TCP 6
#define PROTO_UDP 17

/* ------------------------------------------------------------
   Utilitaires
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

static u32 rd32(const u8* p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static void wr32(u8* p, u32 v)
{
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)(v & 0xFF);
}

static int same4(const u8* a, const u8* b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

/* Somme de controle TCP/UDP avec pseudo-en-tete. Vaut 0 sur un segment valide. */
static u16 l4_checksum(const u8* src, const u8* dst, u8 protocol, const u8* segment, u32 length)
{
    u32 sum = 0;

    sum += ((u32)src[0] << 8) | src[1];
    sum += ((u32)src[2] << 8) | src[3];
    sum += ((u32)dst[0] << 8) | dst[1];
    sum += ((u32)dst[2] << 8) | dst[3];
    sum += protocol;
    sum += length;

    for (u32 i = 0; i + 1 < length; i += 2)
    {
        sum += ((u32)segment[i] << 8) | segment[i + 1];
    }

    if (length & 1u)
    {
        sum += (u32)segment[length - 1] << 8;
    }

    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (u16)(~sum);
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

static void put_ip4(const u8* ip)
{
    for (int i = 0; i < 4; i++)
    {
        put_dec(ip[i]);
        if (i < 3)
        {
            console_write(".");
        }
    }
}

/* Attend qu'un drapeau passe a 1 en traitant les trames recues. */
static int wait_flag(volatile int* flag, u32 ticks)
{
    unsigned long start = timer_ticks();
    u64 spins = 0;
    u64 limit = (u64)ticks * 400000ULL + 1000ULL;

    while (!(*flag))
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

    return *flag;
}

static u16 random_port(u32 salt)
{
    u32 value = (u32)timer_ticks() * 2654435761u + salt * 40503u;
    return (u16)(49152 + ((value >> 8) % 16000));
}

/* ------------------------------------------------------------
   UDP
   ------------------------------------------------------------ */

static void udp_send(const u8* dst_ip, u16 sport, u16 dport, const u8* data, u16 length, const u8* dst_mac)
{
    static u8 segment[1400];

    if (length > sizeof(segment) - 8)
    {
        return;
    }

    wr16(segment + 0, sport);
    wr16(segment + 2, dport);
    wr16(segment + 4, (u16)(8 + length));
    wr16(segment + 6, 0);
    mk_memcpy(segment + 8, data, length);

    u16 sum = l4_checksum(net_ip, dst_ip, PROTO_UDP, segment, (u32)(8 + length));
    if (sum == 0)
    {
        sum = 0xFFFF;
    }
    wr16(segment + 6, sum);

    net_send_ip_packet(dst_ip, PROTO_UDP, segment, (u16)(8 + length), dst_mac);
}

/* ------------------------------------------------------------
   DHCP
   ------------------------------------------------------------ */

static volatile int dhcp_waiting = 0;
static volatile int dhcp_got_offer = 0;
static volatile int dhcp_got_ack = 0;
static u32 dhcp_xid = 0x4D4B4541u;
static u8 dhcp_ip[4];
static u8 dhcp_server[4];
static u8 dhcp_mask[4];
static u8 dhcp_router[4];
static u8 dhcp_dns[4];

int tcpip_accepts_any_ip()
{
    return dhcp_waiting;
}

static void dhcp_send(u8 message_type, const u8* requested_ip, const u8* server_id)
{
    static u8 frame[420];
    mk_memset(frame, 0, sizeof(frame));

    u8* eth = frame;
    u8* ip = frame + 14;
    u8* udp = ip + 20;
    u8* d = udp + 8;

    for (int i = 0; i < 6; i++)
    {
        eth[i] = 0xFF;
    }
    mk_memcpy(eth + 6, net_local_mac(), 6);
    wr16(eth + 12, 0x0800);

    d[0] = 1;  /* BOOTREQUEST */
    d[1] = 1;  /* Ethernet */
    d[2] = 6;
    wr32(d + 4, dhcp_xid);
    wr16(d + 10, 0x8000); /* reponse en diffusion demandee */
    mk_memcpy(d + 28, net_local_mac(), 6);

    d[236] = 99; d[237] = 130; d[238] = 83; d[239] = 99;

    u8* o = d + 240;
    *o++ = 53; *o++ = 1; *o++ = message_type;

    if (requested_ip != (const u8*)0)
    {
        *o++ = 50; *o++ = 4;
        mk_memcpy(o, requested_ip, 4); o += 4;
        *o++ = 54; *o++ = 4;
        mk_memcpy(o, server_id, 4); o += 4;
    }

    *o++ = 55; *o++ = 3; *o++ = 1; *o++ = 3; *o++ = 6; /* masque, routeur, DNS */
    *o++ = 255;

    u16 dhcp_length = (u16)(o - d);
    if (dhcp_length < 300)
    {
        dhcp_length = 300;
    }

    u16 udp_length = (u16)(8 + dhcp_length);
    u16 total = (u16)(20 + udp_length);

    ip[0] = 0x45;
    wr16(ip + 2, total);
    ip[8] = 64;
    ip[9] = PROTO_UDP;
    ip[16] = 255; ip[17] = 255; ip[18] = 255; ip[19] = 255;
    wr16(ip + 10, net_checksum(ip, 20));

    wr16(udp + 0, 68);
    wr16(udp + 2, 67);
    wr16(udp + 4, udp_length);
    /* somme UDP a 0 : autorisee en IPv4 */

    net_send_frame(frame, (u16)(14 + total));
}

static void dhcp_input(const u8* payload, int length)
{
    if (length < 244 || payload[0] != 2 || rd32(payload + 4) != dhcp_xid)
    {
        return;
    }

    if (payload[236] != 99 || payload[237] != 130 || payload[238] != 83 || payload[239] != 99)
    {
        return;
    }

    u8 message_type = 0;
    u8 mask[4] = { 255, 255, 255, 0 };
    u8 router[4] = { 0, 0, 0, 0 };
    u8 dns[4] = { 0, 0, 0, 0 };
    u8 server[4] = { 0, 0, 0, 0 };

    int i = 240;

    while (i < length)
    {
        u8 code = payload[i];

        if (code == 255)
        {
            break;
        }
        if (code == 0)
        {
            i++;
            continue;
        }
        if (i + 1 >= length)
        {
            break;
        }

        u8 option_length = payload[i + 1];

        if (i + 2 + option_length > length)
        {
            break;
        }

        const u8* value = payload + i + 2;

        if (code == 53 && option_length >= 1) { message_type = value[0]; }
        else if (code == 1 && option_length >= 4) { mk_memcpy(mask, value, 4); }
        else if (code == 3 && option_length >= 4) { mk_memcpy(router, value, 4); }
        else if (code == 6 && option_length >= 4) { mk_memcpy(dns, value, 4); }
        else if (code == 54 && option_length >= 4) { mk_memcpy(server, value, 4); }

        i += 2 + option_length;
    }

    if (message_type == 2) /* OFFER */
    {
        mk_memcpy(dhcp_ip, payload + 16, 4);
        mk_memcpy(dhcp_server, server, 4);
        mk_memcpy(dhcp_mask, mask, 4);
        mk_memcpy(dhcp_router, router, 4);
        mk_memcpy(dhcp_dns, dns, 4);
        dhcp_got_offer = 1;
    }
    else if (message_type == 5) /* ACK */
    {
        mk_memcpy(dhcp_ip, payload + 16, 4);
        mk_memcpy(dhcp_mask, mask, 4);
        if (router[0] | router[1] | router[2] | router[3]) { mk_memcpy(dhcp_router, router, 4); }
        if (dns[0] | dns[1] | dns[2] | dns[3]) { mk_memcpy(dhcp_dns, dns, 4); }
        dhcp_got_ack = 1;
    }
}

int dhcp_run()
{
    if (!net_is_up())
    {
        console_write("Reseau indisponible.\n");
        return -1;
    }

    dhcp_xid = 0x4D4B0000u | (u32)(timer_ticks() & 0xFFFF);
    dhcp_got_offer = 0;
    dhcp_got_ack = 0;
    dhcp_waiting = 1;

    for (int attempt = 0; attempt < 3 && !dhcp_got_offer; attempt++)
    {
        dhcp_send(1, (const u8*)0, (const u8*)0);
        wait_flag(&dhcp_got_offer, 200);
    }

    if (!dhcp_got_offer)
    {
        dhcp_waiting = 0;
        console_write("DHCP : aucune offre recue (serveur injoignable).\n");
        return -2;
    }

    for (int attempt = 0; attempt < 3 && !dhcp_got_ack; attempt++)
    {
        dhcp_send(3, dhcp_ip, dhcp_server);
        wait_flag(&dhcp_got_ack, 200);
    }

    dhcp_waiting = 0;

    if (!dhcp_got_ack)
    {
        console_write("DHCP : requete non confirmee par le serveur.\n");
        return -3;
    }

    mk_memcpy(net_ip, dhcp_ip, 4);
    mk_memcpy(net_mask, dhcp_mask, 4);
    if (dhcp_router[0] | dhcp_router[1] | dhcp_router[2] | dhcp_router[3])
    {
        mk_memcpy(net_gateway, dhcp_router, 4);
    }
    if (dhcp_dns[0] | dhcp_dns[1] | dhcp_dns[2] | dhcp_dns[3])
    {
        mk_memcpy(net_dns, dhcp_dns, 4);
    }

    console_write("DHCP : adresse ");
    put_ip4(net_ip);
    console_write(", passerelle ");
    put_ip4(net_gateway);
    console_write(", DNS ");
    put_ip4(net_dns);
    console_write("\n");

    return 0;
}

/* ------------------------------------------------------------
   DNS (enregistrements A)
   ------------------------------------------------------------ */

static volatile int dns_waiting = 0;
static volatile int dns_got = 0;
static volatile int dns_failed = 0;
static u16 dns_id = 0;
static u16 dns_port = 0;
static u8 dns_result[4];

static int dns_skip_name(const u8* msg, int length, int pos)
{
    while (pos < length)
    {
        u8 len = msg[pos];

        if (len == 0)
        {
            return pos + 1;
        }
        if ((len & 0xC0) == 0xC0)
        {
            return pos + 2;
        }
        pos += 1 + len;
    }
    return -1;
}

static void dns_input(const u8* msg, int length)
{
    if (length < 12 || rd16(msg) != dns_id)
    {
        return;
    }

    u16 flags = rd16(msg + 2);

    if (!(flags & 0x8000))
    {
        return; /* ce n'est pas une reponse */
    }

    if ((flags & 0x000F) != 0)
    {
        dns_failed = 1;
        return;
    }

    u16 questions = rd16(msg + 4);
    u16 answers = rd16(msg + 6);
    int pos = 12;

    for (u16 q = 0; q < questions; q++)
    {
        pos = dns_skip_name(msg, length, pos);
        if (pos < 0 || pos + 4 > length)
        {
            return;
        }
        pos += 4;
    }

    for (u16 a = 0; a < answers; a++)
    {
        pos = dns_skip_name(msg, length, pos);
        if (pos < 0 || pos + 10 > length)
        {
            return;
        }

        u16 type = rd16(msg + pos);
        u16 rdlength = rd16(msg + pos + 8);
        pos += 10;

        if (pos + rdlength > length)
        {
            return;
        }

        if (type == 1 && rdlength == 4)
        {
            mk_memcpy(dns_result, msg + pos, 4);
            dns_got = 1;
            return;
        }

        pos += rdlength;
    }

    dns_failed = 1;
}

int dns_resolve(const char* name, u8 out[4])
{
    if (net_parse_ip(name, out))
    {
        return 0;
    }

    if (!net_is_up())
    {
        console_write("Reseau indisponible.\n");
        return -1;
    }

    u32 name_length = mk_strlen(name);

    if (name_length == 0 || name_length > 200)
    {
        return -2;
    }

    u8 query[300];
    mk_memset(query, 0, sizeof(query));

    dns_id = (u16)(timer_ticks() * 31u + 0x4D4B);
    wr16(query + 0, dns_id);
    wr16(query + 2, 0x0100); /* recursion souhaitee */
    wr16(query + 4, 1);

    int pos = 12;
    u32 label_start = (u32)pos;
    pos++;
    u32 label_length = 0;

    for (u32 i = 0; i <= name_length; i++)
    {
        char c = name[i];

        if (c == '.' || c == 0)
        {
            if (label_length == 0 || label_length > 63)
            {
                if (c == 0 && label_length == 0 && i > 0)
                {
                    break; /* point final tolere */
                }
                return -2;
            }

            query[label_start] = (u8)label_length;
            label_start = (u32)pos;
            pos++;
            label_length = 0;
        }
        else
        {
            query[pos++] = (u8)c;
            label_length++;
        }
    }

    query[label_start] = 0;
    wr16(query + pos, 1);       /* type A */
    wr16(query + pos + 2, 1);   /* classe IN */
    int query_length = pos + 4;

    u8 mac[6];

    if (!net_next_hop_mac(net_dns, mac))
    {
        console_write("DNS : impossible de joindre le serveur DNS (ARP).\n");
        return -3;
    }

    dns_port = random_port(dns_id);
    dns_got = 0;
    dns_failed = 0;
    dns_waiting = 1;

    for (int attempt = 0; attempt < 3 && !dns_got && !dns_failed; attempt++)
    {
        udp_send(net_dns, dns_port, 53, query, (u16)query_length, mac);
        wait_flag(&dns_got, 150);
    }

    dns_waiting = 0;

    if (dns_got)
    {
        mk_memcpy(out, dns_result, 4);
        return 0;
    }

    return dns_failed ? -4 : -5;
}

/* ------------------------------------------------------------
   TCP (client, une connexion a la fois)
   ------------------------------------------------------------ */

#define TCP_RX_SIZE 16384
#define TCP_MSS     1200

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10

#define ST_CLOSED      0
#define ST_SYN_SENT    1
#define ST_ESTABLISHED 2
#define ST_FIN_WAIT    3

static struct
{
    volatile int state;
    u8 ip[4];
    u8 mac[6];
    u16 local_port;
    u16 remote_port;
    u32 iss;
    u32 snd_nxt;
    volatile u32 snd_una;
    u32 rcv_nxt;
    volatile int fin_received;
    volatile int reset;
    u32 rx_length;
    u8 rx[TCP_RX_SIZE];
} tcp;

static void tcp_send_segment(u8 flags, u32 seq, const u8* data, u16 length, int with_mss)
{
    static u8 segment[1500];
    u16 header = with_mss ? 24 : 20;

    if ((u32)header + length > sizeof(segment))
    {
        return;
    }

    wr16(segment + 0, tcp.local_port);
    wr16(segment + 2, tcp.remote_port);
    wr32(segment + 4, seq);
    wr32(segment + 8, (flags & TCP_ACK) ? tcp.rcv_nxt : 0);
    segment[12] = (u8)((header / 4) << 4);
    segment[13] = flags;

    u32 free_space = TCP_RX_SIZE - tcp.rx_length;
    wr16(segment + 14, (u16)(free_space > 0xFFFF ? 0xFFFF : free_space));
    wr16(segment + 16, 0);
    wr16(segment + 18, 0);

    if (with_mss)
    {
        segment[20] = 2;
        segment[21] = 4;
        wr16(segment + 22, 1460);
    }

    if (length > 0)
    {
        mk_memcpy(segment + header, data, length);
    }

    u16 sum = l4_checksum(net_ip, tcp.ip, PROTO_TCP, segment, (u32)(header + length));
    wr16(segment + 16, sum);

    net_send_ip_packet(tcp.ip, PROTO_TCP, segment, (u16)(header + length), tcp.mac);
}

static void tcp_send_ack()
{
    tcp_send_segment(TCP_ACK, tcp.snd_nxt, (const u8*)0, 0, 0);
}

static int seq_lt(u32 a, u32 b)
{
    return (int)(a - b) < 0;
}

static int tcp_input(const u8* ip, int ihl, int total_length)
{
    if (tcp.state == ST_CLOSED)
    {
        return 0;
    }

    const u8* t = ip + ihl;
    int segment_length = total_length - ihl;

    if (segment_length < 20)
    {
        return 0;
    }

    if (!same4(ip + 12, tcp.ip) || rd16(t) != tcp.remote_port || rd16(t + 2) != tcp.local_port)
    {
        return 0;
    }

    if (l4_checksum(ip + 12, ip + 16, PROTO_TCP, t, (u32)segment_length) != 0)
    {
        return 1; /* segment corrompu : ignore */
    }

    u8 flags = t[13];
    u32 seq = rd32(t + 4);
    u32 ack = rd32(t + 8);
    int data_offset = (t[12] >> 4) * 4;

    if (data_offset < 20 || data_offset > segment_length)
    {
        return 1;
    }

    const u8* payload = t + data_offset;
    int payload_length = segment_length - data_offset;

    if (flags & TCP_RST)
    {
        tcp.reset = 1;
        tcp.state = ST_CLOSED;
        return 1;
    }

    if (tcp.state == ST_SYN_SENT)
    {
        if ((flags & TCP_SYN) && (flags & TCP_ACK) && ack == tcp.iss + 1)
        {
            tcp.rcv_nxt = seq + 1;
            tcp.snd_una = ack;
            tcp.state = ST_ESTABLISHED;
            tcp_send_ack();
        }
        return 1;
    }

    if (flags & TCP_ACK)
    {
        if (!seq_lt(ack, tcp.snd_una) && !seq_lt(tcp.snd_nxt, ack))
        {
            tcp.snd_una = ack;
        }
    }

    if (seq == tcp.rcv_nxt)
    {
        int accepted = 0;

        if (payload_length > 0)
        {
            u32 space = TCP_RX_SIZE - tcp.rx_length;
            accepted = (u32)payload_length > space ? (int)space : payload_length;

            if (accepted > 0)
            {
                mk_memcpy(tcp.rx + tcp.rx_length, payload, (u32)accepted);
                tcp.rx_length += (u32)accepted;
                tcp.rcv_nxt += (u32)accepted;
            }
        }

        if ((flags & TCP_FIN) && accepted == payload_length)
        {
            tcp.rcv_nxt += 1;
            tcp.fin_received = 1;
        }

        if (payload_length > 0 || (flags & TCP_FIN))
        {
            tcp_send_ack();
        }
    }
    else if (payload_length > 0 || (flags & TCP_FIN))
    {
        tcp_send_ack(); /* segment hors sequence : on reaffirme ce que l'on attend */
    }

    return 1;
}

int tcp_connect(const u8 ip[4], u16 port)
{
    if (!net_is_up())
    {
        console_write("Reseau indisponible.\n");
        return -1;
    }

    mk_memset(&tcp, 0, sizeof(tcp));
    mk_memcpy(tcp.ip, ip, 4);

    if (!net_next_hop_mac(ip, tcp.mac))
    {
        console_write("TCP : impossible de resoudre la passerelle (ARP).\n");
        return -2;
    }

    tcp.local_port = random_port(port);
    tcp.remote_port = port;
    tcp.iss = (u32)timer_ticks() * 64013u + 0x1F3A5C7u;
    tcp.snd_nxt = tcp.iss + 1;
    tcp.snd_una = tcp.iss;
    tcp.state = ST_SYN_SENT;

    for (int attempt = 0; attempt < 4 && tcp.state == ST_SYN_SENT; attempt++)
    {
        tcp_send_segment(TCP_SYN, tcp.iss, (const u8*)0, 0, 1);

        unsigned long start = timer_ticks();
        u64 spins = 0;

        while (tcp.state == ST_SYN_SENT && timer_ticks() - start < 120 && spins < 50000000ULL)
        {
            net_poll();
            spins++;
            asm volatile("pause");
        }
    }

    if (tcp.state != ST_ESTABLISHED)
    {
        tcp.state = ST_CLOSED;
        console_write(tcp.reset ? "TCP : connexion refusee.\n" : "TCP : delai depasse (aucune reponse).\n");
        return -3;
    }

    return 0;
}

int tcp_write(const u8* data, u32 length)
{
    u32 offset = 0;

    while (offset < length)
    {
        if (tcp.state != ST_ESTABLISHED)
        {
            return -1;
        }

        u16 chunk = (u16)((length - offset) > TCP_MSS ? TCP_MSS : (length - offset));
        u32 seq = tcp.snd_nxt;
        u32 expected = seq + chunk;

        tcp.snd_nxt = expected;

        int acked = 0;

        for (int attempt = 0; attempt < 4 && !acked; attempt++)
        {
            tcp_send_segment(TCP_PSH | TCP_ACK, seq, data + offset, chunk, 0);

            unsigned long start = timer_ticks();
            u64 spins = 0;

            while (timer_ticks() - start < 100 && spins < 40000000ULL)
            {
                net_poll();

                if (!seq_lt(tcp.snd_una, expected) || tcp.state != ST_ESTABLISHED)
                {
                    break;
                }

                spins++;
                asm volatile("pause");
            }

            acked = !seq_lt(tcp.snd_una, expected);
        }

        if (!acked)
        {
            return -2;
        }

        offset += chunk;
    }

    return 0;
}

/* Lit jusqu'a "max" octets. >0 = octets lus, 0 = fin de flux, -1 = delai depasse. */
int tcp_read(u8* buffer, u32 max, u32 timeout_ticks)
{
    unsigned long start = timer_ticks();
    u64 spins = 0;
    u64 limit = (u64)timeout_ticks * 400000ULL + 1000ULL;

    while (tcp.rx_length == 0 && !tcp.fin_received && tcp.state != ST_CLOSED)
    {
        net_poll();

        if (timer_ticks() - start >= timeout_ticks || ++spins > limit)
        {
            return -1;
        }

        asm volatile("pause");
    }

    if (tcp.rx_length == 0)
    {
        return 0;
    }

    u32 n = tcp.rx_length < max ? tcp.rx_length : max;
    u32 was_full_enough = (TCP_RX_SIZE - tcp.rx_length) < 4096;

    mk_memcpy(buffer, tcp.rx, n);

    for (u32 i = 0; i + n < tcp.rx_length; i++)
    {
        tcp.rx[i] = tcp.rx[i + n];
    }
    tcp.rx_length -= n;

    if (was_full_enough && tcp.state == ST_ESTABLISHED)
    {
        tcp_send_ack(); /* mise a jour de la fenetre annoncee */
    }

    return (int)n;
}

void tcp_close()
{
    if (tcp.state == ST_ESTABLISHED)
    {
        tcp_send_segment(TCP_FIN | TCP_ACK, tcp.snd_nxt, (const u8*)0, 0, 0);
        tcp.snd_nxt += 1;
        tcp.state = ST_FIN_WAIT;

        unsigned long start = timer_ticks();
        u64 spins = 0;

        while (!tcp.fin_received && timer_ticks() - start < 50 && spins < 20000000ULL)
        {
            net_poll();
            spins++;
            asm volatile("pause");
        }
    }

    tcp.state = ST_CLOSED;
}

/* ------------------------------------------------------------
   Point d'entree de la reception (appele par net.c)
   ------------------------------------------------------------ */

int tcpip_input(u8 protocol, const u8* ip, int ihl, int total_length, const u8* eth_src)
{
    (void)eth_src;

    if (protocol == PROTO_TCP)
    {
        return tcp_input(ip, ihl, total_length);
    }

    if (protocol == PROTO_UDP)
    {
        const u8* u = ip + ihl;
        int available = total_length - ihl;

        if (available < 8)
        {
            return 0;
        }

        int udp_length = (int)rd16(u + 4);

        if (udp_length < 8 || udp_length > available)
        {
            return 0;
        }

        u16 source_port = rd16(u);
        u16 dest_port = rd16(u + 2);

        if (rd16(u + 6) != 0 && l4_checksum(ip + 12, ip + 16, PROTO_UDP, u, (u32)udp_length) != 0)
        {
            return 1;
        }

        if (dhcp_waiting && dest_port == 68 && source_port == 67)
        {
            dhcp_input(u + 8, udp_length - 8);
            return 1;
        }

        if (dns_waiting && dest_port == dns_port && source_port == 53)
        {
            dns_input(u + 8, udp_length - 8);
            return 1;
        }
    }

    return 0;
}

/* ------------------------------------------------------------
   HTTP/1.1 minimal (texte clair, port 80)
   ------------------------------------------------------------ */

#define HTTP_PRINT_LIMIT 3000

int http_get(const char* host, const char* path)
{
    u8 ip[4];

    console_write("Resolution de ");
    console_write(host);
    console_write(" ...\n");

    if (dns_resolve(host, ip) != 0)
    {
        console_write("Echec : nom introuvable (DNS).\n");
        return -1;
    }

    console_write("Connexion a ");
    put_ip4(ip);
    console_write(":80 ...\n");

    if (tcp_connect(ip, 80) != 0)
    {
        return -2;
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

    if (tcp_write((const u8*)request, n) != 0)
    {
        console_write("Echec de l'envoi de la requete.\n");
        tcp_close();
        return -3;
    }

    console_write("---- reponse ----\n");

    u32 printed = 0;
    u8 chunk[200];

    while (printed < HTTP_PRINT_LIMIT)
    {
        int got = tcp_read(chunk, sizeof(chunk) - 1, 300);

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

    console_write("\n---- fin (affichage limite a ");
    put_dec(HTTP_PRINT_LIMIT);
    console_write(" octets) ----\n");

    tcp_close();
    return 0;
}
