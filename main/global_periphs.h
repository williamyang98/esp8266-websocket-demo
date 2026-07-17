#ifndef __GLOBAL_PERIPHERALS_H__
#define __GLOBAL_PERIPHERALS_H__

#include <driver/gpio.h>
#include "dht11.h"
#include "pc_io.h"

extern const gpio_num_t dht11_data_pin;
extern struct PC_IO_Config pc_io_config;

#endif
