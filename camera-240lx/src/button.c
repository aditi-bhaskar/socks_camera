#include "button.h"
#include "gpio.h"
#include "sys_timer.h"

// we do some debouncing stuff lol

Button button_init(uint32_t pin_num) {
    Button btn;
    btn.pin = pin_num;
    Pin p = { .p_num = pin_num };
    gpio_select_input(p);
    gpio_set_pull(p, GPIO_PULL_UP);

    bool initially_pressed = (gpio_read(p) == LOW);
    btn.last_stable = initially_pressed;
    btn.last_raw = initially_pressed;
    btn.last_change_usec = sys_timer_get_usec();
    return btn;
}

static bool button_update(Button *btn) {
    Pin p = { .p_num = btn->pin };
    bool raw_pressed = (gpio_read(p) == LOW);

    if (raw_pressed != btn->last_raw) {
        btn->last_raw = raw_pressed;
        btn->last_change_usec = sys_timer_get_usec();
    } else {
        uint32_t elapsed = sys_timer_get_usec() - btn->last_change_usec;
        if (elapsed >= BUTTON_DEBOUNCE_US) {
            btn->last_stable = raw_pressed;
        }
    }

    return btn->last_stable;
}

bool button_is_pressed(Button *btn) {
    return button_update(btn);
}

bool button_was_pressed(Button *btn) {
    bool prev = btn->last_stable;
    bool now = button_update(btn);
    return !prev && now;
}
