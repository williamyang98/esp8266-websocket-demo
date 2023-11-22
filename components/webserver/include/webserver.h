#ifndef __WEBSERVER_INIT_H__
#define __WEBSERVER_INIT_H__

#include <httpd_server/esp_http_server.h>
#include <esp_err.h>

esp_err_t webserver_register_endpoints(httpd_handle_t server);

#endif