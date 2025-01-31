#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include "pico/cyw43_arch.h"

#define WS_KEY "dGhlIHNhbXBsZSBub25jZQ=="
#define BUF_SIZE 2048

void run_ws_client_test(const char *server_ip, const uint16_t server_port);

#endif /* WS_CLIENT_H */