/**
 * @file main_test.cpp
 * @brief TEST BENCH version for ATtiny202
 *
 * This version cycles through all water levels automatically for testing:
 * - Simulates sensor readings (no FDC1004 needed)
 * - Cycles: Normal → Low → Very-Low → Critical → Normal (repeat)
 * - LED blinks show current level
 * - Allows testing of beep patterns, power gating, sleep cycles
 *
 * Hardware needed:
 * - ATtiny202 with power
 * - DRV8210 + piezo (for beep testing)
 * - LED (for level indication)
 * - Current meter (optional, for power measurement)
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "pins.hpp"
#include "power.h"
#include "rtc.h"
#include "buzzer.h"

// Test configuration
#define TEST_CYCLES_PER_LEVEL  3  // Stay at each level for 3 cycles (30 seconds)

// Simulated sensor readings (femtofarads)
struct TestReading {
    int16_t c1;
    int16_t c2;
    int16_t c3;
    const char* name;
};

// Test scenarios with different capacitance values
static const TestReading TEST_LEVELS[] = {
    {1200, 1100, 1000, "NORMAL (Full tank)"},      // All above thresholds
    {600,  1100, 1000, "LOW (Below CIN1)"},        // CIN1 < 800
    {600,  400,  1000, "VERY-LOW (Below CIN2)"},   // CIN2 < 500
    {600,  400,  200,  "CRITICAL (Below CIN3)"},   // CIN3 < 300
};

#define NUM_TEST_LEVELS (sizeof(TEST_LEVELS) / sizeof(TEST_LEVELS[0]))

// Hardcoded thresholds (same as main_minimal.cpp)
#define THRESHOLD_LOW_FF    800
#define THRESHOLD_VLOW_FF   500
#define THRESHOLD_CRIT_FF   300

// Test state
static uint8_t test_level_index = 0;
static uint8_t cycles_at_level = 0;
static uint8_t last_level = 0;

/**
 * LED blink patterns to show current test level
 * 1 blink = Normal, 2 = Low, 3 = Very-Low, 4 = Critical
 */
static void led_show_level(uint8_t level) {
    uint8_t blinks = level + 1;
    for (uint8_t i = 0; i < blinks; i++) {
        PORTA.OUTSET = pins::LED;
        _delay_ms(200);
        PORTA.OUTCLR = pins::LED;
        if (i < blinks - 1) {
            _delay_ms(200);
        }
    }
}

/**
 * Determine water level from simulated readings
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

    // Update buzzer (blocking)
    while (buzzer_is_active()) {
        buzzer_update();
        _delay_ms(1);
    }
}

/**
 * Long LED blink to show test starting
 */
static void indicate_test_start() {
    for (uint8_t i = 0; i < 5; i++) {
        PORTA.OUTSET = pins::LED;
        _delay_ms(100);
        PORTA.OUTCLR = pins::LED;
        _delay_ms(100);
    }
    _delay_ms(500);
}

int main(void) {
    // Initialize
    power_init();
    rtc_init();
    buzzer_init();

    // Test start indication
    indicate_test_start();

    sei();  // Enable interrupts

    // Main test loop
    while (1) {
        // Get current test level readings
        const TestReading* current = &TEST_LEVELS[test_level_index];

        // Show current level on LED (non-blocking indication)
        led_show_level(test_level_index);

        // Enable peripherals (simulate sensor power-up)
        power_enable_peripherals();
        _delay_ms(5);  // Simulate sensor stabilization

        // Simulate "reading" the sensor
        int16_t c1 = current->c1;
        int16_t c2 = current->c2;
        int16_t c3 = current->c3;

        // Determine level
        uint8_t level = get_level(c1, c2, c3);

        // If level changed and not normal, beep
        if (level != last_level && level > 0) {
            beep_for_level(level);
        }

        last_level = level;

        // Power down
        power_disable_peripherals();

        // Count cycles at this level
        cycles_at_level++;

        // After TEST_CYCLES_PER_LEVEL, move to next level
        if (cycles_at_level >= TEST_CYCLES_PER_LEVEL) {
            cycles_at_level = 0;
            test_level_index++;

            // Wrap around to beginning
            if (test_level_index >= NUM_TEST_LEVELS) {
                test_level_index = 0;

                // Long blink to show test cycle complete
                _delay_ms(1000);
                PORTA.OUTSET = pins::LED;
                _delay_ms(500);
                PORTA.OUTCLR = pins::LED;
                _delay_ms(1000);
            }
        }

        // Sleep until next wake (10 seconds)
        power_sleep();
    }

    return 0;
}
