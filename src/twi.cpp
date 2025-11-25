/**
 * @file twi.cpp
 * @brief TWI (I2C) master driver implementation
 */

#include "twi.h"
#include "pins.hpp"
#include <avr/io.h>
#include <util/delay.h>

// Timeout helpers (using microsecond delays for accuracy)
#define TIMEOUT_START() uint32_t timeout_us = (uint32_t)timeout_ms * 1000
#define TIMEOUT_CHECK() if (timeout_ms && (--timeout_us == 0)) return TwiStatus::TIMEOUT

void twi_init() {
    // Configure I2C pins (already done by power module, but ensure correct state)
    PORTA.DIRCLR = pins::SDA | pins::SCL;  // Inputs (external pull-ups)

    // Calculate baud rate for 100 kHz
    // BAUD = (F_CLK / (2 * f_SCL)) - 5
    // F_CLK = 20 MHz, f_SCL = 100 kHz
    // BAUD = (20000000 / 200000) - 5 = 100 - 5 = 95
    TWI0.MBAUD = 95;

    // Enable TWI master mode
    TWI0.MCTRLA = TWI_ENABLE_bm;

    // Force bus state to IDLE
    TWI0.MSTATUS = TWI_BUSSTATE_IDLE_gc;

    // Small delay for bus to stabilize
    _delay_us(10);
}

void twi_disable() {
    // Disable TWI peripheral
    TWI0.MCTRLA = 0;
}

static TwiStatus twi_wait_bus_ready(uint16_t timeout_ms) {
    TIMEOUT_START();

    // Wait for bus to be idle or owner
    while (!(TWI0.MSTATUS & (TWI_BUSSTATE_IDLE_gc | TWI_BUSSTATE_OWNER_gc))) {
        if (TWI0.MSTATUS & (TWI_ARBLOST_bm | TWI_BUSERR_bm)) {
            return TwiStatus::BUS_ERROR;
        }
        TIMEOUT_CHECK();
        _delay_us(1);
    }

    return TwiStatus::OK;
}

static TwiStatus twi_wait_wif(uint16_t timeout_ms) {
    TIMEOUT_START();

    // Wait for write interrupt flag
    while (!(TWI0.MSTATUS & TWI_WIF_bm)) {
        if (TWI0.MSTATUS & (TWI_ARBLOST_bm | TWI_BUSERR_bm)) {
            return TwiStatus::BUS_ERROR;
        }
        TIMEOUT_CHECK();
        _delay_us(1);
    }

    // Check for NACK
    if (TWI0.MSTATUS & TWI_RXACK_bm) {
        return TwiStatus::NACK;
    }

    return TwiStatus::OK;
}

static TwiStatus twi_wait_rif(uint16_t timeout_ms) {
    TIMEOUT_START();

    // Wait for read interrupt flag
    while (!(TWI0.MSTATUS & TWI_RIF_bm)) {
        if (TWI0.MSTATUS & (TWI_ARBLOST_bm | TWI_BUSERR_bm)) {
            return TwiStatus::BUS_ERROR;
        }
        TIMEOUT_CHECK();
        _delay_us(1);
    }

    return TwiStatus::OK;
}

TwiStatus twi_write(uint8_t addr, const uint8_t* data, uint8_t len, uint16_t timeout_ms) {
    TwiStatus status;

    // Wait for bus ready
    status = twi_wait_bus_ready(timeout_ms);
    if (status != TwiStatus::OK) return status;

    // Send START + address + write bit
    TWI0.MADDR = (addr << 1) | 0;  // Write = 0

    // Wait for WIF and check ACK
    status = twi_wait_wif(timeout_ms);
    if (status != TwiStatus::OK) {
        TWI0.MCTRLB = TWI_MCMD_STOP_gc;  // Send STOP on error
        return status;
    }

    // Write data bytes
    for (uint8_t i = 0; i < len; i++) {
        TWI0.MDATA = data[i];

        status = twi_wait_wif(timeout_ms);
        if (status != TwiStatus::OK) {
            TWI0.MCTRLB = TWI_MCMD_STOP_gc;
            return status;
        }
    }

    // Send STOP
    TWI0.MCTRLB = TWI_MCMD_STOP_gc;

    return TwiStatus::OK;
}

TwiStatus twi_read(uint8_t addr, uint8_t* data, uint8_t len, uint16_t timeout_ms) {
    TwiStatus status;

    if (len == 0) return TwiStatus::OK;

    // Wait for bus ready
    status = twi_wait_bus_ready(timeout_ms);
    if (status != TwiStatus::OK) return status;

    // Send START + address + read bit
    TWI0.MADDR = (addr << 1) | 1;  // Read = 1

    // Wait for RIF
    status = twi_wait_rif(timeout_ms);
    if (status != TwiStatus::OK) {
        TWI0.MCTRLB = TWI_MCMD_STOP_gc;
        return status;
    }

    // Read data bytes
    for (uint8_t i = 0; i < len; i++) {
        data[i] = TWI0.MDATA;

        if (i == len - 1) {
            // Last byte: send NACK + STOP
            TWI0.MCTRLB = TWI_ACKACT_bm | TWI_MCMD_STOP_gc;
        } else {
            // More bytes: send ACK
            TWI0.MCTRLB = TWI_MCMD_RECVTRANS_gc;

            status = twi_wait_rif(timeout_ms);
            if (status != TwiStatus::OK) {
                TWI0.MCTRLB = TWI_MCMD_STOP_gc;
                return status;
            }
        }
    }

    return TwiStatus::OK;
}

TwiStatus twi_write_reg(uint8_t addr, uint8_t reg, uint8_t value, uint16_t timeout_ms) {
    uint8_t data[2] = {reg, value};
    return twi_write(addr, data, 2, timeout_ms);
}

TwiStatus twi_read_reg(uint8_t addr, uint8_t reg, uint8_t* value, uint16_t timeout_ms) {
    TwiStatus status;

    // Write register address
    status = twi_write(addr, &reg, 1, timeout_ms);
    if (status != TwiStatus::OK) return status;

    // Read value
    return twi_read(addr, value, 1, timeout_ms);
}

TwiStatus twi_read_regs(uint8_t addr, uint8_t reg, uint8_t* data, uint8_t len, uint16_t timeout_ms) {
    TwiStatus status;

    // Write starting register address
    status = twi_write(addr, &reg, 1, timeout_ms);
    if (status != TwiStatus::OK) return status;

    // Read multiple bytes
    return twi_read(addr, data, len, timeout_ms);
}
