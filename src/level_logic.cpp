/**
 * @file level_logic.cpp
 * @brief Water level determination implementation
 */

#include "level_logic.h"
#include "fdc1004.h"
#include "log_updi_tx.h"

// Module state
struct LevelState {
    LevelThresholds thresholds;
    CalibrationData calibration;
    WaterLevel current_level;
    uint8_t debounce_counter;
    WaterLevel pending_level;
    int16_t last_c1_ff;
    int16_t last_c2_ff;
    int16_t last_c3_ff;
    bool readings_valid;
};

static LevelState state = {
    .thresholds = {800, 500, 300, 10},  // Default thresholds
    .calibration = {0, 0, 0, false},
    .current_level = WaterLevel::NORMAL,
    .debounce_counter = 0,
    .pending_level = WaterLevel::NORMAL,
    .last_c1_ff = 0,
    .last_c2_ff = 0,
    .last_c3_ff = 0,
    .readings_valid = false
};

// Debounce configuration
constexpr uint8_t DEBOUNCE_SAMPLES = 3;  // Require 3 consistent readings before changing level

void level_init(const LevelThresholds& thresholds, const CalibrationData& calibration) {
    state.thresholds = thresholds;
    state.calibration = calibration;
    state.current_level = WaterLevel::NORMAL;
    state.debounce_counter = 0;
    state.pending_level = WaterLevel::NORMAL;
    state.readings_valid = false;
}

void level_set_thresholds(const LevelThresholds& thresholds) {
    state.thresholds = thresholds;
}

void level_set_calibration(const CalibrationData& calibration) {
    state.calibration = calibration;
}

/**
 * Determine water level from calibrated readings with hysteresis
 */
static WaterLevel determine_level(int16_t c1_ff, int16_t c2_ff, int16_t c3_ff) {
    // Calculate hysteresis offset
    int16_t hyst_low = (state.thresholds.low_ff * state.thresholds.hysteresis_pct) / 100;
    int16_t hyst_vlow = (state.thresholds.vlow_ff * state.thresholds.hysteresis_pct) / 100;
    int16_t hyst_crit = (state.thresholds.crit_ff * state.thresholds.hysteresis_pct) / 100;

    // Apply hysteresis based on current state
    // When transitioning DOWN (water decreasing), use lower threshold
    // When transitioning UP (water increasing), use higher threshold

    int16_t th_crit = state.thresholds.crit_ff;
    int16_t th_vlow = state.thresholds.vlow_ff;
    int16_t th_low = state.thresholds.low_ff;

    // Apply hysteresis band
    if (state.current_level <= WaterLevel::CRITICAL) {
        th_crit += hyst_crit;  // Require more water to exit Critical
    }
    if (state.current_level <= WaterLevel::VERY_LOW) {
        th_vlow += hyst_vlow;  // Require more water to exit Very-Low
    }
    if (state.current_level <= WaterLevel::LOW) {
        th_low += hyst_low;  // Require more water to exit Low
    }

    // Check thresholds in order (worst to best)
    // CIN3 = Critical level
    if (c3_ff < th_crit) {
        return WaterLevel::CRITICAL;
    }

    // CIN2 = Very-Low level
    if (c2_ff < th_vlow) {
        return WaterLevel::VERY_LOW;
    }

    // CIN1 = Low level
    if (c1_ff < th_low) {
        return WaterLevel::LOW;
    }

    return WaterLevel::NORMAL;
}

WaterLevel level_update() {
    // Measure all three channels
    FdcReading r1 = fdc_measure(FdcChannel::C1, 20);
    FdcReading r2 = fdc_measure(FdcChannel::C2, 20);
    FdcReading r3 = fdc_measure(FdcChannel::C3, 20);

    // Check if all readings are valid
    if (!r1.valid || !r2.valid || !r3.valid) {
        state.readings_valid = false;
        state.current_level = WaterLevel::ERROR;
        return WaterLevel::ERROR;
    }

    // Store raw readings
    state.last_c1_ff = r1.capacitance_ff;
    state.last_c2_ff = r2.capacitance_ff;
    state.last_c3_ff = r3.capacitance_ff;
    state.readings_valid = true;

    // Apply calibration offsets if valid
    int16_t c1_cal = r1.capacitance_ff;
    int16_t c2_cal = r2.capacitance_ff;
    int16_t c3_cal = r3.capacitance_ff;

    if (state.calibration.valid) {
        c1_cal -= state.calibration.base_c1_ff;
        c2_cal -= state.calibration.base_c2_ff;
        c3_cal -= state.calibration.base_c3_ff;
    }

    // Log drift-corrected values
    log_drift_corrected(c1_cal, c2_cal, c3_cal);

    // Determine new level with hysteresis
    WaterLevel new_level = determine_level(c1_cal, c2_cal, c3_cal);

    // Apply debouncing
    if (new_level != state.pending_level) {
        // New level detected, reset debounce counter
        state.pending_level = new_level;
        state.debounce_counter = 1;
    } else {
        // Same level as pending, increment counter
        state.debounce_counter++;

        if (state.debounce_counter >= DEBOUNCE_SAMPLES) {
            // Debounce complete, update current level
            state.current_level = new_level;
            state.debounce_counter = DEBOUNCE_SAMPLES;  // Clamp
        }
    }

    return state.current_level;
}

WaterLevel level_get_current() {
    return state.current_level;
}

bool level_get_raw_readings(int16_t* c1_ff, int16_t* c2_ff, int16_t* c3_ff) {
    if (c1_ff) *c1_ff = state.last_c1_ff;
    if (c2_ff) *c2_ff = state.last_c2_ff;
    if (c3_ff) *c3_ff = state.last_c3_ff;
    return state.readings_valid;
}

bool level_validate_cin4(int16_t min_ref_ff) {
    // Measure CIN4 directly (not differential)
    // We'll approximate by checking if any measurement is reasonable
    // A better approach would be to add a direct CIN4 measurement function to fdc1004

    // For now, use a heuristic: if all three differential readings are valid
    // and within reasonable range, CIN4 is probably OK
    if (!state.readings_valid) {
        return false;
    }

    // Check that readings are within expected differential range (±5 pF = ±5000 fF)
    if (state.last_c1_ff < -5000 || state.last_c1_ff > 5000) return false;
    if (state.last_c2_ff < -5000 || state.last_c2_ff > 5000) return false;
    if (state.last_c3_ff < -5000 || state.last_c3_ff > 5000) return false;

    // All readings in valid range
    return true;
}
