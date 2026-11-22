#ifndef MIKEA_RTL8139_H
#define MIKEA_RTL8139_H

#include "../../../include/types.h"
#include "../pci/pci.h"

/*
    Pilote Ethernet Realtek RTL8139 (10EC:8139) : tres repandu, emule
    par QEMU ("-nic user,model=rtl8139"), VirtualBox, Bochs et de
    nombreuses cartes PCI anciennes. Acces par ports d'E/S, mode polling.
*/

int rtl8139_init(pci_device_t* device);
int rtl8139_present();
int rtl8139_link_up();
const u8* rtl8139_mac();
int rtl8139_send(const u8* frame, u16 length);
int rtl8139_recv(u8* buffer, u16 max_length);

#endif
