#include "websocket_handler.h"

#include "global_periphs.h"
#include "shifted_pwm.h"
#include "pc_io.h"
#include "pc_io_interrupt.h"
#include "dht11.h"

#include <esp_log.h>

#define DHT11_CMD 0x03
#define PC_IO_CMD 0x02
#define LED_CMD 0x01

#define PC_IO_OFF   0x01
#define PC_IO_ON    0x02
#define PC_IO_RESET 0x03
#define PC_IO_STATUS 0x04

#define LED_SET 0x01
#define LED_GET 0x02

#define TAG "websocket-handler"


static esp_err_t listen_websocket_data(httpd_req_t *request, uint8_t *data, int length);
static esp_err_t write_websocket_data(httpd_req_t *request, uint8_t *data, int length);
static void handle_dht11(httpd_req_t *request, uint8_t *data, int length);
static void handle_pc_io(httpd_req_t *request, uint8_t *data, int length);
static void handle_led(httpd_req_t *request, uint8_t *data, int length);
static void async_send_dht11(void *ignore);

#define REPLY_BUFFER_SIZE 100
static uint8_t receive_buffer[REPLY_BUFFER_SIZE] = {0};
static uint8_t reply_buffer[REPLY_BUFFER_SIZE] = {0};
static void pc_io_status_listener(bool is_powered, void *args);

// async replies
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

static struct async_resp_arg async_handle_dht11 = {
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
    case LED_CMD:   handle_led(request, cmd_data, cmd_length); break;
    case PC_IO_CMD: handle_pc_io(request, cmd_data, cmd_length); break;
    case DHT11_CMD: handle_dht11(request, cmd_data, cmd_length); break;
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


esp_err_t listen_websocket_start(httpd_req_t *request) {
    return pc_io_status_listen(pc_io_status_listener, (void *)request);
}

esp_err_t listen_websocket_exit(httpd_req_t *request) {
    return pc_io_status_unlisten(pc_io_status_listener, (void *)request);
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

void handle_pc_io(httpd_req_t *request, uint8_t *data, int length) {
    if (length < 1) {
        return;
    }
    uint8_t cmd = data[0];
    ESP_LOGD("pc-io-websocket", "Got command: 0x%02x", cmd);
    esp_err_t resp_status = ESP_OK;
    switch (cmd) {
    case PC_IO_OFF:     resp_status = pc_io_power_off();    break;
    case PC_IO_ON:      resp_status = pc_io_power_on();     break;
    case PC_IO_RESET:   resp_status = pc_io_reset();        break;
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

void pc_io_status_listener(bool is_powered, void *args) {
    httpd_req_t *request = (httpd_req_t *)args;
    if (request == NULL) {
        return;
    }

    reply_buffer[0] = PC_IO_CMD;
    reply_buffer[1] = PC_IO_STATUS;
    reply_buffer[2] = is_powered ? 0x01 : 0x00;
    ESP_LOGD("websocket-listener-pc-io", "ISR is_powered: %d", is_powered);
    write_websocket_data(request, reply_buffer, 3);
}

void handle_led(httpd_req_t *request, uint8_t *data, int length) {
    if (length < 1) {
        return;
    }
    uint8_t mode = data[0];
    if (mode == LED_GET) {
        reply_buffer[0] = LED_CMD;
        reply_buffer[1] = LED_GET;
        reply_buffer[2] = MAX_PWM_PINS;
        for (int i = 0; i < MAX_PWM_PINS; i++) {
            reply_buffer[3+i] = get_pwm_value(i);
        }
        write_websocket_data(request, reply_buffer, 3+MAX_PWM_PINS);
    } else if (mode == LED_SET) {
        for (int i = 1; i < length-1; i+=2) {
            uint8_t pin = data[i];
            uint8_t value = data[i+1];
            if (pin < MAX_PWM_PINS) {
                set_pwm_value(pin, value);
            }
        }
        // disable reply since limits bandwidth
        // reply_buffer[0] = LED_CMD;
        // reply_buffer[1] = LED_SET;
        // write_websocket_data(request, reply_buffer, 2);
    }
}