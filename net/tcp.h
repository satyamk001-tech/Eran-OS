#ifndef ERAN_NET_TCP_H
#define ERAN_NET_TCP_H

#include <stdint.h>
#include <stddef.h>

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

typedef enum {
    TCP_STATE_CLOSED = 0,
    TCP_STATE_SYN_SENT,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT
} tcp_state_t;

typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset_reserved;
    uint8_t  flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed)) tcp_header_t;

typedef struct {
    int id;
    tcp_state_t state;
    uint32_t local_ip;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;
    uint32_t seq_num;
    uint32_t ack_num;
    int is_connected;
} tcp_socket_t;

void tcp_init(void);
int  tcp_socket(void);
int  tcp_connect(int sock_id, const char *ip_str, uint16_t port);
int  tcp_send(int sock_id, const void *data, size_t len);
int  tcp_recv(int sock_id, void *buf, size_t max_len);
void tcp_close(int sock_id);
void tcp_print_status(void);

void tcp_cmd_telnet(const char *ip_str, uint16_t port);
void tcp_cmd_curl(const char *ip_str);

#endif /* ERAN_NET_TCP_H */
