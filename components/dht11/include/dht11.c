#include "dht11.h"

#include <esp_err.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "dht11"
#define TOTAL_DATA_LENGTH 5 // 4 data and 1 checksum

static uint8_t buffer[TOTAL_DATA_LENGTH] = {0};
static uint8_t temperature = 0;
static uint8_t humidity = 0;

static int32_t dht11_wait_signal(uint32_t timeout, uint32_t level);
static esp_err_t IRAM_ATTR dht11_read_data();

// https://github.com/FiendChain/ELEC3117-AVR-PostBox/blob/master/PostBox/PostBox/lib/dht11/dht11.h
// https://www.mouser.com/datasheet/2/758/DHT11-Technical-Data-Sheet-Translated-Version-1143054.pdf

esp_err_t dht11_init() {
    gpio_config_t config = {
        .pin_bit_mask = (1u << DHT11_PIN),
        .mode = GPIO_MODE_OUTPUT_OD, 
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    if (gpio_config(&config) != ESP_OK) {
        ESP_LOGE(TAG, "unable to initialise");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t dht11_read() {
    /// so we can use os_delay without caused a core meditation error
    taskENTER_CRITICAL();
    esp_err_t status = dht11_read_data();
    taskEXIT_CRITICAL();

    if (status != ESP_OK) {
        ESP_LOGE(TAG, "failed to read data");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t IRAM_ATTR dht11_read_data() {
    // pulldown for at least 18ms
    gpio_set_level(DHT11_PIN, 0);
    os_delay_us(20000);
    gpio_set_level(DHT11_PIN, 1);

    // wait for pull down response after 20 to 40 us
    if (!dht11_wait_signal(60, 0)) {
        ESP_LOGE(TAG, "start timeout on pull down #1");
        return ESP_FAIL;
    }
    // wait for pull up after 80us
    if (!dht11_wait_signal(100, 1)) {
        ESP_LOGE(TAG, "start timeout on pull up");
        return ESP_FAIL;
    }
    // pulls down after 80us
    if (!dht11_wait_signal(100, 0)) {
        ESP_LOGE(TAG, "start timeout on pull down #2");
        return ESP_FAIL;
    }

    // DHT11 data signal
	// Each bit starts with 50us low voltage
	// and ends with a high voltage
	// 26-28us means 0
	// 70us means 1
    for (int current_byte = 0; current_byte < TOTAL_DATA_LENGTH; current_byte++) {
        buffer[current_byte] = 0x00;
        for (int i = 0; i < 8; i++) {
            // 50us pulldown
            if (!dht11_wait_signal(70, 1)) {
                ESP_LOGE(TAG, "timeout pulldown on byte %d bit %d", current_byte, i);
                return ESP_FAIL;
            }
            // read pull up length
            int32_t duration = dht11_wait_signal(80, 0); 
            if (!duration) {
                ESP_LOGE(TAG, "timeout pullup on byte %d bit %d", current_byte, i);
                return ESP_FAIL;
            }

            if (duration <= 10 || duration > 80) {
                ESP_LOGE(TAG, "invalid pullup duration %d", duration);
                return ESP_FAIL;
            }

            if (duration <= 30) buffer[current_byte] &= ~(1 << (7-i));
            else                buffer[current_byte] |= (1 << (7-i));
        }
    }

    // confirm checksum
    uint8_t checksum = (buffer[0] + buffer[1] + buffer[2] + buffer[3]) & 0xFF;
    if (checksum != buffer[4]) {
        ESP_LOGE(TAG, "failed checksum 0x%x != 0x%x, calculated != expected", checksum, buffer[4]);
        ESP_LOGE(TAG, "buffer contents are: %d, %d, %d, %d, %d", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
        return ESP_FAIL;
    }

    temperature = buffer[2];
    humidity = buffer[0];
    return ESP_OK;
}

int32_t dht11_wait_signal(uint32_t timeout, uint32_t level) {
    for (uint32_t i = 0; i < timeout; i+=2) {
        if (gpio_get_level(DHT11_PIN) == level) {
            return i;
        }
        os_delay_us(2); // no 1us timings
    }
    return -1;
}

// dht11 only has a resolution of 1'C and 1% RH
uint8_t dht11_get_temperature() { 
    return temperature;
}

uint8_t dht11_get_humidity() {
    return humidity;
}