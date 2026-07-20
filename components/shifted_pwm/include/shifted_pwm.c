#include "shifted_pwm.h"
#include <stdbool.h>
#include <esp_err.h>
#include <driver/gpio.h>
#include <driver/spi.h>
#include <driver/hw_timer.h>
#include <FreeRTOS.h>

static uint8_t SHIFTED_PWM_TRIGGER_VALUES[SHIFTED_PWM_TOTAL_PINS] = {0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t SHIFTED_PWM_COUNTER = 0;
static uint32_t SHIFTED_PWM_MOSI_BUFFER = 0x0000;
static spi_trans_t SHIFTED_PWM_SPI_TRANSMISSION_PARAMETERS = {
    .cmd = NULL,
    .addr = NULL,
    .mosi = &SHIFTED_PWM_MOSI_BUFFER,
    .miso = NULL,
    .bits = {
        .cmd = 0,
        .addr = 0,
        .mosi = SHIFTED_PWM_TOTAL_PINS,
        .miso = 0,
    },
};

static void IRAM_ATTR isr_shifted_pwm_increment_counter(void* ignore);

esp_err_t shifted_pwm_init(void) {
    spi_config_t spi_config;
    // Load default interface parameters
    // CS_EN:1, MISO_EN:1, MOSI_EN:1, BYTE_TX_ORDER:1, BYTE_TX_ORDER:1, BIT_RX_ORDER:0, BIT_TX_ORDER:0, CPHA:0, CPOL:0
    spi_config.interface.val = SPI_DEFAULT_INTERFACE;
    // Load default interrupt enable
    // TRANS_DONE: true, WRITE_STATUS: false, READ_STATUS: false, WRITE_BUFFER: false, READ_BUFFER: false
    spi_config.intr_enable.val = SPI_MASTER_DEFAULT_INTR_ENABLE;
    // Enable hardware cs to toggle RCLK to latch output on 74HC595 shift register
    spi_config.interface.cs_en = 1;
    // Disable MISO pin to free it for other GPIO operations
    spi_config.interface.miso_en = 0;
    // CPOL: 1, CPHA: 1
    spi_config.interface.cpol = 0;
    spi_config.interface.cpha = 0;
    // Set SPI to master mode
    // 8266 Only support half-duplex
    spi_config.mode = SPI_MASTER_MODE;
    // Set the SPI clock frequency division factor
    spi_config.clk_div = SPI_2MHz_DIV;
    // Register SPI event callback function
    spi_config.event_cb = NULL;
    ESP_ERROR_CHECK_WITHOUT_ABORT(spi_init(HSPI_HOST, &spi_config));

    // setup timer to trigger counter isr
    // f_led = f_clk / CLK_DIV / HARDWARE_COUNTER / SOFTWARE_COUNTER
    //       = 80MHz / 16 / 256 / 128
    //       = 152.6Hz
    ESP_ERROR_CHECK_WITHOUT_ABORT(hw_timer_init(isr_shifted_pwm_increment_counter, NULL));
    ESP_ERROR_CHECK_WITHOUT_ABORT(hw_timer_set_clkdiv(TIMER_CLKDIV_16));
    ESP_ERROR_CHECK_WITHOUT_ABORT(hw_timer_set_reload(true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(hw_timer_set_intr_type(TIMER_EDGE_INT));
    ESP_ERROR_CHECK_WITHOUT_ABORT(hw_timer_set_load_data(256));
    ESP_ERROR_CHECK_WITHOUT_ABORT(hw_timer_enable(true));
    // ESP_ERROR_CHECK_WITHOUT_ABORT(hw_timer_alarm_us(51, true));
    return ESP_OK;
}

void IRAM_ATTR isr_shifted_pwm_increment_counter(void* ignore) {
    const uint32_t old_output = SHIFTED_PWM_MOSI_BUFFER;
    for (int i = 0; i < SHIFTED_PWM_TOTAL_PINS; i++) {
        const bool is_triggered = SHIFTED_PWM_COUNTER >= SHIFTED_PWM_TRIGGER_VALUES[i];
        if (is_triggered) {
            SHIFTED_PWM_MOSI_BUFFER &= ~((uint32_t)1 << i);
        } else {
            SHIFTED_PWM_MOSI_BUFFER |=  ((uint32_t)1 << i);
        }
    }
    if (old_output != SHIFTED_PWM_MOSI_BUFFER) {
        spi_trans(HSPI_HOST, &SHIFTED_PWM_SPI_TRANSMISSION_PARAMETERS);
    }

    SHIFTED_PWM_COUNTER += 1;
    // If 255, then can rely on overflow for reseting uint8_t counter
    #if SHIFTED_PWM_MAX_COUNTER_VALUE < 255
    if (SHIFTED_PWM_COUNTER > SHIFTED_PWM_MAX_COUNTER_VALUE) {
        SHIFTED_PWM_COUNTER = 0;
    }
    #endif
}

uint8_t shifted_pwm_get_value(uint8_t pin) {
    assert(pin < SHIFTED_PWM_TOTAL_PINS);
    return SHIFTED_PWM_TRIGGER_VALUES[pin];
}

void shifted_pwm_set_value(uint8_t pin, uint8_t value) {
    assert(pin < SHIFTED_PWM_TOTAL_PINS);
    SHIFTED_PWM_TRIGGER_VALUES[pin] = value;
}
