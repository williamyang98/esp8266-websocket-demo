#include "pc_io.h"
#include "pc_io_interrupt.h"

#include <stdlib.h>
#include <stdbool.h>

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#define TAG "pc-io-interrupt"
#define LL_TAG "pc-io-linked-list"

// Linked list of interrupt callbacks
typedef struct list_node {
    pc_io_status_listener_t listener;
    void *args;
    struct list_node *next;
} list_node_t;
static list_node_t *listeners = NULL; 

// RTOS event queue with rx and tx, type=bool
#define STATE_EVENT_QUEUE_LENGTH 10
static xQueueHandle state_event_queue = NULL;
static void receive_updates_task(void *arg);
static void IRAM_ATTR send_update_isr(void *ignore);

esp_err_t pc_io_interrupt_init() {
    state_event_queue = xQueueCreate(STATE_EVENT_QUEUE_LENGTH, sizeof(bool));
    if (state_event_queue == NULL) {
        ESP_LOGE(TAG, "failed to create event queue");
        return ESP_FAIL;
    }
    // receive updates from event queue
    xTaskCreate(receive_updates_task, "pc-io-int-task", 2048, NULL, 10, NULL);
    // send update to event queue
    gpio_set_intr_type(POWER_STATUS_PIN, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(0);
    esp_err_t status = gpio_isr_handler_add(POWER_STATUS_PIN, send_update_isr, NULL);
    // portENTER_CRITICAL();
    // esp_err_t status = gpio_isr_register(send_update_isr, NULL, 0, NULL);
    // portEXIT_CRITICAL();
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "failed to setup ISR for pc status");
    } else {
        ESP_LOGD(TAG, "successfully add ISR handler for pc status");
    }
    return status;
}

esp_err_t pc_io_status_listen(pc_io_status_listener_t listener, void *args) {
    if (listener == NULL) {
        return ESP_FAIL;
    }

    // append to head
    list_node_t *node = malloc(sizeof(list_node_t));
    node->listener = listener;
    node->args = args;
    node->next = listeners;
    listeners = node;
    ESP_LOGD(LL_TAG, "added listener");
    return ESP_OK;
}

esp_err_t pc_io_status_unlisten(pc_io_status_listener_t listener, void *args) {
    if (listener == NULL) {
        return ESP_FAIL;
    }

    list_node_t **head = &listeners;
    while (*head != NULL) {
        list_node_t *node = *head;
        if ((node->listener == listener) && (node->args == args)) {
            *head = node->next;
            free(node);
            ESP_LOGD(LL_TAG, "removed listener");
            return ESP_OK;
        }
        head = &(node->next);
    }
    return ESP_FAIL;
}

void IRAM_ATTR send_update_isr(void *ignore) {
    bool new_state = pc_io_is_powered();
    xQueueSendFromISR(state_event_queue, &new_state, NULL);
}

void receive_updates_task(void *arg) {
    bool prev_state = false;
    bool curr_state = false;
    while (1) {
        if (xQueueReceive(state_event_queue, &curr_state, portMAX_DELAY)) {
            if (curr_state == prev_state) {
                continue;
            }
            prev_state = curr_state;
            // notify listeners
            list_node_t *head = listeners;
            while (head != NULL) {
                head->listener(curr_state, head->args);
                head = head->next;
            }
        }
    }
}

