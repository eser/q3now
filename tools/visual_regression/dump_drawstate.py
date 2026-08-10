#!/usr/bin/env python3
"""dump_drawstate.py — dump draw-call list + per-draw blend state from a .rdc.

Same qrenderdoc --python shell-out pattern as readback.py (embedded RenderDoc
3.6 script, params via env, result via VR_OUT file). Walks every action in the
capture, and for each draw prints: eventId, name, vertex/index count, and the
colour-blend state (srcBlend/dstBlend/blendEnable) of colour attachment 0.

This answers "is the rocket explosion ADDITIVE (ONE/ONE) or ALPHA
(SRC_ALPHA/INV_SRC_ALPHA)?" from the real GPU pipeline state, not source code.

Usage:
    python dump_drawstate.py capture.rdc [--filter rocket,explosion,flame] [--json]
"""
from __future__ import annotations
import argparse, json, os, subprocess, sys, tempfile

_DEFAULT_QRENDERDOC = os.environ.get("QRENDERDOC", r"C:\Program Files\RenderDoc\qrenderdoc.exe")

_RDOC_SCRIPT = r'''
import sys, os, json
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
except Exception as e:
    emit({"ok": False, "error": "import renderdoc failed: %r" % (e,)}); os._exit(0)

rdc_path = os.environ.get("VR_RDC", "")
if not rdc_path:
    emit({"ok": False, "error": "missing VR_RDC"}); os._exit(0)

cap = rd.OpenCaptureFile()
res = cap.OpenFile(rdc_path, "", None)
ok = getattr(res, "OK", None)
good = res.OK() if callable(ok) else ((res == 0) or bool(res))
if not good:
    emit({"ok": False, "error": "OpenFile failed: %r" % (res,)}); os._exit(0)
open_res = cap.OpenCapture(rd.ReplayOptions(), None)
if isinstance(open_res, (tuple, list)) and len(open_res) == 2:
    status, controller = open_res
else:
    controller = open_res
if controller is None:
    emit({"ok": False, "error": "OpenCapture returned no controller"}); os._exit(0)

def blendstr(b):
    # b is a ColorBlend; map enums to names defensively
    def nm(x):
        return str(x).split(".")[-1]
    try:
        return {"enabled": bool(b.enabled),
                "srcCol": nm(b.colorBlend.source), "dstCol": nm(b.colorBlend.destination),
                "opCol": nm(b.colorBlend.operation)}
    except Exception:
        try:
            return {"enabled": bool(b.enabled),
                    "srcCol": nm(b.blend.source), "dstCol": nm(b.blend.destination)}
        except Exception as e:
            return {"err": repr(e)}

def flatten(actions, out):
    for a in actions:
        out.append(a)
        kids = getattr(a, "children", None)
        if kids:
            flatten(kids, out)

try:
    all_actions = []
    flatten(controller.GetRootActions(), all_actions)
    flt = [s.strip().lower() for s in (os.environ.get("VR_FILTER","") or "").split(",") if s.strip()]
    rows = []
    DRAW = rd.ActionFlags.Drawcall if hasattr(rd, "ActionFlags") else None
    for a in all_actions:
        # NOTE: a.GetName(sdf) CRASHES qrenderdoc --python headless on v1.44;
        # a.customName is the stable accessor (verified 2026-06-29).
        try:
            nm = str(a.customName) if a.customName else ""
        except Exception:
            nm = str(getattr(a, "name", ""))
        is_draw = True
        if DRAW is not None:
            try:
                is_draw = bool(int(a.flags) & int(DRAW))
            except Exception:
                is_draw = True
        if not is_draw:
            continue
        low = nm.lower()
        if flt and not any(f in low for f in flt):
            continue
        rec = {"eid": int(a.eventId), "name": nm,
               "numIndices": int(getattr(a, "numIndices", 0)),
               "numInstances": int(getattr(a, "numInstances", 0))}
        # pull blend state at this draw
        try:
            controller.SetFrameEvent(a.eventId, False)
            pipe = controller.GetPipelineState()
            blends = pipe.GetColorBlends()
            if blends and len(blends) > 0:
                rec["blend0"] = blendstr(blends[0])
                rec["nAttach"] = len(blends)
        except Exception as e:
            rec["blendErr"] = repr(e)
        rows.append(rec)
    emit({"ok": True, "count": len(rows), "draws": rows})
finally:
    try: controller.Shutdown()
    except Exception: pass
os._exit(0)
'''

def find_q(explicit=None):
    cand = explicit or _DEFAULT_QRENDERDOC
    if cand and os.path.isfile(cand):
        return cand
    from shutil import which
    return which("qrenderdoc") or which("qrenderdoc.exe")

def dump(rdc_path, *, filt="", qrenderdoc=None, timeout=180):
    qexe = find_q(qrenderdoc)
    if not qexe:
        raise RuntimeError("qrenderdoc.exe not found")
    if not os.path.isfile(rdc_path):
        raise RuntimeError(f"capture not found: {rdc_path}")
    with tempfile.NamedTemporaryFile("w", suffix=".py", delete=False, encoding="utf-8") as tf:
        tf.write(_RDOC_SCRIPT); script_path = tf.name
    out_fd, out_path = tempfile.mkstemp(suffix=".json"); os.close(out_fd)
    try: os.remove(out_path)
    except OSError: pass
    try:
        env = dict(os.environ)
        env["VR_RDC"] = os.path.abspath(rdc_path)
        env["VR_FILTER"] = filt
        env["VR_OUT"] = out_path
        proc = subprocess.run([qexe, "--python", script_path], capture_output=True, text=True, timeout=timeout, env=env)
        payload = None
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
                    try: payload = json.loads(line); break
                    except json.JSONDecodeError: continue
        if payload is None:
            raise RuntimeError("no JSON from qrenderdoc (exit %d)\n--stdout--\n%s\n--stderr--\n%s"
                               % (proc.returncode, proc.stdout[-2000:], proc.stderr[-2000:]))
        return payload
    finally:
        for p in (script_path, out_path):
            try: os.unlink(p)
            except OSError: pass

def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("rdc")
    ap.add_argument("--filter", default="", help="comma names substring filter, e.g. rocket,explos,flame")
    ap.add_argument("--qrenderdoc", default=None)
    ap.add_argument("--timeout", type=int, default=180)
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args(argv)
    try:
        r = dump(args.rdc, filt=args.filter, qrenderdoc=args.qrenderdoc, timeout=args.timeout)
    except RuntimeError as e:
        sys.stderr.write(f"dump_drawstate: {e}\n"); return 2
    if args.json:
        print(json.dumps(r, indent=2)); return 0 if r.get("ok") else 1
    if not r.get("ok"):
        sys.stderr.write(f"FAIL: {r.get('error')}\n"); return 1
    print(f"draws: {r['count']}")
    for d in r["draws"]:
        b = d.get("blend0", {})
        bs = f"blend(en={b.get('enabled')},src={b.get('srcCol')},dst={b.get('dstCol')})" if b else "blend(n/a)"
        print(f"  eid={d['eid']:5d} idx={d.get('numIndices',0):5d} {bs}  {d['name'][:80]}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
