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

/* PTT inputs (PE13 primary, PE12 external) */
#define BOARD_PTT_PORT         GPIOE
#define BOARD_PTT_PIN          GPIO_PINS_13
#define BOARD_PTT_EXT_PIN      GPIO_PINS_12

/* BK4829 #1 on hardware SPI1, chip select PB12 */
#define BOARD_BK1_CS_PORT      GPIOB
#define BOARD_BK1_CS_PIN       GPIO_PINS_12

/* BK4829 #2 and SPI NOR flash share a bit-banged bus on PA5 (CS), PA6 (CLK), PA7 (MOSI) */
#define BOARD_SWSPI_PORT       GPIOA
#define BOARD_SWSPI_CS_PIN     GPIO_PINS_5
#define BOARD_SWSPI_CLK_PIN    GPIO_PINS_6
#define BOARD_SWSPI_MOSI_PIN   GPIO_PINS_7

#endif
