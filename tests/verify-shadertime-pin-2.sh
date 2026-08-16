#!/usr/bin/env bash
# c2-shadertime-pin-2 determinism check: 6 cold-cache captures × 5 viewpoints.
# Saves PNGs + reports md5/mean per (run, vp). Cleans up at end.
set -u
. "$(cd "$(dirname "$0")" && pwd)/lib/wired_paths.sh"
PRODUCT_DIR="${PRODUCT_DIR:-$WIRED_HOME}"
JSONL="$PRODUCT_DIR/qconsole.jsonl"
SS_DIR="$PRODUCT_DIR/base/screenshots"
WIRED="build/debug/wired.x64.exe"

# The engine writes PNG screenshots; decode to flat RGB via png2raw (the same
# tool the smoke harness uses) before feeding the byte-mean / per-tile math.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PNG2RAW="${PNG2RAW:-$SCRIPT_DIR/../tools/png2raw/png2raw}"
if [ ! -x "$PNG2RAW" ] && [ -x "$PNG2RAW.exe" ]; then PNG2RAW="$PNG2RAW.exe"; fi
if [ ! -x "$PNG2RAW" ]; then echo "FAIL: png2raw not found at $PNG2RAW (build: make png2raw)"; exit 1; fi
WIRED_DIR="$(dirname "$(realpath "$WIRED")")"
WIRED_NAME="$(basename "$WIRED")"

VPS=(
    "A_spawn  arena1   1052 1432 50  135"
    "B_high   arena1   1052 1432 300 0"
    "C_far    arena1    500  500 50  0"
    "D_spawn  arena17   488 1096 378 -90"
    "E_low    arena17   488 1096 200 -90"
)

OUTDIR=/tmp/shadertime-pin-2
mkdir -p "$OUTDIR"
rm -f "$OUTDIR"/*.png "$OUTDIR"/tiles_*.txt

capture() {
    local run="$1" vp_id="$2" map="$3" x="$4" y="$5" z="$6" yaw="$7"
    rm -f "$JSONL"
    rm -f "$PRODUCT_DIR/base/pipelinecache_v1_vulkan.bin"
    find "$SS_DIR" -name '*.png' -delete 2>/dev/null
    local pre=$(date +%s)
    (
        cd "$WIRED_DIR"
        timeout 180 "./$WIRED_NAME" \
            +set sv_pure 0 +set sv_cheats 1 +set r_brightness 1 \
            +set vm_game 0 +set vm_cgame 0 +set log_file_mode overwrite_synced \
            +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
            +set r_fullscreen 0 \
            +set r_pinShaderTime 1.0 \
            +map "$map" +waitForMap +wait 60 +setviewpos "$x" "$y" "$z" "$yaw" \
            +wait 120 +screenshot +wait 30 +quit \
            >/dev/null 2>&1
    )
    local shot; shot="$(find "$SS_DIR" -name '*.png' -newermt "@$pre" | sort | tail -1)"
    if [ ! -s "$shot" ]; then printf "  run%d vp=%-8s NO_CAPTURE\n" "$run" "$vp_id"; return; fi
    local mean md5
    mean=$("$PNG2RAW" "$shot" \
        | od -A n -t u1 -v -w16 \
        | awk 'BEGIN{tot=0;sum=0} { for (j=1;j<=NF;j++) { tot++; sum+=$j } } END{ printf "%.4f", sum/tot }')
    md5=$(md5sum "$shot" | awk '{print $1}')
    cp "$shot" "$OUTDIR/run${run}_${vp_id}.png"
    # per-tile means
    "$PNG2RAW" "$shot" \
      | od -A n -t u1 -v -w96 | awk '{print $1, $2, $3}' \
      | awk -v W=40 -v H=720 -v GW=8 -v GH=8 '
            BEGIN { TW=W/GW; TH=H/GH; for(i=0;i<GW*GH;i++){n[i]=0;s[i]=0} }
            NF>=3 {
                idx=NR-1; r=int(idx/W); c=idx%W
                tr=int(r/TH); if(tr>=GH) tr=GH-1
                tc=int(c/TW); if(tc>=GW) tc=GW-1
                ti=tr*GW+tc; n[ti]++; s[ti]+=$1+$2+$3
            }
            END { for(i=0;i<GW*GH;i++) printf "%d %.3f\n", i, (n[i]>0) ? s[i]/n[i]/3 : 0 }' \
      > "$OUTDIR/tiles_${run}_${vp_id}.txt"
    printf "  run%d vp=%-8s mean=%9s  md5=%s\n" "$run" "$vp_id" "$mean" "$md5"
}

echo "=== c2-shadertime-pin-2 determinism — 6 runs × 5 viewpoints ==="
for r in 1 2 3 4 5 6; do
    for entry in "${VPS[@]}"; do
        set -- $entry
        capture "$r" "$1" "$2" "$3" "$4" "$5" "$6"
    done
done

echo
echo "=== md5 grouping per viewpoint (bit-identical groups) ==="
for vp in A_spawn B_high C_far D_spawn E_low; do
    echo "--- $vp ---"
    md5sum "$OUTDIR"/run*_${vp}.png 2>/dev/null | awk '{print $1}' | sort | uniq -c | sort -rn
done

echo
echo "=== Top per-tile spread per viewpoint ==="
for vp in A_spawn B_high C_far D_spawn E_low; do
    echo "--- $vp ---"
    awk '
        FILENAME ~ /tiles_1_/ { m1[$1] = $2; next }
        FILENAME ~ /tiles_2_/ { m2[$1] = $2; next }
        FILENAME ~ /tiles_3_/ { m3[$1] = $2; next }
        FILENAME ~ /tiles_4_/ { m4[$1] = $2; next }
        FILENAME ~ /tiles_5_/ { m5[$1] = $2; next }
        FILENAME ~ /tiles_6_/ { m6[$1] = $2; next }
        END {
            for (t = 0; t < 64; t++) {
                a[1]=m1[t]; a[2]=m2[t]; a[3]=m3[t]; a[4]=m4[t]; a[5]=m5[t]; a[6]=m6[t]
                minv=999; maxv=-999
                for (i=1;i<=6;i++) { if (a[i]<minv) minv=a[i]; if (a[i]>maxv) maxv=a[i] }
                spread = maxv-minv
                if (spread > 1.0)
                    printf "  tile %2d  min=%.2f max=%.2f spread=%.2f  vals=[%.1f %.1f %.1f %.1f %.1f %.1f]\n",
                        t, minv, maxv, spread, a[1], a[2], a[3], a[4], a[5], a[6]
            }
        }
    ' "$OUTDIR"/tiles_*_${vp}.txt | sort -k 6 -nr | head -8
done
