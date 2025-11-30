/**
 * @file rtc.cpp
 * @brief PIT wake every 1 second, count to 8 for main loop
 */

#include "rtc.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

extern void RTC_PIT_vect_impl();

static volatile uint32_t tick_counter = 0;
static volatile uint8_t second_counter = 0;
static volatile bool wake_flag = false;

void rtc_init()
{
    while (RTC.STATUS > 0);

    // 32.768 kHz internal oscillator
    RTC.CLKSEL = RTC_CLKSEL_INT32K_gc;
    while (RTC.STATUS > 0);

    // Ensure RTC counter is disabled
    RTC.CTRLA = 0;
    while (RTC.STATUS > 0);

    // Wait for clock to stabilize
    _delay_ms(100);

    // PIT fires every 1 second (32768 cycles at 32.768 kHz)
    RTC.PITCTRLA = RTC_PERIOD_CYC32768_gc | RTC_PITEN_bm;
    RTC.PITINTCTRL = RTC_PI_bm;
}

ISR(RTC_PIT_vect)
{
    RTC.PITINTFLAGS = RTC_PI_bm;

    // Count to 8 seconds before waking main loop
    second_counter++;
    if (second_counter >= 8) {
        second_counter = 0;
        tick_counter++;
        wake_flag = true;
        RTC_PIT_vect_impl();
    }
}

uint32_t rtc_get_ticks()
{
    uint32_t t;
    cli();
    t = tick_counter;
    sei();
    return t;
}

bool rtc_should_wake()
{
    bool should_wake;
    cli();
    should_wake = wake_flag;
    wake_flag = false;  // Clear flag
    sei();
    return should_wake;
}
