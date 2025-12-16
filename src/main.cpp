#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay_basic.h>
#include <stdint.h>

#ifndef F_CPU
#error "F_CPU not defined (PlatformIO board_build.f_cpu should define it)"
#endif

// Match your monitor_speed = 9600
#ifndef SOFTUART_BAUD
#define SOFTUART_BAUD 9600UL
#endif

// Timing calibration - adjust BIT_DELAY_LOOPS if you get gibberish
// Try: 84 (default), 80, 87, 90
#ifndef BIT_DELAY_LOOPS_OVERRIDE
#define BIT_DELAY_LOOPS_OVERRIDE 84
#endif

// ===== TX on PA2 =====
#define TX_PIN_bm PIN2_bm

static inline void tx_init()
{
    // Set PA2 as output
    PORTA.DIRSET = TX_PIN_bm;

    // Idle HIGH
    VPORTA.OUT |= TX_PIN_bm;
}

static inline void tx_high() { VPORTA.OUT |= TX_PIN_bm; }
static inline void tx_low() { VPORTA.OUT &= (uint8_t)~TX_PIN_bm; }

// ===== Bit delay =====
// _delay_loop_2(n) burns exactly 4 cycles per iteration.
// At 3.333 MHz, 9600 baud = 347.2 cycles/bit
// Account for ~10 cycles overhead (GPIO toggle + loop logic)
// Adjusted: 347 - 10 = 337 cycles → 337/4 = 84 loops
#define BIT_DELAY_LOOPS BIT_DELAY_LOOPS_OVERRIDE

static inline void delay_1bit()
{
    _delay_loop_2(BIT_DELAY_LOOPS);
}

static inline void uart_tx_byte(uint8_t b)
{
    uint8_t s = SREG;
    cli();

    // Start bit
    tx_low();
    delay_1bit();

    // Data bits (LSB first)
    for (uint8_t i = 0; i < 8; i++)
    {
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

static inline void uart_tx_str(const char *s)
{
    while (*s)
    {
        if (*s == '\n')
            uart_tx_byte('\r');
        uart_tx_byte((uint8_t)*s++);
    }
}

static inline void uart_tx_u32_dec(uint32_t v)
{
    char buf[11];
    uint8_t i = 0;

    if (v == 0)
    {
        uart_tx_byte('0');
        return;
    }

    while (v > 0 && i < sizeof(buf))
    {
        buf[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (i--)
        uart_tx_byte((uint8_t)buf[i]);
}

int main()
{
    tx_init();

    uart_tx_str("\nSoftUART TX on PA2 @ ");
    uart_tx_u32_dec((uint32_t)SOFTUART_BAUD);
    uart_tx_str(" baud, F_CPU=");
    uart_tx_u32_dec((uint32_t)F_CPU);
    uart_tx_str("\n");

    uint32_t cnt = 0;

    while (1)
    {
        uart_tx_str("cnt=");
        uart_tx_u32_dec(cnt++);
        uart_tx_str("\n");

        // crude pause between lines
        for (volatile uint32_t i = 0; i < 40000; i++)
        {
        }
    }
}
