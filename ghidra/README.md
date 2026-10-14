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

The application is linked at **0x08000000** (AT32F403A flash): the vector
table at offset 0 holds the initial SP `0x20017bb0` (96 KiB SRAM) and the
reset vector `0x080032a1`. The older project in
`firmware/RE/decrypted-firmware` imported the image at `0x80000000`, which
leaves every literal-pool pointer into flash unresolved; that is where the
`FUN_8000xxxx` names in older notes come from. Subtract `0x78000000` to get
the real address (`FUN_80007f04` is `0x08007f04`).

The reset handler stores `r0` into `0x2000da82` before anything else, so the
bootloader appears to pass a flag to the application.

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
