#ifndef RT950_BK4829_H
#define RT950_BK4829_H

#include <stdint.h>

/* The two BK4829 transceivers, numbered as the OEM firmware selects them */
typedef enum {
    BK_CHIP_0 = 0, /* chip select PE8 */
    BK_CHIP_1 = 1, /* chip select PE15 */
} bk_chip_t;

void bk4829_bus_init(void);
void bk4829_write(bk_chip_t chip, uint8_t reg, uint16_t value);
uint16_t bk4829_read(bk_chip_t chip, uint8_t reg);

/*
 * Soft reset and the OEM register setup (BK4829_Init, 0x0800ab04). This only
 * configures the receiver path: it does not enable TX, and the PA gate is a
 * separate GPIO this driver never touches.
 */
void bk4829_init(bk_chip_t chip);

#endif
