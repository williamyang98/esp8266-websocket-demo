#pragma once

#include "dht11.h"

static dht11_sensor_t dht11_sensor = {
    .pin_number = 2,
    .temperature = 0,
    .humidity = 0,
};

