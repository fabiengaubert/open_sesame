#include "ws_client.h"

#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

typedef enum ws_state_t {
    WS_DISCONNECTED,
    WS_CONNECTING,
    WS_CONNECTED
} ws_state_t;

typedef struct ws_client_t {
    struct tcp_pcb *pcb;
    ip_addr_t server_ip;
    uint16_t server_port;
    uint8_t send_buffer[BUF_SIZE];
    int send_buffer_len;
    ws_state_t state;
} ws_client_t;

static bool ws_client_open(ws_client_t *client);
static err_t ws_client_close(ws_client_t *client);
static err_t ws_client_sent(void *arg, struct tcp_pcb *pcb, u16_t len);
static err_t ws_client_connected(void *arg, struct tcp_pcb *pcb, err_t err);
static err_t ws_client_poll(void *arg, struct tcp_pcb *pcb);
static void ws_client_err(void *arg, err_t err);
err_t ws_client_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);

static err_t ws_client_close(ws_client_t* client) {
    if (!client) {
        printf("Client is already closed.\n");
        return ERR_ARG;
    }

    if (client->pcb != NULL) {
        tcp_arg(client->pcb, NULL);
        tcp_poll(client->pcb, NULL, 0);
        tcp_sent(client->pcb, NULL);
        tcp_recv(client->pcb, NULL);
        tcp_err(client->pcb, NULL);

        err_t err = tcp_close(client->pcb);

        if (err != ERR_OK) {
            printf("Close failed with error code: %d, aborting.\n", err);
            tcp_abort(client->pcb);
            return err;
        }
        client->pcb = NULL;
    }
    free(client);
    return ERR_OK;
}

static err_t ws_client_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    ws_client_t *client = (ws_client_t*)arg;
    printf("Client sent: %u\n", len);

    return ERR_OK;
}

static err_t ws_client_send_handshake(struct ws_client_t *client) {

    /* HTTP GET request to upgrade to websocket */
    client->send_buffer_len = sprintf((char*)client->send_buffer,
                                    "GET / HTTP/1.1\r\n"
                                    "Host: %s:%d\r\n"
                                    "Upgrade: websocket\r\n"
                                    "Connection: Upgrade\r\n"
                                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                                    "Sec-WebSocket-Protocol: test\r\n"
                                    "Sec-WebSocket-Version: 13\r\n"
                                    "\r\n",
                                    ipaddr_ntoa(&client->server_ip),
                                    client->server_port);

    printf("Sent packet:\n");
    printf("%s", client->send_buffer);

    err_t err = tcp_write(client->pcb, client->send_buffer, client->send_buffer_len, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        printf("Failed to write data %d\n", err);
        ws_client_close(client);
        return err;
    }
    return ERR_OK;
}

static err_t ws_client_connected(void *arg, struct tcp_pcb *pcb, err_t err) {
    ws_client_t *client = (ws_client_t*)arg;
    if (err != ERR_OK) {
        printf("Connection failed %d\n", err);
        ws_client_close(client);
        return err;
    }

    ws_client_send_handshake(client);
    printf("The connection is established. Sending WebSocket handshake.\n");
    client->state = WS_CONNECTED;
    return ERR_OK;
}

static err_t ws_client_poll(void *arg, struct tcp_pcb *pcb) {
    printf("Polling callback.\n");
    return ERR_OK;
}

static void ws_client_err(void *arg, err_t err) {
    ws_client_t *client = (ws_client_t*) arg;
    ws_client_close(client);
    printf("Client error: %d\n", err);
}

err_t ws_client_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    printf("Incoming packet.\r\n");
    ws_client_t *client = (ws_client_t*)arg;
    if (!p) {
        printf("Connection closed by the server.\n");
        ws_client_close(client);
        return err;
    }

    if (client->state == WS_CONNECTING) {
        char *upgrade_string = strstr(p->payload, "Upgrade: websocket");
        if (upgrade_string != NULL) {
            client->state = WS_CONNECTED;
            printf("Handshake accepted.\n");
        }
    }
    else {
        /* Print packet */
        for (struct pbuf *q = p; q != NULL; q = q->next) {
            printf("%.*s\n", q->len, (const char*)q->payload);
        }
    }

    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);

    return ERR_OK;
}

static bool ws_client_open(ws_client_t *client) {
    printf("Connecting to server... \n");
    printf("IP: %s port: %u\n", ip4addr_ntoa(&client->server_ip), client->server_port);

    client->pcb = tcp_new_ip_type(IP_GET_TYPE(&client->server_ip));
    if (!client->pcb) {
        printf("Failed to allocate new PCB.\n");
        return false;
    }

    tcp_arg(client->pcb, client);
    tcp_poll(client->pcb, ws_client_poll, 1);
    tcp_sent(client->pcb, ws_client_sent);
    tcp_recv(client->pcb, ws_client_recv);
    tcp_err(client->pcb, ws_client_err);

    client->send_buffer_len = 0;

    cyw43_arch_lwip_begin();
    err_t err = tcp_connect(client->pcb, &client->server_ip, client->server_port, ws_client_connected);
    cyw43_arch_lwip_end();

    if (err != ERR_OK) {
        printf("Error starting the connection.\n");
        ws_client_close(client);
        return false;
    }

    return true;
}

static ws_client_t* ws_client_init(const char *server_ip, const uint16_t server_port) {
    ws_client_t *client = malloc(sizeof(ws_client_t));

    if (!client) {
        printf("Error allocating client context.\n");
        return NULL;
    }

    if(!ip4addr_aton(server_ip, &client->server_ip)) {
        printf("Server IP address is incorrect.\n");
        client->state = WS_DISCONNECTED;
        return NULL;
    }

    client->server_port = server_port;
    client->state = WS_CONNECTING;
    return client;
}

void run_ws_client_test(const char *server_ip, const uint16_t server_port) {
    ws_client_t *client = ws_client_init(server_ip, server_port);
    if (!client) {
        return;
    }

    if (!ws_client_open(client)) {
        printf("Error, could not open the TCP connection.\n");
        ws_client_close(client);
        return;
    }
}