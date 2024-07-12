#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
//#include "hardware/gpio.h"

#define PIO_USB_DP_PIN_DEFAULT 16
#include "pio_usb.h"
#include "tusb.h"

#include "libsdemu.h"


#define DEBUG_PRINTF(...) printf(__VA_ARGS__)

#define DEFAULT_SPI PICO_DEFAULT_SPI ? spi1 : spi0
#define PICO_DEFAULT_SPI_CSN_PIN 9

#define VBUS_EN_PIN 18
#define VBUS_EN_VAL 1

#define CONN_STATE_NO_CONN 0
#define CONN_STATE_USB_CONN 1

volatile uint8_t _connection_state = CONN_STATE_NO_CONN;
volatile uint8_t _dev_addr = 0;
volatile bool _disk_busy = false;

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

void init_sd() {
    DEBUG_PRINTF("Initializing SPI hardware\n");
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_CSN_PIN, GPIO_FUNC_SPI);

    setup_sd_emu(DEFAULT_SPI);
}

int main()
{
    uint8_t cmd_buf[SD_CMD_LEN];

    // Sysclk must be multiple of 12MHz for PIO USB
    set_sys_clock_khz(120000, true);

    stdio_init_all();
    DEBUG_PRINTF("pico-usb2sd: init\n");

    enable_5v();

    init_sd();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    sleep_ms(10);

    multicore_reset_core1();
    multicore_launch_core1(core1_main);

    sleep_ms(10);

    while (1)
    {
        if (_connection_state == CONN_STATE_USB_CONN) {
            wait_for_cmd(DEFAULT_SPI, cmd_buf);
            handle_cmd(DEFAULT_SPI, cmd_buf);
        }
    }
    return 0;
}

void tuh_msc_mount_cb(uint8_t dev_addr)
{
    DEBUG_PRINTF("A mass storage device is mounted, dev_addr=%d\n", dev_addr);
    uint32_t block_size = tuh_msc_get_block_size(dev_addr, 0);
    DEBUG_PRINTF("Block size: %ld\n", block_size);
    if (block_size != SD_SECTOR_SIZE) {
        DEBUG_PRINTF("Error: block size must be %d\n", SD_SECTOR_SIZE);
        return;
    }
    uint32_t block_num = tuh_msc_get_block_count(dev_addr, 0);
    DEBUG_PRINTF("Block count: %ld\n", block_num);
    _dev_addr = dev_addr;
    _connection_state = CONN_STATE_USB_CONN;
}

void tuh_msc_umount_cb(uint8_t dev_addr)
{
    DEBUG_PRINTF("A mass storage device is unmounted, dev_addr=%d\n", dev_addr);
    _connection_state = CONN_STATE_NO_CONN;
    _disk_busy = false;
    _dev_addr = 0;
}

static bool disk_io_complete(uint8_t dev_addr, tuh_msc_complete_data_t const *cb_data)
{
    (void)dev_addr;
    (void)cb_data;
    _disk_busy = false;
    return true;
}

void printbuf(uint8_t buf[], size_t len)
{
  char resp[128];
  uint32_t resp_count;
  uint32_t resp_idx = 0;
  for (size_t i = 0; i < len; ++i)
  {
    if (i % 16 == 15) {
      resp_count = sprintf(resp + resp_idx, "%02x\r\n", buf[i]);
      resp_idx += resp_count;
      DEBUG_PRINTF(resp);
      resp_idx = 0;
    }
    else
    {
      resp_count = sprintf(resp + resp_idx, "%02x ", buf[i]);
      resp_idx += resp_count;
    }
  }
  DEBUG_PRINTF("\n");
}

bool read_sd_block(uint32_t block_num, uint8_t *buff)
{
    _disk_busy = true;
    tuh_msc_read10(
        _dev_addr,
        0,
        buff,
        block_num,
        1,
        disk_io_complete,
        0
    );
    while(_disk_busy)
        tuh_task();
    //printbuf(_sd_block_data, SD_SECTOR_SIZE);
    return true;
}

bool write_sd_block(uint32_t block_num, uint8_t *buff)
{
    _disk_busy = true;
    tuh_msc_write10(
        _dev_addr,
        0,
        buff,
        block_num,
        1,
        disk_io_complete,
        0
    );
    while(_disk_busy)
        tuh_task();
    //printbuf(_sd_block_data, SD_SECTOR_SIZE);
    return true;
}