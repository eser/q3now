#!/usr/bin/env python3
"""readback.py — pixel readback from a RenderDoc .rdc capture.

Drives `qrenderdoc.exe --python <script>` (RenderDoc ships an embedded
Python 3.6 interpreter — the `renderdoc` module is ABI-locked to it, so
this *cannot* run under the system Python; we shell out). The embedded
script opens the capture, finds the swapchain/backbuffer texture at the
last Present, picks the requested pixels, and writes a JSON blob the
system-Python side reads back.

qrenderdoc --python quirks this code works around (verified on v1.44):
  * sys.argv is NOT populated for the script → parameters are passed via the
    environment (VR_RDC / VR_TEXTURE / VR_COORDS).
  * the script's stdout is SWALLOWED by qrenderdoc → the result JSON is written
    to a file (VR_OUT) which read_pixels() reads (stdout is still echoed as a
    fallback for manual runs).
  * after the script returns, qrenderdoc proceeds to open its GUI (blocks
    forever headlessly) → the script ends with os._exit(0) to hard-quit.
  * a triple-buffered swapchain exposes 3 SwapBuffer textures; only one was
    Presented this frame (the others are often all-zero) → the chooser samples
    each and picks the non-black one.

Default qrenderdoc.exe location on Windows: C:\\Program Files\\RenderDoc\\qrenderdoc.exe
(override with --qrenderdoc or the QRENDERDOC env var).

NOTE: the RenderDoc Python replay API surface varies a little across
versions. The embedded script below targets the 1.x API (OpenCaptureFile
/ OpenCapture / PickPixel). If your RenderDoc build differs, the script is
a single clearly-delimited block — edit `_RDOC_SCRIPT` below. The script
fails loudly (JSON {"ok": false, "error": "..."}) rather than silently.

CLI:
    python readback.py capture.rdc --pixels 640,360 100,200
    python readback.py capture.rdc --pixels 640,360 --texture backbuffer --json
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile

_DEFAULT_QRENDERDOC = os.environ.get(
    "QRENDERDOC", r"C:\Program Files\RenderDoc\qrenderdoc.exe"
)

# ── embedded RenderDoc-Python (3.6) script ─────────────────────────────
# Receives argv: <rdc_path> <texture_selector> <x,y> [<x,y> ...]
# texture_selector: "backbuffer" (the last Present source) or a substring
# of a resource's debug name (e.g. "tonemapped_image", "color_image").
# Prints one line of JSON: {"ok": true, "texture": {...}, "pixels": [...]}.
_RDOC_SCRIPT = r'''
import sys, os, json

# qrenderdoc --python swallows the script's stdout, so write the result JSON to
# the file named by VR_OUT (set by read_pixels) and ALSO echo to stdout (for the
# CLI/manual case). The parent reads VR_OUT.
_VR_OUT = os.environ.get("VR_OUT", "")

def emit(obj):
    line = json.dumps(obj)
    try:
        sys.stdout.write(line + "\n"); sys.stdout.flush()
    except Exception:
        pass
    if _VR_OUT:
        try:
            with open(_VR_OUT, "w", encoding="utf-8") as f:
                f.write(line + "\n")
        except Exception:
            pass

try:
    import renderdoc as rd
except Exception as e:  # noqa: BLE001
    emit({"ok": False, "error": "import renderdoc failed: %r" % (e,)})
    os._exit(0)

# qrenderdoc --python (v1.x) runs this script with the `renderdoc`/`qrenderdoc`
# globals already injected but DOES NOT populate sys.argv — args passed after
# "--" never reach the script. So parameters arrive via environment variables
# (set by read_pixels()): VR_RDC, VR_TEXTURE, VR_COORDS ("x,y;x,y;...").
rdc_path = os.environ.get("VR_RDC", "")
tex_sel = os.environ.get("VR_TEXTURE", "backbuffer")
coords = []
for a in (os.environ.get("VR_COORDS", "") or "").split(";"):
    a = a.strip()
    if not a:
        continue
    xs, ys = a.split(",")
    coords.append((int(float(xs)), int(float(ys))))
if not rdc_path or not coords:
    emit({"ok": False, "error": "missing VR_RDC / VR_COORDS env (got rdc=%r coords=%r)" % (rdc_path, coords)})
    os._exit(0)

cap = rd.OpenCaptureFile()
res = cap.OpenFile(rdc_path, "", None)
# ResultDetails in newer builds; bool/int in older. Be liberal.
ok = getattr(res, "OK", None)
if callable(ok):
    good = res.OK()
else:
    good = (res == 0) or bool(res)
if not good:
    emit({"ok": False, "error": "OpenFile failed: %r" % (res,)})
    os._exit(0)

open_res = cap.OpenCapture(rd.ReplayOptions(), None)
# OpenCapture returns (ResultDetails, ReplayController) on 1.x.
if isinstance(open_res, (tuple, list)) and len(open_res) == 2:
    status, controller = open_res
    sok = getattr(status, "OK", None)
    if callable(sok) and not status.OK():
        emit({"ok": False, "error": "OpenCapture failed: %r" % (status,)})
        os._exit(0)
else:
    controller = open_res
if controller is None:
    emit({"ok": False, "error": "OpenCapture returned no controller"})
    os._exit(0)

try:
    # ── pick the event to inspect: the last action (= end of frame) ──
    def last_action(actions):
        last = None
        for a in actions:
            last = a
            kids = getattr(a, "children", None)
            if kids:
                deeper = last_action(kids)
                if deeper is not None:
                    last = deeper
        return last

    root = controller.GetRootActions()
    end = last_action(root)
    if end is None:
        emit({"ok": False, "error": "no actions in capture"})
        os._exit(0)
    controller.SetFrameEvent(end.eventId, True)

    # ── choose the texture ──
    textures = controller.GetTextures()
    chosen = None
    if tex_sel.lower() in ("backbuffer", "swapchain", "present", ""):
        # A triple-buffered swapchain exposes 3 SwapBuffer textures; only ONE is
        # the frame that was actually Presented (the other two are stale/cleared,
        # often all-zero). Grabbing the first SwapBuffer texture blindly can land
        # on a black buffer. So: collect all SwapBuffer textures, then pick the
        # one whose centre pixel is non-zero (the presented frame). If all read
        # zero (a genuinely black frame), fall back to the first.
        swaps = []
        if hasattr(rd, "TextureCategory"):
            for t in textures:
                cf = int(getattr(t, "creationFlags", 0))
                if cf & int(rd.TextureCategory.SwapBuffer):
                    swaps.append(t)
        if swaps:
            _cast = rd.CompType.UNorm if hasattr(rd, "CompType") else None
            _sub = rd.Subresource() if hasattr(rd, "Subresource") else None
            def _centre_lum(t):
                cx, cy = int(t.width) // 2, int(t.height) // 2
                try:
                    if _sub is not None and _cast is not None:
                        pv = controller.PickPixel(t.resourceId, cx, cy, _sub, _cast)
                    else:
                        pv = controller.PickPixel(t.resourceId, cx, cy, 0, 0, 0)
                    fv = list(getattr(pv, "floatValue", []) or [])
                    return sum(fv[:3]) if len(fv) >= 3 else 0.0
                except Exception:  # noqa: BLE001
                    return 0.0
            chosen = swaps[0]
            best = _centre_lum(chosen)
            for t in swaps[1:]:
                l = _centre_lum(t)
                if l > best:
                    best, chosen = l, t
        if chosen is None:
            # heuristic: last 2D non-array BGRA/RGBA8 texture
            for t in reversed(textures):
                if getattr(t, "dimension", 2) == 2 and getattr(t, "arraysize", 1) == 1:
                    chosen = t
                    break
    else:
        sel = tex_sel.lower()
        for t in textures:
            name = ""
            try:
                name = controller.GetResourceName(t.resourceId)
            except Exception:  # noqa: BLE001
                rid = getattr(t, "resourceId", None)
                name = str(rid)
            if sel in str(name).lower():
                chosen = t
                break
    if chosen is None:
        names = []
        for t in textures:
            try:
                names.append(controller.GetResourceName(t.resourceId))
            except Exception:  # noqa: BLE001
                names.append(str(getattr(t, "resourceId", "?")))
        emit({"ok": False, "error": "no texture matched %r" % (tex_sel,),
              "available": names[:64]})
        os._exit(0)

    tex_id = chosen.resourceId
    try:
        tex_name = controller.GetResourceName(tex_id)
    except Exception:  # noqa: BLE001
        tex_name = str(tex_id)
    tw = int(getattr(chosen, "width", 0))
    th = int(getattr(chosen, "height", 0))

    # ── pick pixels ──
    # For an *_SRGB swapchain we want the value *as stored* (the byte that
    # would appear in a screenshot). PickPixel with a UNorm cast returns
    # the normalised stored value; with the default cast on an SRGB texture
    # it returns the sRGB-decoded (linear) value. We request UNorm so the
    # caller gets the storage byte. If your RenderDoc treats this
    # differently, flip `cast` below.
    cast = rd.CompType.UNorm if hasattr(rd, "CompType") else None
    sub = rd.Subresource() if hasattr(rd, "Subresource") else None

    out_pixels = []
    for (x, y) in coords:
        try:
            if sub is not None and cast is not None:
                pv = controller.PickPixel(tex_id, x, y, sub, cast)
            else:
                pv = controller.PickPixel(tex_id, x, y, 0, 0, 0)
        except Exception as e:  # noqa: BLE001
            out_pixels.append({"x": x, "y": y, "error": "PickPixel failed: %r" % (e,)})
            continue
        fv = list(getattr(pv, "floatValue", []) or [])
        iv = list(getattr(pv, "intValue", []) or [])
        byte = None
        if len(fv) >= 3:
            byte = [int(round(max(0.0, min(1.0, c)) * 255.0)) for c in fv[:4]]
            if len(byte) == 3:
                byte.append(255)
        out_pixels.append({"x": x, "y": y, "floatValue": fv, "intValue": iv, "byte": byte})

    emit({"ok": True,
          "texture": {"name": str(tex_name), "width": tw, "height": th},
          "pixels": out_pixels})
finally:
    try:
        controller.Shutdown()
    except Exception:  # noqa: BLE001
        pass

# Hard-exit so qrenderdoc --python does NOT fall through to opening its GUI
# (which would block forever in a headless run). os._exit skips atexit/Qt
# teardown — fine, we already emitted the result.
os._exit(0)
'''


def find_qrenderdoc(explicit=None):
    cand = explicit or _DEFAULT_QRENDERDOC
    if cand and os.path.isfile(cand):
        return cand
    # Try PATH.
    from shutil import which
    w = which("qrenderdoc") or which("qrenderdoc.exe")
    if w:
        return w
    return None


def read_pixels(rdc_path, pixels, *, texture="backbuffer", qrenderdoc=None, timeout=120):
    """Open `rdc_path`, pick `pixels` (list of (x, y)) from `texture`.
    Returns the parsed JSON dict from the embedded script. Raises
    RuntimeError on driver/launch problems."""
    qexe = find_qrenderdoc(qrenderdoc)
    if not qexe:
        raise RuntimeError(
            f"qrenderdoc.exe not found (looked at {qrenderdoc or _DEFAULT_QRENDERDOC} and PATH); "
            "pass --qrenderdoc or set QRENDERDOC"
        )
    if not os.path.isfile(rdc_path):
        raise RuntimeError(f"capture file not found: {rdc_path}")

    with tempfile.NamedTemporaryFile("w", suffix=".py", delete=False, encoding="utf-8") as tf:
        tf.write(_RDOC_SCRIPT)
        script_path = tf.name
    out_fd, out_path = tempfile.mkstemp(suffix=".json")
    os.close(out_fd)
    try:
        os.remove(out_path)  # the script (re)creates it; absence => script never ran
    except OSError:
        pass
    try:
        # qrenderdoc --python (v1.x): (1) does NOT populate sys.argv, and (2)
        # SWALLOWS the script's stdout. So pass parameters via the environment
        # (VR_RDC / VR_TEXTURE / VR_COORDS) and read the result back from a file
        # (VR_OUT) the script writes. A "--" argv tail is silently dropped.
        coord_env = ";".join(f"{int(x)},{int(y)}" for (x, y) in pixels)
        env = dict(os.environ)
        env["VR_RDC"] = os.path.abspath(rdc_path)
        env["VR_TEXTURE"] = str(texture)
        env["VR_COORDS"] = coord_env
        env["VR_OUT"] = out_path
        cmd = [qexe, "--python", script_path]
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
        payload = None
        # Prefer the result file (qrenderdoc swallows stdout); fall back to
        # scanning stdout for the JSON line (manual/CLI runs without VR_OUT).
        if os.path.isfile(out_path):
            try:
                with open(out_path, encoding="utf-8") as f:
                    payload = json.loads(f.read().strip())
            except (OSError, json.JSONDecodeError):
                payload = None
        if payload is None:
            for line in (proc.stdout or "").splitlines():
                line = line.strip()
                if line.startswith("{") and line.endswith("}"):
                    try:
                        payload = json.loads(line)
                    except json.JSONDecodeError:
                        continue
        if payload is None:
            raise RuntimeError(
                "no JSON from qrenderdoc --python (exit %d); VR_OUT not written "
                "(script may have failed to import renderdoc or hit a launch error)\n"
                "--- stdout ---\n%s\n--- stderr ---\n%s"
                % (proc.returncode, proc.stdout, proc.stderr)
            )
        return payload
    finally:
        for p in (script_path, out_path):
            try:
                os.unlink(p)
            except OSError:
                pass


# ── CLI ────────────────────────────────────────────────────────────────


def _parse_xy(s):
    a, b = s.split(",")
    return (int(float(a)), int(float(b)))


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Read pixels from a RenderDoc .rdc via qrenderdoc --python.")
    ap.add_argument("rdc", help="path to a .rdc capture")
    ap.add_argument("--pixels", nargs="+", required=True, metavar="X,Y",
                    help="one or more pixel coordinates, e.g. 640,360 100,200")
    ap.add_argument("--texture", default="backbuffer",
                    help='"backbuffer" (default) or a substring of a resource name '
                         '(e.g. tonemapped_image, color_image)')
    ap.add_argument("--qrenderdoc", default=None, help="path to qrenderdoc.exe")
    ap.add_argument("--timeout", type=int, default=120)
    ap.add_argument("--json", action="store_true", help="emit raw JSON")
    args = ap.parse_args(argv)

    coords = [_parse_xy(s) for s in args.pixels]
    try:
        result = read_pixels(args.rdc, coords, texture=args.texture,
                             qrenderdoc=args.qrenderdoc, timeout=args.timeout)
    except RuntimeError as e:
        sys.stderr.write(f"readback: {e}\n")
        return 2

    if args.json:
        print(json.dumps(result, indent=2))
        return 0 if result.get("ok") else 1

    if not result.get("ok"):
        sys.stderr.write(f"readback failed: {result.get('error')}\n")
        if "available" in result:
            sys.stderr.write("available textures:\n  " + "\n  ".join(result["available"]) + "\n")
        return 1
    t = result["texture"]
    print(f"texture: {t['name']}  ({t['width']}x{t['height']})")
    for p in result["pixels"]:
        if "error" in p:
            print(f"  ({p['x']},{p['y']}): {p['error']}")
        else:
            print(f"  ({p['x']},{p['y']}): byte={p['byte']}  float={[round(c,5) for c in p['floatValue']]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
