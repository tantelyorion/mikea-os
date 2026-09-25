#ifndef MIKEA_PCI_H
#define MIKEA_PCI_H

#include "../../../include/types.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define PCI_OFFSET_VENDOR_DEVICE 0x00
#define PCI_OFFSET_CLASS_REVISION 0x08
#define PCI_OFFSET_HEADER_TYPE 0x0E

#define MAX_PCI_DEVICES 32

typedef struct {
    u8 bus;
    u8 slot;
    u8 func;
    u16 vendor_id;
    u16 device_id;
    u8 class_code;
    u8 subclass;
    u8 prog_if;
    u8 revision;
} pci_device_t;

u32 pci_read_config(u8 bus, u8 slot, u8 func, u8 offset);
void pci_scan_device(u8 bus, u8 slot);
void pci_scan_bus(u8 bus);
void pci_init(void);

void pci_list_devices(void);
pci_device_t* pci_get_devices(u32* count);

#endif
