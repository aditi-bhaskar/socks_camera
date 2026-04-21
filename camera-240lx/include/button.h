#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include <stdbool.h>

// How long the raw reading must be stable before we accept it as a new state.
#define BUTTON_DEBOUNCE_US 5000

// Buttons are wired active-low: pin -> GND when pressed, pulled up to 3.3V
// at rest. button_was_pressed() returns true once per physical press.

typedef struct {
    uint32_t pin;
    bool     last_stable;       // true = pressed (last confirmed state)
    bool     last_raw;          // raw reading from last poll
    uint32_t last_change_usec;  // sys_timer timestamp of last raw change
} Button;

// Configure pin as input with pull-up. Call once per button at startup.
Button button_init(uint32_t pin_num);

// True if the button is currently pressed (debounced, non-latching).
bool button_is_pressed(Button *btn);

// True once per physical press — returns true on the first poll after a
// confirmed press edge, then false until the button is released and pressed
// again. Call this in your main loop.
bool button_was_pressed(Button *btn);

#endif // BUTTON_H
