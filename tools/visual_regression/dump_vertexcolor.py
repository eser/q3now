#!/usr/bin/env python3
"""dump_vertexcolor.py — dump per-draw vertex INPUT color + screen position.

Sibling of dump_drawstate.py (same qrenderdoc --python shell-out: embedded
RenderDoc 3.6 script, params via env, result via VR_OUT, os._exit(0), the
v1.44 customName/SetFrameEvent gotchas). Where dump_drawstate.py reads the
*blend state* of each draw, this reads the *vertex-input attributes* — the
COLOR0 (vertex color, RGBA) and POSITION (screen xy for 2D HUD) of the FIRST
vertex of each draw.

Why: the q3now 2D HUD fills panels with `trap_R_SetColor(bgColor)` +
whiteShader (RB_AddQuadStamp2 copies backEnd.color2D into tess.vertexColors).
So a panel fill's vertex COLOR0 == the bgColor that actually reached the GPU.
Reading it answers "did the 0.18 panel-bg alpha survive into the draw, or is
it clamped to 1.0 (opaque)?" — directly from GPU vertex data, not source.

It decodes attributes from the bound vertex buffers via
GetVertexInputs()/GetVBuffers()/GetBufferData() and the attribute's
ResourceFormat (offset/stride/compType/compCount/compByteWidth). Color is
reported both raw and normalised; alpha is called out.

Usage:
    python dump_vertexcolor.py capture.rdc --eids 721,726,730 [--json]
    python dump_vertexcolor.py capture.rdc --eid-range 700,940 [--json]
"""
from __future__ import annotations
import argparse, json, os, subprocess, sys, tempfile

_DEFAULT_QRENDERDOC = os.environ.get("QRENDERDOC", r"C:\Program Files\RenderDoc\qrenderdoc.exe")

_RDOC_SCRIPT = r'''
import sys, os, json, struct
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

want_eids = set()
for s in (os.environ.get("VR_EIDS","") or "").split(","):
    s = s.strip()
    if s:
        try: want_eids.add(int(s))
        except ValueError: pass
rng = (os.environ.get("VR_EID_RANGE","") or "").strip()
eid_lo, eid_hi = None, None
if rng and "," in rng:
    a,b = rng.split(",",1)
    try: eid_lo, eid_hi = int(a), int(b)
    except ValueError: pass

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

def nm(x):
    return str(x).split(".")[-1]

# Decode `count` scalar components of `comp_type`/`byte_width` from raw bytes.
def decode_components(buf, base, comp_type, byte_width, count):
    ct = nm(comp_type)  # UNorm, SNorm, UInt, SInt, Float, ...
    vals_raw = []
    vals_norm = []
    for i in range(count):
        off = base + i * byte_width
        chunk = bytes(buf[off:off+byte_width])
        if len(chunk) < byte_width:
            return None, None
        if ct == "Float":
            if byte_width == 4:
                v = struct.unpack("<f", chunk)[0]
            elif byte_width == 2:
                # half float
                h = struct.unpack("<H", chunk)[0]
                v = _half_to_float(h)
            elif byte_width == 8:
                v = struct.unpack("<d", chunk)[0]
            else:
                return None, None
            vals_raw.append(v); vals_norm.append(v)
        else:
            # integer-ish
            signed = ct in ("SNorm", "SInt", "SScaled")
            iv = int.from_bytes(chunk, "little", signed=signed)
            vals_raw.append(iv)
            if ct == "UNorm":
                maxv = (1 << (8*byte_width)) - 1
                vals_norm.append(iv / maxv if maxv else 0.0)
            elif ct == "SNorm":
                maxv = (1 << (8*byte_width - 1)) - 1
                vals_norm.append(max(-1.0, iv / maxv) if maxv else 0.0)
            else:
                vals_norm.append(float(iv))
    return vals_raw, vals_norm

def _half_to_float(h):
    s = (h >> 15) & 0x1
    e = (h >> 10) & 0x1f
    m = h & 0x3ff
    if e == 0:
        f = (m / 1024.0) * (2.0 ** -14)
    elif e == 31:
        f = float("inf") if m == 0 else float("nan")
    else:
        f = (1.0 + m / 1024.0) * (2.0 ** (e - 15))
    return -f if s else f

def flatten(actions, out):
    for a in actions:
        out.append(a)
        kids = getattr(a, "children", None)
        if kids:
            flatten(kids, out)

try:
    all_actions = []
    flatten(controller.GetRootActions(), all_actions)
    DRAW = rd.ActionFlags.Drawcall if hasattr(rd, "ActionFlags") else None
    rows = []
    for a in all_actions:
        is_draw = True
        if DRAW is not None:
            try: is_draw = bool(int(a.flags) & int(DRAW))
            except Exception: is_draw = True
        if not is_draw:
            continue
        eid = int(a.eventId)
        if want_eids and eid not in want_eids:
            continue
        if eid_lo is not None and not (eid_lo <= eid <= eid_hi):
            continue
        rec = {"eid": eid, "numIndices": int(getattr(a, "numIndices", 0))}
        try:
            controller.SetFrameEvent(eid, False)
            pipe = controller.GetPipelineState()
            attrs = pipe.GetVertexInputs()
            vbs = pipe.GetVBuffers()

            # blend (so each row is self-describing)
            try:
                blends = pipe.GetColorBlends()
                if blends:
                    b = blends[0]
                    rec["blend"] = {"en": bool(b.enabled),
                                    "src": nm(b.colorBlend.source),
                                    "dst": nm(b.colorBlend.destination)}
            except Exception:
                pass

            # index of the first vertex to read: use the draw's vertexOffset+0.
            # For an indexed draw we want the first *index*, but for a uniform
            # quad (all 4 verts share color) any vertex gives the same COLOR0,
            # so reading vertex 0 of the bound VB at vertexOffset is sufficient.
            base_vtx = int(getattr(a, "baseVertex", 0) or 0)
            vtx_off = int(getattr(a, "vertexOffset", 0) or 0)
            first_vtx = vtx_off + base_vtx
            nverts = int(os.environ.get("VR_NVERTS", "1") or "1")
            if nverts < 1:
                nverts = 1

            attr_out = []
            for at in attrs:
                aname = ""
                try: aname = str(at.name)
                except Exception: aname = ""
                used = True
                try: used = bool(at.used)
                except Exception: pass
                vbslot = int(getattr(at, "vertexBuffer", 0))
                rel_off = int(getattr(at, "byteOffset", 0))
                fmt = at.format
                comp_type = fmt.compType
                comp_count = int(fmt.compCount)
                comp_bw = int(getattr(fmt, "compByteWidth", 0))
                if comp_bw == 0:
                    # derive from compType / known
                    comp_bw = 4
                if vbslot >= len(vbs):
                    attr_out.append({"name": aname, "err": "vb slot %d OOB" % vbslot})
                    continue
                vb = vbs[vbslot]
                vb_res = vb.resourceId
                vb_byteoff = int(getattr(vb, "byteOffset", 0))
                vb_stride = int(getattr(vb, "byteStride", 0))
                if vb_stride == 0:
                    vb_stride = comp_count * comp_bw
                per_vert = []
                for vi in range(nverts):
                    abs_off = vb_byteoff + (first_vtx + vi) * vb_stride + rel_off
                    need = comp_count * comp_bw
                    try:
                        raw = controller.GetBufferData(vb_res, abs_off, need)
                    except Exception as e:
                        per_vert.append({"err": "GetBufferData: %r" % e}); continue
                    vr, vn = decode_components(raw, 0, comp_type, comp_bw, comp_count)
                    per_vert.append({"raw": vr, "norm": vn})
                entry = {
                    "name": aname, "used": used,
                    "fmt": nm(comp_type), "count": comp_count, "bw": comp_bw,
                    "vbslot": vbslot, "rel_off": rel_off, "stride": vb_stride,
                    "first_vtx": first_vtx,
                    "raw": per_vert[0].get("raw"), "norm": per_vert[0].get("norm"),
                }
                if nverts > 1:
                    entry["verts"] = per_vert
                attr_out.append(entry)
            rec["attrs"] = attr_out
        except Exception as e:
            rec["err"] = repr(e)
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

def dump(rdc_path, *, eids="", eid_range="", qrenderdoc=None, timeout=240):
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
        env["VR_EIDS"] = eids
        env["VR_EID_RANGE"] = eid_range
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
    ap.add_argument("--eids", default="", help="comma-separated eventIds, e.g. 721,726,730")
    ap.add_argument("--eid-range", default="", help="lo,hi inclusive, e.g. 700,940")
    ap.add_argument("--qrenderdoc", default=None)
    ap.add_argument("--timeout", type=int, default=240)
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args(argv)
    try:
        r = dump(args.rdc, eids=args.eids, eid_range=args.eid_range,
                 qrenderdoc=args.qrenderdoc, timeout=args.timeout)
    except RuntimeError as e:
        sys.stderr.write(f"dump_vertexcolor: {e}\n"); return 2
    if args.json:
        print(json.dumps(r, indent=2)); return 0 if r.get("ok") else 1
    if not r.get("ok"):
        sys.stderr.write(f"FAIL: {r.get('error')}\n"); return 1
    print(f"draws: {r['count']}")
    for d in r["draws"]:
        print(f"  eid={d['eid']:5d} idx={d.get('numIndices',0):5d} blend={d.get('blend')}")
        for at in d.get("attrs", []):
            if "err" in at:
                print(f"      {at['name']:12s} ERR {at['err']}")
            else:
                print(f"      {at.get('name',''):12s} fmt={at['fmt']}x{at['count']} raw={at['raw']} norm={[round(v,4) for v in (at['norm'] or [])]}")
        if "err" in d:
            print(f"      DRAW-ERR {d['err']}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
