/*
 * Power off, as the stock firmware does it (0x0800cdb8): when PE0 reads high,
 * and still does 50 ms later, it saves its settings, clears PA11 and resets.
 * PA11 appears to hold the power supply on; the application never sets it,
 * so the bootloader must. We do the same without the settings part.
 *
 * As a guard while this is unverified on hardware, PE0 must first have read
 * low at least once since boot, so a pin that idles high cannot switch the
 * radio off at start-up.
 */
#include <stdbool.h>

#include "power.h"
#include "board.h"
#include "clock.h"
#include "log.h"

static bool seen_on;
static uint32_t high_since;
static bool high;

static void power_off(void)
{
    gpio_init_type cfg;

    log_printf("power switch off\n");
    BOARD_POWER_HOLD_PORT->clr = BOARD_POWER_HOLD_PIN;
    gpio_default_para_init(&cfg);
    cfg.gpio_pins = BOARD_POWER_HOLD_PIN;
    cfg.gpio_mode = GPIO_MODE_OUTPUT;
    cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init(BOARD_POWER_HOLD_PORT, &cfg);
    clock_delay_ms(500);
    /* Still running: the supply is not ours to cut. Reset like the OEM does. */
    NVIC_SystemReset();
}

void power_poll(void)
{
    bool off = (BOARD_POWER_SW_PORT->idt & BOARD_POWER_SW_PIN) != 0;

    if (!off) {
        seen_on = true;
        high = false;
        return;
    }
    if (!seen_on) {
        return;
    }
    if (!high) {
        high = true;
        high_since = clock_millis();
    } else if (clock_millis() - high_since >= 50) {
        power_off();
    }
}
