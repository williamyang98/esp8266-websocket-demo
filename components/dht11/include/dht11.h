#ifndef __DHT11_H__
#define __DHT11_H__

#ifndef DHT11_PIN
#define DHT11_PIN 2
#endif

#include <esp_err.h>

esp_err_t dht11_init();
esp_err_t dht11_read();
// dht11 only has a resolution of 1'C and 1% RH
uint8_t dht11_get_temperature();
uint8_t dht11_get_humidity();

#endif
