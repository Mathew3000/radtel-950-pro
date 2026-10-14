#!/usr/bin/env bash
# Install Ghidra (headless use) into $GHIDRA_INSTALL_DIR if it is not there yet.
# Needs a JDK 21+ on PATH. Safe to run repeatedly.
set -euo pipefail

GHIDRA_VERSION="${GHIDRA_VERSION:-12.1.4}"
GHIDRA_DATE="${GHIDRA_DATE:-20260921}"
GHIDRA_SHA256="${GHIDRA_SHA256:-ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db}"
GHIDRA_INSTALL_DIR="${GHIDRA_INSTALL_DIR:-/opt/ghidra_${GHIDRA_VERSION}_PUBLIC}"

# The SessionStart hook and analyze.sh may both call this at once.
mkdir -p "$(dirname "$GHIDRA_INSTALL_DIR")"
exec 9>"$GHIDRA_INSTALL_DIR.lock"
flock 9

if [ -x "$GHIDRA_INSTALL_DIR/support/analyzeHeadless" ]; then
	echo "Ghidra already installed at $GHIDRA_INSTALL_DIR"
	exit 0
fi

java_major=$(java -version 2>&1 | awk -F'"' '/version/ {split($2, v, "."); print v[1]; exit}')
if [ -z "$java_major" ] || [ "$java_major" -lt 21 ]; then
	echo "Ghidra $GHIDRA_VERSION needs JDK 21+, found '${java_major:-none}'" >&2
	exit 1
fi

zip="ghidra_${GHIDRA_VERSION}_PUBLIC_${GHIDRA_DATE}.zip"
url="https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_${GHIDRA_VERSION}_build/$zip"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

echo "Downloading $url"
curl -fsSL --retry 4 -o "$tmp/$zip" "$url"
echo "$GHIDRA_SHA256  $tmp/$zip" | sha256sum -c -
unzip -q "$tmp/$zip" -d "$tmp/x"
mv "$tmp/x/ghidra_${GHIDRA_VERSION}_PUBLIC" "$GHIDRA_INSTALL_DIR"
echo "Installed Ghidra $GHIDRA_VERSION at $GHIDRA_INSTALL_DIR"
