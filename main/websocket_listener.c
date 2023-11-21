#include "websocket_listener.h"

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

static void handle_dht11(httpd_req_t *request, uint8_t opcode, uint8_t *data, int length);
static void handle_pc_io(httpd_req_t *request, uint8_t opcode, uint8_t *data, int length);
static void handle_led(httpd_req_t *request, uint8_t opcode, uint8_t *data, int length);

#define REPLY_BUFFER_SIZE 100
static uint8_t reply_buffer[REPLY_BUFFER_SIZE] = {0};
static void pc_io_status_listener(bool is_powered, void *args);

esp_err_t listen_websocket_data(httpd_req_t *request, uint8_t opcode, uint8_t *data, int length) {
    if (length < 1) {
        return ESP_FAIL;
    }

    uint8_t cmd_code = data[0];
    uint8_t *cmd_data = &data[1];
    int cmd_length = length-1;

    switch (cmd_code) {
    case LED_CMD:   handle_led(request, opcode, cmd_data, cmd_length); break;
    case PC_IO_CMD: handle_pc_io(request, opcode, cmd_data, cmd_length); break;
    case DHT11_CMD: handle_dht11(request, opcode, cmd_data, cmd_length); break;
    default:        ESP_LOGD("websocket-listener", "Unknown cmd: 0x%02x", cmd_code); break;
    }

    return ESP_OK;
}

esp_err_t listen_websocket_start(httpd_req_t *request) {
    return pc_io_status_listen(pc_io_status_listener, (void *)request);
}

esp_err_t listen_websocket_exit(httpd_req_t *request) {
    return pc_io_status_unlisten(pc_io_status_listener, (void *)request);
}


void handle_dht11(httpd_req_t *request, uint8_t opcode, uint8_t *data, int length) {
    ESP_LOGD("dht11-websocket", "Got request");
    reply_buffer[0] = DHT11_CMD;
    if (dht11_read() != ESP_OK) {
        reply_buffer[1] = 0xFF;
        websocket_write(request, (char *)reply_buffer, 2, opcode);
        return;
    }
    reply_buffer[1] = dht11_get_humidity();
    reply_buffer[2] = dht11_get_temperature();
    websocket_write(request, (char *)reply_buffer, 3, opcode);
}

void handle_pc_io(httpd_req_t *request, uint8_t opcode, uint8_t *data, int length) {
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
    default:            ESP_LOGI("pc-io-websocket", "Unknown command: 0x%02x", cmd); return;
    }

    reply_buffer[0] = PC_IO_CMD;
    reply_buffer[1] = cmd;
    
    if (resp_status == ESP_OK) {
        reply_buffer[2] = 0x01;
    } else {
        reply_buffer[2] = 0x00;
    }
    websocket_write(request, (char *)reply_buffer, 3, opcode);
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
    websocket_write(request, (char *)reply_buffer, 3, WEBSOCKET_OPCODE_BIN);
}

void handle_led(httpd_req_t *request, uint8_t opcode, uint8_t *data, int length) {
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
        websocket_write(request, (char *)reply_buffer, 3 + MAX_PWM_PINS, opcode);
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
        // websocket_write(request, (char *)reply_buffer, 2, opcode);
    }
}