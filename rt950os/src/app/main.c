/*
 * Backlight blink test.
 *
 * The first image for the radio: it only sets up the clocks and blinks the
 * LCD backlight at 1 Hz. A blink is visible whatever the backlight polarity
 * turns out to be, and proves that our image boots and the clock runs at the
 * expected speed.
 */
#include "clock.h"
#include "backlight.h"

/*
 * The OEM bootloader passes a flag in r0 when it starts the application, so
 * it may run before us with interrupts enabled. Start from a clean NVIC so a
 * leftover interrupt cannot land in Default_Handler.
 */
static void nvic_clear(void)
{
    __disable_irq();
    SysTick->CTRL = 0;
    for (unsigned i = 0; i < sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]); i++) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }
    __enable_irq();
}

int main(void)
{
    nvic_clear();
    clock_init();
    backlight_init();

    for (;;) {
        backlight_toggle();
        clock_delay_ms(500);
    }
}
