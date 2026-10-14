#ifndef RT950_SPI_FLASH_H
#define RT950_SPI_FLASH_H

#include <stdint.h>

/*
 * External SPI NOR flash, read only. It holds the stock firmware's
 * calibration (0xF000-0xF24F) and channels, which rt950os must never
 * change: this driver has no write or erase command at all.
 */

/* Calibration block the stock firmware loads at boot (0x0801280e onwards) */
#define SPI_FLASH_CAL_ADDR 0xF000U
#define SPI_FLASH_CAL_SIZE 0x250U

void spi_flash_init(void);

/* JEDEC ID (opcode 0x9F): manufacturer << 16 | type << 8 | capacity */
uint32_t spi_flash_read_id(void);

void spi_flash_read(uint32_t addr, uint8_t *buf, uint32_t len);

#endif
