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
#include <util/delay.h>

// Buzzer state machine
struct BuzzerState
{
    BeepPattern pattern;
    uint8_t beeps_remaining;
    uint8_t beep_phase; // 0 = beep on, 1 = gap
    uint16_t phase_counter;
};

static BuzzerState state = {BeepPattern::NONE, 0, 0, 0};

// Timing constants (in milliseconds, tracked by buzzer_update() calls)
constexpr uint16_t BEEP_DURATION_MS = 150;  // Longer beeps for clarity
constexpr uint16_t BEEP_GAP_MS = 150;       // Longer gaps to distinguish patterns

void buzzer_init()
{
    // Configure PA3 as output for PWM
    PORTA.DIRSET = pins::DRV_IN1;
    PORTA.OUTCLR = pins::DRV_IN1; // Initially LOW

    // Configure TCA0 for PWM generation
    // Target frequency: 4000 Hz (optimal resonant frequency found by testing)
    // Clock source: CLK_PER (10 MHz)
    // Prescaler: DIV16 → 625 kHz timer clock
    // Period for 4000 Hz: 625 kHz / 4000 Hz = 156.25

    TCA0.SINGLE.CTRLA = 0; // Stop timer

    // Single-slope PWM mode
    TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc;

    // Set period for 4000 Hz
    // With prescaler DIV16: f = F_CPU / (16 * (PER + 1))
    // PER = F_CPU / (16 * f) - 1
    // PER = 10000000 / (16 * 4000) - 1 = 156
    TCA0.SINGLE.PER = 156;

    // Set 48% duty cycle (optimal volume found by testing)
    TCA0.SINGLE.CMP0 = 75; // 48% of 156

    // Prescaler DIV16, but don't enable yet
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV16_gc;

    // Clear state
    state.pattern = BeepPattern::NONE;
    state.beeps_remaining = 0;
}

static void buzzer_pwm_enable()
{
    // Ensure GPIO low before peripheral takes over
    PORTA.DIRSET = pins::DRV_IN1;
    PORTA.OUTCLR = pins::DRV_IN1;

    // Start timer from BOTTOM
    TCA0.SINGLE.CNT = 0;

    // Enable output + timer
    TCA0.SINGLE.CTRLB |= TCA_SINGLE_CMP0EN_bm;
    TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;
}

static void buzzer_pwm_disable()
{
    // Wait until we are in the LOW half:
    // CMP0 flag sets when output transitions HIGH->LOW
    while (!(TCA0.SINGLE.INTFLAGS & TCA_SINGLE_CMP0_bm))
    {
        // worst-case wait is < 1/2 period (~130 µs at 3.8 kHz)
    }
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_CMP0_bm; // clear cmp flag

    // Now output is LOW. Disconnect and stop.
    TCA0.SINGLE.CTRLB &= ~TCA_SINGLE_CMP0EN_bm; // release WO0
    TCA0.SINGLE.CTRLA &= ~TCA_SINGLE_ENABLE_bm; // stop timer

    // Hand pin to GPIO and hold low
    PORTA.DIRSET = pins::DRV_IN1;
    PORTA.OUTCLR = pins::DRV_IN1;
}

void buzzer_start(BeepPattern pattern)
{
    if (pattern == BeepPattern::NONE)
    {
        buzzer_stop();
        return;
    }

    // Initialize state machine
    state.pattern = pattern;
    state.beeps_remaining = static_cast<uint8_t>(pattern);
    state.beep_phase = 0; // Start with beep on
    state.phase_counter = 0;

    // Start PWM
    buzzer_pwm_enable();
}

bool buzzer_update()
{
    if (state.pattern == BeepPattern::NONE || state.beeps_remaining == 0)
    {
        return false; // Not active
    }

    // Increment phase counter (assumes called every ~1 ms)
    state.phase_counter++;

    if (state.beep_phase == 0)
    {
        // Currently beeping
        if (state.phase_counter >= BEEP_DURATION_MS)
        {
            state.phase_counter = 0;
            state.beeps_remaining--;

            if (state.beeps_remaining == 0)
            {
                // Pattern complete
                buzzer_stop();
                return false;
            }

            // Move to gap phase
            state.beep_phase = 1;
            buzzer_pwm_disable();
        }
    }
    else
    {
        // Currently in gap
        if (state.phase_counter >= BEEP_GAP_MS)
        {
            state.phase_counter = 0;
            state.beep_phase = 0;
            buzzer_pwm_enable();
        }
    }

    return true; // Still active
}

void buzzer_stop()
{
    buzzer_pwm_disable();
    state.pattern = BeepPattern::NONE;
    state.beeps_remaining = 0;
    state.phase_counter = 0;
    state.beep_phase = 0;
}

bool buzzer_is_active()
{
    return state.pattern != BeepPattern::NONE && state.beeps_remaining > 0;
}
