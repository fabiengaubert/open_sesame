#include "ws_client.h"

#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"


    // From rfc6455
    //
    //   0                   1                   2                   3
    //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //  +-+-+-+-+-------+-+-------------+-------------------------------+
    //  |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
    //  |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
    //  |N|V|V|V|       |S|             |   (if payload len==126/127)   |
    //  | |1|2|3|       |K|             |                               |
    //  +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
    //  |     Extended payload length continued, if payload len == 127  |
    //  + - - - - - - - - - - - - - - - +-------------------------------+
    //  |                               |Masking-key, if MASK set to 1  |
    //  +-------------------------------+-------------------------------+
    //  | Masking-key (continued)       |          Payload Data         |
    //  +-------------------------------- - - - - - - - - - - - - - - - +
    //  :                     Payload Data continued ...                :
    //  + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
    //  |                     Payload Data continued ...                |
    //  +---------------------------------------------------------------+

enum ws_op_code {
    OP_CODE_CONTINUATION = 0x0,
    OP_CODE_TEXT = 0x1,
    OP_CODE_BINARY = 0x2,
    OP_CODE_CONNECTION_CLOSE = 0x8,
    OP_CODE_PING = 0x9,
    OP_CODE_PONG = 0xA
};

enum ws_state {
    WS_DISCONNECTED,
    WS_CONNECTING,
    WS_HANDSHAKING,
    WS_CONNECTED
};

struct ws_client {
    struct tcp_pcb *pcb;
    ip_addr_t server_ip;
    uint16_t server_port;
    uint8_t send_buffer[BUF_SIZE];
    int send_buffer_len;
    enum ws_state state;
};

static void ws_print_buffer(const unsigned char* buffer, uint64_t length) {
    for (uint64_t index = 0; index < length; index++) {
        printf("%c", buffer[index]);
    }
    printf("\n");
}

static bool ws_send_packet(struct ws_client *client) {
    err_t err = tcp_write(client->pcb, client->send_buffer, client->send_buffer_len, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        printf("Failed to send packet: %d\n", err);
        ws_client_close(client);
        return false;
    }
    printf("Packet sent:\n");
    printf("%.*s",client->send_buffer_len, client->send_buffer);
    printf("\n");

    return true;
}

static err_t ws_client_send_handshake(struct ws_client *client) {

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

    return ws_send_packet(client);
}

static void ws_write_mask(uint8_t *buffer) {
    uint32_t mask = rand();
    buffer[0] = (mask >> 24) & 0xFF;
    buffer[1] = (mask >> 16) & 0xFF;
    buffer[2] = (mask >> 8) & 0xFF;
    buffer[3] = mask & 0xFF;
}

static void ws_build_packet(struct ws_client *client, enum ws_op_code op_code, const unsigned char *payload, uint64_t payload_length) {
    /* Size is at least the 2-Byte header */
    client->send_buffer_len = 2;

    /* We add the FIN bit */
    client->send_buffer[0] = op_code | 0x80;

    /* We always add the masking bit because we are a client, see rfc6455 */
    client->send_buffer[1] = payload_length | 0x80;

    uint8_t mask_offset = client->send_buffer_len;
    ws_write_mask(&client->send_buffer[mask_offset]);
    client->send_buffer_len += 4;

    /* Encode the payload with the mask */
    for (uint64_t index = 0; index < payload_length; index++) {
        client->send_buffer[client->send_buffer_len++] = payload[index] ^ client->send_buffer[mask_offset + (index%4)];
    }
}

static bool ws_parse_packet(struct ws_client *client, const unsigned char* buffer, u16_t length) {
    /* Only packets sent by the client should be masked */
    if (buffer[1] & 0x80) {
        printf("Error: packet coming from the server is masked!\n");
        return false;
    }
    if (buffer[0] & 0x70) {
        printf("Error: an extension must be managed!\n");
        return false;
    }

    /* Payload length calculation */
    uint8_t payload_offset = 2;
    uint64_t payload_length = buffer[1] & 0x7F;

    /* 126 and 127 indicate an extention of the payload length */
    if (payload_length == 126) {
        payload_length = (buffer[2] << 8) & buffer[3];
        payload_offset += 2;
    }
    else if (payload_length == 127) {
        payload_length = ((uint64_t)buffer[2] << 56) & ((uint64_t)buffer[3] << 48) & ((uint64_t)buffer[4] << 40) & ((uint64_t)buffer[5] << 32) & ((uint64_t)buffer[6] << 24) & ((uint64_t)buffer[7] << 16) & ((uint64_t)buffer[8] << 8) & buffer[9];
        payload_offset += 8;
    }

    /* Op Code */
    switch (buffer[0] & 0x0F) {
        case OP_CODE_CONTINUATION:
            printf("Packet received: Continuation.\n");
            break;
        case OP_CODE_TEXT:
            printf("Packet received: Text.\n");
            ws_print_buffer(&buffer[payload_offset], payload_length);
            break;
        case OP_CODE_BINARY:
            printf("Packet received: Binary.\n");
            break;
        case OP_CODE_CONNECTION_CLOSE:
            printf("Packet received: Connection closed.\n");
            ws_print_buffer(&buffer[payload_offset], payload_length);
            break;
        case OP_CODE_PING:
            printf("Packet received: Ping.\n");
            ws_build_packet(client, OP_CODE_PONG, &buffer[payload_offset], payload_length);
            ws_send_packet(client);
            break;
        case OP_CODE_PONG:
            printf("Packet received: Pong.\n");
            break;
        default:
            printf("Packet received: unknown.\n");
    }
    return true;
}

/* TCP callbacks */
static err_t ws_client_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    return ERR_OK;
}

static err_t ws_client_connected(void *arg, struct tcp_pcb *pcb, err_t err) {
    struct ws_client *client = (struct ws_client*)arg;
    if (err != ERR_OK) {
        printf("Connection failed %d\n", err);
        ws_client_close(client);
        return err;
    }

    ws_client_send_handshake(client);
    printf("The connection is established. Sending WebSocket handshake.\n");
    client->state = WS_HANDSHAKING;
    return ERR_OK;
}

static err_t ws_client_poll(void *arg, struct tcp_pcb *pcb) {
    return ERR_OK;
}

static void ws_client_err(void *arg, err_t err) {
    struct ws_client *client = (struct ws_client*) arg;
    ws_client_close(client);
    printf("Client error: %d\n", err);
}

err_t ws_client_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    struct ws_client *client = (struct ws_client*)arg;
    if (!p) {
        printf("Empty packet received: connection closed by the server.\n");
        ws_client_close(client);
        return err;
    }

    if (client->state == WS_CONNECTED) {
        int index = 0;
        for (struct pbuf *q = p; q != NULL; q = q->next) {
            ws_parse_packet(client, (const unsigned char*)p->payload, p->len);
        }
    }
    else if (client->state == WS_HANDSHAKING) {
        char *upgrade_string = strstr(p->payload, "Upgrade: websocket");
        if (upgrade_string != NULL) {
            client->state = WS_CONNECTED;
            printf("Packet received: Handshake accepted.\n");
        }
    }
    else {
        printf("Incorrect packet received. Closing connection.\n");
        ws_client_close(client);
        return err;
    }

    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);

    return ERR_OK;
}

/* API functions */
struct ws_client* ws_client_init(const char *server_ip, const uint16_t server_port) {
    struct ws_client *client = malloc(sizeof(struct ws_client));

    if (!client) {
        printf("Error allocating client context.\n");
        return NULL;
    }

    if(!ip4addr_aton(server_ip, &client->server_ip)) {
        printf("Server IP address is incorrect.\n");
        free(client);
        return NULL;
    }

    client->state = WS_DISCONNECTED;
    client->server_port = server_port;
    return client;
}

bool ws_client_open(struct ws_client *client) {
    if (!client) {
        printf("Error: client not initialized.\n");
        return false;
    }

    printf("Connecting to server... \n");
    printf("IP: %s port: %u\n", ip4addr_ntoa(&client->server_ip), client->server_port);

    client->pcb = tcp_new_ip_type(IP_GET_TYPE(&client->server_ip));
    if (!client->pcb) {
        printf("Error: failed to allocate new PCB.\n");
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
        printf("Error starting the connection: %d.\n", err);
        ws_client_close(client);
        return false;
    }
    client->state = WS_CONNECTING;

    return true;
}

bool ws_client_close(struct ws_client* client) {
    if (!client) {
        printf("Error: client is already closed.\n");
        return false;
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
            return false;
        }
    }
    free(client);
    return true;
}
