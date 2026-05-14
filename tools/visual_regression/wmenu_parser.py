#!/usr/bin/env python3
"""wmenu_parser.py — extract authored colours from .wmenu files.

The Wired UI menu format (`.wmenu` / `.whud`) is parsed engine-side by
botlib's script parser, which understands `#include` / `#define`. Menu
files `#include "ui/wmenumacros.h"` and use object-like macros for the
intentional colour palette (WCOLOR_TEXT, WCOLOR_ACCENT, WCOLOR_BG, …) and
function-like widget macros (WBUTTON, WLABEL, …) that expand to whole
`itemDef {}` blocks.

This parser does a two-pass resolution:
  1. parse the object-like macros from wmenumacros.h (#define NAME value)
  2. tokenise the .wmenu file, walk menuDef / itemDef blocks, and for each
     block that carries a `name "..."` record its `backcolor` / `forecolor`
     (resolving any WCOLOR_* tokens via the macro table).

LIMITATION: function-like widget macros (`WBUTTON(...)` etc.) are *not*
expanded — those lines are skipped (a warning is recorded). Visual-
regression-tagged regions are expected to be plain `itemDef { name "..."
backcolor ... }` blocks per the VR workflow, so this is sufficient in
practice. If you need a widget-macro's embedded colour, add a plain
`itemDef` mirror with a `name` tag, or preprocess the file with `cpp`
first and point this parser at the expanded output.

Default macros path: <repo>/modfiles/ui/wmenumacros.h (the .wmenu source
tree lives in modfiles/ui/, *not* baseq3/ui/ — build/debug/pak-staging/ui/
holds the post-build staged copies).

CLI:
    python wmenu_parser.py modfiles/ui/main.wmenu
    python wmenu_parser.py modfiles/ui/main.wmenu --macros modfiles/ui/wmenumacros.h --json
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_REPO_ROOT = os.path.abspath(os.path.join(_THIS_DIR, "..", ".."))
DEFAULT_MACROS = os.path.join(_REPO_ROOT, "modfiles", "ui", "wmenumacros.h")

# A token is: a quoted string, a {, a }, or a run of non-space/non-brace chars.
_TOKEN_RE = re.compile(r'"(?:[^"\\]|\\.)*"|[{}]|[^\s{}]+')
# #define IDENT  rest-of-line   (object-like only — reject IDENT( ... ))
_DEFINE_RE = re.compile(r'^\s*#\s*define\s+([A-Za-z_]\w*)(?!\()\s+(.*?)\s*(?://.*)?$')
# A function-like-macro invocation at the start of significant content.
_FUNC_MACRO_CALL_RE = re.compile(r'^[A-Za-z_]\w*\s*\(')

_COLOR_KEYWORDS = ("backcolor", "forecolor", "bordercolor", "outlinecolor",
                   "focuscolor", "color2", "fadecolor", "disablecolor")
# Keys we surface in the per-element record (mapped to friendlier names).
_COLOR_OUT_KEYS = {
    "backcolor": "backcolor",
    "forecolor": "forecolor",
    "focuscolor": "focuscolor",
}


def parse_macros(path):
    """Return {NAME: 'value text'} for object-like #defines in `path`.

    Function-like macros (NAME(args)) are ignored. Line continuations are
    joined. Values are stored verbatim (not yet split into floats)."""
    macros = {}
    if not path or not os.path.isfile(path):
        return macros
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        raw = fh.read()
    # Join backslash-newline continuations.
    raw = re.sub(r"\\\r?\n", " ", raw)
    for line in raw.splitlines():
        m = _DEFINE_RE.match(line)
        if m:
            macros[m.group(1)] = m.group(2).strip()
    return macros


def _resolve_token(tok, macros, _depth=0):
    """Recursively resolve a single token through the macro table.
    Returns the (possibly multi-token) replacement text, or the token
    itself if it is not a macro."""
    if _depth > 16:
        return tok
    if tok in macros:
        # The replacement may itself contain macro names — resolve each.
        parts = macros[tok].split()
        out = []
        for p in parts:
            out.append(_resolve_token(p, macros, _depth + 1))
        return " ".join(out)
    return tok


def _floats_from_tokens(tokens, macros):
    """Given the tokens that follow a colour keyword (already sliced up to
    the next keyword/brace), resolve macros and pull out up to 4 floats.
    Pads alpha to 1.0 if only 3 are present. Returns None if no floats."""
    flat = []
    for t in tokens:
        for piece in _resolve_token(t, macros).split():
            try:
                flat.append(float(piece))
            except ValueError:
                # non-numeric (e.g. a stray identifier) — stop; colours are
                # whitespace-separated numeric literals or a single macro.
                if flat:
                    break
                return None
    if not flat:
        return None
    rgba = (flat + [1.0, 1.0, 1.0, 1.0])[:4]
    return rgba


def parse_wmenu(path, macros_path=None):
    """Parse a .wmenu file. Returns a dict:
        {
          "elements": { name: {"kind": "menu"|"item",
                                "backcolor": [r,g,b,a]?,
                                "forecolor": [r,g,b,a]?,
                                "focuscolor": [r,g,b,a]? } },
          "warnings": [str, ...],
        }
    Only blocks that declare a `name "..."` are recorded."""
    macros_path = macros_path or DEFAULT_MACROS
    macros = parse_macros(macros_path)
    warnings = []

    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        src = fh.read()

    # Strip // comments and /* */ comments (botlib's parser handles these;
    # we approximate — good enough for colour extraction).
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    src = re.sub(r"//[^\n]*", "", src)
    # Drop preprocessor lines (#include, #define, #if, …) — macros already
    # parsed from wmenumacros.h; nested #defines inside .wmenu are rare.
    keep_lines = []
    for line in src.splitlines():
        s = line.lstrip()
        if s.startswith("#"):
            continue
        # Skip function-like widget-macro invocations (can't expand them).
        if _FUNC_MACRO_CALL_RE.match(s):
            name = s.split("(", 1)[0].strip()
            if name in macros:
                pass  # object-like macro that looks like a call — unlikely; keep
            else:
                warnings.append(f"skipped function-like macro invocation: {name}(...)")
                continue
        keep_lines.append(line)
    src = "\n".join(keep_lines)

    tokens = _TOKEN_RE.findall(src)

    elements = {}
    # Stack of dicts describing the currently-open blocks.
    stack = []

    i = 0
    n = len(tokens)
    while i < n:
        tok = tokens[i]
        low = tok.lower()
        if low in ("menudef", "itemdef"):
            # Expect a "{" soon; open a block.
            kind = "menu" if low == "menudef" else "item"
            # find the opening brace
            j = i + 1
            while j < n and tokens[j] != "{":
                j += 1
            stack.append({"kind": kind, "name": None, "colors": {}})
            i = j + 1
            continue
        if tok == "{":
            # An anonymous nested block (e.g. action {...}, mouseEnter {...},
            # column {...}); push a placeholder so braces balance.
            stack.append(None)
            i += 1
            continue
        if tok == "}":
            if stack:
                blk = stack.pop()
                if blk is not None and blk.get("name"):
                    rec = {"kind": blk["kind"]}
                    rec.update(blk["colors"])
                    # last definition wins if a name repeats
                    elements[blk["name"]] = rec
            i += 1
            continue

        # We only care about keywords inside a *real* block (top of stack
        # is a dict). If top is None (anonymous block) or empty, skip.
        cur = stack[-1] if stack else None
        if isinstance(cur, dict):
            if low == "name" and i + 1 < n:
                val = tokens[i + 1]
                if val.startswith('"') and val.endswith('"'):
                    cur["name"] = val[1:-1]
                else:
                    cur["name"] = val
                i += 2
                continue
            if low in _COLOR_KEYWORDS:
                # Gather following tokens until the next recognised keyword,
                # brace, or quoted string (colours are bare numeric tokens
                # or one macro identifier).
                j = i + 1
                arg_tokens = []
                while j < n:
                    t = tokens[j]
                    tl = t.lower()
                    if t in ("{", "}"):
                        break
                    if t.startswith('"'):
                        break
                    if tl in _COLOR_KEYWORDS or tl in ("name", "rect", "type", "text",
                                                       "font", "visible", "decoration",
                                                       "style", "ownerdraw", "action",
                                                       "menudef", "itemdef", "cvar",
                                                       "background", "textalign", "grow"):
                        break
                    arg_tokens.append(t)
                    j += 1
                    if len(arg_tokens) >= 6:   # colours never need more than this
                        break
                rgba = _floats_from_tokens(arg_tokens, macros)
                out_key = _COLOR_OUT_KEYS.get(low)
                if rgba is not None and out_key is not None:
                    cur["colors"][out_key] = rgba
                i = j
                continue
        i += 1

    if stack:
        warnings.append(f"unbalanced braces — {len(stack)} block(s) left open at EOF")

    return {"elements": elements, "warnings": warnings}


def resolve_macro_color(macro_name, macros_path=None):
    """Resolve a bare WCOLOR_* (or any object-like colour macro) to [r,g,b,a]."""
    macros = parse_macros(macros_path or DEFAULT_MACROS)
    if macro_name not in macros:
        raise KeyError(f"macro {macro_name!r} not found in {macros_path or DEFAULT_MACROS}")
    rgba = _floats_from_tokens([macro_name], macros)
    if rgba is None:
        raise ValueError(f"macro {macro_name!r} = {macros[macro_name]!r} is not a colour")
    return rgba


def resolve_authored_color(source, macros_path=None):
    """Resolve a scene-YAML region `source` spec to [r,g,b,a].

    Accepts one of:
      {"rgba": [r,g,b,a]}                       — literal
      {"macro": "WCOLOR_ACCENT"}                — wmenumacros.h object macro
      {"wmenu": "modfiles/ui/main.wmenu",       — .wmenu element's colour
       "element": "main", "channel": "backcolor"}
        (channel defaults to "backcolor"; element defaults to the region name
         when the caller passes it in — handled by vr_check, not here)
    """
    if "rgba" in source:
        vals = list(source["rgba"])
        return (vals + [1.0, 1.0, 1.0, 1.0])[:4]
    if "macro" in source:
        return resolve_macro_color(source["macro"], macros_path)
    if "wmenu" in source:
        wm_path = source["wmenu"]
        if not os.path.isabs(wm_path):
            wm_path = os.path.join(_REPO_ROOT, wm_path)
        parsed = parse_wmenu(wm_path, macros_path)
        elem_name = source.get("element")
        if elem_name is None:
            raise ValueError("wmenu source requires an 'element' (or pass region name)")
        if elem_name not in parsed["elements"]:
            raise KeyError(f"element {elem_name!r} not found in {wm_path} "
                           f"(have: {sorted(parsed['elements'])[:20]}...)")
        channel = source.get("channel", "backcolor")
        rec = parsed["elements"][elem_name]
        if channel not in rec:
            raise KeyError(f"element {elem_name!r} has no {channel!r} colour "
                           f"(has: {[k for k in rec if k != 'kind']})")
        return list(rec[channel])
    raise ValueError(f"unrecognised source spec: {source!r}")


# ── CLI ────────────────────────────────────────────────────────────────


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Extract authored colours from a .wmenu file.")
    ap.add_argument("wmenu", help="path to a .wmenu file")
    ap.add_argument("--macros", default=DEFAULT_MACROS,
                    help=f"path to wmenumacros.h (default: {DEFAULT_MACROS})")
    ap.add_argument("--json", action="store_true", help="emit JSON")
    args = ap.parse_args(argv)

    if not os.path.isfile(args.wmenu):
        ap.error(f"no such file: {args.wmenu}")

    result = parse_wmenu(args.wmenu, args.macros)
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        macros = parse_macros(args.macros)
        print(f"# macros loaded from {args.macros}: {len(macros)} object-like #defines")
        wcol = {k: v for k, v in macros.items() if k.startswith("WCOLOR_")}
        for k in sorted(wcol):
            try:
                print(f"  {k:22s} -> {resolve_macro_color(k, args.macros)}")
            except Exception as e:  # noqa: BLE001
                print(f"  {k:22s} -> (not a colour: {e})")
        print(f"\n# named elements in {args.wmenu}: {len(result['elements'])}")
        for name in sorted(result["elements"]):
            rec = result["elements"][name]
            cols = {k: v for k, v in rec.items() if k != "kind"}
            print(f"  [{rec['kind']:4s}] {name:28s} {cols if cols else '(no colour keywords)'}")
        if result["warnings"]:
            print("\n# warnings:")
            for w in result["warnings"]:
                print(f"  - {w}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
