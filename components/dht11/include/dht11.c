#include "dht11.h"

#include <esp_err.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char TAG[] = "dht11";
#define TOTAL_DATA_LENGTH 5 // 4 data and 1 checksum
static esp_err_t dht11_read_data(gpio_num_t pin, struct DHT11_Measurement* measurement);
static int32_t dht11_wait_signal(gpio_num_t pin, uint32_t timeout, uint32_t level);

// DOC: https://www.mouser.com/datasheet/2/758/DHT11-Technical-Data-Sheet-Translated-Version-1143054.pdf
esp_err_t dht11_init(gpio_num_t pin) {
    gpio_config_t config = {
        .pin_bit_mask = (1u << pin),
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&config));
    return ESP_OK;
}

esp_err_t dht11_read(gpio_num_t pin, struct DHT11_Measurement* measurement) {
    /// so we can use os_delay without caused a core meditation error
    taskENTER_CRITICAL();
    const esp_err_t status = dht11_read_data(pin, measurement);
    taskEXIT_CRITICAL();
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "failed to read data");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t dht11_read_data(gpio_num_t pin, struct DHT11_Measurement* measurement) {
    // pulldown for at least 18ms
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(pin, 0));
    os_delay_us(20000);
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(pin, 1));

    // wait for pull down response after 20 to 40 us
    if (dht11_wait_signal(pin, 60, 0) < 0) {
        ESP_LOGE(TAG, "start timeout on pull down #1");
        return ESP_FAIL;
    }
    // wait for pull up after 80us
    if (dht11_wait_signal(pin, 100, 1) < 0) {
        ESP_LOGE(TAG, "start timeout on pull up");
        return ESP_FAIL;
    }
    // pulls down after 80us
    if (dht11_wait_signal(pin, 100, 0) < 0) {
        ESP_LOGE(TAG, "start timeout on pull down #2");
        return ESP_FAIL;
    }

    // DHT11 data signal
    // Each bit starts with 50us low voltage
    // and ends with a high voltage
    // 26-28us means 0
    // 70us means 1
    uint8_t buffer[TOTAL_DATA_LENGTH] = {0};
    for (int current_byte = 0; current_byte < TOTAL_DATA_LENGTH; current_byte++) {
        buffer[current_byte] = 0x00;
        for (int current_bit = 0; current_bit < 8; current_bit++) {
            // 50us pulldown
            if (dht11_wait_signal(pin, 70, 1) < 0) {
                ESP_LOGE(TAG, "timeout pulldown on byte %d bit %d", current_byte, current_bit);
                return ESP_FAIL;
            }
            // read pull up length
            const int32_t duration = dht11_wait_signal(pin, 80, 0); 
            if (duration < 0) {
                ESP_LOGE(TAG, "timeout pullup on byte %d bit %d", current_byte, current_bit);
                return ESP_FAIL;
            }

            if (duration <= 10 || duration > 80) {
                ESP_LOGE(TAG, "invalid pullup on byte %d bit %d. duration=%d", current_byte, current_bit, duration);
                return ESP_FAIL;
            }

            if (duration <= 30) {
                // Byte is already zeroed out at start
                // buffer[current_byte] &= ~(1 << (7-current_bit));
            } else {
                buffer[current_byte] |= (1 << (7-current_bit));
            }
        }
    }

    // confirm checksum
    const uint8_t checksum = (buffer[0] + buffer[1] + buffer[2] + buffer[3]) & 0xFF;
    if (checksum != buffer[4]) {
        ESP_LOGE(TAG, "failed checksum 0x%x != 0x%x, calculated != expected", checksum, buffer[4]);
        ESP_LOGE(TAG, "buffer contents are: %d, %d, %d, %d, %d", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
        return ESP_FAIL;
    }

    measurement->temperature = buffer[2];
    measurement->humidity = buffer[0];
    return ESP_OK;
}

int32_t dht11_wait_signal(gpio_num_t pin_number, uint32_t timeout, uint32_t level) {
    for (uint32_t i = 0; i < timeout; i+=2) {
        if (gpio_get_level(pin_number) == level) {
            return i;
        }
        os_delay_us(2); // no 1us timings
    }
    return -1;
}
