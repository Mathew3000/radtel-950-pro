# rt950os

Open replacement firmware for the Radtel RT-950 Pro (AT32F403A). Right now it
is a bring-up test image. It sets the clock to 240 MHz from the internal
oscillator, blinks the LCD backlight (PC12) at 1 Hz, and draws a test screen
on the TFT: text, colour bars, corner markers (red top left, green bottom
right) and a seconds counter.

The display driver uses the OEM pinout and init sequence (an ST7789-style
controller, see `src/drivers/lcd.c`). What to check on the radio: the bars
read white, yellow, cyan, green, red, blue, black, grey from the red corner,
and the counter advances once per second.

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
| `rt950os.bin` | Plain image, flash at `0x08000000` with an SWD probe |
| `rt950os.btf` | fwcrypt-encrypted image for the OEM USB updater or `radtel_flash.py` |

The linker script puts the `RT-950` model block at image offset `0x3E0`,
where the OEM updater reads it, and `tools/mkbtf.py` refuses to wrap an image
without it.

## Before flashing anything

Make a full SWD backup first. We do not yet know whether the USB update mode
lives in a separate bootloader or in the OEM application itself. The app
contains the `PROGRAMBT9000U` and `UPDATE` handshake strings at `0x0802d58d`,
so it may well be the app. If so, flashing this image removes USB updates
until the stock firmware is restored over SWD.

Back up:

- main flash `0x08000000` to `0x080FFFFF`
- boot memory `0x1FFFB000` to `0x1FFFEFFF`
- user system data (option bytes) `0x1FFFF800` to `0x1FFFF82F`
- the external SPI NOR flash, which probably holds calibration and channels

## Flashing

Over SWD (recommended while the update path is unknown): write `rt950os.bin`
at `0x08000000`.

Over USB with the OEM update mode:

```sh
python3 firmware/scripts/radtel_flash.py --port /dev/ttyACM0 rt950os/build/rt950os.btf
```

## Back to stock

Over USB, if update mode still works: send the official BTF unchanged.

```sh
python3 firmware/scripts/radtel_flash.py --port /dev/ttyACM0 "firmware/reference firmware/RT_950Pro_V0.18_250919.BTF"
```

Over SWD: write the main flash backup, or `firmware/decrypted.bin`, at
`0x08000000`.

`radtel_flash.py` used to decrypt a BTF before sending it. The radio decrypts
the data itself, so that would have written a scrambled image. It now sends a
`.btf` unchanged and encrypts a plain `.bin` first.
