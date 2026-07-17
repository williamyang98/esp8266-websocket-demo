#include "websocket_handler.h"

#include "global_periphs.h"
#include "shifted_pwm.h"
#include "pc_io.h"
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

static const char TAG[] = "websocket-handler";

#define REPLY_BUFFER_SIZE 100
static uint8_t receive_buffer[REPLY_BUFFER_SIZE] = {0};
static uint8_t reply_buffer[REPLY_BUFFER_SIZE] = {0};

// websocket handlers
static esp_err_t listen_websocket_open(httpd_req_t *request);
static esp_err_t listen_websocket_close(httpd_req_t *request);
static esp_err_t listen_websocket_binary_data(httpd_req_t *request, uint8_t *data, int length);
static esp_err_t write_websocket_binary_data(httpd_req_t *request, uint8_t *data, int length);
static esp_err_t listen_websocket_ping(httpd_req_t *request) {
    httpd_ws_frame_t pong_frame;
    pong_frame.final = true;
    pong_frame.fragmented = false;
    pong_frame.type = HTTPD_WS_TYPE_PONG;
    pong_frame.payload = NULL;
    pong_frame.len = 0;
    return httpd_ws_send_frame(request, &pong_frame);
}

// keep track of clients
struct WebsocketClient {
    httpd_handle_t server_handle;
    int websocket_fd;
    struct WebsocketClient* next_client;
};

static struct WebsocketClient* clients = NULL;
static struct WebsocketClient* dht11_websocket_client = NULL;

static struct WebsocketClient* add_websocket_client(int websocket_fd) {
    struct WebsocketClient** head = &clients;
    while (*head != NULL) {
        if ((*head)->websocket_fd == websocket_fd) {
            ESP_LOGE(TAG, "Tried to add websocket client with fd=%d again", websocket_fd);
            return NULL;
        }
        head = &((*head)->next_client);
    }

    struct WebsocketClient* client = malloc(sizeof(struct WebsocketClient));
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to create websocket client instance");
        return NULL;
    }
    client->server_handle = NULL;
    client->websocket_fd = websocket_fd;
    client->next_client = NULL;
    *head = client;
    return client;
}

static struct WebsocketClient* pop_websocket_client(int websocket_fd) {
    struct WebsocketClient** head = &clients;
    while (*head != NULL) {
        struct WebsocketClient* client = *head;
        if (client->websocket_fd == websocket_fd) {
            *head = client->next_client;
            client->next_client = NULL;
            return client;
        }
        head = &((*head)->next_client);
    }
    return NULL;
}

static struct WebsocketClient* get_websocket_client(int websocket_fd) {
    struct WebsocketClient* head = clients;
    while (head != NULL) {
        if (head->websocket_fd == websocket_fd) break;
        head = head->next_client;
    }
    return head;
}

static size_t count_total_websocket_clients(void) {
    size_t total = 0;
    struct WebsocketClient* head = clients;
    while (head != NULL) {
        total++;
        head = head->next_client;
    }
    return total;
}

// peripherals
static void handle_dht11(httpd_req_t *request, uint8_t *data, int length);
static void handle_pc_io(httpd_req_t *request, uint8_t *data, int length);
static void handle_led(httpd_req_t *request, uint8_t *data, int length);
static void async_send_dht11(void *ignore);

// attach listener to interrupt which then queues asynchronously to httpd server
static void pc_io_status_listener(bool is_powered, void *args);
static void async_send_pc_io_status(void *args);

struct Async_PC_IO_Status {
    struct WebsocketClient *client;
    bool is_powered;
};

esp_err_t websocket_handler(httpd_req_t *request) {
    if (request->method == HTTP_GET) {
        listen_websocket_open(request);
        return ESP_OK;
    }

    httpd_ws_frame_t frame;
    frame.final = false;
    frame.fragmented = false;
    frame.type = HTTPD_WS_TYPE_BINARY;
    frame.payload = receive_buffer;
    frame.len = 0;

    esp_err_t res;
    // get frame length by passing 0
    res = httpd_ws_recv_frame(request, &frame, 0);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "failed to get frame len with error: %d", res);
        return ESP_FAIL;
    }
    if (frame.len > REPLY_BUFFER_SIZE) {
        ESP_LOGE(TAG, "frame len is above maximum: %u > %d", frame.len, REPLY_BUFFER_SIZE);
        return ESP_FAIL;
    }
    // read full frame
    if (frame.len > 0) {
        res = httpd_ws_recv_frame(request, &frame, frame.len);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "failed to get frame with len %u, error=%d", frame.len, res);
            return ESP_FAIL;
        }
    }

    switch (frame.type) {
        case HTTPD_WS_TYPE_BINARY: return listen_websocket_binary_data(request, frame.payload, frame.len);
        case HTTPD_WS_TYPE_CLOSE: return listen_websocket_close(request);
        case HTTPD_WS_TYPE_PING: return listen_websocket_ping(request);
        default: {
            ESP_LOGW(TAG, "Unhandled websocket frame type: %u", frame.type);
            return ESP_OK;
        }
    }
}

esp_err_t listen_websocket_binary_data(httpd_req_t *request, uint8_t *data, int length) {
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

esp_err_t write_websocket_binary_data(httpd_req_t *request, uint8_t *data, int length) {
    httpd_ws_frame_t ws_pkt = {
        .fragmented = false,
        .final = true,
        .type = HTTPD_WS_TYPE_BINARY,
        .payload = data,
        .len = length 
    };
    return httpd_ws_send_frame(request, &ws_pkt);
}

esp_err_t listen_websocket_open(httpd_req_t *request) {
    const int websocket_fd = httpd_req_to_sockfd(request);
    ESP_LOGI(TAG, "opening websocket connection with socket_id=%d", websocket_fd);
    struct WebsocketClient* client = add_websocket_client(websocket_fd);
    if (client == NULL) {
        return ESP_FAIL;
    }
    client->server_handle = request->handle;
    const size_t total_clients = count_total_websocket_clients();
    ESP_LOGI(TAG, "%u total websocket clients after adding one", total_clients);
    return pc_io_status_listen(&pc_io_config, pc_io_status_listener, (void*)client);
}

esp_err_t listen_websocket_close(httpd_req_t *request) {
    const int websocket_fd = httpd_req_to_sockfd(request);
    ESP_LOGI(TAG, "closing websocket connection with socket_id=%d", websocket_fd);
    struct WebsocketClient* client = pop_websocket_client(websocket_fd);
    if (client == NULL) {
        ESP_LOGE(TAG, "tried to remove untracked websocket client with fd=%d", websocket_fd);
        return ESP_FAIL;
    }

    pc_io_status_unlisten(&pc_io_config,pc_io_status_listener, (void *)client);
    free(client);

    const size_t remaining_clients = count_total_websocket_clients();
    ESP_LOGI(TAG, "%u remaining websocket clients after closing one", remaining_clients);
    return ESP_OK;
}

void handle_dht11(httpd_req_t *request, uint8_t *data, int length) {
    static const char SUBTAG[] = "dht11-websocket";
    ESP_LOGD(SUBTAG, "Got request");
    if (dht11_websocket_client != NULL) {
        ESP_LOGI(SUBTAG, "ignored request since still reading values from websocket_fd=%d", dht11_websocket_client->websocket_fd);
        return;
    } 

    const int websocket_fd = httpd_req_to_sockfd(request);
    dht11_websocket_client = get_websocket_client(websocket_fd);
    if (dht11_websocket_client == NULL) {
        ESP_LOGE(SUBTAG, "failed to find websocket client with fd=%d", websocket_fd);
        return;
    }

    esp_err_t res = httpd_queue_work(request->handle, async_send_dht11, NULL);
    if (res != ESP_OK) {
        ESP_LOGE(SUBTAG, "failed to queue async sensor task");
        dht11_websocket_client = NULL;
        return;
    }
}

void async_send_dht11(void *ignore) {
    static const char SUBTAG[] = "dht11-async-websocket";

    uint8_t data[3];
    size_t length = 0;
    data[0] = DHT11_CMD;
    struct DHT11_Measurement measurement;
    const bool is_read_success = dht11_read(dht11_data_pin, &measurement) == ESP_OK;
    if (is_read_success) {
        data[1] = measurement.humidity;
        data[2] = measurement.temperature;
        length = 3;
    } else {
        data[1] = 0xFF;
        length = 2;
    }

    httpd_ws_frame_t ws_pkt = {
        .fragmented = false,
        .final = true,
        .type = HTTPD_WS_TYPE_BINARY,
        .payload = data,
        .len = length 
    };
    if (dht11_websocket_client == NULL) {
        ESP_LOGE(SUBTAG, "No websocket client to sent dht11 to");
        return;
    }
    const esp_err_t status = httpd_ws_send_frame_async(dht11_websocket_client->server_handle, dht11_websocket_client->websocket_fd, &ws_pkt);
    dht11_websocket_client = NULL;
    if (status == ESP_OK) {
        if (is_read_success) {
            ESP_LOGI(SUBTAG, "Sent dht11 data: humidity=%u, temperature=%u", measurement.humidity, measurement.temperature);
        } else {
            ESP_LOGI(SUBTAG, "Sent dht11 read fail response");
        }
    } else {
        ESP_LOGE(SUBTAG, "Failed to send dht11 data: '%s'", esp_err_to_name(status));
    }
}

void handle_pc_io(httpd_req_t *request, uint8_t *data, int length) {
    static const char SUBTAG[] = "pc-io-websocket";
    if (length < 1) {
        return;
    }
    uint8_t cmd = data[0];
    ESP_LOGD("pc-io-websocket", "Got command: 0x%02x", cmd);
    esp_err_t resp_status = ESP_OK;
    switch (cmd) {
    case PC_IO_OFF:     resp_status = pc_io_power_off(&pc_io_config); break;
    case PC_IO_ON:      resp_status = pc_io_power_on(&pc_io_config); break;
    case PC_IO_RESET:   resp_status = pc_io_reset(&pc_io_config); break;
    case PC_IO_STATUS:  pc_io_is_powered(&pc_io_config) ? (resp_status = ESP_OK) : (resp_status = ESP_FAIL); break;
    default:            ESP_LOGE(SUBTAG, "Unknown command: 0x%02x", cmd); return;
    }

    reply_buffer[0] = PC_IO_CMD;
    reply_buffer[1] = cmd;
    if (resp_status == ESP_OK) {
        reply_buffer[2] = 0x01;
    } else {
        reply_buffer[2] = 0x00;
    }
    write_websocket_binary_data(request, reply_buffer, 3);
}

void pc_io_status_listener(bool is_powered, void *args) {
    static const char SUBTAG[] = "pc-io-status-interrupt-listener-websocket";
    struct WebsocketClient *client = (struct WebsocketClient *)args;
    if (client == NULL) {
        ESP_LOGE(SUBTAG, "No websocket client provided");
        return;
    }

    struct Async_PC_IO_Status* data = malloc(sizeof(struct Async_PC_IO_Status));
    if (data == NULL) {
        ESP_LOGE(SUBTAG, "Failed to create async argument for fd=%d", client->websocket_fd);
        return;
    }
    data->client = client;
    data->is_powered = is_powered;
    esp_err_t res = httpd_queue_work(client->server_handle, async_send_pc_io_status, (void*)data);
    if (res != ESP_OK) {
        ESP_LOGE(SUBTAG, "failed to queue async sensor task");
        dht11_websocket_client = NULL;
        return;
    }
}

void async_send_pc_io_status(void *args) {
    static const char SUBTAG[] = "pc-io-status-async-websocket";
    struct Async_PC_IO_Status* data = (struct Async_PC_IO_Status*)args;
    if (data == NULL) {
        ESP_LOGE(SUBTAG, "No status provided");
        return;
    }

    reply_buffer[0] = PC_IO_CMD;
    reply_buffer[1] = PC_IO_STATUS;
    reply_buffer[2] = data->is_powered ? 0x01 : 0x00;
    httpd_ws_frame_t frame = {
        .fragmented = false,
        .final = true,
        .type = HTTPD_WS_TYPE_BINARY,
        .payload = reply_buffer,
        .len = 3 
    };

    const struct WebsocketClient* client = data->client;
    const esp_err_t status = httpd_ws_send_frame_async(client->server_handle, client->websocket_fd, &frame);
    if (status == ESP_OK) {
        ESP_LOGI(SUBTAG, "Sent pc io button interrupt update: websocket_fd=%d, is_powered=%u", client->websocket_fd, data->is_powered);
    } else {
        ESP_LOGE(SUBTAG, "Failed to button interrupt update: websocket_fd=%d, err='%s'", client->websocket_fd, esp_err_to_name(status));
    }
    free(data);
}

void handle_led(httpd_req_t *request, uint8_t *data, int length) {
    if (length < 1) {
        return;
    }
    uint8_t mode = data[0];
    if (mode == LED_GET) {
        reply_buffer[0] = LED_CMD;
        reply_buffer[1] = LED_GET;
        reply_buffer[2] = SHIFTED_PWM_TOTAL_PINS;
        for (int i = 0; i < SHIFTED_PWM_TOTAL_PINS; i++) {
            reply_buffer[3+i] = shifted_pwm_get_value(i);
        }
        write_websocket_binary_data(request, reply_buffer, 3+SHIFTED_PWM_TOTAL_PINS);
    } else if (mode == LED_SET) {
        for (int i = 1; i < length-1; i+=2) {
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
    }
}