#!/usr/bin/env bash
# Import an RT950 application image into a headless Ghidra project at the
# correct flash base, run auto-analysis, apply known symbols and export the
# decompiled C.
#
#   ghidra/analyze.sh [image.bin] [symbols.csv]
#
# Defaults: firmware/decrypted.bin (V0.18) with ghidra/symbols/rt950_v0.18.csv.
# Output: ghidra/out/<image>.c and ghidra/out/<image>.functions.csv.
# The Ghidra project goes to ghidra/projects/ (git-ignored); open it in the
# Ghidra GUI to keep working interactively. Re-running replaces the program.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
repo=$(dirname "$here")
image=$(realpath "${1:-$repo/firmware/decrypted.bin}")
symbols="${2:-}"
if [ -z "$symbols" ] && [ "$image" = "$(realpath "$repo/firmware/decrypted.bin")" ]; then
	symbols="$here/symbols/rt950_v0.18.csv"
fi

GHIDRA_INSTALL_DIR="${GHIDRA_INSTALL_DIR:-/opt/ghidra_12.1.4_PUBLIC}"
[ -x "$GHIDRA_INSTALL_DIR/support/analyzeHeadless" ] || "$here/setup-ghidra.sh"

name=$(basename "$image")
proj_dir="$here/projects"
out="$here/out"
mkdir -p "$proj_dir" "$out"

post=()
if [ -n "$symbols" ]; then
	post+=(-postScript ApplySymbols.java "$(realpath "$symbols")")
fi

"$GHIDRA_INSTALL_DIR/support/analyzeHeadless" "$proj_dir" RT950 \
	-import "$image" -overwrite \
	-loader BinaryLoader -loader-baseAddr 0x08000000 \
	-processor ARM:LE:32:Cortex -cspec default \
	-scriptPath "$here/scripts" \
	-preScript AT32Setup.java \
	"${post[@]}" \
	-postScript ExportDecompiled.java "$out/$name.c" "$out/$name.functions.csv" \
	-log "$out/$name.log" -scriptlog "$out/$name.script.log"

echo "Decompiled C: $out/$name.c"
