/**
 * @file main.cpp
 * @brief FDC1004 sensor logging test with bit-banged UART
 *
 * This version continuously reads FDC1004 sensors and logs data via UART.
 * Simplified for testing - no power management, sleep, or alerts.
 *
 * Output format matches ESP32 bridge expectations:
 * - Raw readings: "t=123 c1=456 c2=789 c3=321 c4=654"
 * - Drift-corrected: "dC: dc1=-50 dc2=-100 dc3=-200"
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "config.h"
#include "pins.hpp"
#include "power.h"
#include "rtc.h"
#include "button.h"
#include "buzzer.h"
#include "twi.h"
#include "fdc1004.h"
#include "log_updi_tx.h"

// RTC interrupt implementation (required by rtc.cpp)
void RTC_PIT_vect_impl() {
    // PIT wakes every 1 second, counts to 8, then wakes main loop
    // Nothing else needed here
}

// Simple timestamp counter (seconds since boot)
static uint16_t timestamp_sec = 0;

// Baseline values for drift correction (learned on first reading with tank full/wet)
// Calibration: base1, base2, base3, base4 (all 4 channels in wet/normal state)
static int16_t base1 = 0;
static int16_t base2 = 0;
static int16_t base3 = 0;
static int16_t base4 = 0;
static bool baseline_set = false;

// Level state tracking (for detecting transitions)
static bool level1_triggered = false;  // C1 (LOW)
static bool level2_triggered = false;  // C2 (VERY_LOW)
static bool level3_triggered = false;  // C3 (CRITICAL)

/**
 * Runtime delay helper
 */
static void delay_ms(uint16_t ms) {
    while (ms > 0) {
        _delay_ms(1);
        ms--;
    }
}

/**
 * Initialize all peripherals
 */
static void system_init() {
    // Initialize power management (enables PWR_EN pin)
    power_init();

    // Initialize button
    button_init();

    // Initialize buzzer PWM
    buzzer_init();

    // Initialize RTC for sleep/wake timing
    rtc_init();

    // Initialize UART logging FIRST (so we can see errors)
    log_init();

    // Send hello message
    log_hello();
    log_debug("Waiting for button press to calibrate...");
}

/**
 * Play beep pattern (blocking)
 */
static void play_beep(BeepPattern pattern) {
    buzzer_start(pattern);
    while (buzzer_is_active()) {
        buzzer_update();
        delay_ms(1);
    }
}

/**
 * Perform wet baseline calibration
 * Takes multiple samples and averages them
 */
static bool calibrate_baseline() {
    int32_t sum_c1 = 0, sum_c2 = 0, sum_c3 = 0, sum_c4 = 0;
    uint8_t valid_samples = 0;

    log_debug("Calibrating...");

    // Enable VDD_SW to power FDC1004 sensor
    power_enable_peripherals();
    delay_ms(POWER_STABILIZATION_MS);

    // Initialize I2C
    twi_init();

    // Initialize FDC1004 sensor
    if (!fdc_init()) {
        log_debug("ERROR: FDC1004 init failed");
        power_disable_peripherals();
        return false;
    }

    // Take multiple samples
    for (uint8_t i = 0; i < CALIBRATION_SAMPLES; i++) {
        FdcReading r1 = fdc_measure(FdcChannel::C1, 20);
        FdcReading r2 = fdc_measure(FdcChannel::C2, 20);
        FdcReading r3 = fdc_measure(FdcChannel::C3, 20);
        FdcReading r4 = fdc_measure(FdcChannel::C4, 20);

        if (r1.valid && r2.valid && r3.valid && r4.valid) {
            sum_c1 += r1.capacitance_ff;
            sum_c2 += r2.capacitance_ff;
            sum_c3 += r3.capacitance_ff;
            sum_c4 += r4.capacitance_ff;
            valid_samples++;
        }

        delay_ms(CALIBRATION_SAMPLE_DELAY_MS);
    }

    // Check if we got enough valid samples
    if (valid_samples < CALIBRATION_SAMPLES / 2) {
        log_debug("ERROR: Not enough valid samples");
        power_disable_peripherals();
        return false;
    }

    // Calculate averages
    base1 = sum_c1 / valid_samples;
    base2 = sum_c2 / valid_samples;
    base3 = sum_c3 / valid_samples;
    base4 = sum_c4 / valid_samples;
    baseline_set = true;

    log_debug("Calibration complete");

    return true;
}

/**
 * Read all FDC1004 channels and log results
 *
 * Methodology:
 * 1. Read raw r1, r2, r3, r4 (all 4 channels single-ended)
 * 2. Calculate deltas: d = base - measured (so water drop = positive)
 * 3. Drift correct: dc = d - d4 (remove common-mode drift using always-wet reference)
 * 4. Log raw (r1,r2,r3,r4) and drift-corrected (dc1,dc2,dc3)
 */
static void measure_and_log() {
    // Read all 4 channels (including C4 reference)
    FdcReading r1 = fdc_measure(FdcChannel::C1, 20);
    FdcReading r2 = fdc_measure(FdcChannel::C2, 20);
    FdcReading r3 = fdc_measure(FdcChannel::C3, 20);
    FdcReading r4 = fdc_measure(FdcChannel::C4, 20);

    // Check if readings are valid
    if (!r1.valid || !r2.valid || !r3.valid || !r4.valid) {
        log_debug("ERROR: Invalid readings");
        return;
    }

    // Calculate deltas from baseline: d = base - measured
    // (Water dropping past electrode makes d positive)
    int16_t d1 = base1 - r1.capacitance_ff;
    int16_t d2 = base2 - r2.capacitance_ff;
    int16_t d3 = base3 - r3.capacitance_ff;
    int16_t d4 = base4 - r4.capacitance_ff;

    // Drift correction using always-wet reference (C4)
    // dc = d - d4 removes common-mode environmental drift
    int16_t dc1 = d1 - d4;
    int16_t dc2 = d2 - d4;
    int16_t dc3 = d3 - d4;

    // Log raw sensor data (all 4 channels)
    log_sensor_data(r1.capacitance_ff, r2.capacitance_ff, r3.capacitance_ff, r4.capacitance_ff, timestamp_sec);

    // Log drift-corrected values (for threshold tuning)
    log_drift_corrected(dc1, dc2, dc3);

    // Check for level transitions and beep
    // Level 1 (LOW): dc1 > THRESHOLD_LOW
    if (!level1_triggered && dc1 > THRESHOLD_LOW) {
        level1_triggered = true;
        log_debug("LOW level detected");
        play_beep(BeepPattern::SINGLE);  // 1 beep
    }

    // Level 2 (VERY_LOW): dc2 > THRESHOLD_VLOW
    if (!level2_triggered && dc2 > THRESHOLD_VLOW) {
        level2_triggered = true;
        log_debug("VERY_LOW level detected");
        play_beep(BeepPattern::DOUBLE);  // 2 beeps
    }

    // Level 3 (CRITICAL): dc3 > THRESHOLD_CRIT
    if (!level3_triggered && dc3 > THRESHOLD_CRIT) {
        level3_triggered = true;
        log_debug("CRITICAL level detected");
        play_beep(BeepPattern::TRIPLE);  // 3 beeps
    }

    // Check for refill: all dc values drop below threshold with hysteresis
    if ((level1_triggered || level2_triggered || level3_triggered) &&
        dc1 < (THRESHOLD_LOW - REFILL_HYSTERESIS) &&
        dc2 < (THRESHOLD_VLOW - REFILL_HYSTERESIS) &&
        dc3 < (THRESHOLD_CRIT - REFILL_HYSTERESIS)) {
        level1_triggered = false;
        level2_triggered = false;
        level3_triggered = false;
        log_debug("Tank refilled - reset");
        play_beep(BeepPattern::DOUBLE);
    }

    // Increment timestamp by measurement interval
    timestamp_sec += MEASURE_INTERVAL_SEC;
}

int main() {
    // Initialize system
    system_init();

    // Wait for button press to start calibration
    while (!button_is_pressed()) {
        delay_ms(100);
    }

    // Button pressed - acknowledge with single beep
    power_enable_peripherals();  // Enable power for buzzer
    play_beep(BeepPattern::SINGLE);
    log_debug("Button pressed - fill tank");

    // Wait for user to ensure tank is full/wet
    log_debug("Waiting 10 seconds...");
    for (uint8_t i = 0; i < CALIBRATION_WAIT_SEC; i++) {
        delay_ms(1000);
    }

    // Perform wet baseline calibration
    if (!calibrate_baseline()) {
        // Calibration failed - 5 beeps (error)
        play_beep(BeepPattern::FIVE);
        log_debug("Calibration FAILED");
        // Hang - can't continue without calibration
        while (1) {
            delay_ms(1000);
        }
    }

    // Calibration successful - two beeps (confirmation)
    play_beep(BeepPattern::DOUBLE);
    log_debug("Starting measurements");

    // Enable interrupts for RTC
    sei();

    // Main loop - measure every 8 seconds with sleep between
    while (1) {
        // Check for recalibration request (button press during operation)
        if (button_is_pressed()) {
            log_debug("Recalibration requested");
            play_beep(BeepPattern::SINGLE);

            // Wait for button release
            while (button_is_pressed()) {
                delay_ms(100);
            }

            // Wait for user to ensure tank is full/wet
            log_debug("Fill tank - waiting 10 sec");
            for (uint8_t i = 0; i < CALIBRATION_WAIT_SEC; i++) {
                delay_ms(1000);
            }

            // Perform recalibration
            if (calibrate_baseline()) {
                // Reset level flags on successful recalibration
                level1_triggered = false;
                level2_triggered = false;
                level3_triggered = false;
                play_beep(BeepPattern::DOUBLE);
                log_debug("Recalibration complete");
            } else {
                play_beep(BeepPattern::FIVE);
                log_debug("Recalibration failed");
            }
        }

        // Check if it's time to wake (8 seconds elapsed)
        if (rtc_should_wake()) {
            // Measure and log
            measure_and_log();
        }

        // Go to sleep until next RTC wake (PIT wakes every 1s)
        power_sleep();
    }

    return 0;
}
