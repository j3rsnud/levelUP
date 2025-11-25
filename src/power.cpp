/**
 * @file power.cpp
 * @brief Power management implementation
 */

#include "power.h"
#include "pins.hpp"
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// Wake source tracking
static volatile uint8_t wake_sources = 0;

void power_init() {
    // Configure PWR_EN (PA1) as output, initially LOW
    PORTA.DIRSET = pins::PWR_EN;
    PORTA.OUTCLR = pins::PWR_EN;  // VDD_SW OFF

    // Configure LED (PA2) as output, initially LOW
    PORTA.DIRSET = pins::LED;
    PORTA.OUTCLR = pins::LED;

    // Configure button (PA0) as input with pull-up (shared with UPDI)
    PORTA.DIRCLR = pins::BUTTON;
    PORTA.PIN0CTRL = PORT_PULLUPEN_bm;  // Enable pull-up

    // Configure sleep mode: STANDBY (allows RTC to run, ~0.1-0.7µA)
    set_sleep_mode(SLEEP_MODE_STANDBY);
    sleep_enable();
}

void power_enable_peripherals() {
    // Set PWR_EN HIGH to enable VDD_SW
    PORTA.OUTSET = pins::PWR_EN;

    // Wait for VDD_SW to stabilize and FDC1004/DRV8210 to power up
    // TPS22860 rise time ~1ms, add margin for peripheral startup
    _delay_ms(5);
}

void power_disable_peripherals() {
    // CRITICAL: Disable TWI peripheral first
    TWI0.MCTRLA = 0;  // Disable master mode

    // Configure I2C pins (PA6/PA7) as high-impedance inputs
    // This prevents leakage current through pull-ups when VDD_SW = 0V
    PORTA.DIRCLR = pins::SDA | pins::SCL;
    PORTA.PIN6CTRL = 0;  // No pull-up on SDA
    PORTA.PIN7CTRL = 0;  // No pull-up on SCL

    // Now safe to disable VDD_SW
    PORTA.OUTCLR = pins::PWR_EN;
}

void power_sleep() {
    // Ensure interrupts are enabled
    sei();

    // Enter sleep mode
    // CPU stops here until interrupt occurs
    sleep_cpu();

    // Woke up - execution continues here
}

uint8_t power_get_wake_source() {
    return wake_sources;
}

void power_clear_wake_source() {
    wake_sources = 0;
}

// ISR handlers set wake source flags
// These will be defined by rtc.cpp and button.cpp
// but we provide weak symbols here for linker

void __attribute__((weak)) RTC_PIT_vect_impl() {
    wake_sources |= WAKE_RTC;
}

void __attribute__((weak)) PORTA_PORT_vect_impl() {
    wake_sources |= WAKE_BUTTON;
}
