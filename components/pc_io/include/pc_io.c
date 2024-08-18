#include "pc_io.h"

#include "driver/gpio.h"

#include "FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_log.h"

#define TAG "pc-io"

// each task is triggered by it's respective timer
static TimerHandle_t timer_power_on = NULL;
static TimerHandle_t timer_power_off = NULL;
static void task_power_on(TimerHandle_t handle);
static void task_power_off(TimerHandle_t handle);

// only allow one timer to run at any given time
static esp_err_t start_timer(TimerHandle_t handle);
static bool is_busy = false;

void pc_io_init() {
    ESP_LOGI(TAG, "setup gpio pin functions");
    // NOTE: power pin configuration
    // - Wiring is pin --> transitor_to_ground --> switch_high
    // - Power pin turns on a transistor which shorts the switch pin. This simulates a power press.
    gpio_config_t config_power = {
        .pin_bit_mask = (1u << POWER_SW_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    if (gpio_config(&config_power) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init power switch gpio");
        return;
    }
    gpio_set_level(POWER_SW_PIN, 0);
    ESP_LOGI(TAG, "setup gpio config for power switch");
    // NOTE: status pin configuration
    // - Wiring is pin <-- diode <-- resistor <-- 0V/5V led signal
    // - We cannot connect directly with a resistor divider because this screws up the boot sequence.
    // - The reason why we are using open drain output instead of input with pulldown
    //   is because the pulldown resistor is either broken or not implemented for GPIO 0 or 2.
    // - Instead we use open drain to discharge any remaining charge on the pin before a read.
    //   Then we wait for the diode to charge the pin to the status voltage.
    //   Then we perform a GPIO read to get the status voltage.
    gpio_config_t config_status = {
        .pin_bit_mask = (1u << POWER_STATUS_PIN),
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    if (gpio_config(&config_status) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init power status gpio");
        return;
    }
    gpio_pulldown_en(POWER_STATUS_PIN);
    gpio_set_pull_mode(POWER_STATUS_PIN, GPIO_PULLDOWN_ONLY);
    ESP_LOGI(TAG, "setup gpio config for pc status");

    timer_power_on = xTimerCreate("power-on-timer", 0, pdFALSE, NULL, task_power_on);
    timer_power_off = xTimerCreate("power-off-timer", 0, pdFALSE, NULL, task_power_off);
    ESP_LOGI(TAG, "created power gpio timers for power on/off");
}

esp_err_t start_timer(TimerHandle_t handle) {
    if (is_busy) {
        ESP_LOGD(TAG, "timer blocked since busy flag is raised");
        return ESP_FAIL;
    }

    is_busy = true;
    xTimerStart(handle, 0);
    ESP_LOGD(TAG, "timer was executed");
    return ESP_OK;
}

esp_err_t pc_io_power_on() {
    if (timer_power_on == NULL) {
        ESP_LOGE(TAG, "timer_power_on is not initialised");
        return ESP_FAIL;
    }
    if (!pc_io_is_powered()) {
        return start_timer(timer_power_on);
    }
    return ESP_OK;
}

esp_err_t pc_io_power_off() {
    if (timer_power_off == NULL) {
        ESP_LOGE(TAG, "timer_power_off is not initialised");
        return ESP_FAIL;
    }
    if (pc_io_is_powered()) {
        return start_timer(timer_power_off);
    }
    return ESP_OK;
}

bool pc_io_is_powered() {
    taskENTER_CRITICAL();
    gpio_set_level(POWER_STATUS_PIN, 0);
    os_delay_us(20);
    gpio_set_level(POWER_STATUS_PIN, 1);
    os_delay_us(100);
    taskEXIT_CRITICAL();
    return gpio_get_level(POWER_STATUS_PIN);
}

void task_power_on(TimerHandle_t handle) {
    const int HOLD_TIME_MS = 300;
    const int RELEASE_TIME_MS = 200;
    gpio_set_level(POWER_SW_PIN, 1);
    vTaskDelay(HOLD_TIME_MS / portTICK_RATE_MS);
    gpio_set_level(POWER_SW_PIN, 0);
    vTaskDelay(RELEASE_TIME_MS / portTICK_RATE_MS);
    is_busy = false;
}

void task_power_off(TimerHandle_t handle) {
    const int HOLD_TIME_MS = 100;
    gpio_set_level(POWER_SW_PIN, 1);
    for (int i = 0; i < 60 && pc_io_is_powered(); i++) {
        vTaskDelay(HOLD_TIME_MS / portTICK_RATE_MS);
    }
    gpio_set_level(POWER_SW_PIN, 0);
    is_busy = false;
}

