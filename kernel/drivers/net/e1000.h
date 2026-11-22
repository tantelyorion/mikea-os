#ifndef MIKEA_E1000_H
#define MIKEA_E1000_H

#include "../../../include/types.h"
#include "../pci/pci.h"

/*
    Pilote Ethernet Intel e1000 (82540EM) -- la carte reseau
    emulee PAR DEFAUT par QEMU ("-nic model=e1000"), aussi presente
    sur VirtualBox et VMware. Mode "polling" (pas d'IRQ) : simple,
    robuste, sans risque de blocage d'interruption.
*/

#define E1000_MAX_FRAME 1518

/* Renvoie 0 si la carte est prete, un code negatif sinon. */
int e1000_init(pci_device_t* device);

int e1000_present();
int e1000_link_up();
const u8* e1000_mac();

/* Envoie une trame Ethernet complete (en-tete inclus). 0 = ok. */
int e1000_send(const u8* frame, u16 length);

/* Recupere une trame si disponible. Renvoie sa taille, 0 si aucune. */
int e1000_recv(u8* buffer, u16 max_length);

#endif
