#ifndef EDGE_NET_CLIENT_H_
#define EDGE_NET_CLIENT_H_

#include <stdint.h>

int net_client_tcp_connect(const char *host, uint16_t port, int *sock_out);
int net_client_send_all(int sock, const uint8_t *buf, int len);
int net_client_recv_once(int sock, uint8_t *buf, int len, int timeout_ms);
void net_client_close(int *sock);

#endif
