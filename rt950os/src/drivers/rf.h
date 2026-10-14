#ifndef RT950_RF_H
#define RT950_RF_H

#include <stdbool.h>
#include <stdint.h>

#include "bk4829.h"

/* Frequencies are in 10 Hz units, as the BK4829 takes them (14550000 = 145.5 MHz). */

/*
 * Band split used by the OEM firmware (0x080167dc): below 100 MHz and from
 * 300 MHz up the BK4829 on chip select PE15 receives, from 100 to 300 MHz the
 * one on PE8. The RF path selects follow the same split.
 */
typedef enum {
    RF_BAND_LOW, /* below 100 MHz */
    RF_BAND_VHF, /* 100 to 300 MHz */
    RF_BAND_UHF, /* 300 MHz and up */
} rf_band_t;

rf_band_t rf_band(uint32_t freq);
bk_chip_t rf_chip(uint32_t freq);

/* Configure every RF path and audio enable pin as an output, all low. */
void rf_init(void);

/* Receive on freq: wakes the right BK4829, puts the other to sleep, selects the RX path. */
void rf_rx_start(uint32_t freq);

/* Unmute the receiver and turn the audio output on, or the reverse. */
void rf_audio(bool on);

#ifdef RT950_ENABLE_TX
/*
 * Transmit on freq, or return false if freq is outside the amateur bands
 * rt950os allows (144-146 MHz, 430-440 MHz). See rf.c for the power level.
 */
bool rf_tx_start(uint32_t freq);

/* Stop transmitting and go back to receive on rx_freq. */
void rf_tx_stop(uint32_t rx_freq);
#endif

#endif
