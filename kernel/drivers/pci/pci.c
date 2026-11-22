#include "pci.h"
#include "../../cpu/io.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

/* Buses 0..31 suffisent largement pour QEMU et la plupart des PC. */
#define PCI_SCAN_BUSES 32

static pci_device_t devices[PCI_MAX_DEVICES];
static int device_count = 0;

u32 pci_config_read32(u8 bus, u8 dev, u8 fn, u8 offset)
{
    u32 address = 0x80000000u
                | ((u32)bus << 16)
                | ((u32)dev << 11)
                | ((u32)fn << 8)
                | ((u32)offset & 0xFCu);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write32(u8 bus, u8 dev, u8 fn, u8 offset, u32 value)
{
    u32 address = 0x80000000u
                | ((u32)bus << 16)
                | ((u32)dev << 11)
                | ((u32)fn << 8)
                | ((u32)offset & 0xFCu);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

static void pci_probe_function(u8 bus, u8 dev, u8 fn)
{
    u32 id = pci_config_read32(bus, dev, fn, 0x00);

    if ((id & 0xFFFF) == 0xFFFF)
    {
        return;
    }

    if (device_count >= PCI_MAX_DEVICES)
    {
        return;
    }

    pci_device_t* d = &devices[device_count];

    d->bus = bus;
    d->dev = dev;
    d->fn = fn;
    d->vendor = (u16)(id & 0xFFFF);
    d->device = (u16)(id >> 16);

    u32 class_reg = pci_config_read32(bus, dev, fn, 0x08);
    d->class_code = (u8)(class_reg >> 24);
    d->subclass = (u8)(class_reg >> 16);
    d->prog_if = (u8)(class_reg >> 8);

    u32 header = pci_config_read32(bus, dev, fn, 0x0C);
    u8 header_type = (u8)((header >> 16) & 0x7F);

    for (int i = 0; i < 6; i++)
    {
        d->bar[i] = (header_type == 0)
            ? pci_config_read32(bus, dev, fn, (u8)(0x10 + i * 4))
            : 0;
    }

    d->irq = (u8)(pci_config_read32(bus, dev, fn, 0x3C) & 0xFF);

    device_count++;
}

void pci_init()
{
    device_count = 0;

    for (int bus = 0; bus < PCI_SCAN_BUSES; bus++)
    {
        for (int dev = 0; dev < 32; dev++)
        {
            u32 id = pci_config_read32((u8)bus, (u8)dev, 0, 0x00);

            if ((id & 0xFFFF) == 0xFFFF)
            {
                continue;
            }

            pci_probe_function((u8)bus, (u8)dev, 0);

            u32 header = pci_config_read32((u8)bus, (u8)dev, 0, 0x0C);

            if (header & 0x00800000u)
            {
                for (int fn = 1; fn < 8; fn++)
                {
                    pci_probe_function((u8)bus, (u8)dev, (u8)fn);
                }
            }
        }
    }
}

int pci_device_count()
{
    return device_count;
}

const pci_device_t* pci_device_at(int index)
{
    if (index < 0 || index >= device_count)
    {
        return (const pci_device_t*)0;
    }
    return &devices[index];
}

pci_device_t* pci_find_class(u8 class_code, u8 subclass)
{
    for (int i = 0; i < device_count; i++)
    {
        if (devices[i].class_code == class_code && devices[i].subclass == subclass)
        {
            return &devices[i];
        }
    }
    return (pci_device_t*)0;
}

pci_device_t* pci_find_id(u16 vendor, u16 device)
{
    for (int i = 0; i < device_count; i++)
    {
        if (devices[i].vendor == vendor && devices[i].device == device)
        {
            return &devices[i];
        }
    }
    return (pci_device_t*)0;
}

void pci_enable_busmaster(const pci_device_t* d)
{
    /*
        On ecrit uniquement le registre de commande (16 bits bas) :
        les 16 bits hauts sont le registre d'etat, dont certains bits
        s'effacent en y ecrivant 1 -- on les laisse donc a 0.
    */
    u32 command = pci_config_read32(d->bus, d->dev, d->fn, 0x04) & 0xFFFFu;
    command |= 0x0007u;
    pci_config_write32(d->bus, d->dev, d->fn, 0x04, command);
}

const char* pci_class_name(u8 class_code, u8 subclass)
{
    switch (class_code)
    {
        case 0x00: return "Ancien peripherique";
        case 0x01:
            if (subclass == 0x01) { return "Controleur IDE"; }
            if (subclass == 0x06) { return "Controleur SATA"; }
            if (subclass == 0x08) { return "Controleur NVMe"; }
            return "Stockage";
        case 0x02:
            if (subclass == 0x00) { return "Ethernet"; }
            if (subclass == 0x80) { return "Reseau (WiFi/autre)"; }
            return "Reseau";
        case 0x03: return "Affichage (VGA)";
        case 0x04:
            if (subclass == 0x01) { return "Audio"; }
            return "Multimedia";
        case 0x05: return "Memoire";
        case 0x06:
            if (subclass == 0x00) { return "Pont hote"; }
            if (subclass == 0x01) { return "Pont ISA"; }
            return "Pont PCI";
        case 0x07: return "Communication";
        case 0x08: return "Peripherique systeme";
        case 0x0C:
            if (subclass == 0x03) { return "Controleur USB"; }
            return "Bus serie";
        case 0x0D: return "Sans fil (RF/Bluetooth)";
        default: return "Autre";
    }
}
