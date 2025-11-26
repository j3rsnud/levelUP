/**
 * @file eeprom_config.cpp
 * @brief EEPROM configuration implementation
 */

#include "eeprom_config.h"
#include <avr/eeprom.h>
#include <string.h>

// EEPROM storage location (start of EEPROM)
static NvmConfig EEMEM eeprom_storage;

// Cached configuration
static NvmConfig cached_config;

/**
 * Calculate CRC-16/XMODEM
 * Polynomial: 0x1021 (x^16 + x^12 + x^5 + 1)
 * Initial value: 0x0000
 */
static uint16_t calculate_crc16(const uint8_t* data, uint16_t length) {
    uint16_t crc = 0x0000;

    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }

    return crc;
}

/**
 * Validate and fix CRC for config structure
 */
static bool validate_crc(const NvmConfig* config) {
    // Calculate CRC over entire structure except CRC field itself
    uint16_t calc_crc = calculate_crc16(
        (const uint8_t*)config,
        sizeof(NvmConfig) - sizeof(uint16_t)  // Exclude crc16 field
    );

    return calc_crc == config->crc16;
}

/**
 * Update CRC in config structure
 */
static void update_crc(NvmConfig* config) {
    config->crc16 = calculate_crc16(
        (const uint8_t*)config,
        sizeof(NvmConfig) - sizeof(uint16_t)
    );
}

void eeprom_init() {
    // Load from EEPROM
    if (!eeprom_load(&cached_config)) {
        // Load failed or CRC invalid, use factory defaults
        memcpy(&cached_config, &FACTORY_DEFAULTS, sizeof(NvmConfig));
        update_crc(&cached_config);
    }
}

bool eeprom_load(NvmConfig* config) {
    if (!config) return false;

    // Read from EEPROM
    eeprom_read_block(config, &eeprom_storage, sizeof(NvmConfig));

    // Validate version
    if (config->version != NVM_CONFIG_VERSION) {
        return false;
    }

    // Validate CRC
    if (!validate_crc(config)) {
        return false;
    }

    return true;
}

bool eeprom_save(const NvmConfig* config) {
    if (!config) return false;

    // Create a copy and update CRC
    NvmConfig config_copy;
    memcpy(&config_copy, config, sizeof(NvmConfig));
    config_copy.version = NVM_CONFIG_VERSION;
    update_crc(&config_copy);

    // Write to EEPROM
    eeprom_update_block(&config_copy, &eeprom_storage, sizeof(NvmConfig));

    // Update cached copy
    memcpy(&cached_config, &config_copy, sizeof(NvmConfig));

    return true;
}

bool eeprom_factory_reset() {
    NvmConfig defaults;
    memcpy(&defaults, &FACTORY_DEFAULTS, sizeof(NvmConfig));
    return eeprom_save(&defaults);
}

void eeprom_get_config(NvmConfig* config) {
    if (config) {
        memcpy(config, &cached_config, sizeof(NvmConfig));
    }
}

bool eeprom_update_calibration(int16_t c1_ff, int16_t c2_ff, int16_t c3_ff) {
    // Validate calibration values (must be within ±5 pF range)
    if (c1_ff < -5000 || c1_ff > 5000) return false;
    if (c2_ff < -5000 || c2_ff > 5000) return false;
    if (c3_ff < -5000 || c3_ff > 5000) return false;

    // Minimum value check (prevent empty-tank calibration)
    // Each reading must be > 200 fF
    if (c1_ff < 200 || c2_ff < 200 || c3_ff < 200) return false;

    // Update cached config
    cached_config.base_c1_ff = c1_ff;
    cached_config.base_c2_ff = c2_ff;
    cached_config.base_c3_ff = c3_ff;
    cached_config.calibration_valid = 1;

    // Save to EEPROM
    return eeprom_save(&cached_config);
}
