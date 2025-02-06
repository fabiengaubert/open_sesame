#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include "pico/cyw43_arch.h"

#define WS_KEY "dGhlIHNhbXBsZSBub25jZQ=="
#define BUF_SIZE 2048

/* Opaque client context */
struct ws_client;

/** Allocate and initialize a client context
 *
 * @param server_ip server IPv4 address in text form
 * @param server_port server port to connect to
 * @return client context or NULL if error
 */
struct ws_client* ws_client_init(const char *server_ip, const uint16_t server_port);

/** Open and initialize a WebSocket connection
 *
 * @param client client context
 * @return  true if the client is connecting or false otherwise
 */
bool ws_client_open(struct ws_client *client);

/** Close a WebSocket connection and free the client context memory
 *
 * @param client client context
 * @return  true if the connection is closed or false otherwise
 */
bool ws_client_close(struct ws_client *client);

#endif /* WS_CLIENT_H */