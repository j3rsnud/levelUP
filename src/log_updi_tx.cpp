/**
 * @file log_updi_tx.cpp
 * @brief Hardware USART0 TX on PA2 (one-wire / shared RxD), TX-only (RX disabled)
 */

#include "log_updi_tx.h"

#if LOG_ENABLED

#include <avr/io.h>
#include "pins.hpp"

// -------------------------
// Config
// -------------------------
// F_CPU is defined by platformio.ini (8MHz: 16MHz osc / 2 prescaler)
#ifndef F_CPU
#define F_CPU 8000000UL
#endif
#define BAUD_RATE  9600UL
#define SAMPLES    16UL  // NORMAL mode (16x oversampling)

// BAUD_REG = (64 * F_CPU + (SAMPLES*BAUD_RATE)/2) / (SAMPLES * BAUD_RATE)
#define BAUD_REG   ((uint16_t)((64UL * F_CPU + (SAMPLES * BAUD_RATE) / 2) / (SAMPLES * BAUD_RATE)))

// -------------------------
// Low-level USART helpers
// -------------------------
static inline void usart0_init_onewire_tx_on_pa2()
{
    // Select alternate USART0 pins: PA2 is RxD (alternate). PORTMUX.CTRLB bit0 = 1 for alternate location.
    PORTMUX.CTRLB |= (1 << 0); // USART0 = 1 (alternate location - PA2 is RxD)

    // One-wire mode uses RxD pin for TX/RX
    // Step 1: Set RxD pin high (idle state)
    PORTA.OUTSET = pins::LED;

    // Step 2: Set RxD pin as output
    PORTA.DIRSET = pins::LED;

    // Disable RX/TX before reconfig (clean slate)
    USART0.CTRLB &= ~(USART_RXEN_bm | USART_TXEN_bm);

    // Set baud rate (USARTn.BAUD is 16-bit)
    USART0.BAUD = BAUD_REG;

    // 8N1 async: CMODE=async, PMODE=disabled, SBMODE=1 stop, CHSIZE=8-bit
    USART0.CTRLC = USART_CMODE_ASYNCHRONOUS_gc
                 | USART_PMODE_DISABLED_gc
                 | USART_SBMODE_1BIT_gc
                 | USART_CHSIZE_8BIT_gc;

    // One-wire mode: Enable internal TxD→RxD connection (datasheet 23.3.2.9)
    // This allows transmitter to drive the RxD pin (PA2)
    USART0.CTRLA |= USART_LBME_bm;

    // Enable TX (RX disabled for TX-only operation)
    USART0.CTRLB |= USART_TXEN_bm;

    // In one-wire operation, transmitter/receiver share RxD and TxD is internally connected to RxD.
    // Datasheet also notes that for one-wire, the RxD/TxD pin can be overridden to output when TX is enabled.
}

static inline void usart0_write(uint8_t b)
{
    // TXDATA can only be written when DREIF is set
    while ((USART0.STATUS & USART_DREIF_bm) == 0) { }
    USART0.TXDATAL = b;  // TXDATAL is the TX data low byte
}

static inline void uart_tx_string(const char* str)
{
    while (*str) {
        usart0_write((uint8_t)*str++);
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
    usart0_init_onewire_tx_on_pa2();
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
    usart0_write(' ');

    uart_tx_string("c1=");
    uart_tx_string(int_to_str(c1_ff, temp));
    usart0_write(' ');

    uart_tx_string("c2=");
    uart_tx_string(int_to_str(c2_ff, temp));
    usart0_write(' ');

    uart_tx_string("c3=");
    uart_tx_string(int_to_str(c3_ff, temp));
    usart0_write(' ');

    uart_tx_string("c4=");
    uart_tx_string(int_to_str(c4_ff, temp));
    usart0_write('\n');
}

void log_drift_corrected(int16_t dc1c, int16_t dc2c, int16_t dc3c)
{
    char temp[8];

    uart_tx_string(int_to_str(dc1c, temp));
    usart0_write(',');
    uart_tx_string(int_to_str(dc2c, temp));
    usart0_write(',');
    uart_tx_string(int_to_str(dc3c, temp));
    usart0_write('\n');
}

void log_debug(const char* msg)
{
    uart_tx_string(msg);
    uart_tx_string("\n");
}

#endif  // LOG_ENABLED
