#ifndef __SHIFTED_PWM_H__
#define __SHIFTED_PWM_H__

#include <stdint.h>

#define MAX_PWM_CYCLES 128
#define MAX_PWM_PINS 8

void shifted_pwm_init();
uint8_t get_pwm_value(uint8_t pin);
void set_pwm_value(uint8_t pin, uint8_t value); 

#endif