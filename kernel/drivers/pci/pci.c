#include "pci.h"
#include "../../cpu/io.h"
#include "../../console/console.h"

static pci_device_t g_pci_devices[MAX_PCI_DEVICES];
static u32 g_pci_device_count = 0;

u32 pci_read_config(u8 bus, u8 slot, u8 func, u8 offset)
{
    u32 address = (u32)((1U << 31) |
                  ((u32)bus << 16) |
                  ((u32)slot << 11) |
                  ((u32)func << 8) |
                  (offset & 0xFC));

    outl(PCI_CONFIG_ADDRESS, address);

    return inl(PCI_CONFIG_DATA);
}

static void pci_check_function(u8 bus, u8 slot, u8 func)
{
    u32 vendor_device = pci_read_config(bus, slot, func, PCI_OFFSET_VENDOR_DEVICE);
    u16 vendor_id = (u16)(vendor_device & 0xFFFF);
    u16 device_id = (u16)(vendor_device >> 16);

    if(vendor_id == 0xFFFF) return;

    u32 class_rev = pci_read_config(bus, slot, func, PCI_OFFSET_CLASS_REVISION);

    u8 revision = (u8)(class_rev & 0xFF);
    u8 prog_if = (u8)((class_rev >> 8) & 0xFF);
    u8 subclass = (u8)((class_rev >> 16) & 0xFF);
    u8 class_code = (u8)((class_rev >> 24) & 0xFF);

    if(g_pci_device_count < MAX_PCI_DEVICES)
    {
        pci_device_t *dev = &g_pci_devices[g_pci_device_count++];
        dev -> bus = bus;
        dev -> slot = slot;
        dev -> func = func;
        dev -> vendor_id = vendor_id;
        dev -> device_id = device_id;
        dev -> class_code = class_code;
        dev -> subclass = subclass;
        dev -> prog_if = prog_if;
        dev -> revision = revision;
    }
}


void pci_scan_device(u8 bus, u8 slot)
{
    u32 vendor_device = pci_read_config(bus, slot, 0, PCI_OFFSET_VENDOR_DEVICE);
    u16 vendor_id = (u16)(vendor_device & 0xFFFF);

    if(vendor_id == 0xFFFF) return;

    pci_check_function(bus, slot, 0);

    u32 header_type_reg = pci_read_config(bus, slot, 0, PCI_OFFSET_HEADER_TYPE);
    u32 header_type = (u8)((header_type_reg >> 16) & 0xFF);

    if(header_type & 0x80)
    {
        for(u8 func = 1; func < 8; func++)
        {
            u32 dev_check = pci_read_config(bus, slot, func, PCI_OFFSET_VENDOR_DEVICE);
            if ((u16)(dev_check & 0xFFFF) != 0xFFFF)
            {
                pci_check_function(bus, slot, func);
            }
        }
    }
}

void pci_scan_bus(u8 bus)
{
    for(u8 slot = 0; slot < 32; slot++)
    {
        pci_scan_device(bus, slot);
    }
}

void pci_init(void)
{
    console_write("[PCI] Verification et enumeration du bus PCI...\n");
    pci_scan_bus(0);
}

pci_device_t* pci_get_devices(u32* count)
{
    if(count)
    {
        *count = g_pci_device_count;
    }
    return g_pci_devices;;
}

static const char* pci_class_to_string(u8 class_code, u8 subclass)
{
    switch(class_code) {
        case 0x01:
            switch(subclass) {
                case 0x01: return "IDE Controller";
                case 0x06: return "SATA Controller";
                default:   return "Mass Storage";
            }

        case 0x02:
            switch(subclass) {
                case 0x00: return "Ethernet Controller";
                default: return "Network Controller";
            }

        case 0x03:
            switch(subclass) {
                case 0x00: return "VGA Controller";
                default:   return "Display Controller";
            }

        case 0x06:
            switch(subclass) {
                case 0x00: return "Host Bridge";
                case 0x01: return "ISA Bridge";
                case 0x04: return "PCI Bridge";
                default:   return "Bridge Device";
            }

        case 0x07:
            return "Serial/Communication";

        case 0x0C:
            return "USB/Serial Bus";

        default:
            return "Unknown Device";
    }
}

void pci_list_devices(void) {
    console_write("Bus:Slot.Func | Vendor:Device | Description\n");
    console_write("----------------------------------------------\n");
    for (u32 i = 0; i < g_pci_device_count; i++) {
        pci_device_t *dev = &g_pci_devices[i];

        // Format: Bus:Slot.Func
        console_write_dec(dev->bus);
        console_write(":");
        console_write_dec(dev->slot);
        console_write(".");
        console_write_dec(dev->func);
        console_write(" | ");

        // Format: Vendor:Device (hex)
        console_write_hex(dev->vendor_id);
        console_write(":");
        console_write_hex(dev->device_id);
        console_write(" | ");

        // Description
        console_write(pci_class_to_string(dev->class_code, dev->subclass));
        console_write("\n");
    }
}
