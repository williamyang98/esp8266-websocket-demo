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

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#include "dht11.h"
#include "global_periphs.h"
#include "pc_io.h"
#include "pc_io_interrupt.h"
#include "shifted_pwm.h"

#include "webserver.h"
#include "websocket_handler.h"
#include "wifi_sta.h"

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

static esp_err_t init_nvs(void);
static esp_err_t init_spiffs(void);
static esp_err_t init_server(void);

void app_main()
{
    ESP_LOGI(INIT_TAG, "entering main function!");

    if (dht11_init(&dht11_sensor) == ESP_OK) {
        ESP_LOGI(INIT_TAG, "initialised dht11 sensor on pin: %u", dht11_sensor.pin_number);
    } else {
        ESP_LOGE(INIT_TAG, "failed to initialise dht11 sensor on pin: %u", dht11_sensor.pin_number);
    }

    shifted_pwm_init();
    ESP_LOGI(INIT_TAG, "initialised led gpio");

    pc_io_init();
    pc_io_interrupt_init();
    ESP_LOGI(INIT_TAG, "initialised pc io gpio and interrupts");

    init_nvs(); 
    // init_spiffs();
    wifi_init_sta();
    init_server();

    // vTaskStartScheduler();
    // ESP_LOGI(INIT_TAG, "starting task scheduler!");

    ESP_LOGI(INIT_TAG, "finished initialisation");
}

esp_err_t init_spiffs(void) {
    esp_vfs_spiffs_conf_t spiffs_config = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true,
    };
    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    const esp_err_t spiffs_register_status = esp_vfs_spiffs_register(&spiffs_config);
    switch (spiffs_register_status) {
    case ESP_OK:   
        ESP_LOGI(INIT_TAG, "initialising spiffs filesystem"); 
        break;
    case ESP_FAIL: 
        ESP_LOGE(INIT_TAG, "failed to mount or format spiffs filesystem"); 
        break;
    case ESP_ERR_NOT_FOUND: 
        ESP_LOGE(INIT_TAG, "failed to find spiffs partition");
        break;
    default:
        ESP_LOGE(INIT_TAG, "failed to initialize spiffs (%s)", esp_err_to_name(spiffs_register_status));
        break;
    }
    if (spiffs_register_status != ESP_OK) {
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    const esp_err_t spiffs_info_status = esp_spiffs_info(NULL, &total, &used);
    if (spiffs_info_status == ESP_OK) {
        ESP_LOGI(INIT_TAG, "got spiffs partition size: total=%d, used=%d", total, used);
    } else {
        ESP_LOGE(INIT_TAG, "failed to get spiffs partition information (%s)", esp_err_to_name(spiffs_info_status));
        return ESP_FAIL;
    }

    DIR *dir = opendir("/spiffs");
    if (dir == NULL) {
        ESP_LOGE(INIT_TAG, "failed to open '/spiffs' folder");
        return ESP_FAIL;
    }

    ESP_LOGI(INIT_TAG, "listing spiffs files");
    struct stat file_stat;
    while (true) {
        struct dirent *dir_entry = readdir(dir);
        if (dir_entry == NULL) {
            break;
        }
        if (stat(dir_entry->d_name, &file_stat) == -1) {
            ESP_LOGI(INIT_TAG, "  name=%s (stat_failed)", dir_entry->d_name);
            continue;
        }
        ESP_LOGI(INIT_TAG, "  name=%s, size=%ld", dir_entry->d_name, file_stat.st_size);
    }
    closedir(dir);

    return ESP_OK;
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
