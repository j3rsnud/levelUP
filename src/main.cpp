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
#include <util/delay.h>
#include "pins.hpp"
#include "power.h"
#include "button.h"
#include "buzzer.h"
#include "twi.h"
#include "fdc1004.h"
#include "log_updi_tx.h"

// Simple timestamp counter (seconds since boot)
static uint16_t timestamp_sec = 0;

// Baseline values for drift correction (learned on first reading with tank full/wet)
// Calibration: base1, base2, base3, base4 (all 4 channels in wet/normal state)
static int16_t base1 = 0;
static int16_t base2 = 0;
static int16_t base3 = 0;
static int16_t base4 = 0;
static bool baseline_set = false;

// Threshold for level detection (drift-corrected value)
#define THRESHOLD 100

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
    constexpr uint8_t NUM_SAMPLES = 10;
    int32_t sum_c1 = 0, sum_c2 = 0, sum_c3 = 0, sum_c4 = 0;
    uint8_t valid_samples = 0;

    log_debug("Calibrating...");

    // Enable VDD_SW to power FDC1004 sensor
    power_enable_peripherals();
    delay_ms(10);  // Power stabilization

    // Initialize I2C
    twi_init();

    // Initialize FDC1004 sensor
    if (!fdc_init()) {
        log_debug("ERROR: FDC1004 init failed");
        power_disable_peripherals();
        return false;
    }

    // Take multiple samples
    for (uint8_t i = 0; i < NUM_SAMPLES; i++) {
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

        delay_ms(100);  // Small delay between samples
    }

    // Check if we got enough valid samples
    if (valid_samples < NUM_SAMPLES / 2) {
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
    // Level 1 (LOW): dc1 > 100
    if (!level1_triggered && dc1 > THRESHOLD) {
        level1_triggered = true;
        log_debug("LOW level detected");
        play_beep(BeepPattern::SINGLE);  // 1 beep
    }

    // Level 2 (VERY_LOW): dc2 > 100
    if (!level2_triggered && dc2 > THRESHOLD) {
        level2_triggered = true;
        log_debug("VERY_LOW level detected");
        play_beep(BeepPattern::DOUBLE);  // 2 beeps
    }

    // Level 3 (CRITICAL): dc3 > 100
    if (!level3_triggered && dc3 > THRESHOLD) {
        level3_triggered = true;
        log_debug("CRITICAL level detected");
        play_beep(BeepPattern::TRIPLE);  // 3 beeps
    }

    // Increment timestamp (in 1-second intervals)
    timestamp_sec += 1;
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

    // Wait 10 seconds for user to ensure tank is full/wet
    log_debug("Waiting 10 seconds...");
    for (uint8_t i = 0; i < 10; i++) {
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

    // Main loop - measure every 1 second (fast logging for testing)
    while (1) {
        // Measure and log
        measure_and_log();

        // Wait 1 second
        delay_ms(1000);
    }

    return 0;
}
