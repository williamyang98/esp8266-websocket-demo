# Introduction
- Turns a 74HC595 shift register into an 8 channel PWM output using the ESP8266's hardware SPI
- A hardware timer increments a counter in an interrupt service routine (ISR) and updates the 8 channels via SPI
- The duty cycle of each PWM channel is adjusted by setting 8 different values which is compared to the continuously updated counter value
