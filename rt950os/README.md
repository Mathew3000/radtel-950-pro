# rt950os

Open replacement firmware for the Radtel RT-950 Pro (AT32F403A). Right now it
is a bring-up test image that can receive FM. It sets the clock to 240 MHz from the internal
oscillator, blinks the LCD backlight (PC12) at 1 Hz, and draws a test screen
on the TFT: text, colour bars, corner markers (red top left, green bottom
right) and a seconds counter.

The display driver uses the OEM pinout and init sequence (an ST7789-style
controller, see `src/drivers/lcd.c`). What to check on the radio: the bars
read white, yellow, cyan, green, red, blue, black, grey from the red corner,
and the counter advances once per second.

Below that, the screen shows raw inputs for mapping on hardware: a key code
for the key held down (see `src/drivers/input.h`), the PTT (PE3), two inputs
of unknown role (PE0, PC15), and an encoder counter with the raw A/B levels.
Writing down the code for each labelled key gives us the keypad map.

It also brings up both BK4829 transceivers with the OEM register setup (RX
path only, audio muted, TX never enabled) and reads four registers back. Each
chip gets a line like `bk0 ok 4/4 37=9d1f`: green when all four match, red
otherwise (`??` when none do). The RTT log lists every value read. Green on
both proves the 3-wire bus (`src/drivers/bk4829.c`).

Finally it receives FM on 145.500 MHz. The encoder tunes in 12.5 kHz steps
(18 to 520 MHz), the bottom lines show the frequency, which BK4829 is in use
and the RSSI, and the speaker opens while the RSSI is above -110 dBm. The
band split, register values and RF path pins come from the OEM firmware
(`src/drivers/rf.c`, `src/board.h`). There is no calibration yet, so expect
the RSSI to be off by a few dB. Nothing in this image can transmit: the TX
path selects are held low and BK4829 register 0x30 only gets the idle and
receive values.

Turning the power switch off switches the radio off, as with stock firmware
(PE0 goes high, then PA11 is released; `src/app/power.c`).

## Build

Needs `arm-none-eabi-gcc` (tested with 13.2), CMake 3.22 or newer, Ninja and
Python 3.

```sh
cmake -S rt950os -B rt950os/build -G Ninja
cmake --build rt950os/build
```

Outputs in `rt950os/build/`:

| File | Use |
| --- | --- |
| `rt950os.elf` | Debugging over SWD |
| `rt950os.bin` | Plain image, flash at `0x08002C00` with an SWD probe |
| `rt950os.btf` | fwcrypt-encrypted image for the OEM USB updater or `radtel_flash.py` |

The linker script puts the `RT-950` model block at image offset `0x3E0`,
where the OEM updater reads it, and `tools/mkbtf.py` refuses to wrap an image
without it.

## Debug log and emulator test

The firmware logs over SEGGER RTT, which needs no extra pins: J-Link RTT
Viewer, OpenOCD (`rtt setup` / `rtt start`) or pyOCD find the control block in
RAM and show the output. The boot line reports the system clock, and every key
press logs its raw code.

`tools/emu_smoke.py` boots the image in the Unicorn CPU emulator. It checks
the boot log, the TFT init bytes on the bus and the first full-screen fill.
CI runs it on every push:

```sh
python3 -m pip install unicorn
python3 rt950os/tools/emu_smoke.py rt950os/build/rt950os.elf rt950os/build/rt950os.bin
```

## Flash layout

| Range | Contents |
| --- | --- |
| `0x08000000` to `0x08002BFF` | OEM bootloader (11 KB). Never write or erase it. |
| `0x08002C00` onwards | Application. The OEM image and rt950os both link here. |

The OEM application's absolute pointers only make sense at `0x08002C00`. With
that base, the NMI handler is a plain `bx lr`, HardFault loops on itself, and
the keypad tables at `0x0803013a` hold column masks `0x0F 0x0E 0x0D 0x0B
0x07`. At `0x08000000` they all point at garbage.

Image offsets `0x400` to `0x7FF` (`0x08003000` to `0x080033FF`) are a 1 KB
block that fwcrypt does not decrypt. In the V0.18 BTF it looks random, but the
OEM reset vector (`0x080032a1`) points into it, so the bootloader must turn it
into code in some way we have not worked out. rt950os keeps this block empty,
and `tools/mkbtf.py` refuses an image that uses it.

Addresses in rt950os comments are real flash addresses with this base. The
older notes and Ghidra names (`FUN_0800xxxx`, `docs/Function_Names.csv`) were
made with the image loaded at `0x08000000`: add `0x2C00` to a function name to
get its real address. For example, `Keypad_ScanMatrix` (`FUN_08013618`) is at
`0x08016218`.

This also means `firmware/decrypted.bin` is not a complete OEM image: its
`0x400` to `0x7FF` block is not what ends up in flash. Do not use it to
restore the radio over SWD.

## Before flashing anything

Make a full SWD backup first:

- main flash `0x08000000` to `0x080FFFFF` (bootloader and application)
- boot memory `0x1FFFB000` to `0x1FFFEFFF`
- user system data (option bytes) `0x1FFFF800` to `0x1FFFF82F`
- the external SPI NOR flash, which probably holds calibration and channels

The update protocol (`BOOTLOADER_V3`) is served by the bootloader, which
rt950os leaves alone. The OEM updater (`RT-950_EnUPDATE`) has two ways in,
both over the programming cable at 115200 baud:

- **Flashing Mode:** power the radio on while holding side keys 3 and 4. The
  bootloader then waits for the updater by itself, so this works whatever
  application is installed, including a broken one.
- **Upgrade Mode:** with the radio on, the updater sends `PROGRAMBT9000U` and
  `UPDATE`, the application ACKs both with `0x06` and resets into the
  bootloader. Stock firmware does this, and so does rt950os (`src/app/update.c`,
  UART4 on PC10/PC11).

Flashing Mode is the safety net. Neither mode has been tried with rt950os on
a radio yet, so keep the SWD backup.

## Flashing

Over SWD: write `rt950os.bin` at `0x08002C00`. Don't use a full chip erase,
because that also erases the bootloader.

Over the programming cable, with the OEM updater or the script (Upgrade Mode;
add `--skip-handshake` for Flashing Mode):

```sh
python3 firmware/scripts/radtel_flash.py --port /dev/ttyACM0 rt950os/build/rt950os.btf
```

## Back to stock

Over the programming cable: send the official BTF unchanged, with the OEM
updater or the script. If Upgrade Mode does not answer, use Flashing Mode
(side keys 3 and 4 at power on, then `--skip-handshake`).

```sh
python3 firmware/scripts/radtel_flash.py --port /dev/ttyACM0 "firmware/reference firmware/RT_950Pro_V0.18_250919.BTF"
```

Over SWD: write back your main flash backup.

`radtel_flash.py` used to decrypt a BTF before sending it. The radio decrypts
the data itself, so that would have written a scrambled image. It now sends a
`.btf` unchanged and encrypts a plain `.bin` first.
