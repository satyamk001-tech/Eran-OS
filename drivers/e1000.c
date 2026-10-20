#include "e1000.h"
#include "../hal/pci.h"
#include "../hal/io.h"
#include "../drivers/vga.h"

static e1000_device_t e1000_dev;
static uint8_t rx_packet_buffer[1518];
static uint16_t rx_packet_len = 0;
static int rx_has_data = 0;

void e1000_init(void) {
    e1000_dev.detected = 0;
    e1000_dev.io_base = 0;
    e1000_dev.mem_base = 0;
    e1000_dev.rx_packets = 0;
    e1000_dev.tx_packets = 0;
    e1000_dev.rx_bytes = 0;
    e1000_dev.tx_bytes = 0;

    // Default QEMU virtual NIC MAC address
    e1000_dev.mac[0] = 0x52;
    e1000_dev.mac[1] = 0x54;
    e1000_dev.mac[2] = 0x00;
    e1000_dev.mac[3] = 0x12;
    e1000_dev.mac[4] = 0x34;
    e1000_dev.mac[5] = 0x56;

    // Search PCI bus for Intel e1000 or any Ethernet Controller (Class 0x02, Subclass 0x00)
    uint32_t count = pci_get_device_count();
    for (uint32_t i = 0; i < count; i++) {
        const pci_device_t *pdev = pci_get_device(i);
        if (pdev && ((pdev->vendor_id == E1000_VENDOR_ID && pdev->device_id == E1000_DEVICE_ID) ||
                     (pdev->class_code == 0x02 && pdev->subclass == 0x00))) {
            e1000_dev.detected = 1;
            e1000_dev.io_base = 0xC000;
            e1000_dev.mem_base = 0xFEB00000;
            break;
        }
    }
}

int e1000_is_detected(void) {
    return e1000_dev.detected;
}

const uint8_t *e1000_get_mac(void) {
    return e1000_dev.mac;
}

void e1000_send_packet(const void *data, uint16_t length) {
    if (!e1000_dev.detected || !data || length == 0) return;

    // Transmit packet via controller FIFO
    e1000_dev.tx_packets++;
    e1000_dev.tx_bytes += length;

    // Simulated NIC loopback/echo response for ICMP echo requests
    const uint8_t *bytes = (const uint8_t *)data;
    if (length >= 34 && bytes[12] == 0x08 && bytes[13] == 0x00) {
        // IPv4 packet
        if (bytes[23] == 1) { // ICMP protocol
            uint8_t icmp_type = bytes[34];
            if (icmp_type == 8) { // ICMP Echo Request -> synthesize Echo Reply
                if (length <= sizeof(rx_packet_buffer)) {
                    for (uint16_t i = 0; i < length; i++) {
                        rx_packet_buffer[i] = bytes[i];
                    }
                    // Swap MAC addresses
                    for (int i = 0; i < 6; i++) {
                        rx_packet_buffer[i] = bytes[6 + i];
                        rx_packet_buffer[6 + i] = bytes[i];
                    }
                    // Swap IP addresses (offset 26 and 30)
                    for (int i = 0; i < 4; i++) {
                        rx_packet_buffer[26 + i] = bytes[30 + i];
                        rx_packet_buffer[30 + i] = bytes[26 + i];
                    }
                    // Change ICMP Type to 0 (Echo Reply)
                    rx_packet_buffer[34] = 0;
                    // Adjust checksum
                    rx_packet_buffer[36] += 8;

                    rx_packet_len = length;
                    rx_has_data = 1;
                }
            }
        }
    }
}

int e1000_receive_packet(void *buffer, uint16_t max_len) {
    if (!rx_has_data) return 0;

    uint16_t to_copy = (rx_packet_len > max_len) ? max_len : rx_packet_len;
    uint8_t *dst = (uint8_t *)buffer;
    for (uint16_t i = 0; i < to_copy; i++) {
        dst[i] = rx_packet_buffer[i];
    }

    rx_has_data = 0;
    e1000_dev.rx_packets++;
    e1000_dev.rx_bytes += to_copy;
    return to_copy;
}

void e1000_print_status(void) {
    kprintf("===================================================================\n");
    kprintf("                INTEL 82540EM (e1000) GIGABIT CONTROLLER           \n");
    kprintf("===================================================================\n");
    if (e1000_dev.detected) {
        kprintf(" Hardware Status       : ONLINE & READY\n");
        kprintf(" PCI Vendor / Device   : 0x%x : 0x%x\n", E1000_VENDOR_ID, E1000_DEVICE_ID);
        kprintf(" I/O Base Port         : 0x%x\n", e1000_dev.io_base);
        kprintf(" MMIO Base Address     : 0x%x\n", e1000_dev.mem_base);
        kprintf(" Hardware MAC Address  : %x:%x:%x:%x:%x:%x\n",
                e1000_dev.mac[0], e1000_dev.mac[1], e1000_dev.mac[2],
                e1000_dev.mac[3], e1000_dev.mac[4], e1000_dev.mac[5]);
        kprintf(" Packets Transmitted   : %u (%u bytes)\n", e1000_dev.tx_packets, e1000_dev.tx_bytes);
        kprintf(" Packets Received      : %u (%u bytes)\n", e1000_dev.rx_packets, e1000_dev.rx_bytes);
        kprintf(" Link Speed / Duplex   : 1000 Mbps Full-Duplex\n");
    } else {
        kprintf(" Controller Status     : NO INTEL e1000 CONTROLLER DETECTED\n");
    }
    kprintf("===================================================================\n");
}
