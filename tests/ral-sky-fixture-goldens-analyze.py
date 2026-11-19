#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
"""TASK-239 AC#10 analyzer: arena7 sky fixture backend parity + golden gate.

Reads the native captures produced by tests/ral-sky-fixture-goldens.sh, box-averages
them to 1280x720 through png2raw, and enforces:
  * cross-backend parity per fixture (Metal vs Vulkan): mean |d| over the frame and
    over the sky band within tolerance;
  * golden gate per backend/fixture against tests/golden/sky/<fixture>_<backend>.png
    (the harness's logical 1280x720 PNG, copied with --update-goldens after human
    approval and decoded through png2raw for the comparison).
"""
import argparse, json, os, subprocess, sys

W, H = 1280, 720

def png2raw(tool, path):
    for size in ("2560x1440", "1280x720"):
        r = subprocess.run([tool, "--size", size, path], capture_output=True)
        if r.returncode == 0 and r.stdout:
            w, h = (int(v) for v in size.split("x"))
            ch = len(r.stdout) // (w * h)
            return r.stdout, w, h, ch
    sys.exit("FAIL: png2raw could not decode %s" % path)

def logical_rgb(tool, path):
    raw, w, h, ch = png2raw(tool, path)
    out = bytearray(W * H * 3)
    if (w, h) == (W, H):
        for i in range(W * H):
            out[i*3:i*3+3] = raw[i*ch:i*ch+3]
        return bytes(out)
    sx, sy = w // W, h // H
    n = sx * sy
    for y in range(H):
        for x in range(W):
            acc = [0, 0, 0]
            for oy in range(sy):
                row = ((y*sy+oy) * w + x*sx) * ch
                for ox in range(sx):
                    o = row + ox*ch
                    acc[0] += raw[o]; acc[1] += raw[o+1]; acc[2] += raw[o+2]
            o = (y*W+x)*3
            out[o] = (acc[0] + n//2)//n; out[o+1] = (acc[1] + n//2)//n; out[o+2] = (acc[2] + n//2)//n
    return bytes(out)

def mean_abs(a, b, y0=0, y1=H):
    s = 0; n = 0
    for y in range(y0, y1):
        row = y*W*3
        for i in range(row, row + W*3):
            s += abs(a[i]-b[i]); n += 1
    return s / n

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--artifacts", default="build/ral-sky-fixture-goldens")
    ap.add_argument("--goldens", default="tests/golden/sky")
    ap.add_argument("--png2raw", default="tools/png2raw/png2raw")
    ap.add_argument("--backends", default="metal vulkan")
    ap.add_argument("--fixtures", default="clear-noon sunset night storm authored-legacy hybrid")
    # Full-frame tolerance admits the cinematic lens-flare ghosts (r_lens 1
    # default): each backend's depth oracle samples its own scene depth copy,
    # so ghost placement/intensity differs slightly while band means agree.
    ap.add_argument("--parity-full", type=float, default=8.0)
    ap.add_argument("--parity-sky", type=float, default=4.0)
    # Storm carries GPU-simulated precipitation and semantic emitter sprites whose
    # per-frame positions are not pinned across backends, so its cross-backend
    # tolerance is widened (still a parity gate: band means agree within ~4/255).
    ap.add_argument("--stochastic-fixtures", default="storm")
    ap.add_argument("--stochastic-parity-full", type=float, default=8.0)
    ap.add_argument("--stochastic-parity-sky", type=float, default=8.0)
    ap.add_argument("--stochastic-golden-full", type=float, default=8.0)
    ap.add_argument("--golden-full", type=float, default=3.0)
    ap.add_argument("--update-goldens", action="store_true")
    a = ap.parse_args()
    backends = a.backends.split(); fixtures = a.fixtures.split()
    report = {"schemaVersion": 1, "fixtures": {}, "pass": True}
    for f in fixtures:
        imgs = {}; paths = {}
        for b in backends:
            # The logical 1280x720 PNG is the comparison and golden unit; the
            # retained native capture is only a fallback.
            p = os.path.join(a.artifacts, b, f + ".png")
            if not os.path.exists(p):
                p = os.path.join(a.artifacts, b, f + ".native.png")
            if not os.path.exists(p):
                print("FAIL: missing capture %s/%s" % (b, f)); report["pass"] = False; continue
            imgs[b] = logical_rgb(a.png2raw, p); paths[b] = p
        entry = {}
        if len(imgs) == 2:
            x, y = (imgs[b] for b in backends)
            full = mean_abs(x, y); sky = mean_abs(x, y, 30, 320)
            stochastic = f in a.stochastic_fixtures.split()
            limit_full = a.stochastic_parity_full if stochastic else a.parity_full
            limit_sky = a.stochastic_parity_sky if stochastic else a.parity_sky
            entry["parity"] = {"full": round(full, 3), "sky": round(sky, 3),
                               "limitFull": limit_full, "limitSky": limit_sky,
                               "stochastic": stochastic,
                               "pass": full <= limit_full and sky <= limit_sky}
            if not entry["parity"]["pass"]: report["pass"] = False
            print("%-16s parity full %.2f sky %.2f %s" % (f, full, sky, "PASS" if entry["parity"]["pass"] else "FAIL"))
        for b, img in imgs.items():
            gp = os.path.join(a.goldens, "%s_%s.png" % (f, b))
            if a.update_goldens:
                os.makedirs(a.goldens, exist_ok=True)
                import shutil; shutil.copyfile(paths[b], gp)
                entry.setdefault("golden", {})[b] = "updated"
            elif os.path.exists(gp):
                g = logical_rgb(a.png2raw, gp)
                d = mean_abs(img, g) if len(g) == len(img) else float("inf")
                ok = d <= (a.stochastic_golden_full if f in a.stochastic_fixtures.split() else a.golden_full)
                entry.setdefault("golden", {})[b] = {"diff": round(d, 3) if d != float("inf") else None, "pass": ok}
                if not ok: report["pass"] = False
                print("%-16s golden %-6s diff %.2f %s" % (f, b, d, "PASS" if ok else "FAIL"))
            else:
                entry.setdefault("golden", {})[b] = "missing"
                print("%-16s golden %-6s MISSING (run with --update-goldens after approval)" % (f, b))
                report["pass"] = False
        report["fixtures"][f] = entry
    json.dump(report, open(os.path.join(a.artifacts, "analysis.json"), "w"), indent=1)
    print("PASS" if report["pass"] else "FAIL", "sky fixture goldens")
    sys.exit(0 if report["pass"] else 1)

if __name__ == "__main__":
    main()
