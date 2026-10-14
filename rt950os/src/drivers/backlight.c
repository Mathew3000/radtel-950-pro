#include "backlight.h"
#include "board.h"

void backlight_init(void)
{
    gpio_init_type cfg;

    crm_periph_clock_enable(BOARD_BACKLIGHT_CLOCK, TRUE);

    gpio_default_para_init(&cfg);
    cfg.gpio_pins = BOARD_BACKLIGHT_PIN;
    cfg.gpio_mode = GPIO_MODE_OUTPUT;
    cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    cfg.gpio_pull = GPIO_PULL_NONE;
    gpio_init(BOARD_BACKLIGHT_PORT, &cfg);
}

void backlight_set(bool on)
{
    if (on) {
        gpio_bits_set(BOARD_BACKLIGHT_PORT, BOARD_BACKLIGHT_PIN);
    } else {
        gpio_bits_reset(BOARD_BACKLIGHT_PORT, BOARD_BACKLIGHT_PIN);
    }
}

void backlight_toggle(void)
{
    BOARD_BACKLIGHT_PORT->odt ^= BOARD_BACKLIGHT_PIN;
}
