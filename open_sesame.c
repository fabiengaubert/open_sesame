#include <stdio.h>
#include "pico/stdlib.h"

#define SAMPLE_FREQUENCY 16000
/* TIMER_US ~= 1 / SAMPLE_FREQUENCY */
#define TIMER_US 62

bool led_state = true;
struct repeating_timer timer;
volatile uint16_t timer_count = 0;

void led_init(void) {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
}

void led_set(bool led_on) {
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
}

bool timer_callback(struct repeating_timer *t) {
    timer_count++;

    if (timer_count >= SAMPLE_FREQUENCY) {
        timer_count = 0;
        led_state = !led_state;
        led_set(led_state);
    }

    return true;
}

bool timer_init(int64_t delay) {
    return add_repeating_timer_us(delay, timer_callback, NULL, &timer);
}

int main()
{
    bool ret = stdio_init_all();
    hard_assert(ret == true);

    led_init();

    ret = timer_init(TIMER_US);
    hard_assert(ret == true);

    while (true) {
    }

    return 0;
}
