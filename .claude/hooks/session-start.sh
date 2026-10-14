#!/bin/bash
# Install headless Ghidra in Claude Code cloud sessions (see ghidra/README.md).
set -euo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
	exit 0
fi

# Run in the background: the ~570 MB download should not block session start.
# ghidra/analyze.sh waits for it via the install lock if it is still running.
echo '{"async": true, "asyncTimeout": 600000}'

"$CLAUDE_PROJECT_DIR/ghidra/setup-ghidra.sh"
echo "export GHIDRA_INSTALL_DIR=/opt/ghidra_12.1.4_PUBLIC" >> "$CLAUDE_ENV_FILE"
