#ifndef ERAN_DRIVERS_USB_H
#define ERAN_DRIVERS_USB_H

#include <stdint.h>
#include <stddef.h>

#define USB_CLASS_HID   0x03
#define USB_CLASS_HUB   0x09
#define USB_CLASS_MASS  0x08

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed)) usb_device_descriptor_t;

typedef struct {
    uint8_t  address;
    uint8_t  speed;
    uint8_t  class_code;
    uint16_t vendor_id;
    uint16_t product_id;
    const char *name;
    int is_connected;
} usb_device_t;

void usb_init(void);
int  usb_controller_present(void);
void usb_print_status(void);

#endif /* ERAN_DRIVERS_USB_H */
