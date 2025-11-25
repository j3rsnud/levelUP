/**
 * @file rtc.cpp
 * @brief RTC/PIT implementation for 10-second wake timer
 */

#include "rtc.h"
#include "power.h"
#include <avr/io.h>
#include <avr/interrupt.h>

// Tick counter (increments every 10 seconds)
static volatile uint32_t tick_counter = 0;

void rtc_init() {
    // Wait for all RTC registers to be ready
    while (RTC.STATUS > 0);

    // Select 32.768 kHz internal ULP oscillator
    RTC.CLKSEL = RTC_CLKSEL_INT32K_gc;

    // Configure PIT for 10-second period
    // PIT period options on ATtiny202:
    // - RTC_PERIOD_CYC32768_gc = 1 second @ 32.768 kHz
    // - Need 10 seconds, so we'll use RTC_PERIOD_CYC32768_gc and count in ISR
    // OR use longer period if available
    //
    // For 10 seconds with 32.768 kHz clock:
    // Closest available: RTC_PERIOD_CYC16384_gc = 0.5s
    // or RTC_PERIOD_CYC32768_gc = 1s
    //
    // Let's use 1-second period and count 10 ticks in ISR
    // Or better: check datasheet for longer periods
    //
    // ATtiny202 PIT periods (at 32.768 kHz):
    // CYC4_gc = 122 us
    // CYC8_gc = 244 us
    // ...
    // CYC256_gc = 7.8 ms
    // CYC512_gc = 15.6 ms
    // CYC1024_gc = 31.2 ms
    // CYC2048_gc = 62.5 ms
    // CYC4096_gc = 125 ms
    // CYC8192_gc = 250 ms
    // CYC16384_gc = 500 ms
    // CYC32768_gc = 1 second
    //
    // Use 1 second and count 10 in ISR
    RTC.PITCTRLA = RTC_PERIOD_CYC32768_gc | RTC_PITEN_bm;

    // Enable PIT interrupt
    RTC.PITINTCTRL = RTC_PI_bm;

    // Start RTC (just for PIT, we're not using RTC counter itself)
    // PIT runs independently
}

uint32_t rtc_get_ticks() {
    uint32_t ticks;
    // Atomic read of 32-bit counter
    cli();
    ticks = tick_counter;
    sei();
    return ticks;
}

// PIT interrupt service routine
ISR(RTC_PIT_vect) {
    static uint8_t second_counter = 0;

    // Clear interrupt flag
    RTC.PITINTFLAGS = RTC_PI_bm;

    // Count to 10 seconds
    second_counter++;
    if (second_counter >= 10) {
        second_counter = 0;
        tick_counter++;

        // Notify power module of wake source
        RTC_PIT_vect_impl();
    }
}
