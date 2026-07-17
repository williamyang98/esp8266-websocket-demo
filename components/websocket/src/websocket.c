#include "websocket.h"
#include <esp_err.h>
#include <esp_log.h>

static const char TAG[] = "websocket";

static struct WebsocketClient* add_websocket_client(struct Websocket* websocket, int websocket_fd) {
    assert(websocket != NULL);
    struct WebsocketClient** head = &(websocket->clients);
    while (*head != NULL) {
        if ((*head)->websocket_fd == websocket_fd) {
            ESP_LOGE(TAG, "Tried to add websocket client with fd=%d again", websocket_fd);
            return NULL;
        }
        head = &((*head)->next_client);
    }

    struct WebsocketClient* client = malloc(sizeof(struct WebsocketClient));
    assert(client != NULL);
    client->websocket = websocket;
    client->websocket_fd = websocket_fd;
    client->next_client = NULL;
    *head = client;
    return client;
}

static struct WebsocketClient* pop_websocket_client(struct Websocket* websocket, int websocket_fd) {
    assert(websocket != NULL);
    struct WebsocketClient** head = &(websocket->clients);
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

static struct WebsocketClient* get_websocket_client(struct Websocket* websocket, int websocket_fd) {
    assert(websocket != NULL);
    struct WebsocketClient* head = websocket->clients;
    while (head != NULL) {
        if (head->websocket_fd == websocket_fd) break;
        head = head->next_client;
    }
    return head;
}

size_t websocket_count_total_clients(struct Websocket* websocket) {
    assert(websocket != NULL);
    size_t total = 0;
    struct WebsocketClient* head = websocket->clients;
    while (head != NULL) {
        total++;
        head = head->next_client;
    }
    return total;
}

static esp_err_t websocket_handle_open(httpd_req_t* request) {
    assert(request != NULL);
    struct Websocket* websocket = (struct Websocket*)request->user_ctx;
    assert(websocket != NULL);

    const int websocket_fd = httpd_req_to_sockfd(request);
    ESP_LOGI(TAG, "opening websocket connection with socket_id=%d", websocket_fd);
    struct WebsocketClient* client = add_websocket_client(websocket, websocket_fd);
    if (client == NULL) {
        return ESP_FAIL;
    }
    const size_t total_clients = websocket_count_total_clients(websocket);
    ESP_LOGI(TAG, "%u total websocket clients after adding one", total_clients);
    if (websocket->on_open != NULL) websocket->on_open(request, client);
    return ESP_OK;
}

static esp_err_t websocket_handle_close(httpd_req_t* request) {
    assert(request != NULL);
    struct Websocket* websocket = (struct Websocket*)request->user_ctx;
    assert(websocket != NULL);

    const int websocket_fd = httpd_req_to_sockfd(request);
    ESP_LOGI(TAG, "closing websocket connection with socket_id=%d", websocket_fd);
    struct WebsocketClient* client = pop_websocket_client(websocket, websocket_fd);
    if (client == NULL) {
        ESP_LOGE(TAG, "tried to remove untracked websocket client with fd=%d", websocket_fd);
        return ESP_FAIL;
    }

    if (websocket->on_close != NULL) websocket->on_close(request, client);
    free(client);
    const size_t remaining_clients = websocket_count_total_clients(websocket);
    ESP_LOGI(TAG, "%u remaining websocket clients after closing one", remaining_clients);
    return ESP_OK;
}

static esp_err_t websocket_handle_binary_data(httpd_req_t* request, uint8_t *data, int length) {
    assert(request != NULL);
    struct Websocket* websocket = (struct Websocket*)request->user_ctx;
    assert(websocket != NULL);

    const int websocket_fd = httpd_req_to_sockfd(request);
    struct WebsocketClient* client = get_websocket_client(websocket, websocket_fd);
    if (client == NULL) {
        ESP_LOGE(TAG, "failed to find websocket client with fd=%d", websocket_fd);
        return ESP_FAIL;
    }
    if (websocket->on_binary_frame != NULL) websocket->on_binary_frame(request, client, data, length);
    return ESP_OK;
}

static esp_err_t websocket_handle_ping(httpd_req_t* request) {
    assert(request != NULL);
    httpd_ws_frame_t pong_frame;
    pong_frame.final = true;
    pong_frame.fragmented = false;
    pong_frame.type = HTTPD_WS_TYPE_PONG;
    pong_frame.payload = NULL;
    pong_frame.len = 0;
    return httpd_ws_send_frame(request, &pong_frame);
}

static esp_err_t websocket_uri_handler(httpd_req_t* request) {
    assert(request != NULL);
    struct Websocket* websocket = (struct Websocket*)request->user_ctx;
    assert(websocket != NULL);
    assert(websocket->server != NULL);
    assert(websocket->server == request->handle);
    assert(websocket->receive_buffer != NULL);
    assert(websocket->transmit_buffer != NULL);
    assert(websocket->receive_buffer_size > 0);
    assert(websocket->transmit_buffer_size > 0);

    if (request->method == HTTP_GET) {
        return websocket_handle_open(request);
    }

    httpd_ws_frame_t frame;
    frame.final = false;
    frame.fragmented = false;
    frame.type = HTTPD_WS_TYPE_BINARY;
    frame.payload = websocket->receive_buffer;
    frame.len = 0;

    // get frame length by passing 0
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_ws_recv_frame(request, &frame, 0));
    if (frame.len > websocket->receive_buffer_size) {
        ESP_LOGE(TAG, "frame len is above maximum: %u > %u", frame.len, websocket->receive_buffer_size);
        return ESP_FAIL;
    }
    // read full frame
    if (frame.len > 0) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_ws_recv_frame(request, &frame, frame.len));
    }

    switch (frame.type) {
        case HTTPD_WS_TYPE_BINARY: return websocket_handle_binary_data(request, frame.payload, frame.len);
        case HTTPD_WS_TYPE_CLOSE: return websocket_handle_close(request);
        case HTTPD_WS_TYPE_PING: return websocket_handle_ping(request);
        default: {
            ESP_LOGW(TAG, "Unhandled websocket frame type: %u", frame.type);
            return ESP_OK;
        }
    }
}

esp_err_t websocket_register(httpd_handle_t server, struct Websocket* websocket, size_t buffer_size) {
    assert(server != NULL);
    assert(websocket != NULL);
    assert(websocket->uri != NULL);

    websocket->receive_buffer = malloc(buffer_size);
    assert(websocket->receive_buffer != NULL);
    websocket->receive_buffer_size = buffer_size;
    websocket->transmit_buffer = malloc(buffer_size);
    assert(websocket->transmit_buffer != NULL);
    websocket->transmit_buffer_size = buffer_size;
    websocket->server = server;

    httpd_uri_t websocket_uri = {
        .uri = websocket->uri,
        .method = HTTP_GET,
        .handler = websocket_uri_handler,
        .user_ctx = (void*)websocket,
        .is_websocket = true,
        .handle_ws_control_frames = true, // handle httpd_ws_frame_t.type by ourselves
        .supported_subprotocol = NULL,
    };
    return httpd_register_uri_handler(server, &websocket_uri);
}

esp_err_t websocket_send_pending_binary_data_sync(struct WebsocketClient* client, httpd_req_t* request, size_t size) {
    assert(client != NULL);
    struct Websocket* websocket = client->websocket;
    assert(websocket != NULL);
    assert(websocket->transmit_buffer != NULL);
    assert(websocket->transmit_buffer_size >= size);
    httpd_ws_frame_t frame = {
        .fragmented = false,
        .final = true,
        .type = HTTPD_WS_TYPE_BINARY,
        .payload = websocket->transmit_buffer,
        .len = size,
    };
    return httpd_ws_send_frame(request, &frame);
}

esp_err_t websocket_send_pending_binary_data_async(struct WebsocketClient* client, size_t size) {
    assert(client != NULL);
    struct Websocket* websocket = client->websocket;
    assert(websocket != NULL);
    assert(websocket->transmit_buffer != NULL);
    assert(websocket->transmit_buffer_size >= size);
    httpd_handle_t server = websocket->server;
    assert(server != NULL);
    httpd_ws_frame_t frame = {
        .fragmented = false,
        .final = true,
        .type = HTTPD_WS_TYPE_BINARY,
        .payload = websocket->transmit_buffer,
        .len = size,
    };
    return httpd_ws_send_frame_async(server, client->websocket_fd, &frame);
}

struct WebsocketAsyncTaskEntry {
    struct WebsocketClient* client;
    websocket_async_task_t task;
    void* args;
};

static void websocket_run_enqueued_async_task(void* _entry) {
    struct WebsocketAsyncTaskEntry* entry = (struct WebsocketAsyncTaskEntry*)_entry;
    assert(entry != NULL);

    struct WebsocketClient* client = entry->client;
    websocket_async_task_t task = entry->task;
    void* args = entry->args;
    free(entry);

    assert(client != NULL);
    assert(task != NULL);
    task(client, args);
}

esp_err_t websocket_queue_async_task(struct WebsocketClient* client, websocket_async_task_t task, void* args) {
    assert(client != NULL);
    assert(task != NULL);

    struct Websocket* websocket = client->websocket;
    assert(websocket != NULL);
    httpd_handle_t server = websocket->server;
    assert(server != NULL);

    struct WebsocketAsyncTaskEntry* entry = malloc(sizeof(struct WebsocketAsyncTaskEntry));
    assert(entry != NULL);
    entry->client = client;
    entry->task = task;
    entry->args = args;
    const esp_err_t status = httpd_queue_work(server, websocket_run_enqueued_async_task, entry);
    if (status != ESP_OK) {
        free(entry);
    }
    return status;
}
