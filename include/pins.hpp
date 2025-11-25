/**
 * @file pins.hpp
 * @brief Pin definitions for ATtiny202 water level sensor
 *
 * Pin assignments:
 * PA0 (pin 6): UPDI/Button (shared)
 * PA1 (pin 4): PWR_EN (controls TPS22860)
 * PA2 (pin 5): LED
 * PA3 (pin 7): DRV_IN1 (PWM to H-bridge)
 * PA6 (pin 2): SDA (I2C data)
 * PA7 (pin 3): SCL (I2C clock)
 */

#pragma once

#include <avr/io.h>

namespace pins {
    // Pin bitmasks
    constexpr uint8_t BUTTON    = PIN0_bm;  // PA0 - UPDI/Button
    constexpr uint8_t PWR_EN    = PIN1_bm;  // PA1 - Power enable
    constexpr uint8_t LED       = PIN2_bm;  // PA2 - Status LED
    constexpr uint8_t DRV_IN1   = PIN3_bm;  // PA3 - Buzzer PWM
    constexpr uint8_t SDA       = PIN6_bm;  // PA6 - I2C data
    constexpr uint8_t SCL       = PIN7_bm;  // PA7 - I2C clock

    // Pin numbers (for compatibility)
    constexpr uint8_t BUTTON_PIN    = 0;
    constexpr uint8_t PWR_EN_PIN    = 1;
    constexpr uint8_t LED_PIN       = 2;
    constexpr uint8_t DRV_IN1_PIN   = 3;
    constexpr uint8_t SDA_PIN       = 6;
    constexpr uint8_t SCL_PIN       = 7;
}
