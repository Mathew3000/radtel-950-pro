# Headless Ghidra pipeline

Reproducible Ghidra analysis of the OEM RT-950 Pro application firmware,
usable in Claude Code cloud sessions or any Linux box with JDK 21+.

```sh
ghidra/setup-ghidra.sh      # installs Ghidra 12.1.4 to /opt (checksum-verified)
ghidra/analyze.sh           # analyzes firmware/decrypted.bin (V0.18)
ghidra/analyze.sh image.bin [symbols.csv]
```

`analyze.sh` imports the image into `ghidra/projects/RT950.gpr` (git-ignored,
open it in the Ghidra GUI to keep working) and writes
`ghidra/out/<image>.c` (all functions decompiled) and
`ghidra/out/<image>.functions.csv` (address, name, size). A run takes about a
minute.

In cloud sessions the SessionStart hook (`.claude/hooks/session-start.sh`)
installs Ghidra in the background, so it is ready when needed.

## Image layout

The application is linked at **0x08002C00**; `0x08000000-0x08002BFF` is the
bootloader. At that base the vector table resolves to real handlers
(`NMI_Handler` is `bx lr`, `HardFault_Handler` is `b .`) and literal-pool
pointers land on strings and function prologues. The initial SP is
`0x20017bb0` (96 KiB SRAM).

Image offsets `0x400-0x7FF` (`0x08003000-0x080033FF`) are the fwcrypt key
block. `fwcrypt_io.py` leaves it as is, so it is not real code. The reset
vector (`0x080032a1`) and the shared default IRQ handler (`0x080032bb`)
point into it, which means `Reset_Handler` is still unknown.
`AT32Setup.java` maps this range as an uninitialized `FWCRYPT_KEY` block so
Ghidra does not decompile the noise.

Older material used other bases. The GUI project in
`firmware/RE/decrypted-firmware` imported the image at `0x80000000`, so
its `FUN_8000xxxx` names are image offsets: add `0x2C00` and drop the
`0x8` prefix (`FUN_80007f04` is `0x0800AB04`). Addresses in
`firmware/RE/bootloader/bootloader_protocol_notes.md` came from an import
at `0x08000000` and are `0x2C00` too low as well.

Images captured from the updater (`firmware/RE/firmware dumps/upgrade_*.bin`)
are still fwcrypt-encrypted past offset 0x800. Decrypt them first:

```sh
python3 firmware/scripts/fwcrypt_io.py --infile "firmware/RE/firmware dumps/upgrade_reconstructed.bin" --outfile /tmp/upgrade.bin
ghidra/analyze.sh /tmp/upgrade.bin
```

## Scripts (`ghidra/scripts`, Java)

| Script | Stage | What it does |
|---|---|---|
| `AT32Setup.java` | pre-analysis | Read-only flash block, SRAM/boot ROM/XMC/SCS and all AT32 peripheral blocks (volatile), vector table typed and handlers created with CMSIS names |
| `ApplySymbols.java` | post-analysis | Applies names and comments from a symbols CSV |
| `ExportDecompiled.java` | post-analysis | Decompiles every function into one C file plus an index CSV |
| `ExportSymbols.java` | any | Exports user-defined names/comments to a symbols CSV, optionally rebasing addresses |

Jython is no longer bundled with Ghidra 12, so these replace
`firmware/scripts/setup-artery-cortex-m-memory-map.py` for headless use.

## Symbols

`symbols/rt950_v0.18.csv` holds the 48 names from the old Ghidra project
(rebased from 0x80000000) plus the remaining entries and notes from
`docs/Function_Names.csv`. To save names you added in the GUI:

```sh
/opt/ghidra_12.1.4_PUBLIC/support/analyzeHeadless ghidra/projects RT950 \
  -process decrypted.bin -noanalysis -scriptPath ghidra/scripts \
  -postScript ExportSymbols.java ghidra/symbols/rt950_v0.18.csv
```
