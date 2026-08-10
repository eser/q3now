#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# fps-perf-gate.sh -- deterministic real-gameplay FPS + bottleneck benchmark.
#
# A repeatable regression gate for render performance. Launches the engine AS-IS
# (real homepath — NO fs_*path override), loads a representative-load scene (map +
# dlights + several bots + effects) with a PINNED camera and PINNED frame time so
# the per-frame render work is identical every run, then collects — from the
# structured qconsole.jsonl log, parsed as JSON — the total FPS, the
# SCR_UpdateScreen ("end") time, the GPU per-pass breakdown, the fence/present
# split, and the draw count.
#
# It reuses the existing timing infrastructure (com_speeds / r_gpuSpeeds /
# r_vkDebugTiming, the qconsole.jsonl sink, the deterministic-camera recipe from
# visual-render-features.sh + the addbot/sv_seed recipe from nav-trace-gate.sh) —
# it does NOT introduce any new diagnostic cvar or a parallel harness.
#
# Determinism: cg.time is frozen (r_pinFrameTime 1.0), so the CLIENT render is
# pinned regardless of ongoing server bot AI — the same interpolated scene, the
# same draws/dlights in frustum, every frame. sv_seed pins the spawn RNG,
# fixedtime pins the sim delta, noclip removes the gravity settle so the camera
# origin is identical. com_maxfps 0 uncaps the frame rate so the measured number
# is the true render ceiling, not the 250 cap.
#
# Usage:
#   tests/fps-perf-gate.sh --engine build/debug/wired.x64.exe [--map arena1] [--bots 6] [--tag head]
#
# Re-run it on any build to compare; the before/after regression answer just
# needs the same script run against a second (pre-regression) build.

set -u

# ── args ──────────────────────────────────────────────────────────────────
ENGINE=""
MAP="arena1"
BOTS=6
TAG="head"
VIEWPOS="1052 1432 90 135"   # interior lit spot in arena1, action in frustum
SEED="12345"
HOLD_FRAMES=500             # >> 200 so several 200-frame timing averages fire

while [ $# -gt 0 ]; do
    case "$1" in
        --engine)  ENGINE="$2"; shift 2 ;;
        --map)     MAP="$2"; shift 2 ;;
        --bots)    BOTS="$2"; shift 2 ;;
        --tag)     TAG="$2"; shift 2 ;;
        --viewpos) VIEWPOS="$2"; shift 2 ;;
        --seed)    SEED="$2"; shift 2 ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
done

[ -n "$ENGINE" ] || { echo "FAIL: --engine <wired.x64.exe> required" >&2; exit 2; }
[ -f "$ENGINE" ] || { echo "FAIL: engine not found: $ENGINE" >&2; exit 2; }

ENGINE_DIR="$(cd "$(dirname "$ENGINE")" && pwd)"
ENGINE_BIN="./$(basename "$ENGINE")"

# The engine writes qconsole.jsonl to fs_homepath (root). We run AS-IS with the
# real homepath, so this is the canonical per-user location.
HOMEPATH="/c/Users/eser/wired/q3now-preview"
QCONSOLE="$HOMEPATH/qconsole.jsonl"
CONFIG="$HOMEPATH/base/config.cfg"

# Output dir. Use a Windows-native path so the MSYS2 bash tools and the
# Windows-native Python (which does not resolve the MSYS /tmp mount) agree.
OUTDIR="C:/msys64/tmp/fps-gate"
mkdir -p "$OUTDIR"
STDOUT="$OUTDIR/stdout-$TAG.log"
QSNAP="$OUTDIR/qconsole-$TAG.jsonl"

# ── reap: a windowed engine ignores SIGTERM; force-kill on timeout / after ──
reap() {
    ( powershell.exe -NoProfile -Command \
        "Get-Process wired.x64 -ErrorAction SilentlyContinue | Stop-Process -Force" \
        >/dev/null 2>&1 ) || true
}

echo "==> fps-perf-gate: $TAG  map=$MAP bots=$BOTS seed=$SEED viewpos=($VIEWPOS)"

# Back up config so the perf-cvar writes don't leak into the user's config.
CONFIG_BAK=""
if [ -f "$CONFIG" ]; then
    CONFIG_BAK="$OUTDIR/config.cfg.bak"
    cp "$CONFIG" "$CONFIG_BAK"
fi

reap   # no stale engine may hold the window / qconsole file

# Build the staggered addbot chain. The shipped game data registers ONE playable
# character (visor — "CL_Characters: loaded 1 character(s)"), so distinct-name
# addbots would fail; the representative bot load is N visor bots at fixed skill,
# staggered so each finishes spawning before the next (deterministic given
# sv_seed). Fixed count + fixed character + fixed skill = repeatable.
BOT_CHAIN=""
i=0
while [ "$i" -lt "$BOTS" ]; do
    BOT_CHAIN="$BOT_CHAIN +addbot visor 5 +wait 8"
    i=$((i+1))
done

# ── launch ──────────────────────────────────────────────────────────────────
# AS-IS: no fs_homepath override (real homepath → real qconsole.jsonl).
#  sv_cheats 1  : CVAR_LATCH — active only AFTER map spawn. The CVAR_CHEAT timing
#    cvars (r_gpuSpeeds / r_vkDebugTiming / r_pinShaderTime / r_pinFrameTime) are
#    therefore set AFTER +map, not at startup (a startup set is "cheat protected").
#  com_maxfps 0 : uncap — measure the true render ceiling, not the 250 cap.
#  r_swapInterval 0 : vsync off (present-mode IMMEDIATE).
#  com_speeds 2 : per-frame sv/ev/cl/gm/rf/bk + the cl micro (end = SCR_UpdateScreen).
#    (com_speeds is NOT a cheat cvar — safe at startup.)
#  r_gpuSpeeds 1 / r_vkDebugTiming 1 : 200-frame GPU per-pass + fence/present + draws.
#  log_file_severity DEBUG + `log renderer.timing debug` : route the SEV_DEBUG
#    renderer.timing lines to the qconsole.jsonl file sink (after renderer init).
( cd "$ENGINE_DIR" && timeout -s KILL -k 15 200 "$ENGINE_BIN" \
    +set sv_cheats 1 +set sv_pure 0 +set vm_game 0 +set vm_cgame 0 \
    +set bot_enable 1 +set g_gametype 0 +set sv_maxclients 16 \
    +set g_warmup 0 +set g_doWarmup 0 +set sv_seed "$SEED" +set fixedtime 1 \
    +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set r_fullscreen 0 \
    +set com_maxfps 0 +set r_swapInterval 0 \
    +set com_automated 0 \
    +set log_file_severity DEBUG \
    +set com_speeds 2 \
    +map "$MAP" +waitForMap +wait 100 \
    +set r_gpuSpeeds 1 +set r_vkDebugTiming 1 \
    +log renderer.timing debug \
    $BOT_CHAIN \
    +wait 40 \
    +cmd noclip +wait 20 \
    +cmd setviewpos $VIEWPOS +wait 40 +cmd setviewpos $VIEWPOS +wait 20 \
    +set r_pinShaderTime 1.0 +set r_pinFrameTime 1.0 \
    +wait "$HOLD_FRAMES" \
    +quit \
    > "$STDOUT" 2>&1 || true )

reap

# Snapshot qconsole so a later run doesn't clobber the parsed evidence.
cp "$QCONSOLE" "$QSNAP" 2>/dev/null || { echo "FAIL: no qconsole.jsonl at $QCONSOLE" >&2; }

# Restore config.
if [ -n "$CONFIG_BAK" ] && [ -f "$CONFIG_BAK" ]; then
    cp "$CONFIG_BAK" "$CONFIG"
fi

# ── parse (JSON, not string-grep) ─────────────────────────────────────────
python - "$QSNAP" "$TAG" <<'PY'
import json, re, sys, statistics

path, tag = sys.argv[1], sys.argv[2]

# com_speeds line 1: "frame:N all:X sv:X ev:X cl:X gm:X rf:X bk:X"
re_all   = re.compile(r'frame:(\d+)\s+all:\s*(\d+)\s+sv:\s*(\d+)\s+ev:\s*(\d+)\s+cl:\s*(\d+)\s+gm:\s*(\d+)\s+rf:\s*(\d+)\s+bk:\s*(\d+)')
# com_speeds 2 micro: "cl: ... end:X ... (us)"  (microseconds)
re_end   = re.compile(r'\bend:(\d+)')
# r_gpuSpeeds 1 emits a MULTI-LINE block (one log record each): a header
# "gpu (Nf avg, ms):", then "  <label>=X" per pass, then "  total=X".
re_gpu_hdr = re.compile(r'^gpu \((\d+)f avg, ms\):')
re_gpu_lbl = re.compile(r'^\s*([A-Za-z_]+)=([\d.]+)')
re_gpu_tot = re.compile(r'^\s*total=([\d.]+)')
# r_vkDebugTiming 1: "vk timing (Nf avg): fence=X ... present=X draws=N/f(...)"
re_vkt   = re.compile(r'vk timing \(\d+f avg\): fence=(\d+)ms/f\s+ft_fence=(\d+)ms/f\s+acquire=(\d+)ms/f\s+submit=(\d+)ms/f\s+present=(\d+)ms/f\s+draws=(\d+)/f')

alls, ends = [], []
vkt_lines = []
gpu_blocks = []          # list of dicts {label: ms, ..., total: ms}
cur_gpu = None

with open(path, encoding='utf-8', errors='replace') as f:
    for line in f:
        line = line.strip()
        if not line:
            continue
        try:
            o = json.loads(line)
        except Exception:
            continue
        msg = o.get('msg', '').rstrip('\n')
        m = re_all.search(msg)
        if m:
            alls.append(int(m.group(2)))       # all = total frame ms
        me = re_end.search(msg)
        if me and 'cl:' in msg:
            ends.append(int(me.group(1)))       # end = SCR_UpdateScreen (us)
        mv = re_vkt.search(msg)
        if mv:
            vkt_lines.append(mv.groups())
        # multi-line GPU block state machine
        mh = re_gpu_hdr.match(msg)
        if mh:
            cur_gpu = {}
            continue
        if cur_gpu is not None:
            mt = re_gpu_tot.match(msg)
            if mt:
                cur_gpu['total'] = float(mt.group(1))
                gpu_blocks.append(cur_gpu)
                cur_gpu = None
                continue
            ml = re_gpu_lbl.match(msg)
            if ml:
                cur_gpu[ml.group(1)] = float(ml.group(2))
            else:
                cur_gpu = None   # block interrupted; drop partial

# 'all' is INTEGER ms — a true ~3.5ms frame logs as 3 or 4, so the median flips
# on tiny variance (a labeling artifact, not real divergence). The MEAN over the
# steady window (warmup + spikes dropped) is the stable frame-time estimate; the
# µs-precision 'end' below is the robust cross-run comparison basis.
STEADY_SKIP = 2000
def steady(ms_list):
    return [v for v in ms_list[STEADY_SKIP:] if v is not None and v < 20]

print(f"\n=== FPS-PERF-GATE RESULT [{tag}] ===")
print(f"  com_speeds frames parsed : {len(alls)}")
mean_all = None
if alls:
    st = steady(alls)
    if st:
        mean_all = statistics.mean(st)
        med_all  = statistics.median(st)
        fps_mean = 1000.0/mean_all if mean_all>0 else float('inf')
        print(f"  frame time all (ms)      : mean={mean_all:.3f}  median={med_all} (integer-ms; see note)")
        print(f"  TOTAL FPS (mean frame)   : {fps_mean:.1f}   (>=250 target: {'MET' if fps_mean>=250 else 'NOT MET'})")
if ends:
    emed = statistics.median(ends)
    print(f"  end/SCR_UpdateScreen (us): median={emed}  ({emed/1000.0:.2f} ms)  [the measured FPS ceiling]")
    print(f"  end-implied FPS ceiling  : {1000000.0/emed:.1f}" if emed>0 else "  end=0 (<1us)")
gpu_last = gpu_blocks[-1] if gpu_blocks else None
if gpu_last:
    # median of each pass across all 200f-avg blocks (robust)
    passes = [k for k in gpu_last.keys()]
    print(f"  GPU per-pass (r_gpuSpeeds 200f avg, median of {len(gpu_blocks)} blocks):")
    med_passes = {}
    for k in passes:
        vals = [b[k] for b in gpu_blocks if k in b]
        med_passes[k] = statistics.median(vals) if vals else None
    # dominant non-total pass
    dom = max(((k,v) for k,v in med_passes.items() if k!='total' and v is not None),
              key=lambda kv: kv[1], default=(None,None))
    for k in passes:
        mark = "  <-- dominant" if k == dom[0] else ""
        print(f"    {k:>16} = {med_passes[k]:.2f} ms{mark}")
    print(f"  GPU bottleneck           : {dom[0]} ({dom[1]:.2f} ms of {med_passes.get('total',0):.2f} ms total)")
else:
    print(f"  GPU per-pass             : (no r_gpuSpeeds block — needs >=200 frames + renderer.timing routed)")
if vkt_lines:
    fence, ftf, acq, sub, pres, draws = vkt_lines[-1]
    med_draws = int(statistics.median([int(v[5]) for v in vkt_lines]))
    print(f"  vk timing (r_vkDebugTiming 200f avg, last): fence={fence}ms ft_fence={ftf}ms acquire={acq}ms submit={sub}ms present={pres}ms")
    print(f"  DRAW COUNT               : median={med_draws}/f  (last={draws}/f)")
else:
    med_draws = None
    print(f"  vk timing / draw count   : (no r_vkDebugTiming line — needs >=200 frames + renderer.timing routed)")

# CPU-bound vs GPU-bound: compare the end (SCR_UpdateScreen, CPU submit path) to
# the GPU total. If GPU total ~ frame time, GPU-bound; if end >> GPU, CPU-bound.
if ends and gpu_last:
    emed_ms = statistics.median(ends)/1000.0
    gtot = statistics.median([b['total'] for b in gpu_blocks if 'total' in b])
    bound = "GPU-bound" if gtot >= emed_ms*0.7 else "CPU-bound (SCR_UpdateScreen)"
    print(f"  BOUND                    : {bound}  (end={emed_ms:.2f}ms vs GPU total={gtot:.2f}ms)")
print(f"=== END RESULT [{tag}] ===\n")

# emit a compact machine-readable summary for two-run determinism comparison
summary = {
    "tag": tag,
    "frames": len(alls),
    "all_mean_ms": round(mean_all,3) if mean_all else None,
    "fps_mean": round(1000.0/mean_all,1) if mean_all else None,
    "end_median_us": statistics.median(ends) if ends else None,
    "gpu_total_ms": statistics.median([b['total'] for b in gpu_blocks if 'total' in b]) if gpu_blocks else None,
    "draws_median": med_draws,
}
import os
with open(os.path.join(os.path.dirname(path), f"summary-{tag}.json"), "w") as sf:
    json.dump(summary, sf)
print("SUMMARY:", json.dumps(summary))
PY

echo "==> stdout: $STDOUT   qconsole: $QSNAP"
