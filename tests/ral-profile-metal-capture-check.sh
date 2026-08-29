#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# macOS independent-tool receipt for Vulkan debug-utils labels translated by
# MoltenVK into native Metal debug groups. The companion marker harness proves
# balanced engine emission in the same process; this script proves the labels
# reached an actual one-frame .gputrace without requiring Xcode control.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MARKER_HARNESS="$SCRIPT_DIR/ral-profile-markers-check.sh"
EXPECTED_NAMES='wired.main,wired.tonemap,wired.ui,wired.scene-depth-resume,wired.present'

analyze_capture() {
python3 - "$1" "$2" "$EXPECTED_NAMES" <<'PYEOF'
import json,os,plistlib,sys
root,output,expected_csv=sys.argv[1:]
expected=expected_csv.split(",")
if not os.path.isdir(root):raise SystemExit("FAIL Metal capture package missing")
for name in ("capture","unsorted-capture","metadata"):
 path=os.path.join(root,name)
 if not os.path.isfile(path) or os.path.getsize(path)<=0:raise SystemExit(f"FAIL Metal capture member {name}")
with open(os.path.join(root,"metadata"),"rb") as src:metadata=plistlib.load(src)
if metadata.get("DYCaptureEngine.captured_frames_count")!=1:raise SystemExit("FAIL Metal capture frame cardinality")
data=open(os.path.join(root,"capture"),"rb").read()
scopes=[]
for name in expected:
 token=name.encode("ascii")
 if data.count(token)!=1:raise SystemExit(f"FAIL Metal debug-group cardinality {name}: {data.count(token)}")
 scopes.append({"name":name,"eventId":data.index(token),"depth":0,"children":0,"actions":0})
ids=[scope["eventId"] for scope in scopes]
if len(set(ids))!=len(ids):raise SystemExit(f"FAIL Metal debug-group record identity: {ids}")
# children/actions are intentionally zero: the public .gputrace package exposes
# label records but no supported CLI event-tree API. Inventing action counts
# would be weaker evidence than retaining the unknown values. Balanced nesting
# is joined from the exact-process marker receipt run immediately beforehand.
readout={"ok":True,"api":"Metal","labelSource":"moltenvk-vulkan-debug-utils",
 "unbalanced":0,"scopes":scopes}
with open(output,"w") as out:json.dump(readout,out,sort_keys=True)
print("PASS Xcode Metal capture receipt: 1 frame, 5 semantic debug groups")
PYEOF
}

write_self() {
python3 - "$1" "$2" <<'PYEOF'
import os,plistlib,sys
root,mode=sys.argv[1:]
os.makedirs(root,exist_ok=True)
names=["wired.main","wired.tonemap","wired.ui","wired.scene-depth-resume","wired.present"]
if mode=="missing":names.remove("wired.ui")
elif mode=="duplicate":names.insert(3,"wired.ui")
elif mode=="renamed":names[2]="wired.hud"
payload=("header\0"+"\0record\0".join(names)+"\0tail").encode()
for member in ("capture","unsorted-capture"):
 open(os.path.join(root,member),"wb").write(payload)
frames=2 if mode=="frames" else 1
with open(os.path.join(root,"metadata"),"wb") as out:
 plistlib.dump({"DYCaptureEngine.captured_frames_count":frames,"(uuid)":"fixture"},out,fmt=plistlib.FMT_BINARY)
if mode=="empty":open(os.path.join(root,"unsorted-capture"),"wb").close()
PYEOF
}

if [ "${1:-}" = --analyze ];then
	[ "$#" -eq 3 ] || { echo "usage: $0 --analyze <capture.gputrace> <readout.json>";exit 64; }
	analyze_capture "$2" "$3";exit $?
fi

if [ "${1:-}" = --self-test ];then
	SELF_ROOT="$(mktemp -d -t ral-metal-capture-self-XXXXXX 2>/dev/null || mktemp -d)"
	trap 'rm -rf "$SELF_ROOT"' EXIT
	write_self "$SELF_ROOT/clean.gputrace" clean
	analyze_capture "$SELF_ROOT/clean.gputrace" "$SELF_ROOT/clean.json" >/dev/null || { echo "FAIL clean Metal fixture rejected";exit 1; }
	for defect in missing duplicate renamed frames empty;do
		write_self "$SELF_ROOT/$defect.gputrace" "$defect"
		if analyze_capture "$SELF_ROOT/$defect.gputrace" "$SELF_ROOT/$defect.json" >/dev/null 2>&1;then
			echo "FAIL accepted Metal capture defect $defect";exit 1
		fi
	done
	echo "PASS ral-profile-metal-capture analyzer self-test (5 mutations)";exit 0
fi

WIRED="${1:-}"
[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired";exit 64; }
[ "$(uname -s)" = Darwin ] || { echo "SKIP: Xcode Metal capture requires macOS";exit 77; }
[ -x "$MARKER_HARNESS" ] || [ -f "$MARKER_HARNESS" ] || { echo "FAIL marker harness missing";exit 1; }

KEEP="${WIRED_KEEP_ARTIFACTS:-0}"
ROOT="$(mktemp -d -t ral-profile-metal-XXXXXX 2>/dev/null || mktemp -d)"
CHILD_ROOTS=""
cleanup() {
	status=$?;trap - EXIT INT TERM
	if [ "$KEEP" != 1 ];then
		for child in $CHILD_ROOTS;do rm -rf "$child";done
		rm -rf "$ROOT"
	fi
	exit "$status"
}
trap cleanup EXIT;trap 'exit 130' INT;trap 'exit 143' TERM

run=1
while [ "$run" -le 2 ];do
	LOG="$ROOT/run-$run.log"
	if ! env WIRED_METAL_CAPTURE=1 WIRED_KEEP_ARTIFACTS=1 \
		"$MARKER_HARNESS" "$WIRED" >"$LOG" 2>&1;then
		cat "$LOG";echo "FAIL Metal capture run $run";exit 1
	fi
	cat "$LOG"
	CHILD="$(sed -n 's/^PASS retained root: //p' "$LOG" | tail -n 1)"
	[ -n "$CHILD" ] && [ -d "$CHILD/wired-profile.gputrace" ] || { echo "FAIL Metal capture root run $run";exit 1; }
	CHILD_ROOTS="$CHILD_ROOTS $CHILD"
	analyze_capture "$CHILD/wired-profile.gputrace" "$ROOT/run-$run.json" || exit 1
	run=$((run+1))
done

python3 - "$ROOT/run-1.json" "$ROOT/run-2.json" ${CHILD_ROOTS} <<'PYEOF'
import json,os,plistlib,sys
one,two,root1,root2=sys.argv[1:]
a=json.load(open(one));b=json.load(open(two))
if [s["name"] for s in a["scopes"]] != [s["name"] for s in b["scopes"]]:raise SystemExit("FAIL Metal N=2 semantic-name drift")
def uuid(root):
 with open(os.path.join(root,"wired-profile.gputrace","metadata"),"rb") as src:return plistlib.load(src).get("(uuid)")
if not uuid(root1) or not uuid(root2) or uuid(root1)==uuid(root2):raise SystemExit("FAIL Metal N=2 process independence")
print("PASS Xcode Metal capture N=2 independent-process receipt")
PYEOF
echo "PASS Metal capture artifacts: $ROOT"
