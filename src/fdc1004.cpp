/**
 * @file fdc1004.cpp
 * @brief FDC1004 capacitive sensor driver implementation
 */

#include "fdc1004.h"
#include "twi.h"
#include <util/delay.h>

// FDC1004 Register Map
namespace FdcReg {
    constexpr uint8_t MEAS1_MSB    = 0x00;
    constexpr uint8_t MEAS1_LSB    = 0x01;
    constexpr uint8_t MEAS2_MSB    = 0x02;
    constexpr uint8_t MEAS2_LSB    = 0x03;
    constexpr uint8_t MEAS3_MSB    = 0x04;
    constexpr uint8_t MEAS3_LSB    = 0x05;
    constexpr uint8_t MEAS4_MSB    = 0x06;
    constexpr uint8_t MEAS4_LSB    = 0x07;
    constexpr uint8_t CONF_MEAS1   = 0x08;
    constexpr uint8_t CONF_MEAS2   = 0x09;
    constexpr uint8_t CONF_MEAS3   = 0x0A;
    constexpr uint8_t CONF_MEAS4   = 0x0B;
    constexpr uint8_t FDC_CONF     = 0x0C;
    constexpr uint8_t OFFSET_CAL_CIN1 = 0x0D;
    constexpr uint8_t OFFSET_CAL_CIN2 = 0x0E;
    constexpr uint8_t OFFSET_CAL_CIN3 = 0x0F;
    constexpr uint8_t OFFSET_CAL_CIN4 = 0x10;
    constexpr uint8_t GAIN_CAL_CIN1   = 0x11;
    constexpr uint8_t GAIN_CAL_CIN2   = 0x12;
    constexpr uint8_t GAIN_CAL_CIN3   = 0x13;
    constexpr uint8_t GAIN_CAL_CIN4   = 0x14;
    constexpr uint8_t MANUFACTURER_ID = 0xFE;
    constexpr uint8_t DEVICE_ID       = 0xFF;
}

// Configuration bits
namespace FdcConf {
    // CONF_MEASx register bits
    constexpr uint16_t CHA_OFFSET = 13;  // Positive input (bits 15:13)
    constexpr uint16_t CHB_OFFSET = 10;  // Negative input (bits 12:10)
    constexpr uint16_t CAPDAC_OFFSET = 5; // CAPDAC value (bits 9:5)
    constexpr uint16_t CAPDAC_EN = (1 << 4);  // CAPDAC enable

    // FDC_CONF register bits
    constexpr uint16_t RATE_100SPS = (0b01 << 10);  // 100 S/s (bits 11:10)
    constexpr uint16_t REPEAT = (1 << 8);           // Repeat measurements
    constexpr uint16_t MEAS1_EN = (1 << 7);         // Enable MEAS1
    constexpr uint16_t MEAS2_EN = (1 << 6);         // Enable MEAS2
    constexpr uint16_t MEAS3_EN = (1 << 5);         // Enable MEAS3
    constexpr uint16_t MEAS4_EN = (1 << 4);         // Enable MEAS4
    constexpr uint16_t MEAS1_DONE = (1 << 3);       // MEAS1 complete
    constexpr uint16_t MEAS2_DONE = (1 << 2);       // MEAS2 complete
    constexpr uint16_t MEAS3_DONE = (1 << 1);       // MEAS3 complete
    constexpr uint16_t MEAS4_DONE = (1 << 0);       // MEAS4 complete
    constexpr uint16_t RESET = (1 << 15);           // Software reset
}

// Helper: write 16-bit register
static bool write_reg16(uint8_t reg, uint16_t value) {
    uint8_t data[3] = {reg, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)};
    return twi_write(FDC1004_ADDR, data, 3, 20) == TwiStatus::OK;
}

// Helper: read 16-bit register
static bool read_reg16(uint8_t reg, uint16_t* value) {
    uint8_t data[2];
    if (twi_read_regs(FDC1004_ADDR, reg, data, 2, 20) != TwiStatus::OK) {
        return false;
    }
    *value = ((uint16_t)data[0] << 8) | data[1];
    return true;
}

bool fdc_init() {
    // Verify device ID
    uint16_t dev_id;
    if (!fdc_read_device_id(&dev_id)) {
        return false;
    }

    // FDC1004 should return 0x1004 or 0x1005
    if (dev_id != 0x1004 && dev_id != 0x1005) {
        return false;
    }

    // Initialize FDC_CONF register:
    // - 100 S/s sample rate (best SNR)
    // - Single-shot mode (REPEAT = 0)
    // - All measurements disabled initially
    uint16_t conf = FdcConf::RATE_100SPS;
    if (!write_reg16(FdcReg::FDC_CONF, conf)) {
        return false;
    }

    _delay_ms(1);  // Allow config to settle

    return true;
}

bool fdc_trigger_measurement(FdcChannel ch) {
    // Map channel to measurement register and CIN pins
    uint8_t meas_reg;
    uint16_t cin_pos, cin_neg;
    uint16_t meas_enable;

    switch (ch) {
        case FdcChannel::C1:
            meas_reg = FdcReg::CONF_MEAS1;
            cin_pos = 0;  // CIN1
            cin_neg = 7;  // Disabled (single-ended)
            meas_enable = FdcConf::MEAS1_EN;
            break;
        case FdcChannel::C2:
            meas_reg = FdcReg::CONF_MEAS2;
            cin_pos = 1;  // CIN2
            cin_neg = 7;  // Disabled (single-ended)
            meas_enable = FdcConf::MEAS2_EN;
            break;
        case FdcChannel::C3:
            meas_reg = FdcReg::CONF_MEAS3;
            cin_pos = 2;  // CIN3
            cin_neg = 7;  // Disabled (single-ended)
            meas_enable = FdcConf::MEAS3_EN;
            break;
        default:
            return false;
    }

    // Configure measurement: CINx single-ended (CHB disabled), CAPDAC disabled
    uint16_t meas_conf = (cin_pos << FdcConf::CHA_OFFSET) |
                         (cin_neg << FdcConf::CHB_OFFSET);
    if (!write_reg16(meas_reg, meas_conf)) {
        return false;
    }

    // Enable measurement in FDC_CONF
    uint16_t fdc_conf = FdcConf::RATE_100SPS | meas_enable;
    if (!write_reg16(FdcReg::FDC_CONF, fdc_conf)) {
        return false;
    }

    return true;
}

bool fdc_wait_ready(uint16_t timeout_ms) {
    uint32_t timeout_us = (uint32_t)timeout_ms * 1000;

    while (timeout_us > 0) {
        uint16_t fdc_conf;
        if (!read_reg16(FdcReg::FDC_CONF, &fdc_conf)) {
            return false;
        }

        // Check if any DONE bit is set
        if (fdc_conf & (FdcConf::MEAS1_DONE | FdcConf::MEAS2_DONE |
                        FdcConf::MEAS3_DONE | FdcConf::MEAS4_DONE)) {
            return true;
        }

        _delay_us(100);
        timeout_us = (timeout_us > 100) ? (timeout_us - 100) : 0;
    }

    return false;  // Timeout
}

FdcReading fdc_read_result(FdcChannel ch) {
    FdcReading result = {0, false};

    // Map channel to result registers
    uint8_t msb_reg, lsb_reg;
    switch (ch) {
        case FdcChannel::C1:
            msb_reg = FdcReg::MEAS1_MSB;
            lsb_reg = FdcReg::MEAS1_LSB;
            break;
        case FdcChannel::C2:
            msb_reg = FdcReg::MEAS2_MSB;
            lsb_reg = FdcReg::MEAS2_LSB;
            break;
        case FdcChannel::C3:
            msb_reg = FdcReg::MEAS3_MSB;
            lsb_reg = FdcReg::MEAS3_LSB;
            break;
        default:
            return result;
    }

    // Read 24-bit result (MSB + LSB)
    uint16_t msb, lsb;
    if (!read_reg16(msb_reg, &msb) || !read_reg16(lsb_reg, &lsb)) {
        return result;
    }

    // Combine to 24-bit signed value
    int32_t raw = ((int32_t)msb << 8) | (lsb >> 8);

    // Sign extend from 24-bit to 32-bit
    if (raw & 0x00800000) {
        raw |= 0xFF000000;
    }

    // Convert to femtofarads
    // FDC1004 range: ±15 pF over 24-bit signed range
    // Resolution: 15 pF / 2^23 = ~1.79 fF per LSB
    // Conversion: capacitance_fF = raw * 15000000 / 2^23
    // Simplified: capacitance_fF = (raw * 15000000) >> 23
    result.capacitance_ff = (int16_t)((raw * 15000LL) >> 23);  // Use LL for 64-bit intermediate
    result.valid = true;

    return result;
}

FdcReading fdc_measure(FdcChannel ch, uint16_t timeout_ms) {
    FdcReading result = {0, false};

    // Trigger measurement
    if (!fdc_trigger_measurement(ch)) {
        return result;
    }

    // Wait for completion
    if (!fdc_wait_ready(timeout_ms)) {
        return result;
    }

    // Read result
    return fdc_read_result(ch);
}

bool fdc_soft_reset() {
    // Write RESET bit to FDC_CONF
    if (!write_reg16(FdcReg::FDC_CONF, FdcConf::RESET)) {
        return false;
    }

    _delay_ms(10);  // Wait for reset to complete

    // Reinitialize
    return fdc_init();
}

bool fdc_read_device_id(uint16_t* device_id) {
    return read_reg16(FdcReg::DEVICE_ID, device_id);
}
