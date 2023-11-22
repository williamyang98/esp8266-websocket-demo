/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <rom/ets_sys.h>
#include <esp_system.h>
#include <esp_spi_flash.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <nvs_flash.h>

#include "global_periphs.h"
#include "shifted_pwm.h"
#include "wifi_sta.h"
#include "pc_io.h"
#include "pc_io_interrupt.h"
#include "dht11.h"

#include "webserver.h"
#include "websocket.h"
#include "websocket_listener.h"

#define INIT_TAG "main-init"

static httpd_handle_t http_server = NULL;

static websocket_ctx websocket_uri_context = {
    .on_start = listen_websocket_start,
    .on_recieve = listen_websocket_data,
    .on_exit = listen_websocket_exit,
};

static httpd_uri_t websocket_uri = {
    .uri = "/api/v1/websocket",
    .method = HTTP_GET,
    .handler = websocket_handler,
    .user_ctx = &websocket_uri_context
};

void app_main()
{
    ESP_LOGI(INIT_TAG, "Entering main function!\n");
    shifted_pwm_init();

    ESP_LOGI(INIT_TAG, "Starting NVS!\n");
    esp_err_t nvs_status = nvs_flash_init();
    if (nvs_status == ESP_ERR_NVS_NO_FREE_PAGES) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    wifi_init_sta();

    dht11_init(&dht11_sensor);
    pc_io_init();
    pc_io_interrupt_init();
    for (int i = 0; i < 8; i++) {
        set_pwm_value(i, 0);
    }
    ESP_LOGI(INIT_TAG, "setup sensors");

    // startup webserver
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    uint32_t server_port = 80;
    config.server_port = server_port;
    config.ctrl_port = server_port;
    config.recv_wait_timeout = 60*60*1; // 1 hour timeout for websocket
    config.lru_purge_enable = true;

    if (httpd_start(&http_server, &config) == ESP_OK) {
        ESP_LOGI(INIT_TAG, "created http server on port: %d", server_port);
    } else {
        ESP_LOGE(INIT_TAG, "failed to start webserver on port: %d", server_port);
        return;
    }

    if (webserver_register_endpoints(http_server) == ESP_OK) {
        ESP_LOGI(INIT_TAG, "registered webserver endpoints on port: %d", server_port);
    } else {
        ESP_LOGE(INIT_TAG, "failed to register endpoints on port: %d", server_port);
        return;
    }

    httpd_register_uri_handler(http_server, &websocket_uri);
    ESP_LOGI(INIT_TAG, "registered websocket handler on port: %d", server_port);

    // vTaskStartScheduler();
    // ESP_LOGI(INIT_TAG, "Starting task scheduler!\n");
    ESP_LOGI(INIT_TAG, "finished initialisation");
}