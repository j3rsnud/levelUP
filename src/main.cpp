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

    // Enable VDD_SW to power FDC1004 sensor
    power_enable_peripherals();

    // Small delay for power to stabilize
    delay_ms(10);

    // Initialize UART logging FIRST (so we can see errors)
    log_init();

    // Send hello message
    log_hello();

    // Initialize I2C
    twi_init();

    // Initialize FDC1004 sensor
    if (!fdc_init()) {
        // Failed to initialize
        log_debug("ERROR: FDC1004 init failed");
        // Hang here - no sensor, can't continue
        while (1) {
            delay_ms(1000);
        }
    }

    log_debug("FDC1004 init OK");
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

    // Set baseline on first valid reading (with tank full/wet)
    if (!baseline_set) {
        base1 = r1.capacitance_ff;
        base2 = r2.capacitance_ff;
        base3 = r3.capacitance_ff;
        base4 = r4.capacitance_ff;
        baseline_set = true;
        log_debug("Baseline set (wet)");
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

    // Increment timestamp (in 8-second intervals)
    timestamp_sec += 8;
}

int main() {
    // Initialize system
    system_init();

    // Main loop - measure every 8 seconds
    while (1) {
        // Measure and log
        measure_and_log();

        // Wait 8 seconds
        delay_ms(8000);
    }

    return 0;
}
