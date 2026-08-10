#!/usr/bin/env bash
# brightness-probe.sh — capture PROBE log lines from a smoke run to correlate
# capture-time fade alpha against captured brightness. Just the Phase-1 part
# of the smoke (1 wired launch = 2 captures) suffices to probe.
set -u
PRODUCT_DIR="/c/Users/eser/wired/q3now-preview"
WIRED_DIR=$(pwd)/build/debug
RUN_LABEL="${1:-run}"

# Engine screenshots are PNG; decode to flat RGB via png2raw for the byte-mean.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PNG2RAW="${PNG2RAW:-$SCRIPT_DIR/../tools/png2raw/png2raw}"
if [ ! -x "$PNG2RAW" ] && [ -x "$PNG2RAW.exe" ]; then PNG2RAW="$PNG2RAW.exe"; fi
if [ ! -x "$PNG2RAW" ]; then echo "FAIL: png2raw not found at $PNG2RAW (build: make png2raw)"; exit 1; fi

rm -f "$PRODUCT_DIR/base/pipelinecache_v1_vulkan.bin"
find "$PRODUCT_DIR/base/screenshots" -name '*.png' -delete 2>/dev/null || true

PRE_TS=$(date +%s)
cd "$WIRED_DIR"
timeout 240 ./wired.x64.exe \
    +set sv_pure 0 +set sv_cheats 1 +set r_brightness 1 \
    +set vm_game 0 +set vm_cgame 0 +set log_file_mode overwrite_synced \
    +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set r_fullscreen 0 \
    +map arena1 +waitForMap +wait 60 +setviewpos 1052 1432 50 135 +wait 120 +screenshot \
    +wait 30 +map arena17 +waitForMap +wait 60 +setviewpos 488 1096 378 -90 +wait 120 +screenshot \
    +wait 30 +quit \
    > "/tmp/probe-${RUN_LABEL}-wired.log" 2>&1
cd /c/Users/eser/projects/eser/q3now

JSONL="$PRODUCT_DIR/qconsole.jsonl"

echo "==== captures from this run ===="
ls -t "$PRODUCT_DIR/base/screenshots/"*.png 2>/dev/null | head -2 | while read shot; do
    hist=$("$PNG2RAW" "$shot" \
        | od -A n -t u1 -v -w16 \
        | awk 'BEGIN{tot=0;sum=0} { for (i=1;i<=NF;i++) { tot++; sum+=$i } } END{ if(tot>0) printf "mean=%.2f", sum/tot }')
    ts=$(stat -c%y "$shot" | awk '{print $2}' | cut -d. -f1)
    echo "  $ts  $(basename $shot)  $hist"
done

echo
echo "==== PROBE-PIPE lines (renderer spec constants, once per program) ===="
grep "PROBE-PIPE" "$JSONL" | head -10

echo
echo "==== PROBE-FADE just BEFORE each PROBE-CAP marker ===="
# Find PROBE-CAP line numbers and dump the 3 PROBE-FADE lines immediately preceding each
mapfile -t CAP_LINES < <(grep -n "PROBE-CAP" "$JSONL" | cut -d: -f1)
for capln in "${CAP_LINES[@]}"; do
    echo "--- around screenshot at line $capln ---"
    awk -v cap="$capln" 'NR>=cap-3 && NR<=cap { print NR": "$0 }' "$JSONL" | grep -E "PROBE-FADE|PROBE-CAP"
done
