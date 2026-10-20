#ifndef ERAN_NET_NET_H
#define ERAN_NET_NET_H

#include <stdint.h>
#include <stddef.h>

#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_ARP  0x0806

#define IP_PROTO_ICMP  1
#define IP_PROTO_TCP   6
#define IP_PROTO_UDP   17

typedef struct {
    uint8_t  dest_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;
} __attribute__((packed)) ethernet_header_t;

typedef struct {
    uint8_t  version_ihl;
    uint8_t  type_of_service;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment_offset;
    uint8_t  time_to_live;
    uint8_t  protocol;
    uint16_t header_checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} __attribute__((packed)) ipv4_header_t;

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} __attribute__((packed)) icmp_header_t;

typedef struct {
    uint32_t ip_addr;
    uint32_t subnet_mask;
    uint32_t gateway;
    uint32_t dns;
    uint8_t  mac[6];
} net_config_t;

void net_init(void);
void net_print_ifconfig(void);
int  net_ping(const char *ip_str);
uint32_t net_parse_ip(const char *str);
void net_format_ip(uint32_t ip, char *buf);

#endif /* ERAN_NET_NET_H */
