#ifndef __PC_IO_H__
#define __PC_IO_H__

#include <stdbool.h>
#include <esp_err.h>
#include <driver/gpio.h>

typedef void (*PC_IO_Status_Listener)(bool status, void *args);
struct PC_IO_Status_Listeners {
    PC_IO_Status_Listener listener;
    void *args;
    struct PC_IO_Status_Listeners *next;
};

typedef void* TimerHandle_t;
typedef void* TaskHandle_t;
typedef void* QueueHandle_t;

struct PC_IO_Config {
    // gpio setup
    gpio_num_t power_pin; // GPIO_NUM_x
    int power_func; // FUNC_GPIOx
    gpio_num_t reset_pin;
    int reset_func;
    gpio_num_t status_pin;
    int status_func;
    // data attached to this
    bool is_busy;
    TimerHandle_t timer_power_on;
    TimerHandle_t timer_reset;
    TimerHandle_t timer_power_off;
    struct PC_IO_Status_Listeners* listeners_status;
    QueueHandle_t queue_status;
    TaskHandle_t task_status;
};

esp_err_t pc_io_init(struct PC_IO_Config* config);
esp_err_t pc_io_power_on(struct PC_IO_Config* config);
esp_err_t pc_io_power_off(struct PC_IO_Config* config);
esp_err_t pc_io_reset(struct PC_IO_Config* config);
bool pc_io_is_powered(struct PC_IO_Config* config);
esp_err_t pc_io_status_listen(struct PC_IO_Config* config, PC_IO_Status_Listener listener, void *args);
esp_err_t pc_io_status_unlisten(struct PC_IO_Config* config, PC_IO_Status_Listener listener, void *args);

#endif
