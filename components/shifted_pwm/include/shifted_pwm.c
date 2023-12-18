#include "shifted_pwm.h"

#include <stdbool.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include <driver/gpio.h>
#include <driver/hw_timer.h>
#include <driver/spi.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <nvs_flash.h>

#define TAG "shifted_pwm"
#define NVS_KEY "pwm_values"
#define NVS_NAMESPACE "shifted_pwm"
static nvs_handle_t pwm_nvs_handle = 0;

static uint8_t pwm_values[MAX_PWM_PINS] = {0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t current_cycle = 0;
static uint32_t current_value = 0x0000; // cast 32bit for performance?
static spi_trans_t transmission_params = {0};

// Task that checks for saving pwm values
static bool is_values_changed = false;
static TimerHandle_t timer_save_values = NULL;

esp_err_t init_nvs_autosave();
void shifted_pwm_update(void *ignore);
void task_save_values(TimerHandle_t handle);

void shifted_pwm_init() {
    spi_config_t spi_config;
    // Load default interface parameters
    // CS_EN:1, MISO_EN:1, MOSI_EN:1, BYTE_TX_ORDER:1, BYTE_TX_ORDER:1, BIT_RX_ORDER:0, BIT_TX_ORDER:0, CPHA:0, CPOL:0
    spi_config.interface.val = SPI_DEFAULT_INTERFACE;
    // Load default interrupt enable
    // TRANS_DONE: true, WRITE_STATUS: false, READ_STATUS: false, WRITE_BUFFER: false, READ_BUFFER: false
    spi_config.intr_enable.val = SPI_MASTER_DEFAULT_INTR_ENABLE;
    // Cancel hardware cs
    spi_config.interface.cs_en = 1;
    // MISO pin is used for DC
    spi_config.interface.miso_en = 0;
    // CPOL: 1, CPHA: 1
    spi_config.interface.cpol = 0;
    spi_config.interface.cpha = 0;
    // Set SPI to master mode
    // 8266 Only support half-duplex
    spi_config.mode = SPI_MASTER_MODE;
    // Set the SPI clock frequency division factor
    spi_config.clk_div = SPI_20MHz_DIV;
    // Register SPI event callback function
    spi_config.event_cb = NULL;
    spi_init(HSPI_HOST, &spi_config);


    transmission_params.mosi = &current_value;
    transmission_params.bits.mosi = 8;

    hw_timer_init(shifted_pwm_update, NULL);
    hw_timer_set_clkdiv(TIMER_CLKDIV_1);
    hw_timer_set_reload(true);
    hw_timer_set_intr_type(TIMER_EDGE_INT);
    hw_timer_set_load_data(5000);
    hw_timer_enable(true);
    // hw_timer_alarm_us(51, true);

    if (init_nvs_autosave() != ESP_OK) {
        ESP_LOGE(TAG, "failed to start nvs autosave correctly");
    }
}

esp_err_t init_nvs_autosave() {
    const esp_err_t nvs_open_status = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &pwm_nvs_handle);
    if (nvs_open_status != ESP_OK) {
        ESP_LOGE(TAG, "failed to open nvs with namespace %s (%s)", NVS_NAMESPACE, esp_err_to_name(nvs_open_status));
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "opened nvs namespace %s", NVS_NAMESPACE);

    // each pwm pin takes up 1 byte 
    size_t total_pwm_values = MAX_PWM_PINS;
    const esp_err_t init_nvs_load_status = nvs_get_blob(pwm_nvs_handle, NVS_KEY, (void*)pwm_values, &total_pwm_values);
    if (init_nvs_load_status != ESP_OK) {
        ESP_LOGE(TAG, "failed to load pwm values from nvs (%s)", esp_err_to_name(init_nvs_load_status));
        is_values_changed = true;
    } else if (total_pwm_values != MAX_PWM_PINS) {
        ESP_LOGE(TAG, "mismatching number of pwm values. got %u expected %u", total_pwm_values, MAX_PWM_PINS);
        is_values_changed = true;
    } else {
        ESP_LOGI(TAG, "loaded pwm %u values from nvs", total_pwm_values);
        is_values_changed = false;
    }

    const size_t AUTOSAVE_PERIOD_MS = 1000;
    timer_save_values = xTimerCreate(
        "pwm_values_save", 
        AUTOSAVE_PERIOD_MS / portTICK_RATE_MS, 
        pdTRUE, 
        NULL, 
        task_save_values
    );
    if (xTimerStart(timer_save_values, 0) == pdFAIL) {
        ESP_LOGE(TAG, "failed to start nvs autosave task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "started nvs save task every %d ms", AUTOSAVE_PERIOD_MS);
    return ESP_OK;
}

void shifted_pwm_update(void *ignore) {
    uint8_t old_value = current_value;
    if (current_cycle == 0) {
        current_value = 0xFF;
    }
    
    for (int i = 0; i < MAX_PWM_PINS; i++) {
        if (current_cycle == pwm_values[i]) {
            current_value &= ~(1u << i);
        }
    }

    if (current_value != old_value) {
        spi_trans(HSPI_HOST, &transmission_params);
    }

    current_cycle += 1;
    
    // If 255, then can rely on overflow for reset
    #if MAX_PWM_CYCLES < 255
    if (current_cycle > MAX_PWM_CYCLES) {
        current_cycle = 0;
    }
    #endif
}

uint8_t get_pwm_value(uint8_t pin) {
    return pwm_values[pin];
}

void set_pwm_value(uint8_t pin, uint8_t value) {
    if (value > MAX_PWM_CYCLES) {
        value = MAX_PWM_CYCLES;
    }
    pwm_values[pin] = value;
    is_values_changed = true;
}

void task_save_values(TimerHandle_t handle) {
    if (!is_values_changed) {
        return;
    }
    is_values_changed = false;
    const esp_err_t nvs_save_status = nvs_set_blob(pwm_nvs_handle, NVS_KEY, (const void*)pwm_values, sizeof(pwm_values));
    if (nvs_save_status != ESP_OK) {
        ESP_LOGE(TAG, "failed to save pwm values to nvs (%s)", esp_err_to_name(nvs_save_status));
    }
}
