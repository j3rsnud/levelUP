/**
 * @file twi.h
 * @brief Minimal blocking I2C (TWI) master driver for ATtiny202
 *
 * 100 kHz operation, master mode only
 * Blocking implementation (suitable for low-power periodic operation)
 */

#pragma once

#include <stdint.h>

/**
 * TWI operation status codes
 */
enum class TwiStatus : uint8_t {
    OK         = 0,  // Operation successful
    NACK       = 1,  // Received NACK (device not responding or register invalid)
    TIMEOUT    = 2,  // Bus timeout (device hung or not present)
    BUS_ERROR  = 3,  // Bus error (arbitration lost, bus fault)
};

/**
 * @brief Initialize TWI peripheral for 100 kHz master mode
 *
 * Configures:
 * - Master mode
 * - 100 kHz SCL frequency (assuming 20 MHz F_CPU)
 * - PA6 (SDA), PA7 (SCL)
 *
 * NOTE: VDD_SW must be enabled before calling (for I2C pull-ups)
 */
void twi_init();

/**
 * @brief Disable TWI peripheral
 *
 * Must be called before disabling VDD_SW to prevent leakage
 */
void twi_disable();

/**
 * @brief Write data to I2C device
 *
 * @param addr 7-bit I2C address
 * @param data Pointer to data buffer
 * @param len Number of bytes to write
 * @param timeout_ms Timeout in milliseconds (0 = no timeout)
 * @return TwiStatus
 */
TwiStatus twi_write(uint8_t addr, const uint8_t* data, uint8_t len, uint16_t timeout_ms);

/**
 * @brief Read data from I2C device
 *
 * @param addr 7-bit I2C address
 * @param data Pointer to receive buffer
 * @param len Number of bytes to read
 * @param timeout_ms Timeout in milliseconds (0 = no timeout)
 * @return TwiStatus
 */
TwiStatus twi_read(uint8_t addr, uint8_t* data, uint8_t len, uint16_t timeout_ms);

/**
 * @brief Write single byte to register
 *
 * @param addr 7-bit I2C address
 * @param reg Register address
 * @param value Value to write
 * @param timeout_ms Timeout in milliseconds
 * @return TwiStatus
 */
TwiStatus twi_write_reg(uint8_t addr, uint8_t reg, uint8_t value, uint16_t timeout_ms);

/**
 * @brief Read single byte from register
 *
 * @param addr 7-bit I2C address
 * @param reg Register address
 * @param value Pointer to store read value
 * @param timeout_ms Timeout in milliseconds
 * @return TwiStatus
 */
TwiStatus twi_read_reg(uint8_t addr, uint8_t reg, uint8_t* value, uint16_t timeout_ms);

/**
 * @brief Read multiple bytes starting from register
 *
 * @param addr 7-bit I2C address
 * @param reg Starting register address
 * @param data Pointer to receive buffer
 * @param len Number of bytes to read
 * @param timeout_ms Timeout in milliseconds
 * @return TwiStatus
 */
TwiStatus twi_read_regs(uint8_t addr, uint8_t reg, uint8_t* data, uint8_t len, uint16_t timeout_ms);
