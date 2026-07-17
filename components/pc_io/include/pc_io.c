#include "pc_io.h"
#include <driver/gpio.h>
#include <FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <freertos/queue.h>
#include <esp_log.h>

static const char TAG[] = "pc-io";
#define POLL_RATE_MS 100
#define STATUS_QUEUE_LENGTH 10

static esp_err_t try_start_timer(struct PC_IO_Config* config, TimerHandle_t handle);
static void task_power_on(TimerHandle_t handle);
static void task_reset(TimerHandle_t handle);
static void task_power_off(TimerHandle_t handle);
static void IRAM_ATTR isr_enqueue_status_to_queue(void* _config);
static void task_receive_status_from_queue(void* _config);

esp_err_t pc_io_init(struct PC_IO_Config* config) {
    assert(config != NULL);

    PIN_FUNC_SELECT(PERIPHS_GPIO_MUX_REG(config->power_pin), config->power_func);
    PIN_FUNC_SELECT(PERIPHS_GPIO_MUX_REG(config->reset_pin), config->reset_func);
    PIN_FUNC_SELECT(PERIPHS_GPIO_MUX_REG(config->status_pin), config->status_func);
    ESP_LOGD(TAG, "setup gpio pin functions");

    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_direction(config->power_pin, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(config->power_pin, 0));
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_direction(config->reset_pin, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(config->reset_pin, 0));
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_direction(config->status_pin, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_pull_mode(config->status_pin, GPIO_PULLDOWN_ONLY));
    ESP_LOGD(TAG, "setup gpio config");

    config->is_busy = false;
    config->timer_power_on = xTimerCreate("power-on-timer", 0, pdFALSE, (void*)config, task_power_on);
    assert(config->timer_power_on != NULL);
    config->timer_reset = xTimerCreate("reset-timer", 0, pdFALSE, (void*)config, task_reset);
    assert(config->timer_reset != NULL);
    config->timer_power_off = xTimerCreate("power-off-timer", 0, pdFALSE, (void*)config, task_power_off);
    assert(config->timer_power_off != NULL);

    // status listener
    config->listeners_status = NULL;
    config->queue_status = xQueueCreate(STATUS_QUEUE_LENGTH, sizeof(bool));
    assert(config->queue_status != NULL);
    xTaskCreate(task_receive_status_from_queue, "pc-io-int-task", 2048, (void*)config, 10, &config->task_status);
    assert(config->task_status != NULL);
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_intr_type(config->status_pin, GPIO_INTR_ANYEDGE));
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_install_isr_service(0));
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_isr_handler_add(config->status_pin, isr_enqueue_status_to_queue, (void*)config));

    ESP_LOGD(TAG, "created io timers");
    return ESP_OK;
}

esp_err_t try_start_timer(struct PC_IO_Config* config, TimerHandle_t handle) {
    assert(config != NULL);
    assert(handle != NULL);
    if (config->is_busy) {
        ESP_LOGE(TAG, "timer blocked since busy flag is raised");
        return ESP_FAIL;
    }
    config->is_busy = true;
    xTimerStart(handle, 0);
    ESP_LOGD(TAG, "timer was executed");
    return ESP_OK;
}

esp_err_t pc_io_power_on(struct PC_IO_Config* config) {
    assert(config != NULL);
    assert(config->timer_power_on != NULL);
    if (config->is_busy) return ESP_FAIL;
    if (!pc_io_is_powered(config)) {
        return try_start_timer(config, config->timer_power_on);
    }
    return ESP_OK;
}

esp_err_t pc_io_reset(struct PC_IO_Config* config) {
    assert(config != NULL);
    assert(config->timer_reset != NULL);
    return try_start_timer(config, config->timer_reset);
}

esp_err_t pc_io_power_off(struct PC_IO_Config* config) {
    assert(config != NULL);
    assert(config->timer_power_off != NULL);
    if (pc_io_is_powered(config)) {
        return try_start_timer(config, config->timer_power_off);
    }
    return ESP_OK;
}

bool pc_io_is_powered(struct PC_IO_Config* config) {
    assert(config != NULL);
    return gpio_get_level(config->status_pin);
}

void task_power_on(TimerHandle_t handle) {
    assert(handle != NULL);
    struct PC_IO_Config* config = pvTimerGetTimerID(handle);
    assert(config != NULL);
    gpio_set_level(config->power_pin, 1);
    vTaskDelay(POLL_RATE_MS / portTICK_RATE_MS);
    gpio_set_level(config->power_pin, 0);
    config->is_busy = false;
}

void task_reset(TimerHandle_t handle) {
    assert(handle != NULL);
    struct PC_IO_Config* config = pvTimerGetTimerID(handle);
    assert(config != NULL);
    gpio_set_level(config->reset_pin, 1);
    vTaskDelay(POLL_RATE_MS / portTICK_RATE_MS);
    gpio_set_level(config->reset_pin, 0);
    config->is_busy = false;
}

void task_power_off(TimerHandle_t handle) {
    assert(handle != NULL);
    struct PC_IO_Config* config = pvTimerGetTimerID(handle);
    assert(config != NULL);
    // hold power button until computer switches off
    ESP_LOGI(TAG, "Start holding power button down for a while");
    gpio_set_level(config->power_pin, 1);
    bool is_powered = pc_io_is_powered(config);
    const unsigned int MAX_HOLD_DURATION = 60;
    unsigned int hold_counter = 0;
    for (hold_counter = 0; hold_counter < MAX_HOLD_DURATION; hold_counter++) {
        if (!is_powered) break;
        is_powered = pc_io_is_powered(config);
        vTaskDelay(POLL_RATE_MS / portTICK_RATE_MS);
    }
    gpio_set_level(config->power_pin, 0);
    const unsigned int hold_duration = hold_counter * POLL_RATE_MS;
    if (!is_powered) {
        ESP_LOGI(TAG, "Powered off computer successfully after %u milliseconds", hold_duration);
    } else {
        ESP_LOGE(TAG, "Failed to power off computer after %u milliseconds", hold_duration);
    }
    config->is_busy = false;
}

static size_t pc_io_get_total_status_listeners(struct PC_IO_Config* config) {
    assert(config != NULL);
    struct PC_IO_Status_Listeners* head = config->listeners_status;
    size_t total = 0;
    while (head != NULL) {
        total++;
        head = head->next;
    }
    return total;
}

esp_err_t pc_io_status_listen(struct PC_IO_Config* config, PC_IO_Status_Listener listener, void *args) {
    assert(config != NULL);
    assert(listener != NULL);
    struct PC_IO_Status_Listeners* entry = malloc(sizeof(struct PC_IO_Status_Listeners));
    assert(entry != NULL);
    entry->listener = listener;
    entry->args = args;
    entry->next = config->listeners_status;
    config->listeners_status = entry;
    const size_t total_listeners = pc_io_get_total_status_listeners(config);
    ESP_LOGI(TAG, "Added status listener now up to %u listeners", total_listeners);
    return ESP_OK;
}

esp_err_t pc_io_status_unlisten(struct PC_IO_Config* config, PC_IO_Status_Listener listener, void *args) {
    assert(config != NULL);
    assert(listener != NULL);
    struct PC_IO_Status_Listeners** head = &config->listeners_status;
    while (*head != NULL) {
        struct PC_IO_Status_Listeners* curr = *head;
        if ((curr->listener == listener) && (curr->args == args)) {
            *head = curr->next;
            free(curr);
            const size_t total_listeners = pc_io_get_total_status_listeners(config);
            ESP_LOGI(TAG, "Removed status listener with %u remaining", total_listeners);
            return ESP_OK;
        }
        head = &(curr->next);
    }
    ESP_LOGE(TAG, "Failed to remove status listener since no match could be found");
    return ESP_FAIL;
}

void IRAM_ATTR isr_enqueue_status_to_queue(void* _config) {
    struct PC_IO_Config* config = (struct PC_IO_Config*)_config;
    assert(config != NULL);
    const bool status = pc_io_is_powered(config);
    xQueueSendFromISR(config->queue_status, &status, NULL);
}

void task_receive_status_from_queue(void* _config) {
    struct PC_IO_Config* config = (struct PC_IO_Config*)_config;
    assert(config != NULL);
    while (true) {
        bool status = false;
        if (!xQueueReceive(config->queue_status, &status, portMAX_DELAY)) continue;
        // notify all listeners
        struct PC_IO_Status_Listeners* head = config->listeners_status;
        while (head != NULL) {
            head->listener(status, head->args);
            head = head->next;
        }
    }
}
