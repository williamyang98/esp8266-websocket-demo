/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <rom/ets_sys.h>
#include <nvs_flash.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_spi_flash.h>
#include <esp_spiffs.h>
#include <esp_system.h>

#include "dht11.h"
#include "global_periphs.h"
#include "pc_io.h"
#include "shifted_pwm.h"

#include "webserver.h"
#include "websocket_handler.h"
#include "wifi_sta.h"

#define INIT_TAG "main-init"

const gpio_num_t dht11_data_pin = GPIO_NUM_2; // extern
struct PC_IO_Config pc_io_config = { // extern
    // gpio setup
    .power_pin = GPIO_NUM_5,
    .power_func = FUNC_GPIO5,
    .reset_pin = GPIO_NUM_4,
    .reset_func = FUNC_GPIO4,
    .status_pin = GPIO_NUM_12,
    .status_func = FUNC_GPIO12,
};

static httpd_handle_t http_server = NULL;

static httpd_uri_t websocket_uri = {
    .uri = "/api/v1/websocket",
    .method = HTTP_GET,
    .handler = websocket_handler,
    .user_ctx = NULL,
    .is_websocket = true,
    .handle_ws_control_frames = true,
    .supported_subprotocol = NULL,
};

static esp_err_t init_nvs(void);
static esp_err_t init_server(void);

void app_main()
{
    ESP_LOGI(INIT_TAG, "entering main function!");

    if (dht11_init(dht11_data_pin) == ESP_OK) {
        ESP_LOGI(INIT_TAG, "initialised dht11 sensor on pin: %u", dht11_data_pin);
    } else {
        ESP_LOGE(INIT_TAG, "failed to initialise dht11 sensor on pin: %u", dht11_data_pin);
    }

    if (pc_io_init(&pc_io_config) == ESP_OK) {
        ESP_LOGI(INIT_TAG, "initialised pc io");
    } else {
        ESP_LOGE(INIT_TAG, "failed to initialise pc io");
    }

    shifted_pwm_init();
    ESP_LOGI(INIT_TAG, "initialised led gpio");
    for (int i = 0; i < 8; i++) {
        set_pwm_value(i, 0);
    }

    init_nvs();
    wifi_init_sta();
    init_server();

    // LINK: https://esp32.com/viewtopic.php?p=6023&sid=48c7254ec4cbe0d99f743e1d3687894d#p6023
    //       vTaskStartScheduler() is already called before app_main() so don't call it again
    // vTaskStartScheduler();
    // ESP_LOGI(INIT_TAG, "starting task scheduler!");

    ESP_LOGI(INIT_TAG, "finished initialisation");
}

esp_err_t init_server(void) {
    // startup webserver
    const uint16_t port = 80;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;

    const esp_err_t start_status = httpd_start(&http_server, &config);
    if (start_status == ESP_OK) {
        ESP_LOGI(INIT_TAG, "created http server on port=%d", port);
    } else {
        ESP_LOGE(INIT_TAG, "failed to start webserver on port=%d (%s)", port, esp_err_to_name(start_status));
        return ESP_FAIL;
    }
    
    const esp_err_t register_status = webserver_register_endpoints(http_server);
    if (register_status == ESP_OK) {
        ESP_LOGI(INIT_TAG, "registered webserver endpoints on port=%d", port);
    } else {
        ESP_LOGE(INIT_TAG, "failed to register endpoints on port=%d (%s)", port, esp_err_to_name(register_status));
        return ESP_FAIL;
    }

    const esp_err_t websocket_register_status = httpd_register_uri_handler(http_server, &websocket_uri);
    if (websocket_register_status == ESP_OK) {
        ESP_LOGI(INIT_TAG, "registered websocket handler on port=%d", port);
    } else {
        ESP_LOGE(INIT_TAG, "failed to register websocket handler on port=%d (%s)", port, esp_err_to_name(register_status));
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t init_nvs(void) {
    const esp_err_t nvs_status = nvs_flash_init();
    if (nvs_status == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_LOGI(INIT_TAG, "no free NVS pages, erasing and reinitialising");
        nvs_flash_erase();
        nvs_flash_init();
    }
    ESP_LOGI(INIT_TAG, "starting NVS!");
    return ESP_OK;
}
