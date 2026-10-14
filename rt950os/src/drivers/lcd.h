#ifndef RT950_LCD_H
#define RT950_LCD_H

#include <stdint.h>

/*
 * 240 x 320 TFT, ST7789-style controller, RGB565. Coordinates are in the
 * orientation the OEM firmware sets (MADCTL 0xC0): x runs along the 240-pixel
 * side, y along the 320-pixel side. Whether that is portrait or landscape on
 * the radio still needs checking on hardware.
 */
#define LCD_WIDTH  240
#define LCD_HEIGHT 320

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#define LCD_BLACK  0x0000
#define LCD_WHITE  0xFFFF
#define LCD_RED    0xF800
#define LCD_GREEN  0x07E0
#define LCD_BLUE   0x001F
#define LCD_YELLOW 0xFFE0
#define LCD_CYAN   0x07FF
#define LCD_GREY   0x8410

void lcd_init(void);
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void lcd_fill(uint16_t color);
void lcd_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg);
/* Returns the x position after the last character. */
uint16_t lcd_draw_string(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg);

#endif
