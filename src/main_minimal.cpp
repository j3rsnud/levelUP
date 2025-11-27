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
#define THRESHOLD_LOW_FF 800
#define THRESHOLD_VLOW_FF 500
#define THRESHOLD_CRIT_FF 300

// Simple state
static uint8_t last_level = 0; // 0=Normal, 1=Low, 2=VeryLow, 3=Critical

/**
 * Determine water level from raw readings
 */
static uint8_t get_level(int16_t c1, int16_t c2, int16_t c3)
{
    if (c3 < THRESHOLD_CRIT_FF)
        return 3; // Critical
    if (c2 < THRESHOLD_VLOW_FF)
        return 2; // Very Low
    if (c1 < THRESHOLD_LOW_FF)
        return 1; // Low
    return 0;     // Normal
}

/**
 * Play beep pattern for level
 */
static void beep_for_level(uint8_t level)
{
    BeepPattern pattern = BeepPattern::NONE;

    switch (level)
    {
    case 1:
        pattern = BeepPattern::DOUBLE;
        break;
    case 2:
        pattern = BeepPattern::TRIPLE;
        break;
    case 3:
        pattern = BeepPattern::FIVE;
        break;
    default:
        return;
    }

    buzzer_start(pattern);

    // Update buzzer (blocking for simplicity)
    while (buzzer_is_active())
    {
        buzzer_update();
        _delay_ms(1);
    }
}

int main(void)
{
    // Initialize
    power_init();
    buzzer_init();
    power_enable_peripherals();
    _delay_ms(50);
    twi_init();

    // Init sensor - hang if fail
    while (!fdc_init())
    {
        _delay_ms(1000);
    }

    // Two-point calibration: DRY then WET
    _delay_ms(1000);

    // Cal 1: DRY (no water on electrodes)
    TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc | TCA_SINGLE_CMP0EN_bm;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV16_gc | TCA_SINGLE_ENABLE_bm;
    _delay_ms(200);
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV16_gc;

    FdcReading dry1 = fdc_measure(FdcChannel::C1, 50);
    FdcReading dry2 = fdc_measure(FdcChannel::C2, 50);
    FdcReading dry3 = fdc_measure(FdcChannel::C3, 50);

    // Wait for user to wet/cover all electrodes
    _delay_ms(3000);

    // Cal 2: WET (water on electrodes)
    TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc | TCA_SINGLE_CMP0EN_bm;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV16_gc | TCA_SINGLE_ENABLE_bm;
    _delay_ms(200);
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV16_gc;

    FdcReading wet1 = fdc_measure(FdcChannel::C1, 50);
    FdcReading wet2 = fdc_measure(FdcChannel::C2, 50);
    FdcReading wet3 = fdc_measure(FdcChannel::C3, 50);

    // Set thresholds midway between dry and wet
    int16_t thresh1 = (dry1.valid && wet1.valid) ? (dry1.capacitance_ff + wet1.capacitance_ff) / 2 : 500;
    int16_t thresh2 = (dry2.valid && wet2.valid) ? (dry2.capacitance_ff + wet2.capacitance_ff) / 2 : 500;
    int16_t thresh3 = (dry3.valid && wet3.valid) ? (dry3.capacitance_ff + wet3.capacitance_ff) / 2 : 500;

    // Success - 3 quick beeps
    for (uint8_t i = 0; i < 3; i++)
    {
        TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV16_gc | TCA_SINGLE_ENABLE_bm;
        _delay_ms(80);
        TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV16_gc;
        _delay_ms(80);
    }

    // Main loop - monitor water levels
    bool alarm_active = false;

    while (1)
    {
        // Read all 3 channels
        FdcReading r1 = fdc_measure(FdcChannel::C1, 50);
        FdcReading r2 = fdc_measure(FdcChannel::C2, 50);
        FdcReading r3 = fdc_measure(FdcChannel::C3, 50);

        // Check if any electrode is missing water
        alarm_active = false;
        if (r1.valid && r1.capacitance_ff < thresh1) alarm_active = true;
        if (r2.valid && r2.capacitance_ff < thresh2) alarm_active = true;
        if (r3.valid && r3.capacitance_ff < thresh3) alarm_active = true;

        if (alarm_active)
        {
            // Water missing - beep pattern
            TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc | TCA_SINGLE_CMP0EN_bm;
            TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV16_gc | TCA_SINGLE_ENABLE_bm;
            _delay_ms(200);  // Beep on
            TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV16_gc;
            PORTA.OUTCLR = pins::DRV_IN1;
            _delay_ms(800);  // Beep off
        }
        else
        {
            _delay_ms(100);  // Check rate when OK
        }
    }

    return 0;
}
