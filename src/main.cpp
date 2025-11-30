/**
 * @file main.cpp
 * @brief Main state machine for water level sensor
 *
 * Simplified flow (every 8 seconds):
 * 1. Wake from sleep
 * 2. Enable VDD_SW (power on FDC1004)
 * 3. Measure water level
 * 4. If level low and counter < 38: beep + increment
 * 5. If level normal: reset counter
 * 6. Disable VDD_SW
 * 7. Return to sleep
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "pins.hpp"
#include "power.h"
#include "rtc.h"
#include "twi.h"
#include "fdc1004.h"
#include "level_logic.h"
#include "buzzer.h"
#include "button.h"
#include "eeprom_config.h"

// Simple alert tracking
static uint8_t beep_counter = 0;
constexpr uint8_t MAX_BEEPS = 38;  // 38 beeps * 8s = 304s ≈ 5 min

// Runtime delay function (not compile-time constant)
static void delay_ms(uint16_t ms) {
    while (ms > 0) {
        _delay_ms(1);
        ms--;
    }
}

/**
 * Calibration mode: Learn baseline with tank full
 * Takes 6 samples per channel and averages (reduced for flash savings)
 */
static bool perform_calibration() {
    constexpr uint8_t NUM_SAMPLES = 6;
    int32_t sum_c1 = 0, sum_c2 = 0, sum_c3 = 0;
    uint8_t valid_samples = 0;

    // Take multiple samples
    for (uint8_t i = 0; i < NUM_SAMPLES; i++) {
        FdcReading r1 = fdc_measure(FdcChannel::C1, 20);
        FdcReading r2 = fdc_measure(FdcChannel::C2, 20);
        FdcReading r3 = fdc_measure(FdcChannel::C3, 20);

        if (r1.valid && r2.valid && r3.valid) {
            sum_c1 += r1.capacitance_ff;
            sum_c2 += r2.capacitance_ff;
            sum_c3 += r3.capacitance_ff;
            valid_samples++;
        }

        delay_ms(100);  // Small delay between samples
    }

    // Check if we got enough valid samples
    if (valid_samples < NUM_SAMPLES / 2) {
        return false;
    }

    // Calculate averages
    int16_t avg_c1 = sum_c1 / valid_samples;
    int16_t avg_c2 = sum_c2 / valid_samples;
    int16_t avg_c3 = sum_c3 / valid_samples;

    // Save to EEPROM
    bool success = eeprom_update_calibration(avg_c1, avg_c2, avg_c3);

    // Beep feedback: 2 beeps = success, 5 beeps = failed
    if (success) {
        buzzer_start(BeepPattern::DOUBLE);  // 2 beeps = success
    } else {
        buzzer_start(BeepPattern::FIVE);    // 5 beeps = failed
    }

    // Wait for beep to complete
    while (buzzer_is_active()) {
        buzzer_update();
        delay_ms(1);
    }

    return success;
}

/**
 * Check for factory reset on boot (5-second button hold)
 */
static void check_factory_reset() {
    if (button_is_pressed()) {
        // Button pressed on boot, wait to see if held for 5 seconds
        for (uint8_t i = 0; i < 50; i++) {  // 50 × 100ms = 5 seconds
            if (!button_is_pressed()) {
                return;  // Released before 5 seconds
            }
            delay_ms(100);
        }

        // Button held for 5 seconds, perform factory reset
        eeprom_factory_reset();
        delay_ms(1000);  // Give user time to release button
    }
}

/**
 * Initialize all peripherals and modules
 */
static void system_init() {
    // Initialize power management
    power_init();

    // Check for factory reset (no LED indication to save flash)
    check_factory_reset();

    // Initialize button
    button_init();

    // Initialize RTC for 10-second wakes
    rtc_init();

    // Initialize EEPROM config
    eeprom_init();

    // Load configuration
    NvmConfig config;
    eeprom_get_config(&config);

    // Initialize level logic with loaded config
    LevelThresholds thresholds = {
        .low_ff = (int16_t)config.th_low_ff,
        .vlow_ff = (int16_t)config.th_vlow_ff,
        .crit_ff = (int16_t)config.th_crit_ff,
        .hysteresis_pct = (uint8_t)config.hysteresis_pct
    };

    CalibrationData calibration = {
        .base_c1_ff = config.base_c1_ff,
        .base_c2_ff = config.base_c2_ff,
        .base_c3_ff = config.base_c3_ff,
        .valid = (config.calibration_valid != 0)
    };

    level_init(thresholds, calibration);

    // Initialize buzzer
    buzzer_init();

    // Enable global interrupts
    sei();
}

/**
 * Perform measurement cycle
 */
static void measurement_cycle() {
    // Enable peripherals (VDD_SW on)
    power_enable_peripherals();

    // Initialize TWI
    twi_init();

    // Initialize FDC1004
    if (!fdc_init()) {
        // FDC1004 init failed - skip this cycle
        power_disable_peripherals();
        return;
    }

    // Perform level measurement
    level_update();
}

/**
 * Main loop
 */
int main(void) {
    // Initialize system
    system_init();

    // Main loop (PIT wakes every 1 second, measure every 8 seconds)
    while (1) {
        // Check button FIRST on every wake (for responsiveness)
        if (button_is_pressed()) {
            // Button detected - poll every 100ms to measure duration
            uint8_t duration = 0;
            while (button_is_pressed() && duration < 50) {  // Max 5 seconds
                delay_ms(100);
                duration++;
            }

            // Check if it was a long press (≥3 seconds = 30 deciseconds)
            if (duration >= 30) {
                // Indicate calibration mode entry with single beep
                power_enable_peripherals();
                buzzer_start(BeepPattern::SINGLE);
                while (buzzer_is_active()) {
                    buzzer_update();
                    delay_ms(1);
                }
                delay_ms(200);  // Brief pause

                // Enter calibration mode
                twi_init();
                fdc_init();
                perform_calibration();
                power_disable_peripherals();
            }

            // Wait for button release
            while (button_is_pressed()) {
                delay_ms(10);
            }

            // Go back to sleep after button handling
            power_sleep();
            continue;
        }

        // Check if 8 seconds have elapsed for measurement
        if (!rtc_should_wake()) {
            // Not time yet - go back to sleep immediately
            power_sleep();
            continue;
        }

        // 8 seconds elapsed - do measurement and alert logic
        // Single LED blink on wake - easy to time
        PORTA.OUTSET = pins::LED;
        delay_ms(100);
        PORTA.OUTCLR = pins::LED;

        // TEST: Skip measurement to test timing
        // Enable peripherals for buzzer (normally done in measurement_cycle)
        power_enable_peripherals();

        // measurement_cycle();

        // Force critical level for testing
        WaterLevel level = WaterLevel::CRITICAL;  // Force beep for testing
        // WaterLevel level = level_get_current();

        // Simple alert logic
        if (level != WaterLevel::NORMAL && level != WaterLevel::ERROR) {
            // Level is low - beep if under limit
            if (beep_counter < MAX_BEEPS) {
                // Choose pattern based on level
                BeepPattern pattern = BeepPattern::NONE;
                if (level == WaterLevel::LOW) pattern = BeepPattern::DOUBLE;
                else if (level == WaterLevel::VERY_LOW) pattern = BeepPattern::TRIPLE;
                else if (level == WaterLevel::CRITICAL) pattern = BeepPattern::FIVE;

                // Play beep pattern
                buzzer_start(pattern);
                while (buzzer_is_active()) {
                    buzzer_update();
                    delay_ms(1);
                }

                beep_counter++;
            }
            // else: silent, 5-min window expired
        } else {
            // Level normal - reset counter
            beep_counter = 0;
        }

        // Power down peripherals
        power_disable_peripherals();

        // Enter sleep mode
        power_sleep();
    }

    return 0;
}
