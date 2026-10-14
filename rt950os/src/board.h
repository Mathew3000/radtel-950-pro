/*
 * RT-950 Pro pin map.
 *
 * Sources: docs/pinout.md and docs/display.md, which were reverse engineered
 * from the OEM V0.18 firmware. Entries marked "unverified" have not been
 * checked on hardware yet.
 */
#ifndef RT950_BOARD_H
#define RT950_BOARD_H

#include "at32f403a_407.h"

/* LCD backlight (docs/pinout.md: "PC12 strobe/backlight", polarity unverified) */
#define BOARD_BACKLIGHT_PORT   GPIOC
#define BOARD_BACKLIGHT_PIN    GPIO_PINS_12
#define BOARD_BACKLIGHT_CLOCK  CRM_GPIOC_PERIPH_CLOCK

/* Rotary encoder (PB4 phase A, PB5 phase B) */
#define BOARD_ENC_PORT         GPIOB
#define BOARD_ENC_A_PIN        GPIO_PINS_4
#define BOARD_ENC_B_PIN        GPIO_PINS_5

/*
 * PTT on PE3, active low. The OEM power-on key combinations (0x0800a4c2) and
 * the TX loop (0x08010a84) test PE3. docs/pinout.md lists PE13/PE12 as PTT,
 * but those are RF path outputs (see below).
 */
#define BOARD_PTT_PORT         GPIOE
#define BOARD_PTT_PIN          GPIO_PINS_3

/*
 * Inputs the OEM firmware polls whose role is still unknown: PE0 (debounced
 * for 50 ms at 0x0800cdb8) and PC15 (0x08010e60). Shown raw for probing.
 */
#define BOARD_AUX0_PORT        GPIOE
#define BOARD_AUX0_PIN         GPIO_PINS_0
#define BOARD_AUX1_PORT        GPIOC
#define BOARD_AUX1_PIN         GPIO_PINS_15

/*
 * RF path selects, from the OEM routine at 0x0801d704 (named PTT_Relay_Select
 * in docs/Function_Names.csv). One RX select is high while receiving, by band:
 * PB0 for 100-300 MHz, PE12 for 300 MHz and up, PE13 below 100 MHz. The TX
 * selects PB1, PE7 and PE14 follow the same split. rt950os drives the TX
 * selects low and never sets them.
 */
#define BOARD_RX_VHF_PORT      GPIOB
#define BOARD_RX_VHF_PIN       GPIO_PINS_0
#define BOARD_RX_UHF_PORT      GPIOE
#define BOARD_RX_UHF_PIN       GPIO_PINS_12
#define BOARD_RX_LOW_PORT      GPIOE
#define BOARD_RX_LOW_PIN       GPIO_PINS_13
#define BOARD_TX_VHF_PORT      GPIOB
#define BOARD_TX_VHF_PIN       GPIO_PINS_1
#define BOARD_TX_UHF_PORT      GPIOE
#define BOARD_TX_UHF_PIN       GPIO_PINS_7
#define BOARD_TX_LOW_PORT      GPIOE
#define BOARD_TX_LOW_PIN       GPIO_PINS_14

/*
 * Audio output enables. The OEM audio source manager (0x08022d98) sets PE4
 * and PB8 while any source (receive audio, tones) is active and clears them,
 * together with PC9, when the last one stops.
 */
#define BOARD_AUDIO_EN_PORT    GPIOE
#define BOARD_AUDIO_EN_PIN     GPIO_PINS_4
#define BOARD_AUDIO_EN2_PORT   GPIOB
#define BOARD_AUDIO_EN2_PIN    GPIO_PINS_8
#define BOARD_AUDIO_EN3_PORT   GPIOC
#define BOARD_AUDIO_EN3_PIN    GPIO_PINS_9

/* BK4829 3-wire bus: see src/drivers/bk4829.c */

#endif
