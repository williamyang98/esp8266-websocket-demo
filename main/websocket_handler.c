#include "websocket_handler.h"

#include "global_periphs.h"
#include "shifted_pwm.h"
#include "pc_io.h"
#include "dht11.h"

#include <esp_log.h>

static const char TAG[] = "websocket-handler";
static const uint8_t DHT11_CMD = 0x03;
static const uint8_t PC_IO_CMD = 0x02;
static const uint8_t LED_CMD = 0x01;
static const uint8_t PC_IO_OFF = 0x01;
static const uint8_t PC_IO_ON = 0x02;
static const uint8_t PC_IO_RESET = 0x03;
static const uint8_t PC_IO_STATUS = 0x04;
static const uint8_t LED_SET = 0x01;
static const uint8_t LED_GET = 0x02;

static struct WebsocketClient* dht11_websocket_client = NULL;

void websocket_async_send_dht11(struct WebsocketClient* client, void *args) {
    static const char SUBTAG[] = "dht11-async-websocket-handler";
    assert(client != NULL);
    const struct Websocket* websocket = client->websocket;
    assert(websocket != NULL);
    uint8_t* buffer = websocket->transmit_buffer;
    assert(buffer != NULL);

    size_t length = 0;
    buffer[0] = DHT11_CMD;
    struct DHT11_Measurement measurement;
    const bool is_read_success = dht11_read(g_dht11_data_pin, &measurement) == ESP_OK;
    if (is_read_success) {
        buffer[1] = measurement.humidity;
        buffer[2] = measurement.temperature;
        length = 3;
    } else {
        buffer[1] = 0xFF;
        length = 2;
    }
    const esp_err_t status = websocket_send_pending_binary_data_async(client, length);
    if (status != ESP_OK) {
        ESP_LOGE(SUBTAG, "Failed to send dht11 data websocket_fd=%d, humidity=%u, temperature=%u, error='%s'",
            client->websocket_fd, measurement.humidity, measurement.temperature, esp_err_to_name(status)
        );
    }
}

static void websocket_on_dht11_frame(httpd_req_t* request, struct WebsocketClient* client, const uint8_t* data, size_t size) {
    static const char SUBTAG[] = "dht11-websocket-handler";
    assert(request != NULL);
    assert(client != NULL);
    dht11_websocket_client = client;
    const esp_err_t status = websocket_queue_async_task(client, websocket_async_send_dht11, NULL);
    if (status != ESP_OK) {
        ESP_LOGE(SUBTAG, "failed to queue async dht11 task: '%s'", esp_err_to_name(status));
        dht11_websocket_client = NULL;
    }
}

static void websocket_on_pc_io_frame(httpd_req_t* request, struct WebsocketClient* client, const uint8_t* data, size_t size) {
    static const char SUBTAG[] = "pc-io-websocket-handler";
    const struct Websocket* websocket = client->websocket;
    assert(websocket != NULL);
    uint8_t* buffer = websocket->transmit_buffer;
    assert(buffer != NULL);

    if (size == 0) {
        ESP_LOGE(SUBTAG, "Got an unexpected empty command buffer");
        return;
    }

    const uint8_t cmd = data[0];
    ESP_LOGD("pc-io-websocket", "Got command: 0x%02x", cmd);
    esp_err_t resp_status = ESP_OK;
    switch (cmd) {
    case PC_IO_OFF:     resp_status = pc_io_power_off(&g_pc_io_config); break;
    case PC_IO_ON:      resp_status = pc_io_power_on(&g_pc_io_config); break;
    case PC_IO_RESET:   resp_status = pc_io_reset(&g_pc_io_config); break;
    case PC_IO_STATUS:  pc_io_is_powered(&g_pc_io_config) ? (resp_status = ESP_OK) : (resp_status = ESP_FAIL); break;
    default:            ESP_LOGE(SUBTAG, "Unknown command: 0x%02x", cmd); return;
    }

    buffer[0] = PC_IO_CMD;
    buffer[1] = cmd;
    if (resp_status == ESP_OK) {
        buffer[2] = 0x01;
    } else {
        buffer[2] = 0x00;
    }
    const esp_err_t status = websocket_send_pending_binary_data_sync(client, request, 3);
    if (status != ESP_OK) {
        ESP_LOGE(SUBTAG, "failed to send pc io response cmd=%u, err='%s'", cmd, esp_err_to_name(status));
    }
}

static void websocket_on_shifted_pwm_frame(httpd_req_t* request, struct WebsocketClient* client, const uint8_t* data, size_t size) {
    static const char SUBTAG[] = "shifted-pwm-websocket-handler";
    const struct Websocket* websocket = client->websocket;
    assert(websocket != NULL);
    uint8_t* buffer = websocket->transmit_buffer;
    assert(buffer != NULL);

    if (size == 0) {
        ESP_LOGE(SUBTAG, "Got an unexpected empty command buffer");
        return;
    }

    const uint8_t mode = data[0];
    if (mode == LED_GET) {
        buffer[0] = LED_CMD;
        buffer[1] = LED_GET;
        buffer[2] = SHIFTED_PWM_TOTAL_PINS;
        for (int i = 0; i < SHIFTED_PWM_TOTAL_PINS; i++) {
            buffer[3+i] = shifted_pwm_get_value(i);
        }
        const esp_err_t status = websocket_send_pending_binary_data_sync(client, request, 3+SHIFTED_PWM_TOTAL_PINS);
        if (status != ESP_OK) {
            ESP_LOGE(SUBTAG, "failed to send shifted pwm values: err='%s'", esp_err_to_name(status));
        }
    } else if (mode == LED_SET) {
        for (int i = 1; i < size-1; i+=2) {
            const uint8_t pin = data[i];
            const uint8_t value = data[i+1];
            if (pin < SHIFTED_PWM_TOTAL_PINS) {
                shifted_pwm_set_value(pin, value);
            }
        }
        // disable reply since it slows things down
        // reply_buffer[0] = LED_CMD;
        // reply_buffer[1] = LED_SET;
        // write_websocket_data(request, reply_buffer, 2);
    } else {
        ESP_LOGW(SUBTAG, "got unhandled shifted pwm command header=%u", mode);
    }
}

void websocket_async_send_pc_io_status(struct WebsocketClient* client, void *args) {
    static const char SUBTAG[] = "pc-io-status-async-websocket-handler";
    assert(client != NULL);
    const struct Websocket* websocket = client->websocket;
    assert(websocket != NULL);
    uint8_t* buffer = websocket->transmit_buffer;
    assert(buffer != NULL);

    const bool is_powered = (bool)args;
    buffer[0] = PC_IO_CMD;
    buffer[1] = PC_IO_STATUS;
    buffer[2] = is_powered ? 0x01 : 0x00;
    const esp_err_t status = websocket_send_pending_binary_data_async(client, 3);
    if (status != ESP_OK) {
        ESP_LOGE(SUBTAG, "Failed to send pc io status websocket_fd=%d, is_powered=%u, error='%s'",
            client->websocket_fd, is_powered, esp_err_to_name(status)
        );
    } else {
        ESP_LOGI(SUBTAG, "Sent pc io status to websocket_fd=%d, is_powered=%u", client->websocket_fd, is_powered);
    }
}

static void pc_io_status_listener(bool is_powered, void* _client) {
    static const char SUBTAG[] = "pc-io-status-interrupt-listener-websocket-handler";
    struct WebsocketClient *client = (struct WebsocketClient*)_client;
    assert(client != NULL);
    const esp_err_t status = websocket_queue_async_task(client, websocket_async_send_pc_io_status, (void*)is_powered);
    if (status != ESP_OK) {
        ESP_LOGE(SUBTAG, "failed to queue async pc io status: websocket_fd=%d, is_powered=%u, error='%s'",
            client->websocket_fd, is_powered, esp_err_to_name(status)
        );
    }
}

static void websocket_on_open(httpd_req_t* request, struct WebsocketClient* client) {
    pc_io_status_listen(&g_pc_io_config, pc_io_status_listener, (void*)client);
}

static void websocket_on_binary_frame(httpd_req_t* request, struct WebsocketClient* client, const uint8_t* data, size_t size) {
    static const char SUBTAG[] = "binary-frame-dispatcher-websocket-handler";
    assert(client != NULL);
    assert(data != NULL);

    if (size == 0) {
        ESP_LOGE(SUBTAG, "Got an unexpected empty command buffer");
        return;
    }

    const uint8_t cmd_code = data[0];
    const uint8_t *cmd_data = &data[1];
    int cmd_length = size-1;

    switch (cmd_code) {
    case LED_CMD:   websocket_on_shifted_pwm_frame(request, client, cmd_data, cmd_length); break;
    case PC_IO_CMD: websocket_on_pc_io_frame(request, client, cmd_data, cmd_length); break;
    case DHT11_CMD: websocket_on_dht11_frame(request, client, cmd_data, cmd_length); break;
    default:        ESP_LOGD(TAG, "Unknown cmd: 0x%02x", cmd_code); break;
    }
}

// client will be freed after this call
static void websocket_on_close(httpd_req_t* request, struct WebsocketClient* client) {
    pc_io_status_unlisten(&g_pc_io_config, pc_io_status_listener, (void*)client);
}

void websocket_attach_handlers(struct Websocket* websocket) {
    assert(websocket != NULL);
    websocket->on_open = websocket_on_open;
    websocket->on_binary_frame = websocket_on_binary_frame;
    websocket->on_close = websocket_on_close;
}