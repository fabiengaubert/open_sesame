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

err_t ws_client_close(struct ws_client* client) {
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
    struct ws_client *client = (struct ws_client*)arg;
    printf("Client sent: %u\n", len);

    return ERR_OK;
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

static void ws_print_message(const unsigned char* buffer, uint32_t length) {
    for (int index = 0; index < length; index++) {
        printf("%c", buffer[index]);
    }
    printf("\n");
}

static void ws_parse_packet(const unsigned char* buffer, u16_t length) {
    printf("Packet parser!\n");

    /* Only messages sent by the client should be masked */
    if (buffer[1] & 0x80) {
        printf("Error: message coming from the server is masked!\n");
    }
    if (buffer[0] & 0x70) {
        printf("Error: an extension must be managed!\n");
    }

    /* Payload length calculation */
    uint32_t payload_length;
    uint8_t payload_offset = 2;

    payload_length = buffer[1] & 0x7F;

    /* 126 and 127 indicate an extention of the payload length */
    if (payload_length == 126) {
        payload_length = (buffer[2] << 8) & buffer[3];
        payload_offset += 2;
    }
    else if (payload_length == 127) {
        payload_length = (buffer[2] << 24) & (buffer[3] << 16) & (buffer[4] << 8) & buffer[5];
        payload_offset += 4;
    }
    printf("Length of the payload: %d\n", payload_length);

    /* Op Code */
    printf("OP CODE:\n");

    switch (buffer[0] & 0x0F) {
        case OP_CODE_CONTINUATION:
            printf("Continuation of a packet.\n");
            break;
        case OP_CODE_TEXT:
            ws_print_message(&buffer[payload_offset], payload_length);
            break;
        case OP_CODE_BINARY:
            printf("Binary.\n");
            break;
        case OP_CODE_CONNECTION_CLOSE:
            printf("Connection closed.\n");
            break;
        case OP_CODE_PING:
            printf("We received a ping.\n");
            break;
        case OP_CODE_PONG:
            printf("We received a pong.\n");
            break;
        default:
            printf("Error: packet not managed.\n");
    }
    return;
}

err_t ws_client_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    printf("Incoming packet.\r\n");
    struct ws_client *client = (struct ws_client*)arg;
    if (!p) {
        printf("Connection closed by the server.\n");
        ws_client_close(client);
        return err;
    }

    if (client->state == WS_CONNECTED) {
        int index = 0;
        printf("Total packet length: %d\n", p->tot_len);
        printf("Segment length: %d\n", p->len);
        for (struct pbuf *q = p; q != NULL; q = q->next) {
            ws_parse_packet((const unsigned char*)p->payload, p->len);
        }
    }
    else if (client->state == WS_HANDSHAKING) {
        char *upgrade_string = strstr(p->payload, "Upgrade: websocket");
        if (upgrade_string != NULL) {
            client->state = WS_CONNECTED;
            printf("Handshake accepted.\n");
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

bool ws_client_open(struct ws_client *client) {
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
        printf("Error starting the connection: %d.\n", err);
        ws_client_close(client);
        return false;
    }
    client->state = WS_CONNECTING;

    return true;
}

struct ws_client* ws_client_init(const char *server_ip, const uint16_t server_port) {
    struct ws_client *client = malloc(sizeof(struct ws_client));

    if (!client) {
        printf("Error allocating client context.\n");
        return NULL;
    }
    client->state = WS_DISCONNECTED;

    if(!ip4addr_aton(server_ip, &client->server_ip)) {
        printf("Server IP address is incorrect.\n");
        return NULL;
    }

    client->server_port = server_port;
    return client;
}
