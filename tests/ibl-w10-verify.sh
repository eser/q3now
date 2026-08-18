#!/usr/bin/env bash
# IBL base-pass specular-only verify.
#
# The base-pass IBL (gen_frag, worldspawn pbrMap) changed from full
# split-sum ambient (kD*diffuseIBL + specularIBL) to specular-only: the
# lightmap already provides baked diffuse ambient, so the diffuse-IBL half was
# double-counting it. This verifies the result at runtime with a deterministic
# pinned r_pbr 0-vs-1 A/B on a worldspawn floor REMAPPED to a pbrMap material
# (textures/pbr_test/pbr_test — committed in modfiles/scripts/q3now.shader),
# which routes the surface through the USE_IBL gen base-pass pipeline.
#
# This is the BASE-pass (worldspawn) IBL, distinct from the older
# ibl-pbr-verify.sh which exercised the now-removed light_frag (per-dlight)
# IBL via a weapon view-model. This change does not touch light_frag.
#
# Determinism: r_pinShaderTime + r_hdrAutoExposure 0 (pinned A/B recipe).
# Config isolation: the only CVAR_ARCHIVE cvars touched are r_pbr (default 0,
# CVAR_NODEFAULT — restored to 0) and r_hdrAutoExposure (restored to its
# pre-run value passed in via HDR_RESTORE). r_pinShaderTime / r_brightness are
# non-archive. No fs_homepath override, no pak handling — uses the real home
# exactly like the committed ibl-pbr-verify.sh.
#
# Usage:
#   tests/ibl-w10-verify.sh discover                 # dump shaderlist + spawn shot
#   tests/ibl-w10-verify.sh capture <fromShader> <viewpos x y z yaw>
set -u
. "$(cd "$(dirname "$0")" && pwd)/lib/wired_paths.sh"

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# Bare name first, .exe second — the same probe every sibling script uses. Hardcoding
# .exe made this check resolvable only on Windows.
PNG2RAW="${PNG2RAW:-$REPO_ROOT/tools/png2raw/png2raw}"
[ -x "$PNG2RAW" ] || { [ -x "$PNG2RAW.exe" ] && PNG2RAW="$PNG2RAW.exe"; }
HOME_DIR="$WIRED_HOME"
BASE_DIR="$HOME_DIR/base"
SHOT_DIR="$BASE_DIR/screenshots"
JSONL="$HOME_DIR/qconsole.jsonl"
FRAME_W=1280
FRAME_H=720
MAP="${MAP:-arena1}"
HDR_RESTORE="${HDR_RESTORE:-1}"   # value to restore r_hdrAutoExposure to on quit

mkdir -p "$SHOT_DIR"

run_game() {
    # $1 = extra engine args after the pinned render setup.
    ( cd "$REPO_ROOT" && make run-game DEV=1 \
        EXTRA_ARGS="+set r_mode -1 +set r_customwidth $FRAME_W +set r_customheight $FRAME_H +set r_fullscreen 0 +set com_automated 1 +set r_brightness 1 +set r_pinShaderTime 1.0 +set r_hdrAutoExposure 0 +log renderer.shaders info $1" \
        >"$WIRED_TMP/ibl-w10-$2.log" 2>&1 || true )
}

cmd="${1:-}"
shift || true

case "$cmd" in
discover)
    rm -f "$JSONL"
    # Load map, wait for CA_ACTIVE, dump shaderlist into the persistent qconsole,
    # snap a spawn screenshot to orient the viewpoint, restore HDR, quit.
    write="$BASE_DIR/w10disc.cfg"
    rm -f "$HOME_DIR/w10shaders.txt"
    cat > "$write" <<CFG
shaderlist
wait 30
dumpConsole w10shaders.txt
wait 10
screenshot w10_spawn png silent
wait 10
set r_hdrAutoExposure $HDR_RESTORE
wait 5
quit
CFG
    run_game "+map $MAP +waitForMap +wait 80 +exec w10disc.cfg" discover
    rm -f "$write"
    echo "== shaderlist (floor/wall worldspawn candidates) =="
    # dumpConsole captures all channels; pull worldspawn texture shader names.
    if [ -s "$HOME_DIR/w10shaders.txt" ]; then
        grep -oE 'textures/[A-Za-z0-9_/.-]+' "$HOME_DIR/w10shaders.txt" \
          | grep -ivE 'sky|liquid|flame|lava|fog|effect|sprite|trigger|hint|caulk|nodraw|clip' \
          | sort -u | head -80
    else
        echo "(w10shaders.txt not produced)"
    fi
    echo "== spawn shot =="
    ls -la "$SHOT_DIR"/w10_spawn.png 2>/dev/null
    ;;
nullab)
    # NULL A/B: identical determinism settings, but r_pbr held at 0 for BOTH shots
    # (no toggle). Measures the engine's irreducible per-frame nondeterminism
    # (SMAA/dither/jitter) at this viewpoint — the noise floor the real A/B's
    # delta must beat. Same 8 floor remaps + same inter-shot gap as `capture`.
    x="$1"; y="$2"; z="$3"; yaw="$4"
    rm -f "$SHOT_DIR"/w10_n0.png "$SHOT_DIR"/w10_n1.png
    write="$BASE_DIR/w10null.cfg"
    {
      echo "set cg_draw2D 0"; echo "set cg_drawGun 0"; echo "set r_hdrAutoExposure 0"
      echo "set r_dynamiclight 0"; echo "set r_pbr 0"
      for fl in \
        textures/gothic_floor/largerblock3b textures/gothic_floor/largerblock3b_ow \
        textures/gothic_floor/center2trn textures/gothic_floor/blocks17floor2 \
        textures/gothic_floor/xstairtop4 textures/gothic_floor/xstairtop4bbrn \
        textures/gothic_floor/metalbridge06 textures/gothic_block/largerblock3blood ; do
        echo "remapshader $fl textures/pbr_test/pbr_test"
      done
      cat <<CFG
wait 10
setviewpos $x $y $z $yaw
wait 30
screenshot w10_n0 png silent
wait 1
screenshot w10_n1 png silent
wait 3
set r_hdrAutoExposure $HDR_RESTORE
set cg_draw2D 1
set cg_drawGun 1
wait 5
quit
CFG
    } > "$write"
    run_game "+map $MAP +waitForMap +wait 80 +exec w10null.cfg" nullab
    rm -f "$write"
    echo "== null-A/B captures (both r_pbr 0) =="
    ls -la "$SHOT_DIR"/w10_n0.png "$SHOT_DIR"/w10_n1.png 2>/dev/null
    ;;
capture)
    # $1 x $2 y $3 z $4 yaw
    x="$1"; y="$2"; z="$3"; yaw="$4"
    rm -f "$SHOT_DIR"/w10_pbr0.png "$SHOT_DIR"/w10_pbr1.png
    write="$BASE_DIR/w10cap.cfg"
    # Remap every arena1 gothic_floor shader (and a couple of central candidates)
    # to the pbrMap test material so whatever the viewpoint frames carries a
    # pbrMap and routes through the USE_IBL base-pass pipeline.
    {
      echo "// remap all floor shaders to the pbrMap test material"
      for fl in \
        textures/gothic_floor/largerblock3b \
        textures/gothic_floor/largerblock3b_ow \
        textures/gothic_floor/center2trn \
        textures/gothic_floor/blocks17floor2 \
        textures/gothic_floor/xstairtop4 \
        textures/gothic_floor/xstairtop4bbrn \
        textures/gothic_floor/metalbridge06 \
        textures/gothic_block/largerblock3blood ; do
        echo "remapshader $fl textures/pbr_test/pbr_test"
      done
      cat <<CFG
// kill HUD + weapon + drift sources; pitch view down so the frame is almost
// entirely the remapped floor (no sky / no animated entities in view).
set cg_draw2D 0
set cg_drawGun 0
set cg_thirdPerson 0
set r_hdrAutoExposure 0
set r_dynamiclight 0
wait 10
setviewpos $x $y $z $yaw
wait 30
set r_pbr 0
wait 3
screenshot w10_pbr0 png silent
wait 1
set r_pbr 1
wait 1
screenshot w10_pbr1 png silent
wait 3
set r_pbr 0
set r_hdrAutoExposure $HDR_RESTORE
set cg_draw2D 1
set cg_drawGun 1
wait 5
quit
CFG
    } > "$write"
    run_game "+map $MAP +waitForMap +wait 80 +exec w10cap.cfg" capture
    rm -f "$write"
    echo "== remap match check (renderer.shaders WARN = not found) =="
    grep '"cat":"renderer.shaders"' "$JSONL" 2>/dev/null | grep -i 'RemapShader' | grep -i 'not found' | head
    echo "== captures =="
    ls -la "$SHOT_DIR"/w10_pbr0.png "$SHOT_DIR"/w10_pbr1.png 2>/dev/null
    ;;
profile)
    # GPU frametime profile: r_pbr 0 vs 1 at a fixed pbrMap-slab viewpoint.
    # r_gpuSpeeds 1 = 200-frame averages on the renderer.timing channel. Pinned
    # cam + shader-time + dlight-off for determinism. We hold each r_pbr value
    # long enough for at least one 200f window to flush, then read the avg lines.
    x="$1"; y="$2"; z="$3"; yaw="$4"
    write="$BASE_DIR/w10prof.cfg"
    {
      echo "set cg_draw2D 0"; echo "set cg_drawGun 0"; echo "set r_hdrAutoExposure 0"
      echo "set r_dynamiclight 0"
      for fl in \
        textures/gothic_floor/largerblock3b textures/gothic_floor/largerblock3b_ow \
        textures/gothic_floor/center2trn textures/gothic_floor/blocks17floor2 \
        textures/gothic_floor/xstairtop4 textures/gothic_floor/xstairtop4bbrn \
        textures/gothic_floor/metalbridge06 textures/gothic_block/largerblock3blood ; do
        echo "remapshader $fl textures/pbr_test/pbr_test"
      done
      cat <<CFG
setviewpos $x $y $z $yaw
wait 20
set r_pbr 0
set r_gpuSpeeds 1
wait 260
echo PROFILE_PBR0_DONE
wait 5
set r_pbr 1
wait 260
echo PROFILE_PBR1_DONE
wait 5
set r_gpuSpeeds 0
set r_pbr 0
set r_hdrAutoExposure $HDR_RESTORE
set r_dynamiclight 1
set cg_draw2D 1
set cg_drawGun 1
wait 5
quit
CFG
    } > "$write"
    # Enable renderer.timing debug so the gpu-avg lines reach the jsonl.
    rm -f "$JSONL"
    ( cd "$REPO_ROOT" && make run-game DEV=1 \
        EXTRA_ARGS="+set r_mode -1 +set r_customwidth $FRAME_W +set r_customheight $FRAME_H +set r_fullscreen 0 +set com_automated 1 +set r_brightness 1 +set r_pinShaderTime 1.0 +set r_hdrAutoExposure 0 +log renderer.shaders info +log renderer.timing debug +map $MAP +waitForMap +wait 80 +exec w10prof.cfg" \
        >"$WIRED_TMP/ibl-w10-profile.log" 2>&1 || true )
    rm -f "$write"
    echo "== GPU 200f-avg lines (renderer.timing) =="
    grep -E '"cat":"renderer.timing"' "$JSONL" 2>/dev/null \
      | grep -oE '"msg":"[^"]*"' | sed -E 's/"msg":"//; s/\\n"$//; s/"$//' \
      | grep -E 'avg, ms|PROFILE_PBR'
    grep -E 'PROFILE_PBR' "$JSONL" 2>/dev/null | grep -oE 'PROFILE_PBR[A-Z0-9_]+' || true
    ;;
scout)
    # Sweep several viewpoints in ONE launch (HUD/gun/dlight off, floors remapped,
    # r_pbr 1 so the pbr_test floor is obvious) and screenshot each, to find the
    # viewpoint that best frames the remapped floor for the final A/B.
    rm -f "$SHOT_DIR"/w10_scout_*.png
    write="$BASE_DIR/w10scout.cfg"
    {
      echo "set cg_draw2D 0"; echo "set cg_drawGun 0"; echo "set r_hdrAutoExposure 0"
      echo "set r_dynamiclight 0"; echo "set r_pbr 1"
      for fl in \
        textures/gothic_floor/largerblock3b textures/gothic_floor/largerblock3b_ow \
        textures/gothic_floor/center2trn textures/gothic_floor/blocks17floor2 \
        textures/gothic_floor/xstairtop4 textures/gothic_floor/xstairtop4bbrn \
        textures/gothic_floor/metalbridge06 textures/gothic_block/largerblock3blood ; do
        echo "remapshader $fl textures/pbr_test/pbr_test"
      done
      # viewpoint sweep: vary position + yaw across the arena
      i=0
      for vp in \
        "1052 1432 90 135" "1052 1432 90 -45" "1052 1432 200 135" \
        "500 500 90 0" "500 500 90 180" "0 0 120 0" \
        "0 0 120 90" "768 768 100 -135" ; do
        echo "setviewpos $vp"; echo "wait 12"; echo "screenshot w10_scout_$i png silent"; echo "wait 4"
        i=$((i+1))
      done
      echo "set r_pbr 0"; echo "set r_hdrAutoExposure $HDR_RESTORE"; echo "set cg_draw2D 1"; echo "set cg_drawGun 1"
      echo "wait 5"; echo "quit"
    } > "$write"
    run_game "+map $MAP +waitForMap +wait 80 +exec w10scout.cfg" scout
    rm -f "$write"
    echo "== scout shots =="
    ls -la "$SHOT_DIR"/w10_scout_*.png 2>/dev/null
    ;;
*)
    echo "usage: $0 discover | scout | capture <x> <y> <z> <yaw>"
    exit 2
    ;;
esac
