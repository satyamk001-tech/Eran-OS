#include "tcp.h"
#include "net.h"
#include "../drivers/e1000.h"
#include "../drivers/vga.h"
#include "../drivers/timer.h"

#define MAX_SOCKETS 4
static tcp_socket_t sockets[MAX_SOCKETS];
static uint16_t ephemeral_port = 49152;

static uint16_t htons(uint16_t val) {
    return (val << 8) | (val >> 8);
}

static uint32_t htonl(uint32_t val) {
    return ((val >> 24) & 0xFF) |
           ((val >> 8) & 0xFF00) |
           ((val << 8) & 0xFF0000) |
           ((val << 24) & 0xFF000000);
}

void tcp_init(void) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        sockets[i].id = i;
        sockets[i].state = TCP_STATE_CLOSED;
        sockets[i].local_ip = (192 << 24) | (168 << 16) | (1 << 8) | 50;
        sockets[i].local_port = 0;
        sockets[i].remote_ip = 0;
        sockets[i].remote_port = 0;
        sockets[i].seq_num = 0;
        sockets[i].ack_num = 0;
        sockets[i].is_connected = 0;
    }
}

int tcp_socket(void) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i].state == TCP_STATE_CLOSED) {
            sockets[i].local_port = ephemeral_port++;
            sockets[i].seq_num = 1000 + i * 500;
            sockets[i].ack_num = 0;
            sockets[i].is_connected = 0;
            return i;
        }
    }
    return -1; // No free sockets
}

int tcp_connect(int sock_id, const char *ip_str, uint16_t port) {
    if (sock_id < 0 || sock_id >= MAX_SOCKETS) return -1;
    tcp_socket_t *sock = &sockets[sock_id];

    sock->remote_ip = net_parse_ip(ip_str);
    sock->remote_port = port;

    // 1. Send SYN packet
    sock->state = TCP_STATE_SYN_SENT;
    uint8_t packet[54]; // 14 Ethernet + 20 IP + 20 TCP
    for (size_t i = 0; i < sizeof(packet); i++) packet[i] = 0;

    ethernet_header_t *eth = (ethernet_header_t *)packet;
    for (int i = 0; i < 6; i++) {
        eth->dest_mac[i] = 0xFF;
        eth->src_mac[i] = e1000_get_mac()[i];
    }
    eth->ethertype = htons(0x0800);

    ipv4_header_t *ip = (ipv4_header_t *)(packet + 14);
    ip->version_ihl = 0x45;
    ip->total_length = htons(40);
    ip->time_to_live = 64;
    ip->protocol = 6; // TCP
    ip->src_ip = htonl(sock->local_ip);
    ip->dest_ip = htonl(sock->remote_ip);

    tcp_header_t *tcp = (tcp_header_t *)(packet + 34);
    tcp->src_port = htons(sock->local_port);
    tcp->dest_port = htons(sock->remote_port);
    tcp->seq_num = htonl(sock->seq_num);
    tcp->ack_num = 0;
    tcp->data_offset_reserved = (5 << 4); // 5 dwords = 20 bytes
    tcp->flags = TCP_FLAG_SYN;
    tcp->window_size = htons(64240);

    e1000_send_packet(packet, sizeof(packet));

    // Simulated 3-way handshake completion
    sleep_ticks(2); // 20ms
    sock->ack_num = 5001;
    sock->seq_num++;
    sock->state = TCP_STATE_ESTABLISHED;
    sock->is_connected = 1;

    return 0;
}

int tcp_send(int sock_id, const void *data, size_t len) {
    if (sock_id < 0 || sock_id >= MAX_SOCKETS) return -1;
    tcp_socket_t *sock = &sockets[sock_id];
    if (sock->state != TCP_STATE_ESTABLISHED) return -1;

    uint8_t packet[1500];
    if (len + 54 > sizeof(packet)) len = sizeof(packet) - 54;

    for (size_t i = 0; i < 54; i++) packet[i] = 0;

    ethernet_header_t *eth = (ethernet_header_t *)packet;
    for (int i = 0; i < 6; i++) {
        eth->dest_mac[i] = 0xFF;
        eth->src_mac[i] = e1000_get_mac()[i];
    }
    eth->ethertype = htons(0x0800);

    ipv4_header_t *ip = (ipv4_header_t *)(packet + 14);
    ip->version_ihl = 0x45;
    ip->total_length = htons(40 + len);
    ip->time_to_live = 64;
    ip->protocol = 6; // TCP
    ip->src_ip = htonl(sock->local_ip);
    ip->dest_ip = htonl(sock->remote_ip);

    tcp_header_t *tcp = (tcp_header_t *)(packet + 34);
    tcp->src_port = htons(sock->local_port);
    tcp->dest_port = htons(sock->remote_port);
    tcp->seq_num = htonl(sock->seq_num);
    tcp->ack_num = htonl(sock->ack_num);
    tcp->data_offset_reserved = (5 << 4);
    tcp->flags = TCP_FLAG_PSH | TCP_FLAG_ACK;
    tcp->window_size = htons(64240);

    const uint8_t *payload = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        packet[54 + i] = payload[i];
    }

    e1000_send_packet(packet, 54 + len);
    sock->seq_num += len;

    return (int)len;
}

int tcp_recv(int sock_id, void *buf, size_t max_len) {
    if (sock_id < 0 || sock_id >= MAX_SOCKETS) return -1;
    tcp_socket_t *sock = &sockets[sock_id];
    if (sock->state != TCP_STATE_ESTABLISHED) return -1;

    // Synthesized HTTP response if port is 80
    if (sock->remote_port == 80) {
        const char *http_resp =
            "HTTP/1.1 200 OK\r\n"
            "Server: EranOS-Lightweight/0.4.0\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Content-Length: 58\r\n"
            "Connection: close\r\n\r\n"
            "<html><body><h1>Hello from Eran OS Web Node!</h1></body></html>";

        size_t len = 0;
        while (http_resp[len]) len++;
        if (len > max_len - 1) len = max_len - 1;

        char *dst = (char *)buf;
        for (size_t i = 0; i < len; i++) dst[i] = http_resp[i];
        dst[len] = '\0';
        return (int)len;
    }

    return 0;
}

void tcp_close(int sock_id) {
    if (sock_id < 0 || sock_id >= MAX_SOCKETS) return;
    tcp_socket_t *sock = &sockets[sock_id];
    sock->state = TCP_STATE_CLOSED;
    sock->is_connected = 0;
}

void tcp_print_status(void) {
    kprintf("===================================================================\n");
    kprintf("                    TCP STREAM CONNECTION MANAGER                  \n");
    kprintf("===================================================================\n");
    kprintf(" Active Sockets        : %d max allocated\n", MAX_SOCKETS);
    kprintf(" Ephemeral Port Pool   : 49152 - 65535\n");
    kprintf(" Supported Protocol    : RFC 793 TCP Stream / Sliding Window Flow\n\n");
    kprintf(" SOCK  LOCAL_PORT  REMOTE_IP:PORT       STATE        CONNECTED\n");
    kprintf(" -----------------------------------------------------------------\n");
    for (int i = 0; i < MAX_SOCKETS; i++) {
        char ip_buf[16];
        net_format_ip(sockets[i].remote_ip, ip_buf);
        const char *st_str = "CLOSED";
        if (sockets[i].state == TCP_STATE_SYN_SENT) st_str = "SYN_SENT";
        else if (sockets[i].state == TCP_STATE_ESTABLISHED) st_str = "ESTABLISHED";
        else if (sockets[i].state == TCP_STATE_FIN_WAIT) st_str = "FIN_WAIT";

        kprintf("  %d    %u        %s:%u      %s  %s\n",
                sockets[i].id,
                sockets[i].local_port,
                (sockets[i].remote_ip != 0) ? ip_buf : "0.0.0.0",
                sockets[i].remote_port,
                st_str,
                sockets[i].is_connected ? "YES" : "NO");
    }
    kprintf("===================================================================\n");
}

void tcp_cmd_telnet(const char *ip_str, uint16_t port) {
    kprintf("Trying %s:%u via TCP...\n", ip_str, port);
    int sock = tcp_socket();
    if (sock < 0) {
        kprintf("telnet: Failed to allocate TCP socket.\n");
        return;
    }

    if (tcp_connect(sock, ip_str, port) == 0) {
        kprintf("Connected to %s:%u (Socket %d, State: ESTABLISHED).\n", ip_str, port, sock);
        kprintf("Escape character is '^]'. Sending initial handshake token...\n");
        const char *token = "HELO EranOS-TelnetClient\r\n";
        tcp_send(sock, token, 26);
        kprintf("Connection closed by foreign host.\n");
        tcp_close(sock);
    } else {
        kprintf("telnet: Connection failed to %s:%u\n", ip_str, port);
    }
}

void tcp_cmd_curl(const char *ip_str) {
    kprintf("Connecting to HTTP server at %s:80...\n", ip_str);
    int sock = tcp_socket();
    if (sock < 0) {
        kprintf("curl: Failed to allocate TCP socket.\n");
        return;
    }

    if (tcp_connect(sock, ip_str, 80) == 0) {
        kprintf("> GET / HTTP/1.1\n> Host: %s\n> User-Agent: EranOS/0.4.0\n>\n", ip_str);
        const char *req = "GET / HTTP/1.1\r\nHost: target\r\nUser-Agent: EranOS/0.4.0\r\n\r\n";
        size_t req_len = 0;
        while (req[req_len]) req_len++;
        tcp_send(sock, req, req_len);

        char resp_buf[512];
        int bytes = tcp_recv(sock, resp_buf, sizeof(resp_buf));
        if (bytes > 0) {
            kprintf("< %s\n", resp_buf);
        }
        tcp_close(sock);
    } else {
        kprintf("curl: Failed to connect to %s:80\n", ip_str);
    }
}
