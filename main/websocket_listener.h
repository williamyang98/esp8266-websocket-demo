#ifndef __WEBSOCKET_LISTENER_H__
#define __WEBSOCKET_LISTENER_H__

#include "websocket.h"
#include "websocket_io.h"
#include <esp_err.h>

esp_err_t listen_websocket_start(httpd_req_t *request);
esp_err_t listen_websocket_data(httpd_req_t *request, uint8_t opcode, uint8_t *data, int length);
esp_err_t listen_websocket_exit(httpd_req_t *request);

#endif