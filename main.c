#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
//#include "hardware/gpio.h"

#define PIO_USB_DP_PIN_DEFAULT 16
#include "pio_usb.h"
#include "tusb.h"


#define DEBUG_PRINTF(...) printf(__VA_ARGS__)

#define VBUS_EN_PIN 18
#define VBUS_EN_VAL 1

#define CONN_STATE_NO_CONN 0
#define CONN_STATE_USB_CONN 1

volatile uint8_t _connection_state = CONN_STATE_NO_CONN;

void core1_main()
{
    sleep_ms(10);

    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

    tuh_init(1);

    while (1)
    {
        tuh_task();
    }
}

void enable_5v() {
#ifdef VBUS_EN_PIN
    DEBUG_PRINTF("Enabling USB 5V\n");
    gpio_init(VBUS_EN_PIN);
    gpio_set_dir(VBUS_EN_PIN, GPIO_OUT);
    gpio_put(VBUS_EN_PIN, VBUS_EN_VAL);
#endif
}

int main()
{

    // Sysclk must be multiple of 12MHz for PIO USB
    set_sys_clock_khz(120000, true);

    stdio_init_all();
    DEBUG_PRINTF("pico-usb2sd: init\n");

    enable_5v();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    sleep_ms(10);

    multicore_reset_core1();
    multicore_launch_core1(core1_main);

    sleep_ms(10);

    while (1)
    {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(100);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(100);
    }
    return 0;
}

void tuh_msc_mount_cb(uint8_t dev_addr)
{
    DEBUG_PRINTF("A mass storage device is mounted, dev_addr=%d\n", dev_addr);
    uint32_t block_size = tuh_msc_get_block_size(dev_addr, 0);
    DEBUG_PRINTF("Block size: %ld\n", block_size);
    _connection_state = CONN_STATE_USB_CONN;
}

void tuh_msc_umount_cb(uint8_t dev_addr)
{
    DEBUG_PRINTF("A mass storage device is unmounted, dev_addr=%d\n", dev_addr);
    _connection_state = CONN_STATE_NO_CONN;
}