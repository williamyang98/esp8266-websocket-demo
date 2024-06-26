#include "websocket_handler.h"

#include "global_periphs.h"
#include "dht11.h"
#include <driver/adc.h>

#include <esp_log.h>

#define DHT11_CMD 0x03
#define ADC_CMD 0x04

#define TAG "websocket-handler"

static esp_err_t listen_websocket_data(httpd_req_t *request, uint8_t *data, int length);
static void handle_dht11(httpd_req_t *request, uint8_t *data, int length);
static void handle_adc(httpd_req_t *request, uint8_t *data, int length);
static void async_send_dht11(void *ignore);
static void async_send_adc(void *ignore);

#define RECEIVE_BUFFER_SIZE 100
static uint8_t receive_buffer[RECEIVE_BUFFER_SIZE] = {0};

// async replies
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

static struct async_resp_arg async_handle_dht11 = {
    .hd = NULL,
    .fd = -1,
};

static struct async_resp_arg async_handle_adc = {
    .hd = NULL,
    .fd = -1,
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
    
    if (ws_pkt.len > RECEIVE_BUFFER_SIZE) {
        ESP_LOGE(TAG, "frame len is above maximum: %u > %d", ws_pkt.len, RECEIVE_BUFFER_SIZE);
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
    
    return listen_websocket_data(request, receive_buffer, ws_pkt.len);
}

esp_err_t listen_websocket_data(httpd_req_t *request, uint8_t *data, int length) {
    if (length < 1) {
        return ESP_FAIL;
    }

    uint8_t cmd_code = data[0];
    uint8_t *cmd_data = &data[1];
    int cmd_length = length-1;

    switch (cmd_code) {
    case DHT11_CMD: handle_dht11(request, cmd_data, cmd_length); break;
    case ADC_CMD:   handle_adc(request, cmd_data, cmd_length); break;
    default:        ESP_LOGD(TAG, "Unknown cmd: 0x%02x", cmd_code); break;
    }

    return ESP_OK;
}

void handle_dht11(httpd_req_t *request, uint8_t *data, int length) {
    ESP_LOGD("dht11-websocket", "Got request");
    if (async_handle_dht11.hd != NULL || async_handle_dht11.fd != -1) {
        ESP_LOGI("dht11-websocket", "ignored request since still processing");
        return;
    } 

    async_handle_dht11.hd = request->handle;
    async_handle_dht11.fd = httpd_req_to_sockfd(request);
    esp_err_t res = httpd_queue_work(request->handle, async_send_dht11, NULL);
    if (res != ESP_OK) {
        ESP_LOGE("dht11-websocket", "failed to queue async sensor task");
        async_handle_dht11.hd = NULL;
        async_handle_dht11.fd = -1;
        return;
    }
}

void async_send_dht11(void *ignore) {
    uint8_t data[3]; 
    size_t length = 0;
    data[0] = DHT11_CMD;
    if (dht11_read(&dht11_sensor) != ESP_OK) {
        data[1] = 0xFF;
        length = 2;
    } else {
        data[1] = dht11_sensor.humidity;
        data[2] = dht11_sensor.temperature;
        length = 3;
    }

    httpd_ws_frame_t ws_pkt = {
        .fragmented = false,
        .final = true,
        .type = HTTPD_WS_TYPE_BINARY,
        .payload = data,
        .len = length 
    };
    httpd_ws_send_frame_async(async_handle_dht11.hd, async_handle_dht11.fd, &ws_pkt);
    async_handle_dht11.hd = NULL;
    async_handle_dht11.fd = -1;
    ESP_LOGD("dht11-websocket", "replied to request");
}

void handle_adc(httpd_req_t *request, uint8_t *data, int length) {
    ESP_LOGD("adc-websocket", "Got request");
    if (async_handle_adc.hd != NULL || async_handle_adc.fd != -1) {
        ESP_LOGI("adc-websocket", "ignored request since still processing");
        return;
    }
    async_handle_adc.hd = request->handle;
    async_handle_adc.fd = httpd_req_to_sockfd(request);
    esp_err_t res = httpd_queue_work(request->handle, async_send_adc, NULL);
    if (res != ESP_OK) {
        ESP_LOGE("adc-websocket", "failed to queue async sensor task");
        async_handle_adc.hd = NULL;
        async_handle_adc.fd = -1;
        return;
    }
}

void async_send_adc(void *ignore) {
    uint8_t data[3];
    size_t length = 0;

    data[0] = ADC_CMD;
    uint16_t adc_val = 0;
    if (adc_read(&adc_val) != ESP_OK) {
        data[1] = 0xFF;
        length = 2;
    } else {
        data[1] = (uint8_t)(adc_val & 0x00FF);
        data[2] = (uint8_t)((adc_val & 0xFF00) >> 8);
        length = 3;
    }
    httpd_ws_frame_t ws_pkt = {
        .fragmented = false,
        .final = true,
        .type = HTTPD_WS_TYPE_BINARY,
        .payload = data,
        .len = length
    };
    httpd_ws_send_frame_async(async_handle_adc.hd, async_handle_adc.fd, &ws_pkt);
    async_handle_adc.hd = NULL;
    async_handle_adc.fd = -1;
    ESP_LOGD("adc-websocket", "replied to request");
}