#ifndef __WEBSOCKET_H__
#define __WEBSOCKET_H__

#include <httpd_server/esp_http_server.h>
#include <esp_err.h>

// basic websocket implementation that keeps track of clients
struct Websocket;

struct WebsocketClient {
    struct Websocket* websocket;
    int websocket_fd;
    struct WebsocketClient* next_client;
};

struct Websocket {
    // buffers
    size_t receive_buffer_size;
    size_t transmit_buffer_size;
    uint8_t* receive_buffer;
    uint8_t* transmit_buffer;
    // handles
    const char* uri;
    httpd_handle_t server;
    struct WebsocketClient* clients;
    // callbacks
    void (*on_binary_frame)(httpd_req_t* request, struct WebsocketClient* client, const uint8_t* data, size_t size);
    void (*on_open)(httpd_req_t* request, struct WebsocketClient* client);
    // client will be freed after this call
    void (*on_close)(httpd_req_t* request, struct WebsocketClient* client);
};

esp_err_t websocket_register(httpd_handle_t server, struct Websocket* websocket, size_t buffer_size);
size_t websocket_count_total_clients(struct Websocket* websocket);
esp_err_t websocket_send_pending_binary_data_sync(struct WebsocketClient* client, httpd_req_t* request, size_t size);
esp_err_t websocket_send_pending_binary_data_async(struct WebsocketClient* client, size_t size);

typedef void (*websocket_async_task_t)(struct WebsocketClient* client, void* args);
esp_err_t websocket_queue_async_task(struct WebsocketClient* client, websocket_async_task_t task, void* args);

#endif
