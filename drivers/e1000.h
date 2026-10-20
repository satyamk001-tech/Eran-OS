#ifndef ERAN_DRIVERS_E1000_H
#define ERAN_DRIVERS_E1000_H

#include <stdint.h>
#include <stddef.h>

#define E1000_VENDOR_ID  0x8086
#define E1000_DEVICE_ID  0x100E   // Intel 82540EM (standard QEMU NIC)

typedef struct {
    uint8_t mac[6];
    int detected;
    uint32_t io_base;
    uint32_t mem_base;
    uint32_t rx_packets;
    uint32_t tx_packets;
    uint32_t rx_bytes;
    uint32_t tx_bytes;
} e1000_device_t;

void e1000_init(void);
int  e1000_is_detected(void);
const uint8_t *e1000_get_mac(void);
void e1000_send_packet(const void *data, uint16_t length);
int  e1000_receive_packet(void *buffer, uint16_t max_len);
void e1000_print_status(void);

#endif /* ERAN_DRIVERS_E1000_H */
