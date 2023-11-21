#include "server.h"
#include "files.h"

#include <esp_http_server.h>
#include <esp_log.h>

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "webserver"

#define MAIN_JS_ID 1
#define INDEX_JS_ID 2
#define CHUNK_JS_ID 3

static esp_err_t send_file(httpd_req_t *request);

httpd_uri_t index_html_uri = {
    .uri = "/",
    // .uri = "/index.html",
    .method = HTTP_GET,
    .handler = send_file,
    .user_ctx = (void *)INDEX_JS_ID
};

httpd_uri_t main_js_uri = {
    .uri = "/main.js",
    .method = HTTP_GET,
    .handler = send_file,
    .user_ctx = (void *)MAIN_JS_ID
};

httpd_uri_t chunk_js_uri = {
    .uri = "/chunk.js",
    .method = HTTP_GET,
    .handler = send_file,
    .user_ctx = (void *)CHUNK_JS_ID
};

httpd_handle_t start_webserver(uint32_t port) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &index_html_uri);
        httpd_register_uri_handler(server, &main_js_uri);
        httpd_register_uri_handler(server, &chunk_js_uri);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

esp_err_t send_file(httpd_req_t *request) {
    int fileID = (int)(request->user_ctx);
    switch (fileID) {
    case MAIN_JS_ID:
        httpd_resp_send(request, main_js_file, sizeof(main_js_file));
        break;
    case CHUNK_JS_ID:
        httpd_resp_send(request, chunk_js_file, sizeof(chunk_js_file));
        break;
    case INDEX_JS_ID:
        httpd_resp_send(request, index_js_file, sizeof(index_js_file));
        break;
    default:
        httpd_resp_send_404(request);
        break;
    }

    return ESP_OK;
}