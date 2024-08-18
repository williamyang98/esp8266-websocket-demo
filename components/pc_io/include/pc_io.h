#ifndef __PC_IO_H__
#define __PC_IO_H__

#include <stdbool.h>
#include <esp_err.h>

// Pin setup
#define POWER_SW_PIN 0
#define POWER_STATUS_PIN 2

void pc_io_init();
esp_err_t pc_io_power_on();
// esp_err_t pc_io_power_off(); // SAFETY: disable power off
bool pc_io_is_powered();

#endif
