/**
 * @file rtc.h
 * @brief RTC/PIT for 10-second periodic wake timer
 *
 * Uses the internal 32.768 kHz Ultra-Low-Power (ULP) oscillator
 * and Periodic Interrupt Timer (PIT) to wake from sleep every 10 seconds
 */

#pragma once

#include <stdint.h>

/**
 * @brief Initialize RTC and PIT for 10-second periodic interrupts
 *
 * Configures:
 * - 32.768 kHz internal ULP oscillator
 * - PIT for 10-second period
 * - Enables PIT interrupt
 */
void rtc_init();

/**
 * @brief Get elapsed ticks since boot
 *
 * @return Number of 10-second ticks elapsed
 */
uint32_t rtc_get_ticks();

/**
 * @brief Check if 8 seconds have elapsed and it's time to wake
 *
 * @return true if main loop should run, false if should sleep immediately
 */
bool rtc_should_wake();
