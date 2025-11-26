/**
 * @file eeprom_config.h
 * @brief EEPROM configuration storage with CRC16 validation
 *
 * Stores:
 * - Thresholds (low, very-low, critical)
 * - Calibration baseline values
 * - Hysteresis settings
 * - Installation validation flag
 */

#pragma once

#include <stdint.h>

/**
 * Non-volatile configuration structure (stored in EEPROM)
 * Total size: ~24 bytes (well under 64-byte ATtiny202 EEPROM limit)
 */
struct __attribute__((packed)) NvmConfig {
    uint16_t version;           // Config version (0x0001)
    uint16_t th_low_ff;         // Low threshold (femtofarads)
    uint16_t th_vlow_ff;        // Very-Low threshold (fF)
    uint16_t th_crit_ff;        // Critical threshold (fF)
    uint16_t hysteresis_pct;    // Hysteresis percentage (0-100)
    int16_t  base_c1_ff;        // Baseline CIN1-CIN4 (fF)
    int16_t  base_c2_ff;        // Baseline CIN2-CIN4 (fF)
    int16_t  base_c3_ff;        // Baseline CIN3-CIN4 (fF)
    uint8_t  calibration_valid; // 1 if calibration performed, 0 otherwise
    uint8_t  reserved[5];       // Reserved for future use
    uint16_t crc16;             // CRC-16/XMODEM checksum
};

// Config version
constexpr uint16_t NVM_CONFIG_VERSION = 0x0001;

// Factory defaults
constexpr NvmConfig FACTORY_DEFAULTS = {
    .version = NVM_CONFIG_VERSION,
    .th_low_ff = 800,
    .th_vlow_ff = 500,
    .th_crit_ff = 300,
    .hysteresis_pct = 10,
    .base_c1_ff = 0,
    .base_c2_ff = 0,
    .base_c3_ff = 0,
    .calibration_valid = 0,
    .reserved = {0},
    .crc16 = 0  // Will be calculated
};

/**
 * @brief Initialize EEPROM config module
 *
 * Loads configuration from EEPROM and validates CRC
 * If invalid, loads factory defaults
 */
void eeprom_init();

/**
 * @brief Load configuration from EEPROM
 *
 * @param config Pointer to structure to fill
 * @return true if loaded and CRC valid, false otherwise
 */
bool eeprom_load(NvmConfig* config);

/**
 * @brief Save configuration to EEPROM
 *
 * Calculates CRC before writing
 *
 * @param config Pointer to configuration to save
 * @return true if saved successfully
 */
bool eeprom_save(const NvmConfig* config);

/**
 * @brief Factory reset - write defaults to EEPROM
 *
 * @return true if successful
 */
bool eeprom_factory_reset();

/**
 * @brief Get current configuration
 *
 * Returns cached copy (loaded at init or last save)
 *
 * @param config Pointer to structure to fill
 */
void eeprom_get_config(NvmConfig* config);

/**
 * @brief Update calibration values
 *
 * Convenience function to update only calibration data
 *
 * @param c1_ff CIN1-CIN4 baseline (fF)
 * @param c2_ff CIN2-CIN4 baseline (fF)
 * @param c3_ff CIN3-CIN4 baseline (fF)
 * @return true if saved successfully
 */
bool eeprom_update_calibration(int16_t c1_ff, int16_t c2_ff, int16_t c3_ff);
