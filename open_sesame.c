#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

#define SAMPLE_FREQUENCY 16000
#define RECORDING_TIME_S 3
/* TIMER_US ~= 1 / SAMPLE_FREQUENCY */
#define TIMER_PERIOD_US 62
#define ADC0_PIN 26

/* Save samples in RAM */
volatile static uint16_t sample_tab[SAMPLE_FREQUENCY * RECORDING_TIME_S];

/* State variables */
bool led_state = true;
struct repeating_timer timer;
volatile uint16_t sample_index = 0;

void print_samples(uint16_t num_elements) {
	fflush(stdout);
    for (int i = 0; i < num_elements; i++) {
        /* convert 12 bits integer into float for test */
        printf("%f ", ((float)sample_tab[i] * 2.0f) / 4095.0f - 1.0f);
    }
    printf("\n");
}

/* Led */
void led_init(void) {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
}

void led_set(bool led_on) {
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
}

/* Timer */
bool timer_callback(struct repeating_timer *t) {
    adc_select_input(0);
    sample_tab[sample_index] = adc_read();
    sample_index++;

    /* when we stored one second of data */
    if (sample_index >= SAMPLE_FREQUENCY) {
        sample_index = 0;
        led_state = !led_state;
        led_set(led_state);

        /* print the 20 first values for test */
        print_samples(20);
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

    adc_init();
    adc_gpio_init(ADC0_PIN);

    led_init();

    ret = timer_init(TIMER_PERIOD_US);
    hard_assert(ret == true);

    while (true) {
    }

    return 0;
}
