/**
 * @file main_minimal.cpp
 * @brief MINIMAL version for ATtiny202 (2KB flash) testing
 *
 * Features included:
 * - Basic sensor reading (FDC1004)
 * - Simple 3-level detection (hardcoded thresholds)
 * - Beep patterns (2/3/5 beeps)
 * - 10-second wake cycle
 * - Power gating
 *
 * Features REMOVED (for ATtiny402):
 * - EEPROM config
 * - Calibration
 * - 5-minute alert windows
 * - Hysteresis
 * - Debouncing
 * - Button support
 * - LED diagnostics
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "pins.hpp"
#include "power.h"
#include "rtc.h"
#include "twi.h"
#include "fdc1004.h"
#include "buzzer.h"

// Hardcoded thresholds (femtofarads)
#define THRESHOLD_LOW_FF    800
#define THRESHOLD_VLOW_FF   500
#define THRESHOLD_CRIT_FF   300

// Simple state
static uint8_t last_level = 0;  // 0=Normal, 1=Low, 2=VeryLow, 3=Critical

/**
 * Determine water level from raw readings
 */
static uint8_t get_level(int16_t c1, int16_t c2, int16_t c3) {
    if (c3 < THRESHOLD_CRIT_FF) return 3;  // Critical
    if (c2 < THRESHOLD_VLOW_FF) return 2;  // Very Low
    if (c1 < THRESHOLD_LOW_FF) return 1;   // Low
    return 0;  // Normal
}

/**
 * Play beep pattern for level
 */
static void beep_for_level(uint8_t level) {
    BeepPattern pattern = BeepPattern::NONE;

    switch (level) {
        case 1: pattern = BeepPattern::DOUBLE; break;
        case 2: pattern = BeepPattern::TRIPLE; break;
        case 3: pattern = BeepPattern::FIVE; break;
        default: return;
    }

    buzzer_start(pattern);

    // Update buzzer (blocking for simplicity)
    while (buzzer_is_active()) {
        buzzer_update();
        _delay_ms(1);
    }
}

int main(void) {
    // Initialize
    power_init();
    rtc_init();
    buzzer_init();

    // Boot indication
    PORTA.OUTSET = pins::LED;
    _delay_ms(100);
    PORTA.OUTCLR = pins::LED;

    sei();  // Enable interrupts

    // Main loop
    while (1) {
        // Enable peripherals
        power_enable_peripherals();

        // Initialize I2C and sensor
        twi_init();

        if (fdc_init()) {
            // Read all three channels
            FdcReading r1 = fdc_measure(FdcChannel::C1, 20);
            FdcReading r2 = fdc_measure(FdcChannel::C2, 20);
            FdcReading r3 = fdc_measure(FdcChannel::C3, 20);

            // Check if valid
            if (r1.valid && r2.valid && r3.valid) {
                // Determine level
                uint8_t level = get_level(r1.capacitance_ff, r2.capacitance_ff, r3.capacitance_ff);

                // If level changed and not normal, beep
                if (level != last_level && level > 0) {
                    beep_for_level(level);
                }

                last_level = level;
            }
        }

        // Power down
        power_disable_peripherals();

        // Sleep until next wake
        power_sleep();
    }

    return 0;
}
