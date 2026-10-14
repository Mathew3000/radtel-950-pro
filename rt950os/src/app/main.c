/*
 * Bring-up test image.
 *
 * Sets up the clocks, draws a test screen on the TFT and blinks the LCD
 * backlight at 1 Hz. A blink is visible whatever the backlight polarity turns
 * out to be. The colour bars show whether the colour order and orientation
 * are right, and the seconds counter proves the clock runs at the expected
 * speed. Below them, raw key codes, PTT levels and an encoder counter let us
 * map the keypad on the radio.
 */
#include "clock.h"
#include "backlight.h"
#include "lcd.h"
#include "input.h"
#include "log.h"

/*
 * The OEM bootloader starts the application and may leave interrupts enabled.
 * Point VTOR at our vector table and start from a clean NVIC, so a leftover
 * interrupt cannot land in the bootloader's handlers or Default_Handler.
 */
static void nvic_clear(void)
{
    extern const uint32_t g_pfnVectors[];

    __disable_irq();
    /* SystemInit points VTOR at 0x08000000, which is the bootloader's table */
    SCB->VTOR = (uint32_t)g_pfnVectors;
    __DSB();
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

/* Format a signed decimal into buf (at least 12 bytes), right-aligned to width. */
static const char *fmt_int(char *buf, int32_t v, unsigned width)
{
    char tmp[12];
    unsigned n = 0, pos = 0;
    uint32_t u = v < 0 ? (uint32_t)-v : (uint32_t)v;

    do {
        tmp[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u && n < 10);
    if (v < 0) {
        tmp[n++] = '-';
    }
    while (width > n && pos < 11) {
        buf[pos++] = ' ';
        width--;
    }
    while (n) {
        buf[pos++] = tmp[--n];
    }
    buf[pos] = '\0';
    return buf;
}

static void draw_field(uint16_t y, const char *label, const char *value)
{
    uint16_t x = lcd_draw_string(8, y, label, LCD_GREY, LCD_BLACK);
    lcd_draw_string(x, y, value, LCD_WHITE, LCD_BLACK);
}

static void draw_inputs(const input_raw_t *in, int32_t enc_count)
{
    char buf[12];

    draw_field(136, "key ", in->key == INPUT_KEY_NONE ? "  --" : fmt_int(buf, in->key, 4));
    draw_field(156, "ptt ", in->ptt ? "DOWN" : "  up");
    draw_field(176, "ext ", in->ptt_ext ? "DOWN" : "  up");
    draw_field(196, "enc ", fmt_int(buf, enc_count, 6));
    buf[0] = (in->enc_ab & 2) ? '1' : '0';
    buf[1] = (in->enc_ab & 1) ? '1' : '0';
    buf[2] = '\0';
    draw_field(216, "A B ", buf);
}

int main(void)
{
    uint32_t next = 0;
    uint32_t next_input = 0;
    uint32_t seconds = 0;
    int32_t enc_count = 0;
    uint8_t last_key = INPUT_KEY_NONE;
    char buf[12];

    nvic_clear();
    clock_init();
    log_init();
    log_printf("rt950os bring-up, sysclk %lu Hz\n", (unsigned long)system_core_clock);
    backlight_init();
    backlight_set(true);
    lcd_init();
    input_init();
    draw_test_screen();

    for (;;) {
        enc_count += input_encoder_poll();

        if ((int32_t)(clock_millis() - next_input) >= 0) {
            input_raw_t in;
            next_input += 20;
            input_read(&in);
            draw_inputs(&in, enc_count);
            if (in.key != last_key) {
                if (in.key != INPUT_KEY_NONE) {
                    log_printf("key %u\n", in.key);
                }
                last_key = in.key;
            }
        }

        if ((int32_t)(clock_millis() - next) >= 0) {
            next += 500;
            backlight_toggle();
            if ((next / 500) % 2 == 0) {
                draw_field(112, "up ", fmt_int(buf, (int32_t)seconds++, 6));
            }
        }
    }
}
