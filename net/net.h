#ifndef MIKEA_NET_H
#define MIKEA_NET_H

#include "../include/types.h"

/*
    Pile reseau minimale et SECURISEE par defaut :
    Ethernet + ARP + IPv4 + ICMP (ping), par-dessus le pilote e1000.

    Politique de securite :
      - trafic entrant non sollicite : REFUSE (aucun port ouvert) ;
      - mode furtif actif : les "ping" entrants sont ignores ;
      - une entree ARP existante n'est JAMAIS ecrasee silencieusement
        (detection d'usurpation ARP / ARP spoofing) ;
      - tout evenement suspect est journalise (commande "seclog").
*/

/* Configuration IPv4 courante (DHCP peut la modifier). */
extern u8 net_ip[4];
extern u8 net_mask[4];
extern u8 net_gateway[4];
extern u8 net_dns[4];

/* Services pour la pile haut niveau (net/tcpip.c). */
u16 net_checksum(const u8* data, u32 length);
const u8* net_local_mac();
int net_send_frame(const u8* frame, u16 length);
void net_send_ip_packet(const u8* dst_ip, u8 protocol, const u8* payload, u16 payload_length, const u8* dst_mac);
int net_next_hop_mac(const u8* destination, u8* mac_out);

/* Fournis par net/tcpip.c : renvoie 1 si le paquet a ete consomme. */
int tcpip_input(u8 protocol, const u8* ip, int ihl, int total_length, const u8* eth_src);
int tcpip_accepts_any_ip();

/* Services internet (net/tcpip.c). */
int dhcp_run();
int dns_resolve(const char* name, u8 out[4]);
int tcp_connect(const u8 ip[4], u16 port);
int tcp_write(const u8* data, u32 length);
int tcp_read(u8* buffer, u32 max, u32 timeout_ticks);
void tcp_close();
int http_get(const char* host, const char* path);
int https_get(const char* host, const char* path, const char* pin_text);

void net_init();

/* Traite les trames recues (appeler regulierement). */
void net_poll();

int net_is_up();

/* Journal de securite (ring buffer en memoire). */
void sec_log(const char* message);

void net_print_status();
void net_print_arp();
void net_print_firewall();
void net_print_seclog();
void net_set_stealth(int enabled);
void net_arp_flush();

/* Renvoie 0 si ok ; ecrit le resultat dans la console. */
int net_ping(const u8 destination[4]);

/* Analyse "a.b.c.d". Renvoie 1 si valide. */
int net_parse_ip(const char* text, u8 out[4]);

/* Commandes shell reseau/materiel/IA. Renvoie 1 si la ligne a ete traitee. */
int mikea_extra_command(char* line);

#endif
