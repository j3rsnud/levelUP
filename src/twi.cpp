/**
 * @file twi.cpp
 * @brief Software I2C (bit-bang) implementation for PA6/PA7
 *
 * ATtiny202 hardware TWI only works on PA1/PA2, but our hardware uses PA6/PA7.
 * This implements I2C in software by manually toggling the pins.
 */

#include "twi.h"
#include "pins.hpp"
#include <avr/io.h>
#include <util/delay.h>

// I2C timing for ~100 kHz (5us half-period = 10us full period = 100kHz)
#define I2C_DELAY_US 5

// Software I2C pin control
// To drive low: set as output (DIRSET), pin will output 0
// To release high: set as input (DIRCLR), external pull-up pulls high
static inline void sda_low()  { PORTA.DIRSET = pins::SDA; }
static inline void sda_high() { PORTA.DIRCLR = pins::SDA; }
static inline void scl_low()  { PORTA.DIRSET = pins::SCL; }
static inline void scl_high() { PORTA.DIRCLR = pins::SCL; }
static inline bool sda_read() { return (PORTA.IN & pins::SDA) != 0; }

// I2C START condition: SDA falls while SCL is high
static void i2c_start()
{
    sda_high();
    scl_high();
    _delay_us(I2C_DELAY_US);
    sda_low();
    _delay_us(I2C_DELAY_US);
    scl_low();
    _delay_us(I2C_DELAY_US);
}

// I2C STOP condition: SDA rises while SCL is high
static void i2c_stop()
{
    sda_low();
    scl_low();
    _delay_us(I2C_DELAY_US);
    scl_high();
    _delay_us(I2C_DELAY_US);
    sda_high();
    _delay_us(I2C_DELAY_US);
}

// Write one byte, return true if ACK received
static bool i2c_write_byte(uint8_t byte)
{
    // Write 8 data bits
    for (uint8_t i = 0; i < 8; i++)
    {
        if (byte & 0x80)
            sda_high();
        else
            sda_low();

        byte <<= 1;
        _delay_us(I2C_DELAY_US);
        scl_high();
        _delay_us(I2C_DELAY_US);
        scl_low();
    }

    // Read ACK bit
    sda_high();  // Release SDA
    _delay_us(I2C_DELAY_US);
    scl_high();
    _delay_us(I2C_DELAY_US);
    bool ack = !sda_read();  // ACK=0, NACK=1
    scl_low();
    _delay_us(I2C_DELAY_US);

    return ack;
}

// Read one byte, send ACK if send_ack=true
static uint8_t i2c_read_byte(bool send_ack)
{
    uint8_t byte = 0;

    sda_high();  // Release SDA for reading

    // Read 8 data bits
    for (uint8_t i = 0; i < 8; i++)
    {
        byte <<= 1;
        _delay_us(I2C_DELAY_US);
        scl_high();
        _delay_us(I2C_DELAY_US);

        if (sda_read())
            byte |= 1;

        scl_low();
    }

    // Send ACK or NACK
    if (send_ack)
        sda_low();   // ACK
    else
        sda_high();  // NACK

    _delay_us(I2C_DELAY_US);
    scl_high();
    _delay_us(I2C_DELAY_US);
    scl_low();
    _delay_us(I2C_DELAY_US);
    sda_high();  // Release

    return byte;
}

void twi_init()
{
    // Configure pins as inputs (external pull-ups will pull high)
    // Ensure output registers are 0 so when we set DIRSET, pin drives low
    PORTA.OUTCLR = pins::SDA | pins::SCL;
    PORTA.DIRCLR = pins::SDA | pins::SCL;

    // Give bus time to settle
    _delay_us(50);
}

void twi_disable()
{
    // Nothing to disable for software I2C
    PORTA.DIRCLR = pins::SDA | pins::SCL;
}

TwiStatus twi_write(uint8_t addr, const uint8_t *data, uint8_t len, uint16_t timeout_ms)
{
    (void)timeout_ms;  // Timeout not implemented for software I2C

    // START condition
    i2c_start();

    // Send address with write bit (0)
    if (!i2c_write_byte((addr << 1) | 0))
    {
        i2c_stop();
        return TwiStatus::NACK;
    }

    // Write data bytes
    for (uint8_t i = 0; i < len; i++)
    {
        if (!i2c_write_byte(data[i]))
        {
            i2c_stop();
            return TwiStatus::NACK;
        }
    }

    // STOP condition
    i2c_stop();

    return TwiStatus::OK;
}

TwiStatus twi_read(uint8_t addr, uint8_t *data, uint8_t len, uint16_t timeout_ms)
{
    (void)timeout_ms;  // Timeout not implemented for software I2C

    if (len == 0)
        return TwiStatus::OK;

    // START condition
    i2c_start();

    // Send address with read bit (1)
    if (!i2c_write_byte((addr << 1) | 1))
    {
        i2c_stop();
        return TwiStatus::NACK;
    }

    // Read data bytes
    for (uint8_t i = 0; i < len; i++)
    {
        bool is_last = (i == len - 1);
        data[i] = i2c_read_byte(!is_last);  // ACK all except last byte
    }

    // STOP condition
    i2c_stop();

    return TwiStatus::OK;
}

TwiStatus twi_write_reg(uint8_t addr, uint8_t reg, uint8_t value, uint16_t timeout_ms)
{
    uint8_t data[2] = {reg, value};
    return twi_write(addr, data, 2, timeout_ms);
}

TwiStatus twi_read_reg(uint8_t addr, uint8_t reg, uint8_t *value, uint16_t timeout_ms)
{
    TwiStatus status;

    // Write register address
    status = twi_write(addr, &reg, 1, timeout_ms);
    if (status != TwiStatus::OK)
        return status;

    // Read value
    return twi_read(addr, value, 1, timeout_ms);
}

TwiStatus twi_read_regs(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len, uint16_t timeout_ms)
{
    TwiStatus status;

    // Write starting register address
    status = twi_write(addr, &reg, 1, timeout_ms);
    if (status != TwiStatus::OK)
        return status;

    // Read multiple bytes
    return twi_read(addr, data, len, timeout_ms);
}
