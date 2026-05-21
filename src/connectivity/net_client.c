#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "net_client.h"

LOG_MODULE_REGISTER(net_client, LOG_LEVEL_INF);

int net_client_tcp_connect(const char *host, uint16_t port, int *sock_out)
{
    char port_str[8];
    struct zsock_addrinfo hints = {0};
    struct zsock_addrinfo *res = NULL;
    struct zsock_addrinfo *ai;
    int rc;
    int sock = -1;

    if (host == NULL || sock_out == NULL) {
        return -EINVAL;
    }

    (void)snprintf(port_str, sizeof(port_str), "%u", (unsigned int)port);

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    rc = zsock_getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0 || res == NULL) {
        return -EHOSTUNREACH;
    }

    for (ai = res; ai != NULL; ai = ai->ai_next) {
        sock = zsock_socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sock < 0) {
            continue;
        }

        rc = zsock_connect(sock, ai->ai_addr, ai->ai_addrlen);
        if (rc == 0) {
            break;
        }

        zsock_close(sock);
        sock = -1;
    }

    zsock_freeaddrinfo(res);

    if (sock < 0) {
        return -ECONNREFUSED;
    }

    *sock_out = sock;
    return 0;
}

int net_client_send_all(int sock, const uint8_t *buf, int len)
{
    int sent = 0;

    if (sock < 0 || buf == NULL || len <= 0) {
        return -EINVAL;
    }

    while (sent < len) {
        int rc = zsock_send(sock, &buf[sent], len - sent, 0);
        if (rc <= 0) {
            return (rc < 0) ? rc : -EIO;
        }
        sent += rc;
    }

    return 0;
}

int net_client_recv_once(int sock, uint8_t *buf, int len, int timeout_ms)
{
    struct zsock_timeval tv;

    if (sock < 0 || buf == NULL || len <= 0) {
        return -EINVAL;
    }

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    (void)zsock_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    return zsock_recv(sock, buf, len, 0);
}

void net_client_close(int *sock)
{
    if (sock == NULL || *sock < 0) {
        return;
    }

    (void)zsock_close(*sock);
    *sock = -1;
}
