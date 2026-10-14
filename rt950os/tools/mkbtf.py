#!/usr/bin/env python3
"""Wrap a plain rt950os image into a .btf file for the OEM USB updater.

The OEM updater (RT-950_EnUPDATE) sends the .btf bytes to the radio unchanged,
and the radio undoes the fwcrypt XOR itself. fwcrypt is its own inverse, so
encrypting is the same transform as firmware/scripts/fwcrypt_io.py applies.

Before writing, the image is checked for the things the radio is known to look
at: a sane vector table for an image linked at 0x08002C00, the "RT-950" model
block at offset 0x3E0, and an empty 0x400-0x7FF block.

Usage: mkbtf.py rt950os.bin rt950os.btf
"""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "firmware" / "scripts"))
from fwcrypt_io import build_key, fwcrypt  # noqa: E402

APP_BASE = 0x08002C00          # the OEM bootloader owns 0x08000000-0x08002BFF
APP_SIZE = 1024 * 1024 - 0x2C00
RAM_BASE = 0x20000000
RAM_SIZE = 96 * 1024
MODEL_OFFSET = 0x3E0
MODEL = b"RT-950      \x00\x00\x00\x00"


def check_image(image: bytes) -> None:
    if len(image) <= 0x800:
        raise SystemExit("image is too small to carry the fwcrypt key at 0x400")
    if len(image) > APP_SIZE:
        raise SystemExit(f"image is {len(image)} bytes, larger than the {APP_SIZE} byte app area")

    sp, reset = struct.unpack_from("<II", image, 0)
    if not RAM_BASE < sp <= RAM_BASE + RAM_SIZE:
        raise SystemExit(f"initial SP 0x{sp:08x} is outside RAM")
    if not (APP_BASE <= reset < APP_BASE + len(image) and reset & 1):
        raise SystemExit(f"reset vector 0x{reset:08x} is not a Thumb address inside the image")
    if APP_BASE + 0x400 <= reset < APP_BASE + 0x800:
        raise SystemExit("reset handler lies in the 0x400-0x7FF block the bootloader may transform")
    if any(image[0x400:0x800]):
        raise SystemExit("image offsets 0x400-0x7FF must stay zero, check the .btf_reserved section")

    if image[MODEL_OFFSET:MODEL_OFFSET + len(MODEL)] != MODEL:
        raise SystemExit("model block missing at 0x3E0, check the .model_info section")


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2

    image = Path(sys.argv[1]).read_bytes()
    check_image(image)

    encrypted = bytes(fwcrypt(bytearray(image), build_key(image)))
    if bytes(fwcrypt(bytearray(encrypted), build_key(encrypted))) != image:
        raise SystemExit("fwcrypt round trip failed")

    Path(sys.argv[2]).write_bytes(encrypted)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
