# Introduction
- Turns a 74HC595 shift register into an 8 channel PWM output using the ESP8266's hardware SPI
- A hardware timer increments a counter in an interrupt service routine (ISR) and updates the 8 channels via SPI
- The duty cycle of each PWM channel is adjusted by setting 8 different values which is compared to the continuously updated counter value

### Pinout
| 74HC595 | Description |
| --- | --- |
| SER (14) | Connected to ESP8266 MOSI (D7/GPIO13) for shift register data |
| OE (13) | Connected to ground to enable output |
| SRCLR (10) | Connected to vcc to stop clearing output |
| SRCLK (11) | Connected to ESP8266 SCLK (D5/GPIO14) for shift register clock |
| RCLK (12) | Connected to ESP8266 CS (D8/GPIO15) to latch shift register state to output registers to output new data |
