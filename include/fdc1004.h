/**
 * @file fdc1004.h
 * @brief Driver for TI FDC1004 capacitive sensor
 *
 * Differential capacitance measurement:
 * - MEAS1: CIN1 - CIN4 (Low level)
 * - MEAS2: CIN2 - CIN4 (Very-Low level)
 * - MEAS3: CIN3 - CIN4 (Critical level)
 * - CIN4 is the "always-wet" reference electrode
 */

#pragma once

#include <stdint.h>

// FDC1004 I2C address
constexpr uint8_t FDC1004_ADDR = 0x50;

/**
 * FDC1004 measurement channels (single-ended mode)
 */
enum class FdcChannel : uint8_t {
    C1 = 0,  // CIN1 single-ended
    C2 = 1,  // CIN2 single-ended
    C3 = 2,  // CIN3 single-ended
    C4 = 3,  // CIN4 single-ended (always-wet reference)
};

/**
 * Measurement result
 */
struct FdcReading {
    int16_t capacitance_ff;  // Capacitance in femtofarads (fF)
    bool valid;              // True if reading is valid
};

/**
 * @brief Initialize FDC1004
 *
 * - Verifies device ID
 * - Configures for differential mode
 * - Sets sample rate to 100 S/s (best SNR)
 *
 * @return true if initialization successful
 */
bool fdc_init();

/**
 * @brief Trigger single-shot measurement on specified channel
 *
 * Configures the measurement (CINx - CIN4) and starts conversion
 *
 * @param ch Channel to measure
 * @return true if trigger successful
 */
bool fdc_trigger_measurement(FdcChannel ch);

/**
 * @brief Wait for measurement to complete
 *
 * Polls DONE flag until measurement ready
 * Expected time: ~10 ms @ 100 S/s
 *
 * @param timeout_ms Maximum time to wait
 * @return true if measurement completed within timeout
 */
bool fdc_wait_ready(uint16_t timeout_ms);

/**
 * @brief Read measurement result
 *
 * Reads the result register and converts to femtofarads
 *
 * @param ch Channel to read
 * @return FdcReading structure with capacitance and validity
 */
FdcReading fdc_read_result(FdcChannel ch);

/**
 * @brief Perform complete measurement sequence
 *
 * Convenience function that triggers, waits, and reads
 *
 * @param ch Channel to measure
 * @param timeout_ms Timeout for wait
 * @return FdcReading structure
 */
FdcReading fdc_measure(FdcChannel ch, uint16_t timeout_ms = 20);

/**
 * @brief Software reset FDC1004
 *
 * Useful for recovery from I2C errors
 *
 * @return true if reset successful
 */
bool fdc_soft_reset();

/**
 * @brief Read device ID register
 *
 * Should return 0x1004 or 0x1005 for FDC1004
 *
 * @param device_id Pointer to store device ID
 * @return true if read successful
 */
bool fdc_read_device_id(uint16_t* device_id);
