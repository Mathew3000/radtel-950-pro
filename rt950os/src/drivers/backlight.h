#ifndef RT950_BACKLIGHT_H
#define RT950_BACKLIGHT_H

#include <stdbool.h>

void backlight_init(void);
void backlight_set(bool on);
void backlight_toggle(void);

#endif
