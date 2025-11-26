/**
 * @file level_logic.h
 * @brief Water level determination with thresholds, hysteresis, and debouncing
 *
 * Computes ΔC = (CINx - CIN4) and compares against thresholds
 * Applies hysteresis and multi-sample debouncing to prevent chatter
 */

#pragma once

#include <stdint.h>
#include "fdc1004.h"

/**
 * Water level states
 */
enum class WaterLevel : uint8_t {
    NORMAL = 0,    // Above Low threshold
    LOW = 1,       // Below Low, above Very-Low
    VERY_LOW = 2,  // Below Very-Low, above Critical
    CRITICAL = 3,  // Below Critical threshold
    ERROR = 0xFF   // Sensor error (CIN4 invalid, I2C failure, etc.)
};

/**
 * Level thresholds structure (in femtofarads)
 */
struct LevelThresholds {
    int16_t low_ff;       // Low water threshold
    int16_t vlow_ff;      // Very-Low water threshold
    int16_t crit_ff;      // Critical water threshold
    uint8_t hysteresis_pct;  // Hysteresis percentage (0-100)
};

/**
 * Calibration baseline values (per-channel offsets in fF)
 */
struct CalibrationData {
    int16_t base_c1_ff;
    int16_t base_c2_ff;
    int16_t base_c3_ff;
    bool valid;
};

/**
 * @brief Initialize level logic module
 *
 * @param thresholds Threshold configuration
 * @param calibration Baseline calibration data
 */
void level_init(const LevelThresholds& thresholds, const CalibrationData& calibration);

/**
 * @brief Update thresholds
 *
 * @param thresholds New threshold configuration
 */
void level_set_thresholds(const LevelThresholds& thresholds);

/**
 * @brief Update calibration baseline
 *
 * @param calibration New calibration data
 */
void level_set_calibration(const CalibrationData& calibration);

/**
 * @brief Perform single level measurement and update state
 *
 * Measures all three channels (CIN1-3 vs CIN4), applies calibration,
 * compares against thresholds with hysteresis, and updates level state
 *
 * @return Current water level after update
 */
WaterLevel level_update();

/**
 * @brief Get current water level (without new measurement)
 *
 * @return Current level state
 */
WaterLevel level_get_current();

/**
 * @brief Get raw capacitance readings (for diagnostics/calibration)
 *
 * @param c1_ff Pointer to store CIN1-CIN4 reading (fF)
 * @param c2_ff Pointer to store CIN2-CIN4 reading (fF)
 * @param c3_ff Pointer to store CIN3-CIN4 reading (fF)
 * @return true if readings are valid
 */
bool level_get_raw_readings(int16_t* c1_ff, int16_t* c2_ff, int16_t* c3_ff);

/**
 * @brief Check if CIN4 reference is valid
 *
 * Validates that the "always-wet" reference electrode is functioning
 * Used for installation validation
 *
 * @param min_ref_ff Minimum expected reference capacitance (fF)
 * @return true if CIN4 is valid
 */
bool level_validate_cin4(int16_t min_ref_ff);
