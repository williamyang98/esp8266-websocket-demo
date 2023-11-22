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
#include "websocket_handler.h"

#define INIT_TAG "main-init"

static httpd_handle_t http_server = NULL;

static httpd_uri_t websocket_uri = {
    .uri = "/api/v1/websocket",
    .method = HTTP_GET,
    .handler = websocket_handler,
    .user_ctx = NULL,
    .is_websocket = true,
    .handle_ws_control_frames = false, // let backend handle control frames
    .supported_subprotocol = NULL,
};

void app_main()
{
    ESP_LOGI(INIT_TAG, "entering main function!");
    shifted_pwm_init();

    ESP_LOGI(INIT_TAG, "starting NVS!");
    esp_err_t nvs_status = nvs_flash_init();
    if (nvs_status == ESP_ERR_NVS_NO_FREE_PAGES) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    wifi_init_sta();

    if (dht11_init(&dht11_sensor) == ESP_OK) {
        ESP_LOGI(INIT_TAG, "initialised dht11 sensor on pin: %u", dht11_sensor.pin_number);
    } else {
        ESP_LOGE(INIT_TAG, "failed to initialise dht11 sensor on pin: %u", dht11_sensor.pin_number);
    }
    pc_io_init();
    pc_io_interrupt_init();
    for (int i = 0; i < 8; i++) {
        set_pwm_value(i, 0);
    }
    ESP_LOGI(INIT_TAG, "setup sensors");

    // startup webserver
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&http_server, &config) == ESP_OK) {
        ESP_LOGI(INIT_TAG, "created http server on port: %d", config.server_port);
    } else {
        ESP_LOGE(INIT_TAG, "failed to start webserver on port: %d", config.server_port);
        return;
    }

    if (webserver_register_endpoints(http_server) == ESP_OK) {
        ESP_LOGI(INIT_TAG, "registered webserver endpoints on port: %d", config.server_port);
    } else {
        ESP_LOGE(INIT_TAG, "failed to register endpoints on port: %d", config.server_port);
        return;
    }

    httpd_register_uri_handler(http_server, &websocket_uri);
    ESP_LOGI(INIT_TAG, "registered websocket handler on port: %d", config.server_port);

    // vTaskStartScheduler();
    // ESP_LOGI(INIT_TAG, "starting task scheduler!");

    ESP_LOGI(INIT_TAG, "finished initialisation");
}