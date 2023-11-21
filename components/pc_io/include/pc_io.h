#ifndef __PC_IO_H__
#define __PC_IO_H__

#include <stdbool.h>
#include "esp_err.h"

#define POWER_SW_PIN 5
#define POWER_SW_FUNC FUNC_GPIO5

#define RESET_SW_PIN 4
#define RESET_SW_FUNC FUNC_GPIO4

#define POWER_STATUS_PIN 12
#define POWER_STATUS_FUNC FUNC_GPIO12

void pc_io_init();
esp_err_t pc_io_power_on();
esp_err_t pc_io_power_off();
esp_err_t pc_io_reset();
bool pc_io_is_powered();

#endif
