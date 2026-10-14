/*
 * Bring-up test image.
 *
 * Sets up the clocks, draws a test screen on the TFT and blinks the LCD
 * backlight at 1 Hz. A blink is visible whatever the backlight polarity turns
 * out to be. The colour bars show whether the colour order and orientation
 * are right, and the seconds counter proves the clock runs at the expected
 * speed.
 */
#include "clock.h"
#include "backlight.h"
#include "lcd.h"

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

static void draw_test_screen(void)
{
    static const uint16_t bars[] = {
        LCD_WHITE, LCD_YELLOW, LCD_CYAN, LCD_GREEN, LCD_RED, LCD_BLUE, LCD_BLACK, LCD_GREY,
    };
    const uint16_t bar_w = LCD_WIDTH / (sizeof(bars) / sizeof(bars[0]));

    lcd_fill(LCD_BLACK);
    lcd_draw_string(8, 8, "rt950os", LCD_WHITE, LCD_BLACK);
    lcd_draw_string(8, 28, "bring-up test", LCD_GREY, LCD_BLACK);

    for (unsigned i = 0; i < sizeof(bars) / sizeof(bars[0]); i++) {
        lcd_fill_rect(i * bar_w, 56, bar_w, 40, bars[i]);
    }

    /* Corner markers: red top left, green bottom right */
    lcd_fill_rect(0, 0, 4, 4, LCD_RED);
    lcd_fill_rect(LCD_WIDTH - 4, LCD_HEIGHT - 4, 4, 4, LCD_GREEN);
}

static void draw_uptime(uint32_t seconds)
{
    char buf[16] = "up ";
    char digits[10];
    unsigned n = 0, pos = 3;

    do {
        digits[n++] = (char)('0' + seconds % 10);
        seconds /= 10;
    } while (seconds && n < sizeof(digits));
    while (n) {
        buf[pos++] = digits[--n];
    }
    buf[pos++] = 's';
    buf[pos] = '\0';

    lcd_draw_string(8, 112, buf, LCD_WHITE, LCD_BLACK);
}

int main(void)
{
    uint32_t next = 0;
    uint32_t seconds = 0;

    nvic_clear();
    clock_init();
    backlight_init();
    backlight_set(true);
    lcd_init();
    draw_test_screen();

    for (;;) {
        if ((int32_t)(clock_millis() - next) >= 0) {
            next += 500;
            backlight_toggle();
            if ((next / 500) % 2 == 0) {
                draw_uptime(seconds++);
            }
        }
    }
}
