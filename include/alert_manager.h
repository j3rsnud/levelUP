/**
 * @file alert_manager.h
 * @brief Manages 5-minute alert windows with escalating patterns
 *
 * Alert patterns:
 * - Low: 2 beeps every 10s for 5 minutes
 * - Very-Low: 3 beeps every 8s for 5 minutes
 * - Critical: 5 beeps every 5s for 5 minutes
 *
 * Behavior:
 * - Escalation: If level worsens during alert, restart 5-min timer at new level
 * - De-escalation: If level improves, stop alerts
 * - Button: Can silence current alert window
 */

#pragma once

#include <stdint.h>
#include "level_logic.h"
#include "buzzer.h"

/**
 * Alert configuration for each level
 */
struct AlertConfig {
    BeepPattern pattern;     // Number of beeps per burst
    uint16_t cadence_sec;    // Seconds between bursts
    uint16_t duration_sec;   // Total alert window duration (300s = 5min)
};

/**
 * @brief Initialize alert manager
 *
 * Sets up alert configurations and clears state
 */
void alert_init();

/**
 * @brief Update alert manager with new water level
 *
 * Called when water level changes. Handles alert window start/stop/escalation
 *
 * @param level Current water level
 */
void alert_on_level_change(WaterLevel level);

/**
 * @brief Update alert manager (call every 10s tick)
 *
 * Manages alert timing, triggers beep patterns, and handles window expiration
 *
 * @param tick Current tick counter (increments every 10 seconds)
 * @return true if alert is active (for keeping VDD_SW on during beeping)
 */
bool alert_update(uint32_t tick);

/**
 * @brief Silence current alert window
 *
 * Called when user presses button to acknowledge alert
 * Does not prevent future alerts if level worsens or after re-fill
 */
void alert_silence();

/**
 * @brief Check if alert is currently active
 *
 * @return true if within an alert window
 */
bool alert_is_active();

/**
 * @brief Get time remaining in current alert window
 *
 * @return Seconds remaining in current alert, 0 if no active alert
 */
uint16_t alert_get_remaining_sec();
