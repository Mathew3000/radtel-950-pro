#ifndef RT950_CLOCK_H
#define RT950_CLOCK_H

#include <stdint.h>
#include "at32f403a_407.h"

void clock_init(void);
uint32_t clock_millis(void);
void clock_delay_ms(uint32_t ms);
void clock_delay_us(uint32_t us);

#endif
