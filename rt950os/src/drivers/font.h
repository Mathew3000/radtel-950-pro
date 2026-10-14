#ifndef RT950_FONT_H
#define RT950_FONT_H

#include <stdint.h>

#define FONT_W     8
#define FONT_H     16
#define FONT_FIRST 0x20
#define FONT_LAST  0x7E

extern const uint8_t font8x16[FONT_LAST - FONT_FIRST + 1][FONT_H];

#endif
