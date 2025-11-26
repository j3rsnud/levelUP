/**
 * @file button.cpp
 * @brief Button handler implementation
 */

#include "button.h"
#include "pins.hpp"
#include "power.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// Forward declaration for power module wake source notification
extern void PORTA_PORT_vect_impl();

// Button state
struct ButtonState {
    bool pressed;
    bool was_pressed;
    uint8_t press_duration;  // In ~100ms units (deciseconds)
    bool event_pending;
    ButtonEvent pending_event;
};

static ButtonState state = {false, false, 0, false, ButtonEvent::NONE};

// Timing constants
constexpr uint8_t SHORT_PRESS_THRESHOLD = 30;  // 3 seconds = 30 deciseconds
constexpr uint8_t BOOT_HOLD_THRESHOLD = 50;    // 5 seconds = 50 deciseconds

void button_init() {
    // Configure PA0 as input with pull-up (already done in power_init, but ensure)
    PORTA.DIRCLR = pins::BUTTON;
    PORTA.PIN0CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;  // Pull-up + both edges interrupt

    // Clear state
    state.pressed = false;
    state.was_pressed = false;
    state.press_duration = 0;
    state.event_pending = false;
    state.pending_event = ButtonEvent::NONE;
}

bool button_is_pressed() {
    // Button pressed = PA0 is LOW (pulled to GND through 220Ω)
    return !(PORTA.IN & pins::BUTTON);
}

ButtonEvent button_check() {
    bool currently_pressed = button_is_pressed();

    // Detect press/release edges
    if (currently_pressed && !state.was_pressed) {
        // Button just pressed
        state.pressed = true;
        state.press_duration = 0;
    } else if (!currently_pressed && state.was_pressed) {
        // Button just released
        state.pressed = false;

        // Determine event based on duration
        if (state.press_duration < SHORT_PRESS_THRESHOLD) {
            state.pending_event = ButtonEvent::SHORT_PRESS;
            state.event_pending = true;
        } else {
            state.pending_event = ButtonEvent::LONG_PRESS;
            state.event_pending = true;
        }

        state.press_duration = 0;
    } else if (currently_pressed) {
        // Button still pressed, increment duration
        // Note: This assumes button_check() is called at regular intervals
        // For 10s wakes, we won't get good resolution. Better to sample more frequently
        // during active periods or use a timer
        state.press_duration++;
    }

    state.was_pressed = currently_pressed;

    // Return pending event if any
    if (state.event_pending) {
        ButtonEvent event = state.pending_event;
        state.event_pending = false;
        state.pending_event = ButtonEvent::NONE;
        return event;
    }

    return ButtonEvent::NONE;
}

uint8_t button_get_press_duration() {
    return state.press_duration;
}

bool button_wait_pressed(uint16_t timeout_ms) {
    // Blocking wait for button press
    uint32_t timeout_count = timeout_ms;

    while (timeout_count > 0) {
        if (button_is_pressed()) {
            return true;
        }
        _delay_ms(1);
        timeout_count--;
    }

    return false;
}

// Pin change interrupt for PA0 (button wake)
ISR(PORTA_PORT_vect) {
    // Clear interrupt flag for PA0
    PORTA.INTFLAGS = pins::BUTTON;

    // Notify power module
    PORTA_PORT_vect_impl();
}
