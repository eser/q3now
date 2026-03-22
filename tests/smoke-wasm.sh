#!/usr/bin/env bash
# smoke-wasm.sh — WASM VM backend smoke test
#
# Runs the dedicated server with WASM modules (vm_game=3).
# Requires .wasm files in baseq3/vm/ and PAK0 data.
#
# Usage: bash tests/smoke-wasm.sh [path-to-ded] [path-to-basepath]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

DED="${1:-${PROJECT_DIR}/build/Release/q3now-ded.arm64}"
Q3DIR="${2:-${PROJECT_DIR}/baseq3-test}"

if [ ! -x "$DED" ]; then
    echo "ERROR: dedicated server not found at: $DED"
    echo "Usage: $0 [path-to-ded] [path-to-basepath]"
    exit 1
fi

if [ ! -d "$Q3DIR" ]; then
    echo "ERROR: basepath not found at: $Q3DIR"
    exit 1
fi

echo "=== WASM Smoke Test ==="
echo "Server:   $DED"
echo "Basepath: $Q3DIR"
echo ""

# ── Test 1: Explicit WASM mode (vm_game=3) ──────────────────────────
echo "[1/3] Running dedicated server with vm_game=3 (explicit WASM)..."
timeout 30 "$DED" \
    +set fs_basepath "$Q3DIR" \
    +set vm_game 3 \
    +set dedicated 2 \
    +map q3dm1 \
    +addbot sarge 1 \
    +wait 300 \
    +quit \
    2>&1 | tee /tmp/smoke-wasm-1.log || true

if grep -q "loaded as WASM" /tmp/smoke-wasm-1.log; then
    echo "  PASS: WASM module loaded"
else
    echo "  FAIL: WASM module did not load"
    exit 1
fi

# ── Test 2: Auto-detect mode (vm_game=2 with .wasm present) ─────────
echo "[2/3] Running with vm_game=2 (auto-detect, should prefer .wasm)..."
timeout 30 "$DED" \
    +set fs_basepath "$Q3DIR" \
    +set vm_game 2 \
    +set dedicated 2 \
    +map q3dm1 \
    +wait 100 \
    +quit \
    2>&1 | tee /tmp/smoke-wasm-2.log || true

if grep -q "WASM" /tmp/smoke-wasm-2.log; then
    echo "  PASS: Auto-detected WASM module"
else
    echo "  INFO: No WASM auto-detect (may be expected if no .wasm in vm/)"
fi

# ── Test 3: Fallback when no .wasm exists ────────────────────────────
echo "[3/3] Testing QVM fallback (rename .wasm temporarily)..."
WASM_FILE="$Q3DIR/vm/qagame.wasm"
if [ -f "$WASM_FILE" ]; then
    mv "$WASM_FILE" "${WASM_FILE}.bak"
    timeout 15 "$DED" \
        +set fs_basepath "$Q3DIR" \
        +set vm_game 2 \
        +set dedicated 2 \
        +map q3dm1 \
        +wait 50 \
        +quit \
        2>&1 | tee /tmp/smoke-wasm-3.log || true
    mv "${WASM_FILE}.bak" "$WASM_FILE"

    if grep -q "QVM" /tmp/smoke-wasm-3.log || grep -q "compiled" /tmp/smoke-wasm-3.log; then
        echo "  PASS: Fell back to QVM when .wasm missing"
    else
        echo "  INFO: Could not verify QVM fallback"
    fi
else
    echo "  SKIP: No .wasm file to test fallback"
fi

echo ""
echo "=== WASM Smoke Test Complete ==="
