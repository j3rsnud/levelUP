/**
 * @file buzzer.cpp
 * @brief PWM tone generation for piezo buzzer via DRV8210
 *
 * Generates ~2.7 kHz tone using TCA0 WO0 on PA3
 * DRV8210 is configured in MODE=HIGH (complementary single-input mode)
 */

#include "buzzer.h"
#include "pins.hpp"
#include <avr/io.h>

// Buzzer state machine
struct BuzzerState {
    BeepPattern pattern;
    uint8_t beeps_remaining;
    uint8_t beep_phase;  // 0 = beep on, 1 = gap
    uint16_t phase_counter;
};

static BuzzerState state = {BeepPattern::NONE, 0, 0, 0};

// Timing constants (in milliseconds, tracked by buzzer_update() calls)
constexpr uint16_t BEEP_DURATION_MS = 100;
constexpr uint16_t BEEP_GAP_MS = 100;

void buzzer_init() {
    // Configure PA3 as output for PWM
    PORTA.DIRSET = pins::DRV_IN1;
    PORTA.OUTCLR = pins::DRV_IN1;  // Initially LOW

    // Configure TCA0 for PWM generation
    // Target frequency: 3800 Hz (resonant frequency)
    // Clock source: CLK_PER (10 MHz)
    // Prescaler: DIV16 → 625 kHz timer clock
    // Period for 3800 Hz: 625 kHz / 3800 Hz ≈ 164

    TCA0.SINGLE.CTRLA = 0;  // Stop timer

    // Single-slope PWM mode
    TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc;

    // Set period for 3800 Hz
    // With prescaler DIV16: f = F_CPU / (16 * (PER + 1))
    // PER = F_CPU / (16 * f) - 1
    // PER = 10000000 / (16 * 3800) - 1 ≈ 164
    TCA0.SINGLE.PER = 164;

    // Set 50% duty cycle
    TCA0.SINGLE.CMP0 = 82;  // 50% of 164

    // Prescaler DIV16, but don't enable yet
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV16_gc;

    // Clear state
    state.pattern = BeepPattern::NONE;
    state.beeps_remaining = 0;
}

static void buzzer_pwm_enable() {
    // Enable PWM output on WO0 (PA3)
    TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc | TCA_SINGLE_CMP0EN_bm;

    // Enable timer
    TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;
}

static void buzzer_pwm_disable() {
    // Disable timer
    TCA0.SINGLE.CTRLA &= ~TCA_SINGLE_ENABLE_bm;

    // Disable PWM output
    TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc;

    // Ensure pin is LOW
    PORTA.OUTCLR = pins::DRV_IN1;
}

void buzzer_start(BeepPattern pattern) {
    if (pattern == BeepPattern::NONE) {
        buzzer_stop();
        return;
    }

    // Initialize state machine
    state.pattern = pattern;
    state.beeps_remaining = static_cast<uint8_t>(pattern);
    state.beep_phase = 0;  // Start with beep on
    state.phase_counter = 0;

    // Start PWM
    buzzer_pwm_enable();
}

bool buzzer_update() {
    if (state.pattern == BeepPattern::NONE || state.beeps_remaining == 0) {
        return false;  // Not active
    }

    // Increment phase counter (assumes called every ~1 ms)
    state.phase_counter++;

    if (state.beep_phase == 0) {
        // Currently beeping
        if (state.phase_counter >= BEEP_DURATION_MS) {
            state.phase_counter = 0;
            state.beeps_remaining--;

            if (state.beeps_remaining == 0) {
                // Pattern complete
                buzzer_stop();
                return false;
            }

            // Move to gap phase
            state.beep_phase = 1;
            buzzer_pwm_disable();
        }
    } else {
        // Currently in gap
        if (state.phase_counter >= BEEP_GAP_MS) {
            state.phase_counter = 0;
            state.beep_phase = 0;
            buzzer_pwm_enable();
        }
    }

    return true;  // Still active
}

void buzzer_stop() {
    buzzer_pwm_disable();
    state.pattern = BeepPattern::NONE;
    state.beeps_remaining = 0;
    state.phase_counter = 0;
    state.beep_phase = 0;
}

bool buzzer_is_active() {
    return state.pattern != BeepPattern::NONE && state.beeps_remaining > 0;
}
