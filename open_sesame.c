#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"

#include "ws_client.h"

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
        /* Convert 12 bits int to float, compensates for the 1.25v offset of the microphone */
        printf("%f ", (float)sample_tab[i] * 2.0f * 3.3f / (2.5f * 4095.0f) - 1.25f);
    }
    printf("\n");
}

/* WiFi */
bool wifi_init(void) {
    /* Initialise the WiFi chip */
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return false;
    }
    /* Enable station mode */
    cyw43_arch_enable_sta_mode();
    return true;
}

bool wifi_connect(void) {
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Failed to connect.\n");
        return false;
    }
    printf("WiFi is connected.\n");
    uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
    printf("Raspberry IP address is %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    return true;
}

/* Led - on Pico W initialisation is made by cyw43_arch_init */
void led_set(bool led_on) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
}

/* Timer */
bool timer_callback(struct repeating_timer *t) {
    adc_select_input(0);
    sample_tab[sample_index] = adc_read();
    sample_index++;

    /* We stored one second of data */
    if (sample_index >= SAMPLE_FREQUENCY) {
        sample_index = 0;
        led_state = !led_state;
        led_set(led_state);

        /* Print the 20 first values for test */
        //print_samples(20);
    }

    return true;
}

bool timer_init(int64_t delay) {
    return add_repeating_timer_us(delay, timer_callback, NULL, &timer);
}

bool ws_client_start(const char *server_ip, const uint16_t server_port) {
    struct ws_client* client = ws_client_init(server_ip, server_port);
    if (!client) {
        return false;
    }

    if (!ws_client_open(client)) {
        printf("Error, could not open the TCP connection.\n");
        ws_client_close(client);
        return false;
    }
}

int main()
{
    bool ret = stdio_init_all();
    hard_assert(ret == true);

    wifi_init();
    ret = wifi_connect();
    hard_assert(ret == true);

    adc_init();
    adc_gpio_init(ADC0_PIN);

    ret = timer_init(TIMER_PERIOD_US);
    hard_assert(ret == true);

    sleep_ms(1000);
    printf("Start client.\n");
    ws_client_start(SERVER_IP, 80);

    while (true) {
    }

    /* Always call from the same core than cyw43_arch_init() */
    cyw43_arch_deinit();
    return 0;
}
