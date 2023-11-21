#ifndef __WEBSOCKET_H__
#define __WEBSOCKET_H__

#include <esp_http_server.h>

#define WEBSOCKET_OPCODE_CONTINUATION 0x00
#define WEBSOCKET_OPCODE_TEXT 0x01
#define WEBSOCKET_OPCODE_BIN 0x02
#define WEBSOCKET_OPCODE_PING 0x09
#define WEBSOCKET_OPCODE_PONG 0x0A
#define WEBSOCKET_OPCODE_CLOSE 0x08

typedef esp_err_t (*websocket_recieve_callback) (httpd_req_t *, uint8_t opcode, uint8_t *, int);
typedef esp_err_t (*websocket_start_callback) (httpd_req_t *);
typedef esp_err_t (*websocket_exit_callback) (httpd_req_t *);

typedef struct websocket_ctx {
    websocket_start_callback on_start;
    websocket_recieve_callback on_recieve;
    websocket_exit_callback on_exit;
} websocket_ctx;

httpd_handle_t start_websocket(uint32_t port);

#endif