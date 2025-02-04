#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include "pico/cyw43_arch.h"

#define WS_KEY "dGhlIHNhbXBsZSBub25jZQ=="
#define BUF_SIZE 2048

struct ws_client;

struct ws_client* ws_client_init(const char *server_ip, const uint16_t server_port);
bool ws_client_open(struct ws_client *client);
err_t ws_client_close(struct ws_client *client);

#endif /* WS_CLIENT_H */