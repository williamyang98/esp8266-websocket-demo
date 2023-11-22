#include "pc_io.h"
#include "pc_io_interrupt.h"

#include "driver/gpio.h"

#include "FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_log.h"

#define TAG "pc-io"
#define POLL_RATE_MS 100

// each task is triggered by it's respective timer
static TimerHandle_t timer_power_on = NULL;
static TimerHandle_t timer_reset = NULL;
static TimerHandle_t timer_power_off = NULL;
static void task_power_on(TimerHandle_t handle);
static void task_reset(TimerHandle_t handle);
static void task_power_off(TimerHandle_t handle);

// only allow one timer to run at any given time
static esp_err_t start_timer(TimerHandle_t handle);
static bool is_busy = false;

void pc_io_init() {
    PIN_FUNC_SELECT(PERIPHS_GPIO_MUX_REG(POWER_SW_PIN), POWER_SW_FUNC);
    PIN_FUNC_SELECT(PERIPHS_GPIO_MUX_REG(RESET_SW_PIN), RESET_SW_FUNC);
    PIN_FUNC_SELECT(PERIPHS_GPIO_MUX_REG(POWER_STATUS_PIN), POWER_STATUS_FUNC);
    ESP_LOGD(TAG, "setup gpio pin functions");

    gpio_set_direction(POWER_SW_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(POWER_SW_PIN, 0);
    gpio_set_direction(RESET_SW_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RESET_SW_PIN, 0);
    gpio_set_direction(POWER_STATUS_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(POWER_STATUS_PIN, GPIO_PULLDOWN_ONLY);
    ESP_LOGD(TAG, "setup gpio config");

    timer_power_on = xTimerCreate("power-on-timer", 0, pdFALSE, NULL, task_power_on);
    timer_reset = xTimerCreate("reset-timer", 0, pdFALSE, NULL, task_reset);
    timer_power_off = xTimerCreate("power-off-timer", 0, pdFALSE, NULL, task_power_off);
    ESP_LOGD(TAG, "created io timers");
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

esp_err_t pc_io_reset() {
    if (timer_reset == NULL) {
        ESP_LOGE(TAG, "timer_reset is not initialised");
        return ESP_FAIL;
    }
    return start_timer(timer_reset);
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
    return gpio_get_level(POWER_STATUS_PIN);
}

void task_power_on(TimerHandle_t handle) {
    gpio_set_level(POWER_SW_PIN, 1);
    vTaskDelay(POLL_RATE_MS / portTICK_RATE_MS);
    gpio_set_level(POWER_SW_PIN, 0);
    is_busy = false;
}

void task_reset(TimerHandle_t handle) {
    gpio_set_level(RESET_SW_PIN, 1);
    vTaskDelay(POLL_RATE_MS / portTICK_RATE_MS);
    gpio_set_level(RESET_SW_PIN, 0);
    is_busy = false;
}

void task_power_off(TimerHandle_t handle) {
    gpio_set_level(POWER_SW_PIN, 1);
    for (int i = 0; i < 60 && pc_io_is_powered(); i++) {
        vTaskDelay(POLL_RATE_MS / portTICK_RATE_MS);
    }
    gpio_set_level(POWER_SW_PIN, 0);
    is_busy = false;
}

