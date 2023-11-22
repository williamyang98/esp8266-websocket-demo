#ifndef __DHT11_H__
#define __DHT11_H__

#include <esp_err.h>

typedef struct dht11_sensor {
    uint8_t pin_number;
    uint8_t temperature;
    uint8_t humidity;
} dht11_sensor_t;

esp_err_t dht11_init(dht11_sensor_t *s);
esp_err_t dht11_read(dht11_sensor_t *s);

#endif
