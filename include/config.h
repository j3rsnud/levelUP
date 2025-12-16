/**
 * @file config.h
 * @brief Configuration parameters for water level sensor
 *
 * All tunable parameters in one place for easy adjustment
 */

#pragma once

#include <stdint.h>

// ==================== MEASUREMENT TIMING ====================

// How often to wake and measure (in seconds)
// RTC wakes every 8 seconds
constexpr uint8_t MEASURE_INTERVAL_SEC = 8;

// ==================== LEVEL DETECTION THRESHOLDS ====================

// Threshold for detecting water drop (drift-corrected value)
// When dc > THRESHOLD, water has dropped past that sensor
// Found by testing: 100 works well for all sensors
constexpr int16_t THRESHOLD_LOW   = 100;  // C1 (LOW level)
constexpr int16_t THRESHOLD_VLOW  = 100;  // C2 (VERY_LOW level)
constexpr int16_t THRESHOLD_CRIT  = 100;  // C3 (CRITICAL level)

// Hysteresis for refill detection (threshold - hysteresis)
// Prevents oscillation at threshold boundary
constexpr int16_t REFILL_HYSTERESIS = 20;

// ==================== BUZZER CONFIGURATION ====================

// Buzzer frequency (Hz) - tuned to resonant peak
constexpr uint16_t BUZZER_FREQ_HZ = 4000;

// Buzzer duty cycle (0-100%)
constexpr uint8_t BUZZER_DUTY_PCT = 48;

// Beep timing (milliseconds)
constexpr uint16_t BEEP_DURATION_MS = 150;
constexpr uint16_t BEEP_GAP_MS = 150;

// ==================== CALIBRATION ====================

// Number of samples to average during calibration
constexpr uint8_t CALIBRATION_SAMPLES = 10;

// Delay between calibration samples (milliseconds)
constexpr uint16_t CALIBRATION_SAMPLE_DELAY_MS = 100;

// Calibration wait time after button press (seconds)
constexpr uint8_t CALIBRATION_WAIT_SEC = 10;

// ==================== LOGGING ====================

// Enable/disable UART logging (set to 0 to disable)
#define LOG_ENABLED 1

// UART baud rate
#define LOG_BAUD 9600

// ==================== POWER MANAGEMENT ====================

// Power stabilization delay after enabling peripherals (milliseconds)
constexpr uint8_t POWER_STABILIZATION_MS = 10;
