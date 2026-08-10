#!/usr/bin/env bash
# wiredui-fwdplus-xorsplit-check.sh — guards the Forward+ XOR-split symmetry (#1).
#
# Bug #1 was a predicate ASYMMETRY: the plain-surface SKIP in RB_RenderLitSurfList
# gated on `r_forwardPlus->integer` only, while the two RB_RenderForwardPlusUnion
# RENDER sites also required `vk.fpActive`. When vk_forwardplus_dispatch bails for
# a frame (e.g. a singular view matrix → vk.fpActive false) the union never runs,
# so the buggy skip dropped plain dynamically-lit surfaces from BOTH passes (they
# lost their dlight that frame). The fix makes all three predicates identical:
#   r_forwardPlus && r_forwardPlus->integer && vk.fpActive
# so a plain surface is drawn exactly once in either fpActive state (union when
# active, per-light fallback when not).
#
# The bug's ONLY regression vector is a predicate diverging again, so the gate is
# a deterministic STATIC INVARIANT: every Forward+ XOR-split predicate in
# tr_backend.c (the skip + both union calls) must carry `vk.fpActive`. This is
# why the gate can be source-based — the bug is structural, not a pixel value.
# (A pixel A/B was attempted but is not viable headless: the synthetic test
# dlight r_dlightShadowTest does not populate the per-light litSurfs[] list in a
# headless capture, so there is no plain lit surface to route, and the spawn-area
# animation drift dwarfs any signal. The runtime both-fpActive-states check needs
# a real dlight lighting plain geometry, which only happens on a desktop/gameplay
# frame — so #1 is verified here by the deterministic static-symmetry invariant.)
#
# Self-test:  --self-test  (proves the gate FAILS on a re-introduced asymmetry)
# Usage:      tests/wiredui-fwdplus-xorsplit-check.sh
# Exit:       0 PASS   1 FAIL

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC="$REPO_ROOT/code/renderervk/tr_backend.c"

# Count, in tr_backend.c, the Forward+ XOR-split sites:
#   union-render sites: a call to RB_RenderForwardPlusUnion() guarded by a predicate
#   plain-skip site:    R_LitSurfIsPlain used as a skip predicate
# and assert EVERY one of those predicates contains `vk.fpActive`. The bug = a
# predicate without it.
analyze() {
    local src="$1"
    python3 - "$src" <<'PYEOF'
import sys
src = sys.argv[1]
lines = open(src, encoding="utf-8").read().splitlines()
issues=[]; sites=0
def window(i, back=0, fwd=0):
    lo=max(0,i-back); hi=min(len(lines),i+fwd+1)
    return " ".join(lines[lo:hi])
for i,l in enumerate(lines):
    # union-render site: a call to RB_RenderForwardPlusUnion(); its guard is the
    # immediately-preceding `if (...)` line.
    if "RB_RenderForwardPlusUnion()" in l and i>0 and "if (" in lines[i-1]:
        sites+=1
        pred = lines[i-1]
        if "vk.fpActive" not in pred:
            issues.append((i, "union-render guard lacks vk.fpActive", pred.strip()))
    # plain-skip site: the `if (...)` predicate using R_LitSurfIsPlain. The predicate
    # can wrap onto the next line, with the `if (` head on the PREVIOUS line — so test
    # a window spanning [prev .. this+next].
    if "R_LitSurfIsPlain(" in l:
        sites+=1
        w = window(i, back=1, fwd=1)
        if "r_forwardPlus" in w and "vk.fpActive" not in w:
            issues.append((i, "plain-skip predicate lacks vk.fpActive", w.strip()[:120]))
print(f"  Forward+ XOR-split predicate sites found: {sites}")
if sites < 3:
    print(f"  FAIL: expected >=3 XOR-split sites (1 skip + 2 union), found {sites} — structure changed")
    sys.exit(1)
if issues:
    for ln,why,txt in issues:
        print(f"  FAIL @ tr_backend.c:{ln+1}: {why}\n        {txt}")
    print("  → an asymmetric predicate: a plain surface can be dropped when fpActive is false (#1 regressed)")
    sys.exit(1)
print(f"  PASS: all {sites} XOR-split predicates carry vk.fpActive (skip == union) — #1 symmetric")
sys.exit(0)
PYEOF
}

if [ "${1:-}" = "--self-test" ]; then
    echo "==> Forward+ XOR-split check SELF-TEST (gate-has-teeth)"
    ST="$(mktemp -d -t wired-fpst-XXXXXX 2>/dev/null || mktemp -d)"
    trap 'rm -rf "$ST"' EXIT INT TERM
    # GOOD copy (current source, fixed).
    cp "$SRC" "$ST/good.c"
    # BROKEN copy: strip `&& vk.fpActive` from the plain-skip predicate (re-introduce #1).
    python3 - "$SRC" "$ST/broken.c" <<'PYEOF'
import sys
s=open(sys.argv[1],encoding="utf-8").read()
broken=s.replace(
"if ( r_forwardPlus && r_forwardPlus->integer && vk.fpActive\n\t\t\t&& R_LitSurfIsPlain( shader, dl, fogNum ) ) {",
"if ( r_forwardPlus && r_forwardPlus->integer\n\t\t\t&& R_LitSurfIsPlain( shader, dl, fogNum ) ) {", 1)
assert broken!=s, "self-test could not synthesize the buggy variant (skip predicate text changed)"
open(sys.argv[2],"w",encoding="utf-8").write(broken)
PYEOF
    rc_good=0; rc_broken=0
    echo "  -- GOOD source (fixed, symmetric) → expect PASS --"
    analyze "$ST/good.c"   || rc_good=$?
    echo "  -- BROKEN source (skip predicate missing vk.fpActive = #1) → expect FAIL --"
    analyze "$ST/broken.c" || rc_broken=$?
    if [ "$rc_good" -eq 0 ] && [ "$rc_broken" -ne 0 ]; then
        echo "==> SELF-TEST PASS: gate accepts the symmetric fix AND rejects a re-introduced asymmetry (it has teeth)"
        exit 0
    fi
    echo "==> SELF-TEST FAIL: good_rc=$rc_good (want 0), broken_rc=$rc_broken (want !=0)"
    exit 1
fi

echo "==> Forward+ XOR-split symmetry check (#1): $SRC"
analyze "$SRC"
RC=$?
[ "$RC" -eq 0 ] && echo "==> Forward+ XOR-split check: PASS" || echo "==> Forward+ XOR-split check: FAIL"
exit $RC
