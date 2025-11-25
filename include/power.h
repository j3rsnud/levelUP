/**
 * @file power.h
 * @brief Power management for ultra-low-power operation
 *
 * Manages:
 * - PWR_EN control (VDD_SW rail)
 * - Sleep modes
 * - TWI pin configuration (critical for leakage prevention)
 * - Wake source detection
 */

#pragma once

#include <stdint.h>

/**
 * Wake sources returned by power_get_wake_source()
 */
enum WakeSource : uint8_t {
    WAKE_NONE    = 0,
    WAKE_RTC     = (1 << 0),  // RTC PIT timer
    WAKE_BUTTON  = (1 << 1),  // Button press on PA0
};

/**
 * @brief Initialize power management
 *
 * - Configures PWR_EN pin (PA1) as output, initially LOW
 * - Sets up sleep mode (STANDBY for RTC operation)
 * - Configures watchdog if needed
 */
void power_init();

/**
 * @brief Enable switched power rail (VDD_SW)
 *
 * Sets PWR_EN HIGH, waits for VDD_SW to stabilize (~3ms)
 * Must be called before using FDC1004, DRV8210, or I2C
 */
void power_enable_peripherals();

/**
 * @brief Disable switched power rail (VDD_SW)
 *
 * CRITICAL: Must disable TWI peripheral and set PA6/PA7 to
 * high-impedance input (no pull-ups) BEFORE setting PWR_EN LOW
 * to prevent ~1mA leakage through I2C pull-ups
 */
void power_disable_peripherals();

/**
 * @brief Enter sleep mode
 *
 * Enters STANDBY sleep (RTC continues running)
 * Wakes on RTC PIT interrupt or PA0 pin change
 * Returns after wake interrupt occurs
 */
void power_sleep();

/**
 * @brief Get wake source(s)
 *
 * @return Bitmask of WakeSource flags
 */
uint8_t power_get_wake_source();

/**
 * @brief Clear wake source flags
 */
void power_clear_wake_source();
