#ifndef __DHT11_H__
#define __DHT11_H__

#include <esp_err.h>
#include <driver/gpio.h>

struct DHT11_Measurement {
    uint8_t temperature;
    uint8_t humidity;
};

esp_err_t dht11_init(gpio_num_t pin);
esp_err_t dht11_read(gpio_num_t pin, struct DHT11_Measurement* measurement);

#endif
