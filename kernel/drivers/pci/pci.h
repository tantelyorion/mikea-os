#ifndef MIKEA_PCI_H
#define MIKEA_PCI_H

#include "../../../include/types.h"

/*
    Pilote PCI reel (remplace l'ancien stub vide) : enumeration du
    bus par les ports de configuration 0xCF8/0xCFC, lecture des
    BAR, activation du bus-mastering. Base de tous les pilotes
    materiels (reseau, stockage, audio...).
*/

#define PCI_MAX_DEVICES 64

typedef struct
{
    u8  bus;
    u8  dev;
    u8  fn;
    u16 vendor;
    u16 device;
    u8  class_code;
    u8  subclass;
    u8  prog_if;
    u8  irq;
    u32 bar[6];
} pci_device_t;

void pci_init();

int pci_device_count();
const pci_device_t* pci_device_at(int index);

/* Premier peripherique de cette classe/sous-classe, ou 0. */
pci_device_t* pci_find_class(u8 class_code, u8 subclass);

/* Premier peripherique de ce vendor/device, ou 0. */
pci_device_t* pci_find_id(u16 vendor, u16 device);

u32 pci_config_read32(u8 bus, u8 dev, u8 fn, u8 offset);
void pci_config_write32(u8 bus, u8 dev, u8 fn, u8 offset, u32 value);

/* Active acces memoire + E/S + bus-mastering (DMA). */
void pci_enable_busmaster(const pci_device_t* d);

const char* pci_class_name(u8 class_code, u8 subclass);

#endif
