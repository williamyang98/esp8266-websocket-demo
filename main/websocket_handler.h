#ifndef __WEBSOCKET_LISTENER_H__
#define __WEBSOCKET_LISTENER_H__

#include <httpd_server/esp_http_server.h>
#include <esp_err.h>

esp_err_t websocket_handler(httpd_req_t *request);

#endif