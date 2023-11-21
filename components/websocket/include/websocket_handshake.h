#ifndef __WEBSOCKET_HANDSHAKE_H__
#define __WEBSOCKET_HANDSHAKE_H__

#include <esp_http_server.h>
#include <esp_err.h>

esp_err_t perform_websocket_handshake(httpd_req_t *request);
esp_err_t validate_websocket_request(httpd_req_t *request);

#endif