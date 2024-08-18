#include "websocket_handler.h"
#include "pc_io.h"
#include <esp_log.h>

#define PC_IO_CMD 0x02
// #define PC_IO_OFF   0x01
#define PC_IO_ON    0x02
#define PC_IO_STATUS 0x04
#define TAG "websocket-handler"


static esp_err_t read_websocket_data(httpd_req_t *request, uint8_t *data, int length);
static esp_err_t write_websocket_data(httpd_req_t *request, uint8_t *data, int length);
static void handle_pc_io(httpd_req_t *request, uint8_t *data, int length);

#define REPLY_BUFFER_SIZE 100
static uint8_t receive_buffer[REPLY_BUFFER_SIZE] = {0};
static uint8_t reply_buffer[REPLY_BUFFER_SIZE] = {0};

// async replies
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

esp_err_t websocket_handler(httpd_req_t *request) {
    if (request->method == HTTP_GET) {
        ESP_LOGI(TAG, "handshake successful, connection is open");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    ws_pkt.final = false;
    ws_pkt.fragmented = false;
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;
    ws_pkt.payload = receive_buffer;
    ws_pkt.len = 0;

    // TODO: Figure out how to handle different types of websocket types
    //       I.e. Manually handle PING, OPEN, CLOSE, etc...
    
    esp_err_t res; 
    res = httpd_ws_recv_frame(request, &ws_pkt, 0);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "failed to get frame len with error: %d", res);
        return ESP_FAIL;
    }
    
    if (ws_pkt.len > REPLY_BUFFER_SIZE) {
        ESP_LOGE(TAG, "frame len is above maximum: %u > %d", ws_pkt.len, REPLY_BUFFER_SIZE);
        return ESP_FAIL;
    }

    res = httpd_ws_recv_frame(request, &ws_pkt, ws_pkt.len);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "failed to get frame with len %u, error=%d", ws_pkt.len, res);
        return ESP_FAIL;
    }

    if (ws_pkt.len == 0) {
        return ESP_OK;
    }
    
    return read_websocket_data(request, receive_buffer, ws_pkt.len);
}

esp_err_t read_websocket_data(httpd_req_t *request, uint8_t *data, int length) {
    if (length < 1) {
        return ESP_FAIL;
    }

    uint8_t cmd_code = data[0];
    uint8_t *cmd_data = &data[1];
    int cmd_length = length-1;

    switch (cmd_code) {
    case PC_IO_CMD: handle_pc_io(request, cmd_data, cmd_length); break;
    default:        ESP_LOGD(TAG, "Unknown cmd: 0x%02x", cmd_code); break;
    }

    return ESP_OK;
}

esp_err_t write_websocket_data(httpd_req_t *request, uint8_t *data, int length) {
    httpd_ws_frame_t ws_pkt = {
        .fragmented = false,
        .final = true,
        .type = HTTPD_WS_TYPE_BINARY,
        .payload = data,
        .len = length 
    };
    return httpd_ws_send_frame(request, &ws_pkt);
}

void handle_pc_io(httpd_req_t *request, uint8_t *data, int length) {
    if (length < 1) {
        return;
    }
    uint8_t cmd = data[0];
    ESP_LOGD("pc-io-websocket", "Got command: 0x%02x", cmd);
    esp_err_t resp_status = ESP_OK;
    switch (cmd) {
    // case PC_IO_OFF:     resp_status = pc_io_power_off();    break;
    case PC_IO_ON:      resp_status = pc_io_power_on();     break;
    case PC_IO_STATUS:  pc_io_is_powered() ? (resp_status = ESP_OK) : (resp_status = ESP_FAIL); break;
    default:            ESP_LOGE("pc-io-websocket", "Unknown command: 0x%02x", cmd); return;
    }

    reply_buffer[0] = PC_IO_CMD;
    reply_buffer[1] = cmd;
    if (resp_status == ESP_OK) {
        reply_buffer[2] = 0x01;
    } else {
        reply_buffer[2] = 0x00;
    }
    write_websocket_data(request, reply_buffer, 3);
}