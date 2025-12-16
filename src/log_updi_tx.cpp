/**
 * @file log_updi_tx.cpp
 * @brief Bit-banged software UART TX on PA2 (LED pin) for debug logging
 */

#include "log_updi_tx.h"

#if LOG_ENABLED

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay_basic.h>
#include "pins.hpp"

// -------------------------
// Bit-banged UART config
// -------------------------
#ifndef F_CPU
#error "F_CPU not defined"
#endif

// Timing calibration for bit-banged UART
// At 3.333 MHz, 9600 baud = 347.2 cycles/bit
// Account for ~10 cycles overhead → 337 cycles → 84 loops
#ifndef BIT_DELAY_LOOPS
#define BIT_DELAY_LOOPS 84
#endif

// -------------------------
// Low-level bit-banged UART
// -------------------------
static inline void tx_high() {
    VPORTA.OUT |= pins::LED;
}

static inline void tx_low() {
    VPORTA.OUT &= (uint8_t)~pins::LED;
}

static inline void delay_1bit() {
    _delay_loop_2(BIT_DELAY_LOOPS);
}

static inline void uart_tx_byte(uint8_t b) {
    uint8_t s = SREG;
    cli();

    // Start bit
    tx_low();
    delay_1bit();

    // Data bits (LSB first)
    for (uint8_t i = 0; i < 8; i++) {
        if (b & 0x01)
            tx_high();
        else
            tx_low();
        delay_1bit();
        b >>= 1;
    }

    // Stop bit
    tx_high();
    delay_1bit();

    SREG = s;
}

static inline void uart_tx_string(const char* str) {
    while (*str) {
        if (*str == '\n')
            uart_tx_byte('\r');
        uart_tx_byte((uint8_t)*str++);
    }
}

// -------------------------
// Small int formatting helpers (unchanged)
// -------------------------
static char* int_to_str(int16_t value, char* buffer)
{
    char* p = buffer;

    if (value < 0) {
        *p++ = '-';
        value = -value;
    }

    if (value == 0) {
        *p++ = '0';
        *p = '\0';
        return buffer;
    }

    char temp[6];
    uint8_t i = 0;
    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        *p++ = temp[--i];
    }
    *p = '\0';
    return buffer;
}

static char* uint_to_str(uint16_t value, char* buffer)
{
    char* p = buffer;

    if (value == 0) {
        *p++ = '0';
        *p = '\0';
        return buffer;
    }

    char temp[6];
    uint8_t i = 0;
    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        *p++ = temp[--i];
    }
    *p = '\0';
    return buffer;
}

// -------------------------
// Public API (same signatures)
// -------------------------
void log_init()
{
    // Set PA2 as output
    PORTA.DIRSET = pins::LED;

    // Idle HIGH (UART idle state)
    VPORTA.OUT |= pins::LED;
}

void log_hello()
{
    uart_tx_string("hello\n");
}

void log_sensor_data(int16_t c1_ff, int16_t c2_ff, int16_t c3_ff, int16_t c4_ff, uint16_t timestamp_sec)
{
    char temp[8];

    uart_tx_string("t=");
    uart_tx_string(uint_to_str(timestamp_sec, temp));
    uart_tx_byte(' ');

    uart_tx_string("c1=");
    uart_tx_string(int_to_str(c1_ff, temp));
    uart_tx_byte(' ');

    uart_tx_string("c2=");
    uart_tx_string(int_to_str(c2_ff, temp));
    uart_tx_byte(' ');

    uart_tx_string("c3=");
    uart_tx_string(int_to_str(c3_ff, temp));
    uart_tx_byte(' ');

    uart_tx_string("c4=");
    uart_tx_string(int_to_str(c4_ff, temp));
    uart_tx_byte('\n');
}

void log_drift_corrected(int16_t dc1c, int16_t dc2c, int16_t dc3c)
{
    char temp[8];

    // Format: "dC: dc1=-50 dc2=-100 dc3=-200" to match ESP32 bridge parser
    uart_tx_string("dC: dc1=");
    uart_tx_string(int_to_str(dc1c, temp));
    uart_tx_string(" dc2=");
    uart_tx_string(int_to_str(dc2c, temp));
    uart_tx_string(" dc3=");
    uart_tx_string(int_to_str(dc3c, temp));
    uart_tx_byte('\n');
}

void log_debug(const char* msg)
{
    uart_tx_string(msg);
    uart_tx_string("\n");
}

#endif  // LOG_ENABLED
