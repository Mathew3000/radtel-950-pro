/*
 * SPI NOR flash on SPI2: SCK PB13, MISO PB14, MOSI PB15, chip select PB12.
 *
 * From the stock firmware: the read routine (0x08023d80) drops PB12, sends
 * opcode 0x03 and a 24-bit address, then clocks bytes through SPI2
 * (0x40003800). The SPI2 setup (0x08017070) is master, 8 bit, MSB first,
 * clock idle high, data on the second edge (mode 3), software chip select.
 * docs/spi_flash.md had this on PA5-PA7; that is not what the code does.
 */
#include "spi_flash.h"
#include "at32f403a_407.h"

#define CS_PORT GPIOB
#define CS_PIN  GPIO_PINS_12

static uint8_t xfer(uint8_t out)
{
    while (!(SPI2->sts & SPI_I2S_TDBE_FLAG)) {
    }
    SPI2->dt = out;
    while (!(SPI2->sts & SPI_I2S_RDBF_FLAG)) {
    }
    return (uint8_t)SPI2->dt;
}

void spi_flash_init(void)
{
    gpio_init_type cfg;
    spi_init_type spi;

    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_SPI2_PERIPH_CLOCK, TRUE);

    CS_PORT->scr = CS_PIN;
    gpio_default_para_init(&cfg);
    cfg.gpio_pins = CS_PIN;
    cfg.gpio_mode = GPIO_MODE_OUTPUT;
    cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(CS_PORT, &cfg);

    cfg.gpio_pins = GPIO_PINS_13 | GPIO_PINS_15;
    cfg.gpio_mode = GPIO_MODE_MUX;
    gpio_init(GPIOB, &cfg);

    gpio_default_para_init(&cfg);
    cfg.gpio_pins = GPIO_PINS_14;
    cfg.gpio_mode = GPIO_MODE_INPUT;
    cfg.gpio_pull = GPIO_PULL_UP;
    gpio_init(GPIOB, &cfg);

    spi_default_para_init(&spi);
    spi.transmission_mode = SPI_TRANSMIT_FULL_DUPLEX;
    spi.master_slave_mode = SPI_MODE_MASTER;
    /* APB1 is 120 MHz: 15 MHz, slower than the stock /4 to be safe */
    spi.mclk_freq_division = SPI_MCLK_DIV_8;
    spi.first_bit_transmission = SPI_FIRST_BIT_MSB;
    spi.frame_bit_num = SPI_FRAME_8BIT;
    spi.clock_polarity = SPI_CLOCK_POLARITY_HIGH;
    spi.clock_phase = SPI_CLOCK_PHASE_2EDGE;
    spi.cs_mode_selection = SPI_CS_SOFTWARE_MODE;
    spi_init(SPI2, &spi);
    spi_enable(SPI2, TRUE);
}

uint32_t spi_flash_read_id(void)
{
    uint32_t id;

    CS_PORT->clr = CS_PIN;
    xfer(0x9F);
    id = (uint32_t)xfer(0xFF) << 16;
    id |= (uint32_t)xfer(0xFF) << 8;
    id |= xfer(0xFF);
    CS_PORT->scr = CS_PIN;
    return id;
}

void spi_flash_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    CS_PORT->clr = CS_PIN;
    xfer(0x03);
    xfer((uint8_t)(addr >> 16));
    xfer((uint8_t)(addr >> 8));
    xfer((uint8_t)addr);
    while (len--) {
        *buf++ = xfer(0xFF);
    }
    CS_PORT->scr = CS_PIN;
}
