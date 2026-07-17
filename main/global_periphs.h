#ifndef __GLOBAL_PERIPHERALS_H__
#define __GLOBAL_PERIPHERALS_H__

#include <driver/gpio.h>
#include "dht11.h"
#include "pc_io.h"
#include "websocket.h"

extern const gpio_num_t g_dht11_data_pin;
extern struct PC_IO_Config g_pc_io_config;
extern struct Websocket g_websocket;

#endif
