#ifndef ERAN_HAL_PCI_H
#define ERAN_HAL_PCI_H

#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

typedef struct {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    const char *description;
} pci_device_t;

void pci_init(void);
uint32_t pci_get_device_count(void);
const pci_device_t *pci_get_device(uint32_t index);
void pci_print_devices(void);

#endif /* ERAN_HAL_PCI_H */
