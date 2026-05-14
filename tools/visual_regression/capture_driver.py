#!/usr/bin/env python3
"""capture_driver.py — launch wired.x64.exe under RenderDoc and grab a frame.

Uses the engine's in-app capture trigger (`WN_RDOC_CAPTURE`, see
code/renderervk/tr_backend.c): when the engine is launched with
renderdoc.dll injected, it acquires the in-app API and TriggerCapture()s
on a fixed frame (currently frame 600), writing the .rdc with the path
template `wn_capture` relative to its working directory, then self-quits a
short while later. So this driver:

  1. clears <working-dir>/layoutdump.jsonl (the layout-dump file)
  2. launches `renderdoccmd capture --working-dir <build/debug> -- <binary> <cvars> [+map ...]`
     with `+set r_layoutDump 1` so the layout pass dumps coordinates
  3. waits for the engine to self-quit (or kills it after --timeout)
  4. returns (rdc_path, layoutdump_path)

REQUIREMENTS
  * wired.x64.exe built with WN_RDOC_CAPTURE != 0. In the current working
    tree tr_backend.c defaults it to 1, so a fresh `ninja` build has the
    trigger. If the trigger never fires (no .rdc, no "[WN_RDOC]" lines),
    rebuild:  cmake -S . -B build/debug -DWN_RDOC_CAPTURE=1 && ninja -C build/debug
  * renderdoccmd.exe on PATH or at C:\\Program Files\\RenderDoc\\renderdoccmd.exe
    (override with --renderdoccmd or the RENDERDOCCMD env var).

The scene YAML's `capture_frame` is informational — the captured frame is
whatever the WN_RDOC_CAPTURE build hard-codes (frame 600 at time of
writing). For a static menu that distinction is immaterial.
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import subprocess
import sys
import time

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_REPO_ROOT = os.path.abspath(os.path.join(_THIS_DIR, "..", ".."))

_DEFAULT_BINARY = os.path.join(_REPO_ROOT, "build", "debug", "wired.x64.exe")
_DEFAULT_WORKDIR = os.path.join(_REPO_ROOT, "build", "debug")
_DEFAULT_RENDERDOCCMD = os.environ.get(
    "RENDERDOCCMD", r"C:\Program Files\RenderDoc\renderdoccmd.exe"
)
_RDC_TEMPLATE = "wn_capture"          # must match SetCaptureFilePathTemplate() in tr_backend.c
_LAYOUTDUMP = "layoutdump.jsonl"      # must match WUI_LAYOUT_DUMP_FILE in cl_wired_layout_dump.c

# cvars every VR run pins (representative readback per the project's smoke
# convention: r_brightness 1, sound off, layout dump on).
_BASE_CVARS = {"s_initsound": 0, "r_brightness": 1, "r_layoutDump": 1, "in_mouse": 0}


def find_exe(explicit, default, name):
    cand = explicit or default
    if cand and os.path.isfile(cand):
        return cand
    from shutil import which
    w = which(name) or which(name + ".exe")
    if w:
        return w
    return None


def detect_wn_rdoc_capture(binary_path):
    """Best-effort: scan the engine binaries for the WN_RDOC marker the
    capture-trigger block logs ("[WN_RDOC]"). The trigger lives in the
    renderer DLL (tr_backend.c -> wired_<api>_*.dll), not the client exe,
    so this scans the .dll/.exe files alongside `binary_path`. Returns
    True/False/None (None = couldn't tell). Not authoritative — the real
    proof is whether a .rdc appears after a capture run."""
    if not binary_path:
        return None
    bdir = os.path.dirname(os.path.abspath(binary_path))
    candidates = []
    # renderer DLLs first (most likely host of the trigger), then the exe
    for pat in ("wired_*x86_64.dll", "wired_*.dll", os.path.basename(binary_path)):
        candidates += glob.glob(os.path.join(bdir, pat))
    seen = set()
    found_any_file = False
    for c in candidates:
        if c in seen or not os.path.isfile(c):
            continue
        seen.add(c)
        found_any_file = True
        try:
            with open(c, "rb") as fh:
                if b"[WN_RDOC]" in fh.read():
                    return True
        except OSError:
            continue
    return False if found_any_file else None


def build_engine_args(scene_cfg):
    """Compose the `+set ...` / `+map ...` argument list from a scene cfg."""
    cvars = dict(_BASE_CVARS)
    for k, v in (scene_cfg.get("cvars") or {}).items():
        cvars[k] = v
    args = []
    for k, v in cvars.items():
        args += ["+set", str(k), str(v)]
    # Map: scene may say `map: <name>` (load a level) or `no_map: true` /
    # nothing (stay on the main menu).
    mp = scene_cfg.get("map")
    if mp and not scene_cfg.get("no_map"):
        args += ["+map", str(mp)]
    return args


def run_capture(scene_cfg, *, binary=None, workdir=None, renderdoccmd=None,
                timeout=90, keep=False, verbose=True):
    """Launch the engine under RenderDoc, capture a frame, return a dict:
        {"rdc": <path or None>, "layoutdump": <path or None>,
         "returncode": int, "stdout": str, "stderr": str,
         "wn_rdoc_marker": bool|None, "elapsed_s": float}
    Raises RuntimeError on missing tools."""
    binary = find_exe(binary, _DEFAULT_BINARY, "wired.x64")
    if not binary:
        raise RuntimeError(f"wired.x64.exe not found (looked at {_DEFAULT_BINARY} and PATH). "
                           "Build it: ninja -C build/debug")
    rdcmd = find_exe(renderdoccmd, _DEFAULT_RENDERDOCCMD, "renderdoccmd")
    if not rdcmd:
        raise RuntimeError(f"renderdoccmd.exe not found (looked at {_DEFAULT_RENDERDOCCMD} and PATH). "
                           "Install RenderDoc or pass --renderdoccmd.")
    workdir = workdir or _DEFAULT_WORKDIR
    if not os.path.isdir(workdir):
        raise RuntimeError(f"working dir does not exist: {workdir}")

    marker = detect_wn_rdoc_capture(binary)
    if marker is False and verbose:
        sys.stderr.write(
            "capture_driver: WARNING — '[WN_RDOC]' marker not found in the binary; the in-app "
            "capture trigger may be compiled out. If no .rdc appears, rebuild with "
            "-DWN_RDOC_CAPTURE=1.\n"
        )

    layoutdump_path = os.path.join(workdir, _LAYOUTDUMP)
    # Clear stale layoutdump + stale .rdc so we read fresh data.
    for pat in (layoutdump_path, os.path.join(workdir, _RDC_TEMPLATE + "*")):
        for p in glob.glob(pat):
            try:
                os.remove(p)
            except OSError:
                pass

    engine_args = build_engine_args(scene_cfg)
    # `renderdoccmd capture` injects renderdoc.dll, then execs the target.
    # --working-dir sets the CWD the engine sees (so the in-app trigger's
    # "wn_capture" template and our layoutdump land in `workdir`).
    cmd = [rdcmd, "capture", "--working-dir", workdir, "--", binary, *engine_args]
    if verbose:
        sys.stderr.write("capture_driver: " + " ".join(cmd) + "\n")

    t0 = time.time()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, cwd=workdir)
        rc, out, err, timed_out = proc.returncode, proc.stdout, proc.stderr, False
    except subprocess.TimeoutExpired as e:
        # Engine didn't self-quit in time — that's expected if the trigger
        # didn't fire, or just slow. Output may still contain a usable .rdc.
        rc, out, err, timed_out = 124, (e.stdout or ""), (e.stderr or ""), True
        if verbose:
            sys.stderr.write(f"capture_driver: timeout after {timeout}s (engine still running?)\n")
    elapsed = time.time() - t0

    # Find the newest wn_capture*.rdc in workdir.
    rdcs = sorted(glob.glob(os.path.join(workdir, _RDC_TEMPLATE + "*")),
                  key=lambda p: os.path.getmtime(p) if os.path.exists(p) else 0)
    rdc = rdcs[-1] if rdcs else None
    layoutdump = layoutdump_path if os.path.isfile(layoutdump_path) else None

    if verbose:
        sys.stderr.write(f"capture_driver: rdc={rdc!r} layoutdump={layoutdump!r} "
                         f"rc={rc} timed_out={timed_out} elapsed={elapsed:.1f}s\n")

    return {
        "rdc": rdc, "layoutdump": layoutdump, "returncode": rc,
        "timed_out": timed_out, "stdout": out, "stderr": err,
        "wn_rdoc_marker": marker, "elapsed_s": elapsed, "workdir": workdir,
        "binary": binary, "cmd": cmd,
    }


def load_layoutdump(path):
    """Parse layoutdump.jsonl. Returns {region: {x,y,w,h,rgba_authored,rgba_forecolor,...}}
    using the *last* occurrence of each region (the menu is static frame to
    frame, so any frame's entry is representative; last wins)."""
    out = {}
    if not path or not os.path.isfile(path):
        return out
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = line.strip()
            if not line or not line.startswith("{"):
                continue
            try:
                rec = json.loads(line)
            except json.JSONDecodeError:
                continue
            r = rec.get("region")
            if r:
                out[r] = rec
    return out


# ── CLI ────────────────────────────────────────────────────────────────


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Launch wired.x64.exe under RenderDoc and capture a frame.")
    ap.add_argument("--map", help="map to load (omit to stay on the main menu)")
    ap.add_argument("--cvar", action="append", default=[], metavar="NAME=VALUE",
                    help="extra cvar to set (repeatable)")
    ap.add_argument("--binary", default=None, help="path to wired.x64.exe")
    ap.add_argument("--workdir", default=None, help="engine working dir (default build/debug)")
    ap.add_argument("--renderdoccmd", default=None, help="path to renderdoccmd.exe")
    ap.add_argument("--timeout", type=int, default=90)
    ap.add_argument("--keep", action="store_true", help="(reserved) keep artifacts")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args(argv)

    cvars = {}
    for kv in args.cvar:
        if "=" not in kv:
            ap.error(f"--cvar expects NAME=VALUE, got {kv!r}")
        k, v = kv.split("=", 1)
        cvars[k] = v
    scene_cfg = {"cvars": cvars}
    if args.map:
        scene_cfg["map"] = args.map
    else:
        scene_cfg["no_map"] = True

    try:
        res = run_capture(scene_cfg, binary=args.binary, workdir=args.workdir,
                          renderdoccmd=args.renderdoccmd, timeout=args.timeout, keep=args.keep)
    except RuntimeError as e:
        sys.stderr.write(f"capture_driver: {e}\n")
        return 2

    if args.json:
        # Don't dump the (potentially huge) stdout/stderr in JSON mode.
        slim = {k: v for k, v in res.items() if k not in ("stdout", "stderr")}
        print(json.dumps(slim, indent=2))
    else:
        print(f"rdc        : {res['rdc']}")
        print(f"layoutdump : {res['layoutdump']}")
        if res["layoutdump"]:
            ld = load_layoutdump(res["layoutdump"])
            print(f"  regions in layoutdump: {sorted(ld)}")
        print(f"returncode : {res['returncode']}  (timed_out={res['timed_out']})")
        print(f"wn_rdoc marker in binary: {res['wn_rdoc_marker']}")
        print(f"elapsed    : {res['elapsed_s']:.1f}s")
    # Success = we got a capture file.
    return 0 if res["rdc"] else 1


if __name__ == "__main__":
    sys.exit(main())
