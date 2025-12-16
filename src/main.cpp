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

// Baseline values for drift correction (learned on first reading)
static int16_t baseline_c1 = 0;
static int16_t baseline_c2 = 0;
static int16_t baseline_c3 = 0;
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
 */
static void measure_and_log() {
    // Read all channels
    FdcReading r1 = fdc_measure(FdcChannel::C1, 20);
    FdcReading r2 = fdc_measure(FdcChannel::C2, 20);
    FdcReading r3 = fdc_measure(FdcChannel::C3, 20);

    // Check if readings are valid
    if (!r1.valid || !r2.valid || !r3.valid) {
        log_debug("ERROR: Invalid readings");
        return;
    }

    // Set baseline on first valid reading
    if (!baseline_set) {
        baseline_c1 = r1.capacitance_ff;
        baseline_c2 = r2.capacitance_ff;
        baseline_c3 = r3.capacitance_ff;
        baseline_set = true;
        log_debug("Baseline set");
    }

    // Log raw sensor data
    // Note: c4 set to 0 since we're using single-ended mode
    log_sensor_data(r1.capacitance_ff, r2.capacitance_ff, r3.capacitance_ff, 0, timestamp_sec);

    // Calculate drift-corrected deltas
    int16_t dc1 = r1.capacitance_ff - baseline_c1;
    int16_t dc2 = r2.capacitance_ff - baseline_c2;
    int16_t dc3 = r3.capacitance_ff - baseline_c3;

    // Log drift-corrected values
    log_drift_corrected(dc1, dc2, dc3);

    // Increment timestamp
    timestamp_sec++;
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
