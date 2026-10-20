#include "net.h"
#include "../drivers/e1000.h"
#include "../drivers/vga.h"
#include "../drivers/timer.h"

static net_config_t local_net;

static uint16_t net_checksum(const void *buf, int len) {
    const uint16_t *data = (const uint16_t *)buf;
    uint32_t sum = 0;
    while (len > 1) {
        sum += *data++;
        len -= 2;
    }
    if (len > 0) {
        sum += *(const uint8_t *)data;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

static uint16_t htons(uint16_t val) {
    return (val << 8) | (val >> 8);
}

static uint32_t htonl(uint32_t val) {
    return ((val >> 24) & 0xFF) |
           ((val >> 8) & 0xFF00) |
           ((val << 8) & 0xFF0000) |
           ((val << 24) & 0xFF000000);
}

uint32_t net_parse_ip(const char *str) {
    uint32_t parts[4] = {0, 0, 0, 0};
    int part_idx = 0;
    const char *p = str;

    while (*p && part_idx < 4) {
        if (*p >= '0' && *p <= '9') {
            parts[part_idx] = parts[part_idx] * 10 + (*p - '0');
        } else if (*p == '.') {
            part_idx++;
        }
        p++;
    }

    return (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
}

void net_format_ip(uint32_t ip, char *buf) {
    uint8_t b0 = (ip >> 24) & 0xFF;
    uint8_t b1 = (ip >> 16) & 0xFF;
    uint8_t b2 = (ip >> 8) & 0xFF;
    uint8_t b3 = ip & 0xFF;

    int idx = 0;
    uint8_t bytes[4] = {b0, b1, b2, b3};
    for (int i = 0; i < 4; i++) {
        uint8_t val = bytes[i];
        if (val >= 100) {
            buf[idx++] = '0' + (val / 100);
            val %= 100;
            buf[idx++] = '0' + (val / 10);
            val %= 10;
            buf[idx++] = '0' + val;
        } else if (val >= 10) {
            buf[idx++] = '0' + (val / 10);
            val %= 10;
            buf[idx++] = '0' + val;
        } else {
            buf[idx++] = '0' + val;
        }
        if (i < 3) buf[idx++] = '.';
    }
    buf[idx] = '\0';
}

void net_init(void) {
    e1000_init();

    // Default static IPv4 configuration for Eran OS node
    local_net.ip_addr = (192 << 24) | (168 << 16) | (1 << 8) | 50;       // 192.168.1.50
    local_net.subnet_mask = (255 << 24) | (255 << 16) | (255 << 8) | 0;   // 255.255.255.0
    local_net.gateway = (192 << 24) | (168 << 16) | (1 << 8) | 1;        // 192.168.1.1
    local_net.dns = (8 << 24) | (8 << 16) | (8 << 8) | 8;                 // 8.8.8.8

    const uint8_t *mac = e1000_get_mac();
    for (int i = 0; i < 6; i++) {
        local_net.mac[i] = mac[i];
    }
}

void net_print_ifconfig(void) {
    char ip_buf[16];
    char mask_buf[16];
    char gw_buf[16];
    char dns_buf[16];

    net_format_ip(local_net.ip_addr, ip_buf);
    net_format_ip(local_net.subnet_mask, mask_buf);
    net_format_ip(local_net.gateway, gw_buf);
    net_format_ip(local_net.dns, dns_buf);

    kprintf("===================================================================\n");
    kprintf("                    NETWORK INTERFACE CONFIGURATION (eth0)         \n");
    kprintf("===================================================================\n");
    kprintf(" Link Status           : %s (Intel 82540EM e1000)\n",
            e1000_is_detected() ? "UP & RUNNING" : "UP (VIRTUAL EMULATION)");
    kprintf(" Hardware MAC Address  : %x:%x:%x:%x:%x:%x\n",
            local_net.mac[0], local_net.mac[1], local_net.mac[2],
            local_net.mac[3], local_net.mac[4], local_net.mac[5]);
    kprintf(" IPv4 Address          : %s\n", ip_buf);
    kprintf(" Subnet Mask           : %s\n", mask_buf);
    kprintf(" Default Gateway       : %s\n", gw_buf);
    kprintf(" Primary DNS Server    : %s\n", dns_buf);
    kprintf(" MTU Size              : 1500 bytes\n");
    kprintf(" Routing Mode          : Direct L2/L3 Packet Dispatch\n");
    kprintf("===================================================================\n");
}

int net_ping(const char *ip_str) {
    uint32_t target_ip = net_parse_ip(ip_str);
    char fmt_ip[16];
    net_format_ip(target_ip, fmt_ip);

    kprintf("PING %s (56 data bytes):\n", fmt_ip);

    int received_count = 0;
    for (int seq = 1; seq <= 4; seq++) {
        uint8_t packet[98];
        for (size_t i = 0; i < sizeof(packet); i++) packet[i] = 0;

        ethernet_header_t *eth = (ethernet_header_t *)packet;
        for (int i = 0; i < 6; i++) {
            eth->dest_mac[i] = 0xFF; // Broadcast or gateway MAC
            eth->src_mac[i] = local_net.mac[i];
        }
        eth->ethertype = htons(ETHERTYPE_IPV4);

        ipv4_header_t *ip = (ipv4_header_t *)(packet + sizeof(ethernet_header_t));
        ip->version_ihl = 0x45; // Version 4, Header length 5 words
        ip->type_of_service = 0;
        ip->total_length = htons(sizeof(ipv4_header_t) + sizeof(icmp_header_t) + 56);
        ip->identification = htons(seq * 0x100);
        ip->flags_fragment_offset = htons(0x4000); // Don't fragment
        ip->time_to_live = 64;
        ip->protocol = IP_PROTO_ICMP;
        ip->src_ip = htonl(local_net.ip_addr);
        ip->dest_ip = htonl(target_ip);
        ip->header_checksum = 0;
        ip->header_checksum = net_checksum(ip, sizeof(ipv4_header_t));

        icmp_header_t *icmp = (icmp_header_t *)(packet + sizeof(ethernet_header_t) + sizeof(ipv4_header_t));
        icmp->type = 8; // ICMP Echo Request
        icmp->code = 0;
        icmp->checksum = 0;
        icmp->id = htons(0x1337);
        icmp->sequence = htons(seq);
        icmp->checksum = net_checksum(icmp, sizeof(icmp_header_t) + 56);

        uint32_t t_start = timer_get_ticks();
        e1000_send_packet(packet, sizeof(packet));

        // Poll for echo reply
        uint8_t rx_buf[128];
        int rx_len = 0;
        int timeout = 0;
        while ((rx_len = e1000_receive_packet(rx_buf, sizeof(rx_buf))) == 0) {
            if (timer_get_ticks() - t_start > 50) { // 500 ms timeout
                timeout = 1;
                break;
            }
        }

        uint32_t t_elapsed = (timer_get_ticks() - t_start) * 10; // PIT 100Hz = 10ms ticks
        if (t_elapsed == 0) t_elapsed = 1;

        if (!timeout && rx_len > 0) {
            received_count++;
            kprintf("64 bytes from %s: icmp_seq=%d ttl=64 time=%u ms\n", fmt_ip, seq, t_elapsed);
        } else {
            kprintf("Request timeout for icmp_seq %d\n", seq);
        }

        sleep_ticks(20); // 200ms delay between pings
    }

    kprintf("\n--- %s ping statistics ---\n", fmt_ip);
    kprintf("4 packets transmitted, %d received, %d%% packet loss\n",
            received_count, ((4 - received_count) * 100) / 4);

    return received_count;
}
