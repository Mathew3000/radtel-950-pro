#!/usr/bin/env python3
"""Boot rt950os in the Unicorn CPU emulator and check what it does.

This is a smoke test that needs no radio. It runs the image from the reset
vector with just enough peripheral behaviour to get through start-up:
- CRM: oscillator and PLL "stable" flags read as set, and the clock switch
  status follows the requested source.
- SysTick: Unicorn does not deliver exceptions, so each read of the tick
  counter advances it by 1 ms.
Everything else is plain memory.

It then checks:
- the RTT log carries the boot line with a 240 MHz system clock;
- the bytes clocked out on the TFT bus (GPIOD, latched on WR rising) match
  the OEM init sequence;
- a full-screen fill went out, so the test screen was drawn.

Usage: emu_smoke.py build/rt950os.elf build/rt950os.bin
"""

import struct
import subprocess
import sys

from unicorn import Uc, UcError, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_PC, UC_CPU_ARM_CORTEX_M4

FLASH, RAM, PERIPH, PPB = 0x08000000, 0x20000000, 0x40000000, 0xE0000000
CRM = 0x40021000
GPIOD = 0x40011400
GPIO_SCR, GPIO_CLR = 0x10, 0x14

# Expected TFT traffic up to display on: (is_data, byte)
LCD_EXPECTED = [(0, 0x11)]
for cmd, data in [
    (0xB2, [0x05, 0x05, 0x00, 0x33, 0x33]), (0xB7, [0x35]), (0xC0, [0x2C]), (0xC2, [0x01]),
    (0xC3, [0x0F]), (0xC4, [0x20]), (0xC6, [0x11]), (0xD0, [0xA4, 0xA1]), (0xE8, [0x03]),
    (0xE9, [0x09, 0x09, 0x08]), (0xBB, [0x3F]),
    (0xE0, [0xD0, 0x05, 0x09, 0x09, 0x08, 0x14, 0x28, 0x33, 0x3F, 0x07, 0x13, 0x14, 0x28, 0x30]),
    (0xE1, [0xD0, 0x05, 0x09, 0x09, 0x08, 0x03, 0x24, 0x32, 0x32, 0x3B, 0x14, 0x13, 0x28, 0x2F]),
    (0x36, [0xC0]), (0x3A, [0x05]), (0x21, []), (0x29, []),
]:
    LCD_EXPECTED.append((0, cmd))
    LCD_EXPECTED.extend((1, b) for b in data)


def symbols(elf):
    out = subprocess.run(["arm-none-eabi-nm", elf], capture_output=True, text=True, check=True).stdout
    syms = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3:
            syms[parts[2]] = int(parts[0], 16)
    return syms


def main():
    elf, binpath = sys.argv[1], sys.argv[2]
    syms = symbols(elf)
    image = open(binpath, "rb").read()

    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    uc.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M4)
    uc.mem_map(FLASH, 1024 * 1024)
    uc.mem_map(RAM, 96 * 1024)
    uc.mem_map(PERIPH, 0x30000)
    uc.mem_map(PPB, 0x100000)
    uc.mem_write(FLASH, image)

    tick = syms["tick_ms"]
    gpiod_odt = [0xFFFF]
    lcd = []

    def on_read(uc, access, addr, size, value, data):
        if addr == CRM:
            v = struct.unpack("<I", uc.mem_read(CRM, 4))[0]
            uc.mem_write(CRM, struct.pack("<I", v | (1 << 1) | (1 << 17) | (1 << 25)))
        elif addr == CRM + 4:
            v = struct.unpack("<I", uc.mem_read(CRM + 4, 4))[0]
            uc.mem_write(CRM + 4, struct.pack("<I", (v & ~0xC) | ((v & 3) << 2)))
        elif addr == tick:
            v = struct.unpack("<I", uc.mem_read(tick, 4))[0]
            uc.mem_write(tick, struct.pack("<I", v + 1))

    def on_write(uc, access, addr, size, value, data):
        if addr not in (GPIOD + GPIO_SCR, GPIOD + GPIO_CLR):
            return
        old = gpiod_odt[0]
        if addr == GPIOD + GPIO_SCR:
            new = (old | (value & 0xFFFF)) & ~((value >> 16) & 0xFFFF)
        else:
            new = old & ~(value & 0xFFFF)
        gpiod_odt[0] = new
        wr_rise = not (old & 1) and (new & 1)
        cs_low = not (new & 2)
        if wr_rise and cs_low:
            lcd.append((1 if new & 8 else 0, (new >> 8) & 0xFF))

    uc.hook_add(UC_HOOK_MEM_READ, on_read, begin=CRM, end=CRM + 8)
    uc.hook_add(UC_HOOK_MEM_READ, on_read, begin=tick, end=tick + 3)
    uc.hook_add(UC_HOOK_MEM_WRITE, on_write, begin=GPIOD, end=GPIOD + 0x20)

    sp, reset = struct.unpack_from("<II", image, 0)
    uc.reg_write(UC_ARM_REG_SP, sp)
    try:
        uc.emu_start(reset | 1, 0, count=3_000_000)
    except UcError as exc:
        pc = uc.reg_read(UC_ARM_REG_PC)
        raise SystemExit(f"emulation stopped at pc=0x{pc:08x}: {exc}")

    failures = []

    cb = syms["rtt_cb"]
    up = struct.unpack("<IIIII", uc.mem_read(cb + 24, 20))
    buf, wr = up[1], up[3]
    log = bytes(uc.mem_read(buf, wr)).decode(errors="replace")
    print("RTT log:", repr(log))
    if uc.mem_read(cb, 10) != b"SEGGER RTT":
        failures.append("RTT control block ID not set")
    if "sysclk 240000000 Hz" not in log:
        failures.append("boot line with 240 MHz sysclk missing from RTT log")

    got = lcd[: len(LCD_EXPECTED)]
    if got != LCD_EXPECTED:
        for i, (g, e) in enumerate(zip(got, LCD_EXPECTED)):
            if g != e:
                failures.append(f"TFT byte {i}: got {g}, expected {e}")
                break
        else:
            failures.append(f"TFT sequence short: {len(got)} of {len(LCD_EXPECTED)} bytes")
    print(f"TFT: {len(lcd)} bytes on the bus, init sequence {'ok' if got == LCD_EXPECTED else 'WRONG'}")

    if len(lcd) < 2 * 240 * 320:
        failures.append(f"only {len(lcd)} bytes on the TFT bus, the full-screen fill is missing")

    if failures:
        for f in failures:
            print("FAIL:", f)
        return 1
    print("ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
