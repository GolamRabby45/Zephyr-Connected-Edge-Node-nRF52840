#ifndef EDGE_WS_CLIENT_H_
#define EDGE_WS_CLIENT_H_

#include <stdbool.h>
#include <stdint.h>

struct edge_ws_handle {
    int tcp_sock;
    int ws_sock;
    uint8_t tmp_buf[512];
    bool connected;
};

int ws_telemetry_start(void);
int ws_client_connect(struct edge_ws_handle *handle, const char *host, uint16_t port, const char *url);
int ws_client_send_text(struct edge_ws_handle *handle, const uint8_t *payload, uint32_t payload_len);
void ws_client_disconnect(struct edge_ws_handle *handle);

#endif
