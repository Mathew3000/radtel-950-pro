SPI Flash Notes
================

The Radtel 950 Pro uses an external SPI NOR device on hardware SPI2: SCK PB13, MISO PB14, MOSI PB15, chip select PB12. The read routine at real address 0x08023d80 (Ghidra FUN_80021180) drops PB12, sends 0x03 and a 24-bit address and clocks bytes through SPI2 (0x40003800). SPI2 is set up at 0x08017070 as master, 8 bit, MSB first, mode 3. The PA5-PA7 bus described below is not what the code does, and the second BK4829 is on GPIOE (see rt950os/src/drivers/bk4829.c).

Calibration: at boot the stock firmware reads 0xF000-0xF24F into RAM in 16-byte rows (0x08012822 onwards), for example 0xF0C0 to 0x200000e4, which the band setup uses for squelch thresholds. rt950os reads this block (read only) and dumps it over RTT.

Key entry points:

- `Software_SPI_Write_Byte` / `Software_SPI_Write_Block`: fundamental byte/stream writers that toggle PA5 (CS), PA6 (SCLK), and PA7 (MOSI).
- `Software_SPI_FlashErase4K` (`FUN_800210c0`): issues opcode `0x20` plus a 24-bit address, then polls the status register until the write-in-progress bit clears.
- `Software_SPI_FlashErase32KBlock` (`FUN_80020f80`): same as above but sends opcode `0x52` (32 KB erase).
- `Software_SPI_FlashErase64KBlock` (`FUN_80020ff0`): sends opcode `0xD8` (64 KB erase).
- `FUN_80021180`: performs opcode `0x03` and reads blocks into RAM (`software SPI read`), used for channel memories and BT firmware transfers.

Where they are used:

- The Bluetooth command sequencer (`FUN_80008518`) calls the erase helpers when flashing the Bluetooth module, so captured traffic shows a mix of 4K/32K/64K erases. -- I bet this is CPS/channel/settings storage as I think this radio has the ability to reprogram over BT.
- Channel/programming routines elsewhere in the firmware (e.g., around 0x80018504–0x80019540) reference these helpers with addresses like `0x11000`, `0x21000`, etc., indicating sector layouts.

Hardware mapping: see the first paragraph (SPI2, CS PB12).

Notes:

- All erase routines poll `FUN_80020f6c()` for completion; if you port these, keep the delay loops or replace them with a proper status read.
- If you plan to dump the flash, reuse `FUN_80021180` and ensure PA5 is asserted for the duration of the read (the OEM code toggles in `Software_SPI_Write_Block`).
