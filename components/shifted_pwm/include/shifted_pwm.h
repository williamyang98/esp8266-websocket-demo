#ifndef __SHIFTED_PWM_H__
#define __SHIFTED_PWM_H__

#include <stdint.h>
#include <esp_err.h>

#define SHIFTED_PWM_MAX_COUNTER_VALUE 128
#define SHIFTED_PWM_TOTAL_PINS 8

esp_err_t shifted_pwm_init(void);
uint8_t shifted_pwm_get_value(uint8_t pin);
// duty_cycle = value / SHIFTED_PWM_MAX_COUNTER_VALUE
void shifted_pwm_set_value(uint8_t pin, uint8_t value);

#endif