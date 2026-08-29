#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

# Same-process Metal -> Vulkan vid_restart regression gate for the three UI
# surfaces that previously diverged: gameplay HUD/crosshair, console and pause
# menu. The window is always 1280x720 logical; screenshots intentionally retain
# the native backing extent (1280x720 or Retina 2560x1440).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
# shellcheck source=tests/lib/wired_paths.sh
. "$SCRIPT_DIR/lib/wired_paths.sh"

ENGINE="${1:-${WIRED_BINARY:-}}"
if [ -z "${Q3DIR:-}" ] && [[ "$ENGINE" == *.app/Contents/MacOS/* ]]; then
  # Keep the renderer modules, MoltenVK and paks in the same app bundle as the
  # explicitly selected engine.  Falling back to WIRED_INSTALL here silently
  # mixed a repo-built executable with a stale /Applications renderer.
  # dirname(engine) is <bundle>/Contents/MacOS; two parents reach the .app.
  # Three parents resolve to /Applications and make CL_InitRef search the
  # nonexistent /Applications/Contents/MacOS renderer directory.
  Q3DIR="$(cd "$(dirname "$ENGINE")/../.." && pwd)"
else
  Q3DIR="${Q3DIR:-$WIRED_INSTALL}"
fi
OUT="${RENDERER_SWITCH_UI_OUT:-$REPO_ROOT/build/renderer-switch-ui}"
WATCHDOG="$SCRIPT_DIR/run-with-timeout.py"
PNG2RAW="${PNG2RAW:-$REPO_ROOT/tools/png2raw/png2raw}"

[ "$(uname -s)" = Darwin ] || { echo "SKIP: Metal -> Vulkan switch requires macOS"; exit 77; }
[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE"; exit 1; }
[ -x "$PNG2RAW" ] || { echo "FAIL: png2raw not executable: $PNG2RAW"; exit 1; }
for renderer in wired_metal_arm64.dylib wired_vulkan_arm64.dylib; do
  [ -f "$(dirname "$ENGINE")/$renderer" ] || { echo "FAIL: missing renderer: $renderer"; exit 1; }
done
if rg -n -P '^\s*(?!//)(?:(?!//).)*\b(?:rect|position\s+absolute)\b' \
  "$REPO_ROOT/modfiles/ui" --glob '*.wui' --glob '*.wmenu' --glob '*.whud'; then
  echo "FAIL: WiredUI source still contains viewport-position literals"
  exit 1
fi

mkdir -p "$OUT"
rm -f "$(dirname "$ENGINE")/layoutdump.jsonl"
RUN_ROOT="$(mktemp -d "$WIRED_TMP/q3now-renderer-switch-ui-XXXXXX")"
trap 'rm -rf "$RUN_ROOT"' EXIT
WIRED_TMP="$RUN_ROOT"
HOME_DIR="$(wired_isolated_home renderer-switch-ui)"
mkdir -p "$HOME_DIR/base/screenshots"
CFG="$HOME_DIR/base/renderer-switch-ui.cfg"
LOG="$OUT/renderer-switch-ui.log"

printf '%s\n' \
  'set r_atmosphericGPU 0' \
  'set r_flares 0' \
  'set r_lens 0' \
  'set r_halos 0' \
  'set r_sunRayIntensity 0' \
  'set cg_lensFlare 0' \
  'set cg_missileFlare 0' \
  'set hud default' \
  'map arena1' \
  'waitForMap' \
  'wait 90' \
  'cmd noclip' \
  'cmd setviewpos 1052 1432 50 135' \
  'set cg_crosshairHealth 0' \
  'set cg_crosshairColor "1 0 1"' \
  'set cg_crosshairAlpha 1' \
  'wait 90' \
  'set r_layoutDump 1' \
  'wait 1' \
  'screenshot switch_metal_game png silent' \
  'set r_layoutDump 0' \
	'cmd give all' \
	'wait 5' \
	'weapnext' \
	'wait 1' \
	'screenshot switch_metal_weapon png silent' \
	'wait 90' \
  'wait 5' \
  'set cg_crosshairAlpha 0' \
  'wait 1' \
  'screenshot switch_metal_crosshair_off png silent' \
  'wait 5' \
  'set cg_crosshairAlpha 1' \
  'wait 1' \
  'toggleconsole' \
  'waitms 750' \
  'screenshot switch_metal_console png silent' \
  'wait 5' \
  'toggleconsole' \
  'wui_push ingame' \
  'set r_layoutDump 1' \
  'waitms 500' \
  'screenshot switch_metal_menu png silent' \
  'set r_layoutDump 0' \
  'wait 5' \
  'wui_test_keydown 27' \
  'wait 30' \
  'set cl_renderer vulkan' \
  'vid_restart' \
  'wait 240' \
  'waitForMap' \
  'cmd setviewpos 1052 1432 50 135' \
  'wait 90' \
  'set r_layoutDump 1' \
  'wait 1' \
  'screenshot switch_vulkan_game png silent' \
  'set r_layoutDump 0' \
	'weapnext' \
	'wait 1' \
	'screenshot switch_vulkan_weapon png silent' \
	'wait 90' \
  'wait 5' \
  'set cg_crosshairAlpha 0' \
  'wait 1' \
  'screenshot switch_vulkan_crosshair_off png silent' \
  'wait 5' \
  'set cg_crosshairAlpha 1' \
  'wait 1' \
  'toggleconsole' \
  'waitms 750' \
  'screenshot switch_vulkan_console png silent' \
  'wait 5' \
  'toggleconsole' \
  'wui_push ingame' \
  'set r_layoutDump 1' \
  'waitms 500' \
  'screenshot switch_vulkan_menu png silent' \
  'set r_layoutDump 0' \
  'wait 30' \
  'quit' >"$CFG"

python3 "$WATCHDOG" --timeout 150 --kill-after 15 \
  --cwd "$(dirname "$ENGINE")" --stdout "$LOG" -- \
  "$ENGINE" \
  +set fs_installpath "$Q3DIR" \
  +set fs_homepath "$HOME_DIR" \
  +set cl_renderer metal \
  +set net_port 0 \
  +set com_automated 1 \
  +set com_noHardReboot 1 \
  +set s_initsound 0 \
  +set sv_pure 0 \
  +set sv_cheats 1 \
  +set vm_game 0 \
  +set vm_cgame 0 \
  +set r_fullscreen 0 \
  +set r_mode -1 \
  +set r_customwidth 1280 \
  +set r_customheight 720 \
  +set r_renderScale 1 \
  +set r_renderWidth 720 \
  +set r_renderHeight 450 \
  +set r_hdr 0 \
  +set r_hdrDisplay 0 \
  +set r_hdrAutoExposure 0 \
  +set con_notifytime 0 \
  +log renderer.init info \
  +exec renderer-switch-ui.cfg

for backend in metal vulkan; do
  for surface in game weapon crosshair_off console menu; do
    shot="$HOME_DIR/base/screenshots/switch_${backend}_${surface}.png"
    [ -s "$shot" ] || { echo "FAIL: missing $backend/$surface screenshot"; exit 1; }
    cp "$shot" "$OUT/${backend}-${surface}.png"
  done
done

[ -s "$(dirname "$ENGINE")/layoutdump.jsonl" ] || {
  echo "FAIL: missing Metal/Vulkan layout dump"; exit 1;
}
cp "$(dirname "$ENGINE")/layoutdump.jsonl" "$OUT/layoutdump.jsonl"

python3 - "$OUT" "$PNG2RAW" <<'PY'
import json,statistics,struct,subprocess,sys
from pathlib import Path
root=Path(sys.argv[1])
decoder=sys.argv[2]
extents=set()
for path in sorted(root.glob("*.png")):
    data=path.read_bytes()[:24]
    if len(data)!=24 or data[:8]!=b"\x89PNG\r\n\x1a\n" or data[12:16]!=b"IHDR":
        raise SystemExit(f"FAIL: invalid PNG: {path}")
    extent=struct.unpack(">II",data[16:24])
    if extent[0] % 1280 or extent[1] % 720 or extent[0]//1280 != extent[1]//720:
        raise SystemExit(f"FAIL: non-presentation screenshot extent {extent}: {path}")
    extents.add(extent)
if len(extents)!=1:
    raise SystemExit(f"FAIL: renderer/surface screenshot extents differ: {sorted(extents)}")
width,height=next(iter(extents))
print(f"  PASS screenshot presentation extent: {width}x{height}")

# A screenshot-content check alone can miss a legacy draw routine masking a
# collapsed flex parent.  Require the authored corner grid itself to resolve
# positive, in-bounds bottom-left and bottom-right boxes in both game captures.
frames={}
for line in (root/"layoutdump.jsonl").read_text().splitlines():
    entry=json.loads(line)
    if entry.get("menu") != "classic":
        continue
    frames.setdefault(entry["frame"], {})[entry["region"]]=entry
required=("hud_corner_grid","hud_active_health_panel",
          "hud_active_armor_panel","hud_ammo_readout",
          "hud_active_crosshair","hud_gametime","hud_fps","hud_netstats",
          "hud_holdables","hud_weapon_carousel","hud_msgqueue",
          "hud_reward_icon","hud_reward_count","hud_warmup")
game_frames=[regions for regions in frames.values()
             if all(region in regions for region in required)]
if len(game_frames) != 2:
    raise SystemExit(f"FAIL: expected two backend HUD layout frames, got {len(game_frames)}")
for index,regions in enumerate(game_frames,1):
    grid=regions["hud_corner_grid"]
    health=regions["hud_active_health_panel"]
    armor=regions["hud_active_armor_panel"]
    ammo=regions["hud_ammo_readout"]
    crosshair=regions["hud_active_crosshair"]
    gametime=regions["hud_gametime"]
    fps=regions["hud_fps"]
    netstats=regions["hud_netstats"]
    holdables=regions["hud_holdables"]
    carousel=regions["hud_weapon_carousel"]
    gw,gh=grid["w"],grid["h"]
    if abs(gw-width)>1 or abs(gh-height)>1:
        raise SystemExit(f"FAIL: HUD grid extent {gw}x{gh} differs from presentation {width}x{height}")
    for name,item in (("health",health),("armor",armor),("ammo",ammo),
                      ("crosshair",crosshair),("gametime",gametime),
                      ("fps",fps),("netstats",netstats),("holdables",holdables),
                      ("carousel",carousel),("msgqueue",regions["hud_msgqueue"]),
                      ("reward_icon",regions["hud_reward_icon"]),
                      ("reward_count",regions["hud_reward_count"]),
                      ("warmup",regions["hud_warmup"])):
        if item["w"] <= 0 or item["h"] <= 0:
            raise SystemExit(f"FAIL: backend HUD frame {index} has collapsed {name} box: {item}")
        if item["x"] < -1 or item["y"] < -1 or item["x"]+item["w"] > gw+1 or item["y"]+item["h"] > gh+1:
            raise SystemExit(f"FAIL: backend HUD frame {index} has out-of-bounds {name} box: {item}")
        if name in ("health","armor","ammo") and item["y"] < gh*.65:
            raise SystemExit(f"FAIL: backend HUD frame {index} {name} is not in the lower HUD region: {item}")
    if health["x"] > gw*.08 or armor["x"] > gw*.30 or ammo["x"]+ammo["w"] < gw*.92:
        raise SystemExit(f"FAIL: backend HUD frame {index} lower HUD blocks do not occupy the authored corners")
    if armor["x"] <= health["x"] or ammo["x"] <= armor["x"]:
        raise SystemExit(f"FAIL: backend HUD frame {index} health/armor/ammo ordering drifted")
    if abs((crosshair["x"]+crosshair["w"]*.5)-gw*.5)>gw*.03 or abs((crosshair["y"]+crosshair["h"]*.5)-gh*.5)>gh*.06:
        raise SystemExit(f"FAIL: backend HUD frame {index} crosshair carrier is off-centre: {crosshair}")
    if gametime["y"]>gh*.1 or abs((gametime["x"]+gametime["w"]*.5)-gw*.5)>gw*.08:
        raise SystemExit(f"FAIL: backend HUD frame {index} gametime is not top-centred: {gametime}")
    if fps["x"]<gw*.75 or netstats["x"]<gw*.75 or fps["y"]>gh*.1 or netstats["y"]>gh*.1:
        raise SystemExit(f"FAIL: backend HUD frame {index} telemetry is not top-right")
    if holdables["x"]<gw*.85 or not gh*.25<holdables["y"]<gh*.75:
        raise SystemExit(f"FAIL: backend HUD frame {index} holdables are not middle-right: {holdables}")
    if not gw*.35<carousel["x"]<gw*.65 or carousel["y"]<gh*.65:
        raise SystemExit(f"FAIL: backend HUD frame {index} weapon carousel is misplaced: {carousel}")
print("  PASS Metal/Vulkan native HUD flex geometry: corners + aim + telemetry + transient regions")

for backend in ("metal","vulkan"):
    path=root/f"{backend}-game.png"
    weapon_path=root/f"{backend}-weapon.png"
    off_path=root/f"{backend}-crosshair_off.png"
    raw=subprocess.check_output([decoder,str(path)])
    weapon_raw=subprocess.check_output([decoder,str(weapon_path)])
    off=subprocess.check_output([decoder,str(off_path)])
    # Detect the neutral-bright authored reticle directly.  Comparing two
    # gameplay frames is not reliable here: animated lens flares and temporal
    # world effects can cross the centre ROI between queued screenshots.
    # The off frame remains the negative control, so this still proves that
    # cg_drawCrosshair owns the detected centre mark.
    scale=width//1280
    centre_x,centre_y=width//2,height//2
    radius=32*scale
    hits=[]
    off_hits=[]
    for y in range(centre_y-radius,centre_y+radius+1):
        for x in range(centre_x-radius,centre_x+radius+1):
            i=(y*width+x)*3
            rgb=raw[i:i+3]
            off_rgb=off[i:i+3]
            if min(rgb)>=150 and max(rgb)-min(rgb)<=45:
                hits.append((x,y))
            if min(off_rgb)>=150 and max(off_rgb)-min(off_rgb)<=45:
                off_hits.append((x,y))
    if not hits:
        raise SystemExit(f"FAIL: {backend} crosshair-on frame has no neutral-bright reticle near aim centre")
    if len(off_hits) > 2*scale*scale:
        raise SystemExit(
            f"FAIL: {backend} crosshair-off frame retains {len(off_hits)} neutral-bright centre pixels")
    span=max(max(x for x,_ in hits)-min(x for x,_ in hits),
             max(y for _,y in hits)-min(y for _,y in hits))+1
    if span < 14*scale:
        raise SystemExit(f"FAIL: {backend} crosshair span {span}px is below DPI-scaled minimum {14*scale}px")
    if span > 24*scale:
        raise SystemExit(f"FAIL: {backend} crosshair span {span}px exceeds DPI-scaled maximum {24*scale}px")
    print(f"  PASS {backend} crosshair evidence: {len(hits)} pixels, span={span}px, dpi={scale}x")

    # The carousel carrier can resolve correctly while its native HUD element
    # paints nothing. Compare the immediate post-weapnext frame with the later
    # crosshair-off frame (captured after the carousel's fade window) in the
    # authored bottom-centre region. A vid_restart itself may refresh the
    # weapon-select timestamp, so the earlier gameplay frame is not a reliable
    # negative control on the Vulkan half of this same-process test.
    changed=0
    x0,x1=width*35//100,width*65//100
    y0,y1=height*65//100,height*92//100
    for y in range(y0,y1):
        for x in range(x0,x1):
            i=(y*width+x)*3
            if max(abs(weapon_raw[i+c]-off[i+c]) for c in range(3)) > 48:
                changed += 1
    minimum_changed=240*scale*scale
    if changed < minimum_changed:
        raise SystemExit(
            f"FAIL: {backend} weapnext painted only {changed} changed pixels "
            f"in the weapon-carousel region (need {minimum_changed})")
    print(f"  PASS {backend} weapon carousel evidence: {changed} changed pixels")

    for surface in ("menu","console"):
        surface_path=root/f"{backend}-{surface}.png"
        pixels=subprocess.check_output([decoder,str(surface_path)])
        if surface == "menu":
            x0,x1=width*35//100,width*65//100
            y0,y1=height*10//100,height*90//100
            minimum_bright=0.005
            minimum_edges=0.005
        else:
            x0,x1=0,width
            y0,y1=0,height*90//100
            minimum_bright=0.02
            minimum_edges=0.02
        bright=edge=0
        total=(x1-x0)*(y1-y0)
        for y in range(y0,y1):
            for x in range(x0,x1):
                i=(y*width+x)*3
                luminance=(pixels[i]*54+pixels[i+1]*183+pixels[i+2]*19)//256
                bright += luminance > 150
                if x+1 < x1:
                    edge += max(abs(pixels[i+c]-pixels[i+3+c]) for c in range(3)) > 32
        bright_ratio=bright/total
        edge_ratio=edge/total
        if bright_ratio < minimum_bright or edge_ratio < minimum_edges:
            raise SystemExit(
                f"FAIL: {backend} {surface} lacks readable UI content "
                f"(bright={bright_ratio:.3%}, edges={edge_ratio:.3%})")
        print(f"  PASS {backend} {surface} content: bright={bright_ratio:.2%}, edges={edge_ratio:.2%}")

# The first pause-menu button is an opaque `$lineBright` (#3a322a) fill.
# Sampling a text-free interior patch catches both a frontend+shader double
# sRGB decode and a backend that forgets to decode authored UI colour.
button_colors={}
for backend in ("metal","vulkan"):
    pixels=subprocess.check_output([decoder,str(root/f"{backend}-menu.png")])
    channels=([],[],[])
    for y in range(height*27//100,height*30//100):
        for x in range(width*38//100,width*43//100):
            i=(y*width+x)*3
            for channel in range(3):
                channels[channel].append(pixels[i+channel])
    button_colors[backend]=tuple(round(statistics.median(values)) for values in channels)
authored=(0x3a,0x32,0x2a)
for backend,color in button_colors.items():
    if max(abs(color[channel]-authored[channel]) for channel in range(3)) > 4:
        raise SystemExit(f"FAIL: {backend} opaque UI fill {color} differs from authored sRGB {authored}")
if max(abs(button_colors["metal"][channel]-button_colors["vulkan"][channel])
       for channel in range(3)) > 3:
    raise SystemExit(f"FAIL: Metal/Vulkan opaque UI fill mismatch: {button_colors}")
print(f"  PASS authored UI color parity: {button_colors} vs lineBright={authored}")
PY

for required in \
  'Wired native Metal RAL: content receipt' \
  'Wired Vulkan RAL: content receipt' \
  'client-ui-resolution'; do
  grep -q "$required" "$LOG" || { echo "FAIL: missing runtime evidence: $required"; exit 1; }
done
if grep -Eiq 'VUID-|validation error|lifecycle failure|renderer .*failed to initialize|failed to initialize renderer|Sys_Error|\bFATAL\b|died on signal' "$LOG"; then
  echo "FAIL: forbidden runtime diagnostic"; grep -Ei 'VUID-|validation error|lifecycle failure|renderer .*failed to initialize|failed to initialize renderer|Sys_Error|\bFATAL\b|died on signal' "$LOG" | head -n 20; exit 1
fi

echo "PASS retained Metal -> Vulkan vid_restart UI evidence: $OUT"
