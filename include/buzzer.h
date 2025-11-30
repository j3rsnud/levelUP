/**
 * @file buzzer.h
 * @brief PWM tone generation for piezo buzzer via DRV8210
 *
 * Generates ~2.7 kHz tone using TCA0 WO0 on PA3
 * Beep patterns: 2, 3, or 5 beeps (100 ms each) with 100 ms gaps
 */

#pragma once

#include <stdint.h>

/**
 * Beep patterns (number of beeps in burst)
 */
enum class BeepPattern : uint8_t {
    NONE = 0,
    SINGLE = 1,   // Calibration entry
    DOUBLE = 2,   // Low level
    TRIPLE = 3,   // Very-Low level
    FIVE = 5,     // Critical level
};

/**
 * @brief Initialize buzzer PWM
 *
 * Configures TCA0 for ~2.7 kHz PWM output on PA3 (DRV_IN1)
 * PWM starts disabled
 */
void buzzer_init();

/**
 * @brief Start beep pattern
 *
 * Begins playing the specified pattern
 * Non-blocking - call buzzer_update() in main loop to advance pattern
 *
 * @param pattern Number of beeps to play
 */
void buzzer_start(BeepPattern pattern);

/**
 * @brief Update buzzer state machine
 *
 * Call this regularly from main loop (every ~1 ms or so)
 * Manages beep timing and pattern sequencing
 *
 * @return true if still playing, false if pattern complete
 */
bool buzzer_update();

/**
 * @brief Stop buzzer immediately
 *
 * Disables PWM and clears pattern
 */
void buzzer_stop();

/**
 * @brief Check if buzzer is currently active
 *
 * @return true if pattern is playing
 */
bool buzzer_is_active();
