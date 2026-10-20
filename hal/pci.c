#include "pci.h"
#include "io.h"
#include "../drivers/vga.h"

#define MAX_PCI_DEVICES 32
static pci_device_t pci_devices[MAX_PCI_DEVICES];
static uint32_t pci_device_count = 0;

static uint32_t pci_read_config_dword(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    uint32_t address = ((uint32_t)1 << 31)
                     | ((uint32_t)bus << 16)
                     | ((uint32_t)device << 11)
                     | ((uint32_t)func << 8)
                     | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static uint16_t pci_read_config_word(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    uint32_t dword = pci_read_config_dword(bus, device, func, offset);
    return (uint16_t)((dword >> ((offset & 2) * 8)) & 0xFFFF);
}

static const char *pci_class_to_string(uint8_t class_code, uint8_t subclass) {
    switch (class_code) {
        case 0x01:
            if (subclass == 0x01) return "IDE Controller";
            if (subclass == 0x06) return "SATA AHCI Controller";
            return "Mass Storage Controller";
        case 0x02:
            if (subclass == 0x00) return "Ethernet Controller";
            return "Network Controller";
        case 0x03:
            if (subclass == 0x00) return "VGA Compatible Controller";
            return "Display Controller";
        case 0x04:
            if (subclass == 0x01) return "Audio Device (AC97)";
            if (subclass == 0x03) return "Intel HD Audio Controller";
            return "Multimedia Controller";
        case 0x06:
            if (subclass == 0x00) return "Host / PCI Bridge";
            if (subclass == 0x01) return "ISA Bridge";
            if (subclass == 0x04) return "PCI-to-PCI Bridge";
            return "Bridge Device";
        case 0x0C:
            if (subclass == 0x03) return "USB Host Controller";
            return "Serial Bus Controller";
        default:
            return "Generic Peripheral";
    }
}

void pci_init(void) {
    pci_device_count = 0;

    for (uint16_t bus = 0; bus < 8; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            uint16_t vendor = pci_read_config_word((uint8_t)bus, dev, 0, 0x00);
            if (vendor == 0xFFFF || vendor == 0x0000) {
                continue; // Device doesn't exist
            }

            uint16_t device_id = pci_read_config_word((uint8_t)bus, dev, 0, 0x02);
            uint16_t class_rev = pci_read_config_word((uint8_t)bus, dev, 0, 0x0A);
            uint8_t class_code = (uint8_t)((class_rev >> 8) & 0xFF);
            uint8_t subclass   = (uint8_t)(class_rev & 0xFF);

            if (pci_device_count < MAX_PCI_DEVICES) {
                pci_devices[pci_device_count].bus = (uint8_t)bus;
                pci_devices[pci_device_count].device = dev;
                pci_devices[pci_device_count].function = 0;
                pci_devices[pci_device_count].vendor_id = vendor;
                pci_devices[pci_device_count].device_id = device_id;
                pci_devices[pci_device_count].class_code = class_code;
                pci_devices[pci_device_count].subclass = subclass;
                pci_devices[pci_device_count].description = pci_class_to_string(class_code, subclass);
                pci_device_count++;
            }
        }
    }
}

uint32_t pci_get_device_count(void) {
    return pci_device_count;
}

const pci_device_t *pci_get_device(uint32_t index) {
    if (index >= pci_device_count) return 0;
    return &pci_devices[index];
}

void pci_print_devices(void) {
    kprintf("===================================================================\n");
    kprintf(" BUS:DEV.FN  VENDOR:DEVICE  CLASS:SUB  DESCRIPTION                 \n");
    kprintf("===================================================================\n");
    for (uint32_t i = 0; i < pci_device_count; i++) {
        pci_device_t *d = &pci_devices[i];
        kprintf(" %d:%d.%d     %x:%x      %x:%x      %s\n",
                d->bus, d->device, d->function,
                d->vendor_id, d->device_id,
                d->class_code, d->subclass,
                d->description);
    }
    kprintf("Total hardware devices discovered on PCI bus: %d\n", pci_device_count);
}
