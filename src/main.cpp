/**
 * @file main.cpp
 * @brief Main state machine for water level sensor
 *
 * State flow:
 * BOOT → SLEEP → (wake) → MEASURE → ALERT_CHECK → TEARDOWN → SLEEP → ...
 *
 * Every 10 seconds:
 * 1. Wake from sleep
 * 2. Enable VDD_SW (power on FDC1004 + DRV8210)
 * 3. Measure water level
 * 4. Update alert state
 * 5. If alert active, play beep pattern
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
#include "alert_manager.h"
#include "buzzer.h"
#include "button.h"
#include "eeprom_config.h"

// LED helper functions
static void led_on() {
    PORTA.OUTSET = pins::LED;
}

static void led_off() {
    PORTA.OUTCLR = pins::LED;
}

// Runtime delay function (not compile-time constant)
static void delay_ms(uint16_t ms) {
    while (ms > 0) {
        _delay_ms(1);
        ms--;
    }
}

static void led_blink(uint8_t count, uint16_t on_ms, uint16_t off_ms) {
    for (uint8_t i = 0; i < count; i++) {
        led_on();
        delay_ms(on_ms);
        led_off();
        if (i < count - 1) {
            delay_ms(off_ms);
        }
    }
}

/**
 * Calibration mode: Learn baseline with tank full
 * Takes 8 samples per channel and averages (reduced from 16 for flash savings)
 */
static bool perform_calibration() {
    constexpr uint8_t NUM_SAMPLES = 8;
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

    // Initialize alert manager
    alert_init();

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
    WaterLevel old_level = level_get_current();
    WaterLevel new_level = level_update();

    // Check if level changed
    if (new_level != old_level && new_level != WaterLevel::ERROR) {
        alert_on_level_change(new_level);
    }

    // Disable TWI and power down peripherals (unless alert is about to beep)
    // We'll check alert status after this
}

/**
 * Main loop
 */
int main(void) {
    // Initialize system
    system_init();

    // Main loop
    while (1) {
        // Get current tick count
        uint32_t current_tick = rtc_get_ticks();

        // Perform measurement cycle
        measurement_cycle();

        // Update alert manager
        bool alert_active = alert_update(current_tick);

        // If alert is active, keep VDD_SW on and update buzzer
        if (alert_active) {
            // Keep buzzer updated (1ms granularity for timing)
            uint16_t timeout = 0;
            while (buzzer_is_active() && timeout < 2000) {  // Max 2 second beep sequence
                buzzer_update();
                delay_ms(1);
                timeout++;

                // Check button during beep
                if (button_is_pressed()) {
                    alert_silence();
                    buzzer_stop();
                    break;
                }
            }
        }

        // Power down peripherals
        power_disable_peripherals();

        // Check button for calibration request
        ButtonEvent btn_event = button_check();
        if (btn_event == ButtonEvent::LONG_PRESS) {
            // Enter calibration mode
            power_enable_peripherals();
            twi_init();
            fdc_init();
            perform_calibration();
            power_disable_peripherals();
        } else if (btn_event == ButtonEvent::SHORT_PRESS) {
            // Silence alert
            alert_silence();
        }

        // Enter sleep mode
        power_sleep();

        // Wake up here (RTC or button)
        // Loop continues...
    }

    return 0;
}
