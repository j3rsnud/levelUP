/**
 * @file alert_manager.cpp
 * @brief Alert manager implementation
 */

#include "alert_manager.h"
#include "buzzer.h"

// Alert state
struct AlertState
{
    WaterLevel current_alert_level; // Level being alerted for
    bool active;                    // True if in alert window
    uint32_t alert_start_tick;      // Tick when alert started
    uint32_t last_beep_tick;        // Tick of last beep
    AlertConfig config;             // Current alert configuration
};

static AlertState state = {
    .current_alert_level = WaterLevel::NORMAL,
    .active = false,
    .alert_start_tick = 0,
    .last_beep_tick = 0,
    .config = {BeepPattern::NONE, 0, 0}};

// Alert configurations by level
static const AlertConfig ALERT_CONFIGS[] = {
    {BeepPattern::NONE, 0, 0},      // NORMAL - no alert
    {BeepPattern::DOUBLE, 30, 300}, // LOW - 2 beeps every 10s for 5min
    {BeepPattern::TRIPLE, 23, 300}, // VERY_LOW - 3 beeps every 8s for 5min
    {BeepPattern::FIVE, 15, 300},   // CRITICAL - 5 beeps every 5s for 5min
};

void alert_init()
{
    state.current_alert_level = WaterLevel::NORMAL;
    state.active = false;
    state.alert_start_tick = 0;
    state.last_beep_tick = 0;
    state.config = ALERT_CONFIGS[0];
}

void alert_on_level_change(WaterLevel level)
{
    // Ignore ERROR state
    if (level == WaterLevel::ERROR)
    {
        return;
    }

    // If level improved to NORMAL, stop any active alert
    if (level == WaterLevel::NORMAL)
    {
        if (state.active)
        {
            state.active = false;
            buzzer_stop();
        }
        return;
    }

    // If we're already alerting for this level or worse, no change needed
    // Exception: if level worsened (escalation), restart timer
    if (state.active)
    {
        if (level > state.current_alert_level)
        {
            // Escalation: worse level detected
            // Restart alert window at new level
            state.current_alert_level = level;
            state.config = ALERT_CONFIGS[static_cast<uint8_t>(level)];
            state.alert_start_tick = 0; // Will be set on next update
            state.last_beep_tick = 0;
        }
        else if (level < state.current_alert_level)
        {
            // De-escalation: better level (but not NORMAL)
            // Stop current alert, don't start new one
            state.active = false;
            buzzer_stop();
        }
        // If same level, continue current alert
        return;
    }

    // Not currently alerting, and level is not NORMAL
    // Start new alert window
    state.current_alert_level = level;
    state.config = ALERT_CONFIGS[static_cast<uint8_t>(level)];
    state.active = true;
    state.alert_start_tick = 0; // Will be set on next update
    state.last_beep_tick = 0;
}

bool alert_update(uint32_t tick)
{
    if (!state.active)
    {
        return false; // No alert active
    }

    // Initialize start tick on first update
    if (state.alert_start_tick == 0)
    {
        state.alert_start_tick = tick;
        state.last_beep_tick = tick - (state.config.cadence_sec / 10); // Force immediate beep
    }

    // Check if alert window expired
    uint32_t elapsed_ticks = tick - state.alert_start_tick;
    uint32_t elapsed_sec = elapsed_ticks * 10; // Each tick is 10 seconds

    if (elapsed_sec >= state.config.duration_sec)
    {
        // Alert window expired
        state.active = false;
        buzzer_stop();
        return false;
    }

    // Check if it's time for next beep
    uint32_t ticks_since_beep = tick - state.last_beep_tick;
    uint32_t sec_since_beep = ticks_since_beep * 10;

    if (sec_since_beep >= state.config.cadence_sec)
    {
        // Time to beep
        buzzer_start(state.config.pattern);
        state.last_beep_tick = tick;
    }

    // Return true if buzzer is active (to keep VDD_SW on)
    return buzzer_is_active();
}

void alert_silence()
{
    if (state.active)
    {
        state.active = false;
        buzzer_stop();
    }
}

bool alert_is_active()
{
    return state.active;
}

uint16_t alert_get_remaining_sec()
{
    if (!state.active || state.alert_start_tick == 0)
    {
        return 0;
    }

    // Calculate remaining time
    uint32_t elapsed_ticks = 0; // Would need current tick passed in
    // For now, return config duration (this is a simplified implementation)
    return state.config.duration_sec;
}
