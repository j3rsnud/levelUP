/**
 * @file log_updi_tx.h
 * @brief Hardware USART0 one-wire TX on PA2 (LED pin) for debug logging
 *
 * Allows logging FDC1004 sensor data over the PA2 pin using hardware USART0.
 * TX-only mode (RX disabled) using one-wire configuration.
 * Use with a separate bridge board (ESP Feather V2).
 *
 * Hardware setup:
 * - PA2 (LED pin / USART0 TX) → ESP UART RX
 * - VDD → ESP 3V3
 * - GND → ESP GND
 *
 * Configuration:
 * - Baud rate: 9600 (16x oversampling)
 * - Frame: 8N1 (8 data bits, no parity, 1 stop bit)
 * - One-wire mode: TxD/RxD share PA2 (default PORTMUX location)
 *
 * Note: LED must be disconnected to use PA2 for UART TX
 */

#pragma once

#include <stdint.h>

// Logging configuration
#ifndef LOG_BAUD
#define LOG_BAUD 9600  // Safe with pull-up resistor, can try higher if stable
#endif

// Compile-time enable/disable
#ifndef LOG_ENABLED
#define LOG_ENABLED 1  // Set to 0 to completely disable logging at compile time
#endif

#if LOG_ENABLED

/**
 * @brief Initialize UART TX on PA2 (LED pin)
 *
 * Configures PA2 as output and sets idle state HIGH.
 * Call this once during system initialization.
 */
void log_init();

/**
 * @brief Send a test "hello" message
 *
 * Useful for verifying UART communication works.
 * Sends: "hello\n"
 */
void log_hello();

/**
 * @brief Log raw sensor readings
 *
 * @param c1_ff CIN1 reading (femtofarads)
 * @param c2_ff CIN2 reading (femtofarads)
 * @param c3_ff CIN3 reading (femtofarads)
 * @param c4_ff CIN4 reading (femtofarads)
 * @param timestamp_sec Measurement timestamp (seconds since boot)
 *
 * Output format: "t=123 c1=456 c2=789 c3=321 c4=654\n"
 */
void log_sensor_data(int16_t c1_ff, int16_t c2_ff, int16_t c3_ff, int16_t c4_ff, uint16_t timestamp_sec);

/**
 * @brief Log drift-corrected deltas for threshold tuning
 *
 * @param dc1c CIN1 drift-corrected delta (fF)
 * @param dc2c CIN2 drift-corrected delta (fF)
 * @param dc3c CIN3 drift-corrected delta (fF)
 *
 * Output format: "-50,-100,-200\n"
 */
void log_drift_corrected(int16_t dc1c, int16_t dc2c, int16_t dc3c);

/**
 * @brief Log a simple debug message
 *
 * @param msg Null-terminated string to send
 */
void log_debug(const char* msg);

#else
// Logging disabled - provide empty stubs
inline void log_init() {}
inline void log_hello() {}
inline void log_sensor_data(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
inline void log_drift_corrected(int16_t, int16_t, int16_t) {}
inline void log_debug(const char*) {}
#endif
