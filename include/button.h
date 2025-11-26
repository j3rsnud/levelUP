/**
 * @file button.h
 * @brief Button handler for PA0 (shared with UPDI)
 *
 * Functions:
 * - Long press (≥3s): Start calibration mode
 * - Hold on boot (≥5s): Factory reset
 * - Short press: Silence current alert
 *
 * Note: Button shares pin with UPDI programmer.
 * Button connects to PA0 via 220Ω series resistor.
 * Internal pull-up enabled, button connects to GND when pressed.
 */

#pragma once

#include <stdint.h>

/**
 * Button events
 */
enum class ButtonEvent : uint8_t {
    NONE = 0,
    SHORT_PRESS,      // < 3 seconds
    LONG_PRESS,       // ≥ 3 seconds (calibration)
    BOOT_HOLD,        // ≥ 5 seconds on boot (factory reset)
};

/**
 * @brief Initialize button
 *
 * Configures PA0 as input with pull-up and enables pin change interrupt
 */
void button_init();

/**
 * @brief Check button state and return events
 *
 * Call this periodically (e.g., every 10s wake or more frequently during active monitoring)
 *
 * @return ButtonEvent if detected, NONE otherwise
 */
ButtonEvent button_check();

/**
 * @brief Check if button is currently pressed
 *
 * @return true if button is pressed (PA0 = LOW)
 */
bool button_is_pressed();

/**
 * @brief Wait for button press with timeout (blocking)
 *
 * Used during boot to detect 5-second hold for factory reset
 *
 * @param timeout_ms Maximum time to wait
 * @return true if button was pressed within timeout
 */
bool button_wait_pressed(uint16_t timeout_ms);

/**
 * @brief Get button press duration
 *
 * Returns how long button has been pressed (in approximate deciseconds)
 * Used to distinguish short/long/boot presses
 *
 * @return Duration in ~100ms units (deciseconds)
 */
uint8_t button_get_press_duration();
