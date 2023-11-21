#ifndef __WEBSOCKET_IO_H__
#define __WEBSOCKET_IO_H__

#include <esp_http_server.h>
#include <esp_err.h>

#include "websocket.h"

esp_err_t websocket_write(httpd_req_t *request, char *data, int length, uint8_t opcode);
esp_err_t websocket_handler(httpd_req_t *request);

#endif