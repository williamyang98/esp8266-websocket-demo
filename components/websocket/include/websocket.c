#include "websocket.h"
#include "websocket_handshake.h"

#include <esp_http_server.h>
#include <esp_log.h>
#include "./port/esp_httpd_priv.h"

#include <string.h>
#include <mbedtls/sha1.h>
#include <mbedtls/base64.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "websocket-io"
#define PROTOCOL_BUFFER_SIZE 125
#undef MIN
#define MIN(x, y) ((x > y) ? y : x)

static uint8_t write_buffer[PROTOCOL_BUFFER_SIZE+2] = {0};
static uint8_t read_buffer[PROTOCOL_BUFFER_SIZE+2] = {0};
static uint8_t exit_response[2] = {0x80, 0x00};

static esp_err_t websocket_read_data(httpd_req_t *request);

esp_err_t websocket_write(httpd_req_t *request, char *data, int length, uint8_t opcode) {
    if (length > PROTOCOL_BUFFER_SIZE) {
        ESP_LOGE(TAG, "length of buffer exceed protocol length: %d > %d", length, PROTOCOL_BUFFER_SIZE);
        return ESP_FAIL;
    }
    write_buffer[0] = 0x80 | opcode;
    write_buffer[1] = length;
    memcpy(&write_buffer[2], data, length);
    if (httpd_send(request, (char *)write_buffer, length+2) <= 0) {
        ESP_LOGI(TAG, "Failed send");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t websocket_handler(httpd_req_t *request) {
    if (validate_websocket_request(request) != ESP_OK) {
        ESP_LOGE(TAG, "Failed validation");
        return ESP_FAIL;
    } 

    if (perform_websocket_handshake(request) != ESP_OK) {
        ESP_LOGE(TAG, "Failed handshake");
        return ESP_FAIL;
    }


    ESP_LOGI(TAG, "Starting websocket");
    websocket_ctx *context = (websocket_ctx *)(request->user_ctx);
    websocket_start_callback start_callback = (context != NULL) ? context->on_start : NULL;
    websocket_exit_callback exit_callback = (context != NULL) ? context->on_exit : NULL;

    if (start_callback != NULL) {
        start_callback(request);
    }
    while (websocket_read_data(request) == ESP_OK) {

    }
    ESP_LOGI(TAG, "Closing websocket");
    if (exit_callback != NULL) {
        exit_callback(request);
    }
    return ESP_FAIL;
}

esp_err_t websocket_read_data(httpd_req_t *request) {
    websocket_ctx *context = (websocket_ctx *)(request->user_ctx);
    websocket_recieve_callback callback = (context != NULL) ? context->on_recieve : NULL;
    if (callback == NULL) {
        return ESP_OK;
    }

    // struct httpd_req_aux *ra = request->aux;
    // int total_data = ra->sd->recv_fn(ra->sd->handle, ra->sd->fd, (char *)read_buffer, sizeof(read_buffer), 0);
    int total_data = httpd_recv_with_opt(request, (char *)read_buffer, sizeof(read_buffer), false);
    // int total_data = httpd_recv_with_opt(request, (char *)read_buffer, sizeof(read_buffer), false);
    ESP_LOGD(TAG, "httpd response: %d", total_data);
    if (total_data >= 7) {
        uint8_t opcode = read_buffer[0] & 0x7F;
        // unmask
        switch (opcode) {
        case WEBSOCKET_OPCODE_CONTINUATION:
        case WEBSOCKET_OPCODE_BIN: 
        case WEBSOCKET_OPCODE_TEXT: 
        case WEBSOCKET_OPCODE_PING:
            if (total_data > 6) {
                total_data -= 6;
                for (int i = 0; i < total_data; i++) {
                    read_buffer[i+6] ^= read_buffer[2 + i % 4];
                }
                int8_t length = read_buffer[1] + 128;
                if (opcode != WEBSOCKET_OPCODE_PING) {
                    callback(request, opcode, &read_buffer[6], length);
                } else {
                    ESP_LOGI(TAG, "sent ping");
                    websocket_write(request, (char *)&read_buffer[6], length, WEBSOCKET_OPCODE_PONG);
                }
            }
            break;
        case WEBSOCKET_OPCODE_CLOSE:
            ESP_LOGI(TAG, "closing websocket");
            return ESP_FAIL;
        }
    // invalid packet 
    } else {
        ESP_LOGE(TAG, "websocket failed to get data (%d < 7bytes), terminating", total_data);
        websocket_write(request, (char *)exit_response, sizeof(exit_response), WEBSOCKET_OPCODE_BIN);
        return ESP_FAIL;
    }
    return ESP_OK;
}