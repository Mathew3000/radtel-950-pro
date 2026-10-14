/*
 * TFT driver on the GPIOD 8080 bus.
 *
 * Pins and init sequence come from the OEM V0.18 firmware:
 * - LCD_WriteCommand (0x08029dc0) and LCD_WriteData (0x08029e20): CS on PD1,
 *   D/C on PD3 (low for commands), WR on PD0, data byte on PD8-PD15.
 * - The unnamed init routine at 0x08029f5c: reset pulse on PD2, then the
 *   command table below. The commands (B2 porch, BB VCOM, E0/E1 gamma,
 *   3A 0x05 for RGB565, 21 inversion on) match an ST7789.
 */
#include "lcd.h"
#include "board.h"
#include "clock.h"
#include "font.h"

#define LCD_PORT   GPIOD
#define LCD_WR     GPIO_PINS_0
#define LCD_CS     GPIO_PINS_1
#define LCD_RST    GPIO_PINS_2
#define LCD_DC     GPIO_PINS_3
#define LCD_DATA   (GPIO_PINS_8 | GPIO_PINS_9 | GPIO_PINS_10 | GPIO_PINS_11 | \
                    GPIO_PINS_12 | GPIO_PINS_13 | GPIO_PINS_14 | GPIO_PINS_15)

/* Put a byte on PD8-PD15 in one write: set bits in the low half of SCR, clear bits in the high half. */
static inline void bus_put(uint8_t b)
{
    LCD_PORT->scr = ((uint32_t)b << 8) | ((uint32_t)(uint8_t)~b << 24);
}

static inline void bus_strobe(void)
{
    LCD_PORT->clr = LCD_WR;
    __NOP();
    __NOP();
    LCD_PORT->scr = LCD_WR;
}

static void write_cmd(uint8_t cmd)
{
    LCD_PORT->clr = LCD_CS | LCD_DC;
    bus_put(cmd);
    bus_strobe();
    LCD_PORT->scr = LCD_CS;
}

static void write_data(uint8_t data)
{
    LCD_PORT->clr = LCD_CS;
    LCD_PORT->scr = LCD_DC;
    bus_put(data);
    bus_strobe();
    LCD_PORT->scr = LCD_CS;
}

/* Init table: command, number of data bytes, data. 0xFF ends it. */
static const uint8_t init_seq[] = {
    0xB2, 5, 0x05, 0x05, 0x00, 0x33, 0x33,
    0xB7, 1, 0x35,
    0xC0, 1, 0x2C,
    0xC2, 1, 0x01,
    0xC3, 1, 0x0F,
    0xC4, 1, 0x20,
    0xC6, 1, 0x11,
    0xD0, 2, 0xA4, 0xA1,
    0xE8, 1, 0x03,
    0xE9, 3, 0x09, 0x09, 0x08,
    0xBB, 1, 0x3F,
    0xE0, 14, 0xD0, 0x05, 0x09, 0x09, 0x08, 0x14, 0x28, 0x33, 0x3F, 0x07, 0x13, 0x14, 0x28, 0x30,
    0xE1, 14, 0xD0, 0x05, 0x09, 0x09, 0x08, 0x03, 0x24, 0x32, 0x32, 0x3B, 0x14, 0x13, 0x28, 0x2F,
    0x36, 1, 0xC0,
    0x3A, 1, 0x05,
    0x21, 0,
    0x29, 0,
    0xFF,
};

static void gpio_setup(void)
{
    gpio_init_type cfg;

    crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);

    LCD_PORT->scr = LCD_WR | LCD_CS | LCD_RST | LCD_DC;

    gpio_default_para_init(&cfg);
    cfg.gpio_pins = LCD_WR | LCD_CS | LCD_RST | LCD_DC | LCD_DATA;
    cfg.gpio_mode = GPIO_MODE_OUTPUT;
    cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    cfg.gpio_pull = GPIO_PULL_NONE;
    gpio_init(LCD_PORT, &cfg);
}

void lcd_init(void)
{
    const uint8_t *p = init_seq;

    gpio_setup();

    /* Same reset timing as the OEM: high 1 ms, low 1 ms, high 120 ms */
    LCD_PORT->scr = LCD_RST;
    clock_delay_ms(1);
    LCD_PORT->clr = LCD_RST;
    clock_delay_ms(1);
    LCD_PORT->scr = LCD_RST;
    clock_delay_ms(120);

    write_cmd(0x11); /* sleep out */
    clock_delay_ms(120);

    while (*p != 0xFF) {
        uint8_t cmd = *p++;
        uint8_t n = *p++;
        write_cmd(cmd);
        while (n--) {
            write_data(*p++);
        }
    }
}

static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    write_cmd(0x2A);
    write_data(x0 >> 8);
    write_data(x0 & 0xFF);
    write_data(x1 >> 8);
    write_data(x1 & 0xFF);
    write_cmd(0x2B);
    write_data(y0 >> 8);
    write_data(y0 & 0xFF);
    write_data(y1 >> 8);
    write_data(y1 & 0xFF);
    write_cmd(0x2C);
}

/* Stream pixels with CS held low and D/C high, after set_window. */
static inline void pixel(uint16_t color)
{
    bus_put(color >> 8);
    bus_strobe();
    bus_put(color & 0xFF);
    bus_strobe();
}

static inline void pixels_begin(void)
{
    LCD_PORT->clr = LCD_CS;
    LCD_PORT->scr = LCD_DC;
}

static inline void pixels_end(void)
{
    LCD_PORT->scr = LCD_CS;
}

void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT || w == 0 || h == 0) {
        return;
    }
    if (x + w > LCD_WIDTH) {
        w = LCD_WIDTH - x;
    }
    if (y + h > LCD_HEIGHT) {
        h = LCD_HEIGHT - y;
    }

    set_window(x, y, x + w - 1, y + h - 1);
    pixels_begin();
    for (uint32_t n = (uint32_t)w * h; n; n--) {
        pixel(color);
    }
    pixels_end();
}

void lcd_fill(uint16_t color)
{
    lcd_fill_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

void lcd_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg)
{
    if (x + FONT_W > LCD_WIDTH || y + FONT_H > LCD_HEIGHT) {
        return;
    }
    if (c < FONT_FIRST || c > FONT_LAST) {
        c = '?';
    }

    const uint8_t *glyph = font8x16[c - FONT_FIRST];

    set_window(x, y, x + FONT_W - 1, y + FONT_H - 1);
    pixels_begin();
    for (unsigned row = 0; row < FONT_H; row++) {
        for (uint8_t mask = 0x80; mask; mask >>= 1) {
            pixel((glyph[row] & mask) ? fg : bg);
        }
    }
    pixels_end();
}

uint16_t lcd_draw_string(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg)
{
    while (*s) {
        lcd_draw_char(x, y, *s++, fg, bg);
        x += FONT_W;
    }
    return x;
}
