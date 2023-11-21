#include "websocket.h"
#include "websocket_handshake.h"

#include <esp_http_server.h>
#include <esp_log.h>

#include <string.h>
#include <mbedtls/sha1.h>
#include <mbedtls/base64.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "websocket"

httpd_handle_t start_websocket(uint32_t port) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.ctrl_port = 32767;
    config.recv_wait_timeout = 60 * 60 * 1; // 1 hour timeout for recieve
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}



