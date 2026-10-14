#ifndef RT950_BK4829_H
#define RT950_BK4829_H

#include <stdbool.h>
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

/* Power down (OEM 0x0801d556) and power up (0x0801d588) the chip's RF blocks. */
void bk4829_sleep(bk_chip_t chip);
void bk4829_wake(bk_chip_t chip);

/*
 * Tune to freq (10 Hz units) and enter receive with the audio muted, as the
 * OEM RX start (0x0801ebf8) does for a channel without tones.
 */
void bk4829_rx_on(bk_chip_t chip, uint32_t freq);

/* Demodulated FM audio to the AF output, or muted. */
void bk4829_af(bk_chip_t chip, bool on);

/* Received signal strength in dBm (register 0x67, same scale as the BK4819). */
int16_t bk4829_rssi_dbm(bk_chip_t chip);

#endif
