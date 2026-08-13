#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# Loose-demo disappearance through the authored Main -> Demos -> Play route.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WATCHER="$SCRIPT_DIR/wiredui-demo-disappeared-watcher.py"
CFG_SOURCE="$SCRIPT_DIR/fixtures/wiredui-demo-disappeared.cfg"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
DEMO_HEX="0100000000000000ffffffffffffffff"
DEMO_SHA="db0550d553e2a146e34164d19cd55f006c38d700d8f9a4e3ba2c889a1d7c26b2"

analyze_contract() {
python3 - "$1" "$2" "$DEMO_SHA" <<'PYEOF'
import json,re,sys
log_path,event_path,wanted_sha=sys.argv[1:]
def load(path):
 out=[]
 for n,line in enumerate(open(path,encoding="utf-8",errors="replace"),1):
  if not line.strip():continue
  try:row=json.loads(line)
  except Exception as exc:raise SystemExit(f"FAIL disappeared {path}:{n}: {exc}")
  if not isinstance(row,dict):raise SystemExit("FAIL disappeared schema")
  out.append(row)
 return out
rows=load(log_path);events=load(event_path)
if not rows or not events:raise SystemExit("FAIL disappeared empty evidence")
msgs=[str(r.get("msg","")) for r in rows]
def norm(v):return v[:-1] if v.endswith("\n") and not v.endswith("\n\n") else v
vals=[norm(v) for v in msgs]
claimed=("WiredUI: push menu ","WiredUI: pop menu ","WiredUI: demos loaded ",
 "WiredUI: demo feeder row=","WiredUI: demo feeder selection ",
 "WiredUI: queued validated demo playback ","WiredUI: close all postcondition ",
 "wui_menu_nav:","wui_menu_nav focus:","Demo playback open ",
 "Not found: demos/","WiredUI: no demo selected",
 "Error: The selected demo ","Demo file: demos/","Demo playback rejected ",
 "Demo playback recovery ","CL_NextDemo:","Q0_DEMO_DISAPPEAR_")
for value in vals:
 parts=re.split(r"[\r\n]",value)
 if len(parts)>1 and any(part.startswith(claimed) for part in parts):
  raise SystemExit("FAIL disappeared claimed logical-line smuggling")
def family(prefix):return [(i,rows[i],v) for i,v in enumerate(vals) if v.startswith(prefix)]
def exact(prefix,wanted,sev,cat,label):
 hit=family(prefix)
 if len(hit)!=1:raise SystemExit(f"FAIL disappeared {label} cardinality {len(hit)}")
 i,row,value=hit[0]
 if value!=wanted or str(row.get("sev","")).upper()!=sev or str(row.get("cat","")).lower()!=cat:
  raise SystemExit(f"FAIL disappeared {label} body/metadata")
 return i

pushes=family("WiredUI: push menu ")
wanted_push=["WiredUI: push menu 'main' (depth 1)","WiredUI: push menu 'demos' (depth 2)",
 "WiredUI: push menu 'main' (depth 1)","WiredUI: push menu 'demos' (depth 2)",
 "WiredUI: push menu 'error_popup' (depth 3)"]
if [x[2] for x in pushes]!=wanted_push:raise SystemExit("FAIL disappeared push inventory")
if any(str(x[1].get("sev","")).upper()!="DEBUG" or str(x[1].get("cat","")).lower()!="ui" for x in pushes):
 raise SystemExit("FAIL disappeared push metadata")
loads=family("WiredUI: demos loaded ")
if [x[2] for x in loads] != [
 "WiredUI: demos loaded protocol=74 count=1 generation=1",
 "WiredUI: demos loaded protocol=74 count=1 generation=2",
 "WiredUI: demos loaded protocol=74 count=0 generation=3"]:
 raise SystemExit("FAIL disappeared inventory lifecycle")
feed=family("WiredUI: demo feeder row=")
if [x[2] for x in feed] != [
 "WiredUI: demo feeder row=0 name=stale.loose generation=1",
 "WiredUI: demo feeder row=0 name=stale.loose generation=2"]:
 raise SystemExit("FAIL disappeared feeder inventory")
if any(str(x[1].get("sev","")).upper()!="DEBUG" or str(x[1].get("cat","")).lower()!="ui" for x in loads+feed):
 raise SystemExit("FAIL disappeared inventory metadata")
selection=exact("WiredUI: demo feeder selection ",
 "WiredUI: demo feeder selection row=0 name=stale.loose generation=2","DEBUG","ui","selection")
armed=exact("Q0_DEMO_DISAPPEAR_ARMED","Q0_DEMO_DISAPPEAR_ARMED","INFO","system","armed")
queue=exact("WiredUI: queued validated demo playback ",
 "WiredUI: queued validated demo playback name=stale.loose","DEBUG","ui","queue")
close=exact("WiredUI: close all postcondition ",
 "WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0","DEBUG","ui","close")
not_found=exact("Not found: demos/","Not found: demos/stale.loose.dm_74","INFO","client","filesystem miss")
reject=exact("Demo playback open rejected ",
 "Demo playback open rejected origin=demo-ui stage=probe continuation=0 disconnect=0","INFO","client","open rejection")
error=exact("Error: The selected demo ",
 "Error: The selected demo is no longer available.","ERROR","system","generic error")
recovery=exact("Demo playback open recovery ",
 "Demo playback open recovery origin=demo-ui stage=probe depth=3 popup=1","INFO","client","recovery")
open_family=family("Demo playback open ")
if [x[2] for x in open_family] != [
 "Demo playback open rejected origin=demo-ui stage=probe continuation=0 disconnect=0",
 "Demo playback open recovery origin=demo-ui stage=probe depth=3 popup=1"]:
 raise SystemExit("FAIL disappeared open lifecycle family")
if any(str(x[1].get("sev","")).upper()!="INFO" or str(x[1].get("cat","")).lower()!="client" for x in open_family):
 raise SystemExit("FAIL disappeared open lifecycle metadata")
complete=exact("Q0_DEMO_DISAPPEAR_COMPLETE","Q0_DEMO_DISAPPEAR_COMPLETE","INFO","system","complete")
refusal=exact("WiredUI: no demo selected","WiredUI: no demo selected","WARN","ui","empty post-dismiss refusal")
nav=family("wui_menu_nav:")
if [x[2] for x in nav] != ["wui_menu_nav: K_DOWNARROW dispatched"]*3+["wui_menu_nav: K_ENTER dispatched"]*4:
 raise SystemExit("FAIL disappeared nav inventory")
focus=family("wui_menu_nav focus:")
wanted_focus=("focused item 'demolist'","focused item 'btn_play_demo'","focused item 'btn_back'","focused item 'btn_play_demo'")
if len(focus)!=4 or any(wanted_focus[i] not in focus[i][2] for i in range(4)):
 raise SystemExit("FAIL disappeared focus inventory")
if any(str(x[1].get("sev","")).upper()!="DEBUG" or str(x[1].get("cat","")).lower()!="ui" for x in nav+focus):
 raise SystemExit("FAIL disappeared navigation metadata")
pops=family("WiredUI: pop menu ")
if (len(pops)!=1 or pops[0][2]!="WiredUI: pop menu (depth 2)"
 or str(pops[0][1].get("sev","")).upper()!="DEBUG" or str(pops[0][1].get("cat","")).lower()!="ui"):
 raise SystemExit("FAIL disappeared popup dismissal")
state_rows=[(i,r,v) for i,r,v in [(i,rows[i],vals[i]) for i in range(len(rows))] if v.startswith('"com_errorMessage" is:')]
if [x[2] for x in state_rows] != ['"com_errorMessage" is:"The selected demo is no longer available."','"com_errorMessage" is:""']:
 raise SystemExit("FAIL disappeared error state")
if any(str(x[1].get("sev","")).upper()!="INFO" or str(x[1].get("cat","")).lower()!="system" for x in state_rows):
 raise SystemExit("FAIL disappeared error state metadata")
retry_rows=[(i,rows[i],vals[i]) for i in range(len(rows)) if vals[i].startswith('"ui_errorRetry" is:')]
if [x[2] for x in retry_rows] != ['"ui_errorRetry" is:"0"']:raise SystemExit("FAIL disappeared retry state")
if any(str(x[1].get("sev","")).upper()!="INFO" or str(x[1].get("cat","")).lower()!="system" for x in retry_rows):
 raise SystemExit("FAIL disappeared retry state metadata")
nextdemo_rows=[(i,rows[i],vals[i]) for i in range(len(rows)) if vals[i].startswith('"nextdemo" is:')]
if [x[2] for x in nextdemo_rows] != ['"nextdemo" is:""']:raise SystemExit("FAIL disappeared continuation state")
if any(str(x[1].get("sev","")).upper()!="INFO" or str(x[1].get("cat","")).lower()!="system" for x in nextdemo_rows):
 raise SystemExit("FAIL disappeared continuation state metadata")
order=[pushes[0][0],pushes[1][0],loads[1][0],feed[1][0],selection,armed,focus[1][0],queue,close,
 nav[4][0],not_found,reject,error,pushes[2][0],pushes[3][0],loads[2][0],pushes[4][0],recovery,
 state_rows[0][0],retry_rows[0][0],nextdemo_rows[0][0],focus[2][0],pops[0][0],nav[5][0],state_rows[1][0],focus[3][0],refusal,nav[6][0],complete]
if order!=sorted(order) or len(set(order))!=len(order):raise SystemExit("FAIL disappeared causal order")
zero=("couldn't open ","Demo file: demos/","Demo playback rejected ","Demo playback recovery ",
 "CL_NextDemo:","QUIC client: TLV ACCEPT","SV_OnPlayerConnect:","Q0_DEMO_DISAPPEAR_HIJACKED")
for prefix in zero:
 if family(prefix):raise SystemExit(f"FAIL disappeared forbidden {prefix}")
if any("FIRST GAMEPLAY FRAME" in value for value in vals):raise SystemExit("FAIL disappeared forbidden FIRST")
if sum(str(r.get("sev","")).upper()=="ERROR" for r in rows)!=1:raise SystemExit("FAIL disappeared ERROR inventory")
if any(str(r.get("sev","")).upper()=="FATAL" for r in rows):raise SystemExit("FAIL disappeared FATAL")
if any(str(r.get("sev","")).upper()=="WARN" and str(r.get("cat","")).lower()=="ui"
       and norm(str(r.get("msg","")))!="WiredUI: no demo selected" for r in rows):
 raise SystemExit("FAIL disappeared UI WARN")

expected_kinds=["ready","marker","removed","stopped"]
if [e.get("kind") for e in events]!=expected_kinds:raise SystemExit("FAIL disappeared watcher lifecycle")
expected_keys=[{"kind","basename","bytes","sha256","elapsed_ms"},
 {"kind","marker","marker_line","selection_line","selection_name","selection_generation","elapsed_ms"},
 {"kind","basename","bytes","sha256","exists_after","queue_seen_before_remove","elapsed_ms"},
 {"kind","reason","elapsed_ms"}]
if any(set(event)!=expected_keys[i] for i,event in enumerate(events)):
 raise SystemExit("FAIL disappeared watcher schema")
for i,event in enumerate(events):
 if not isinstance(event.get("elapsed_ms"),int) or event["elapsed_ms"]<0:raise SystemExit("FAIL disappeared watcher time")
 if i and event["elapsed_ms"]<events[i-1]["elapsed_ms"]:raise SystemExit("FAIL disappeared watcher chronology")
for event in (events[0],events[2]):
 if event.get("basename")!="stale.loose.dm_74" or event.get("bytes")!=16 or event.get("sha256")!=wanted_sha:
  raise SystemExit("FAIL disappeared watcher file authority")
if (events[1].get("marker")!="Q0_DEMO_DISAPPEAR_ARMED"
 or events[1].get("selection_name")!="stale.loose" or events[1].get("selection_generation")!=2
 or not isinstance(events[1].get("selection_line"),int) or not isinstance(events[1].get("marker_line"),int)
 or events[1]["selection_line"]>=events[1]["marker_line"]):raise SystemExit("FAIL disappeared watcher marker")
physical={"selection":[],"marker":[]}
for line_number,line in enumerate(open(log_path,encoding="utf-8",errors="replace"),1):
 try:record=json.loads(line)
 except Exception:continue
 if not isinstance(record,dict):continue
 message=record.get("msg")
 if (record.get("sev")=="DEBUG" and record.get("cat")=="ui"
     and message=="WiredUI: demo feeder selection row=0 name=stale.loose generation=2\n"):
  physical["selection"].append(line_number)
 if (record.get("sev")=="INFO" and record.get("cat")=="system"
     and message=="Q0_DEMO_DISAPPEAR_ARMED\n"):
  physical["marker"].append(line_number)
if (physical["selection"]!=[events[1]["selection_line"]]
    or physical["marker"]!=[events[1]["marker_line"]]):
 raise SystemExit("FAIL disappeared watcher physical-line identity")
if events[2].get("exists_after") is not False or events[2].get("queue_seen_before_remove") is not False:
 raise SystemExit("FAIL disappeared watcher removal boundary")
if events[3].get("reason")!="complete":raise SystemExit("FAIL disappeared watcher stop")
print(f"PASS UI-owned disappeared demo recovery fixture_sha256={wanted_sha}")
PYEOF
}

write_clean() {
python3 - "$1" "$2" "$DEMO_SHA" "$3" <<'PYEOF'
import json,sys
lp,ep,sha,mode=sys.argv[1:];rows=[]
def add(sev,cat,msg):rows.append({"sev":sev,"cat":cat,"msg":msg+"\n"})
add("DEBUG","ui","WiredUI: demos loaded protocol=74 count=1 generation=1")
add("DEBUG","ui","WiredUI: demo feeder row=0 name=stale.loose generation=1")
add("DEBUG","ui","WiredUI: push menu 'main' (depth 1)")
for _ in range(3):add("DEBUG","ui","wui_menu_nav: K_DOWNARROW dispatched")
add("DEBUG","ui","WiredUI: push menu 'demos' (depth 2)")
add("DEBUG","ui","WiredUI: demos loaded protocol=74 count=1 generation=2")
add("DEBUG","ui","WiredUI: demo feeder row=0 name=stale.loose generation=2")
add("DEBUG","ui","wui_menu_nav: K_ENTER dispatched")
add("DEBUG","ui","WiredUI: demo feeder selection row=0 name=stale.loose generation=2")
add("DEBUG","ui","wui_menu_nav focus: focused item 'demolist' (top index -1)")
add("INFO","system","Q0_DEMO_DISAPPEAR_ARMED")
add("DEBUG","ui","wui_menu_nav focus: focused item 'btn_play_demo' (top index -1)")
add("DEBUG","ui","WiredUI: queued validated demo playback name=stale.loose")
add("DEBUG","ui","WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0")
add("DEBUG","ui","wui_menu_nav: K_ENTER dispatched")
add("INFO","client","Not found: demos/stale.loose.dm_74")
add("INFO","client","Demo playback open rejected origin=demo-ui stage=probe continuation=0 disconnect=0")
add("ERROR","system","Error: The selected demo is no longer available.")
add("DEBUG","ui","WiredUI: push menu 'main' (depth 1)")
add("DEBUG","ui","WiredUI: push menu 'demos' (depth 2)")
add("DEBUG","ui","WiredUI: demos loaded protocol=74 count=0 generation=3")
add("DEBUG","ui","WiredUI: push menu 'error_popup' (depth 3)")
add("INFO","client","Demo playback open recovery origin=demo-ui stage=probe depth=3 popup=1")
add("INFO","system",'"com_errorMessage" is:"The selected demo is no longer available."')
add("INFO","system",'"ui_errorRetry" is:"0"')
add("INFO","system",'"nextdemo" is:""')
add("DEBUG","ui","wui_menu_nav focus: focused item 'btn_back' (top index -1)")
add("DEBUG","ui","WiredUI: pop menu (depth 2)")
add("DEBUG","ui","wui_menu_nav: K_ENTER dispatched")
add("INFO","system",'"com_errorMessage" is:""')
add("DEBUG","ui","wui_menu_nav focus: focused item 'btn_play_demo' (top index -1)")
add("WARN","ui","WiredUI: no demo selected")
add("DEBUG","ui","wui_menu_nav: K_ENTER dispatched")
add("INFO","system","Q0_DEMO_DISAPPEAR_COMPLETE")
def find(text):return next(i for i,r in enumerate(rows) if text in r["msg"])
events=[{"kind":"ready","basename":"stale.loose.dm_74","bytes":16,"sha256":sha,"elapsed_ms":0},
 {"kind":"marker","marker":"Q0_DEMO_DISAPPEAR_ARMED","marker_line":find("Q0_DEMO_DISAPPEAR_ARMED")+1,"selection_line":find("demo feeder selection")+1,"selection_name":"stale.loose","selection_generation":2,"elapsed_ms":10},
 {"kind":"removed","basename":"stale.loose.dm_74","bytes":16,"sha256":sha,"exists_after":False,"queue_seen_before_remove":False,"elapsed_ms":11},
 {"kind":"stopped","reason":"complete","elapsed_ms":12}]
if mode=="missing-reject":rows.pop(find("open rejected"))
elif mode=="path-error":rows[find("Error: The selected")]["msg"]="Error: couldn't open demos/stale.loose.dm_74\n"
elif mode=="disconnect":rows[find("disconnect=0")]["msg"]=rows[find("disconnect=0")]["msg"].replace("disconnect=0","disconnect=1")
elif mode=="direct-origin":rows[find("origin=demo-ui")]["msg"]=rows[find("origin=demo-ui")]["msg"].replace("origin=demo-ui","origin=direct")
elif mode=="nextdemo":rows.insert(find("open rejected"),{"sev":"DEBUG","cat":"client","msg":"CL_NextDemo: exec successor.cfg\n"})
elif mode=="missing-zero":rows[find("count=0 generation=3")]["msg"]=rows[find("count=0 generation=3")]["msg"].replace("count=0","count=1")
elif mode=="stale-row":rows.insert(find("error_popup"),{"sev":"DEBUG","cat":"ui","msg":"WiredUI: demo feeder row=0 name=stale.loose generation=3\n"})
elif mode=="retry":rows[find("ui_errorRetry")]["msg"]='"ui_errorRetry" is:"1"\n'
elif mode=="missing-popup":rows.pop(find("error_popup"))
elif mode=="wrong-stage":rows[find("open rejected")]["msg"]=rows[find("open rejected")]["msg"].replace("stage=probe","stage=playback")
elif mode=="duplicate-recovery":
 i=find("open recovery");rows.insert(i,dict(rows[i]))
elif mode=="extra-enter":rows.insert(find("open rejected"),{"sev":"DEBUG","cat":"ui","msg":"wui_menu_nav: K_ENTER dispatched\n"})
elif mode=="wrong-selection":rows[find("demo feeder selection")]["msg"]=rows[find("demo feeder selection")]["msg"].replace("generation=2","generation=1")
elif mode=="wrong-queue":rows[find("queued validated")]["msg"]=rows[find("queued validated")]["msg"].replace("stale.loose","other")
elif mode=="multiline":rows.insert(find("open rejected"),{"sev":"INFO","cat":"client","msg":"benign\rDemo playback open rejected origin=demo-ui stage=probe continuation=0 disconnect=0"})
elif mode=="watch-timeout":events[-1]["reason"]="timeout"
elif mode=="watch-order":events[1],events[2]=events[2],events[1]
elif mode=="watch-hash":events[2]["sha256"]="0"*64
elif mode=="watch-after-queue":events[2]["queue_seen_before_remove"]=True
elif mode=="missing-complete":rows.pop(find("Q0_DEMO_DISAPPEAR_COMPLETE"))
elif mode=="watch-exists":events[2]["exists_after"]=True
elif mode=="not-found":rows[find("Not found:")]["msg"]="Not found: demos/other.dm_74\n"
elif mode=="not-found-metadata":rows[find("Not found:")]["sev"]="WARN"
elif mode=="play-enter-order":
 i=find("Not found:"); enter=max(j for j,r in enumerate(rows[:i]) if r["msg"]=="wui_menu_nav: K_ENTER dispatched\n"); rows.insert(i+1,rows.pop(enter))
elif mode=="back-enter-order":
 i=find('com_errorMessage" is:""'); enter=max(j for j,r in enumerate(rows[:i]) if r["msg"]=="wui_menu_nav: K_ENTER dispatched\n"); rows.insert(i+1,rows.pop(enter))
elif mode=="missing-refusal":rows.pop(find("WiredUI: no demo selected"))
elif mode=="first-substring":rows.insert(find("Q0_DEMO_DISAPPEAR_COMPLETE"),{"sev":"INFO","cat":"client","msg":"cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME: arena7)\n"})
elif mode=="pop-metadata":rows[find("WiredUI: pop menu")]["sev"]="INFO"
elif mode=="retry-metadata":rows[find("ui_errorRetry")]["cat"]="network"
elif mode=="additive-open":rows.insert(find("open rejected"),{"sev":"INFO","cat":"client","msg":"Demo playback open forged additive\n"})
elif mode=="continuation-live":rows[find('nextdemo" is:')]["msg"]='"nextdemo" is:"echo Q0_DEMO_DISAPPEAR_HIJACKED"\n'
elif mode=="watch-lines":events[1]["selection_line"]=1;events[1]["marker_line"]=99999
with open(lp,"w") as out:
 for row in rows:out.write(json.dumps(row)+"\n")
with open(ep,"w") as out:
 for event in events:out.write(json.dumps(event)+"\n")
PYEOF
}

if [ "${1:-}" = --analyze ]; then
 [ "$#" -eq 3 ] || { echo "usage: $0 --analyze <qconsole.jsonl> <watcher.jsonl>"; exit 64; }
 analyze_contract "$2" "$3"; exit $?
fi
if [ "${1:-}" = --self-test ]; then
 ROOT="$(mktemp -d -t wired-demo-disappeared-self-XXXXXX 2>/dev/null || mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
 write_clean "$ROOT/clean" "$ROOT/events" clean
 analyze_contract "$ROOT/clean" "$ROOT/events" >/dev/null || exit 1
 defects=(missing-reject path-error disconnect direct-origin nextdemo missing-zero stale-row retry missing-popup wrong-stage duplicate-recovery extra-enter wrong-selection wrong-queue multiline watch-timeout watch-order watch-hash watch-after-queue missing-complete watch-exists not-found not-found-metadata play-enter-order back-enter-order missing-refusal first-substring pop-metadata retry-metadata additive-open continuation-live watch-lines)
 for defect in "${defects[@]}"; do
  write_clean "$ROOT/$defect" "$ROOT/$defect.events" "$defect"
  if analyze_contract "$ROOT/$defect" "$ROOT/$defect.events" >/dev/null 2>&1; then echo "FAIL accepted $defect"; exit 1; fi
 done
 echo "PASS disappeared-demo analyzer self-test (${#defects[@]} mutations)"; exit 0
fi

WIRED="${1:-}"; [ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired"; exit 64; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"; WD="$(dirname "$WIRED")"
PACK=""; for candidate in "$WD" "$WD/../Resources"; do [ -f "$candidate/base/pax21.sw3z" ] && PACK="$candidate" && break; done
[ -n "$PACK" ] || { echo "SKIP: current pax21 unavailable"; exit 77; }
CONTENT="${WIRED_CONTENT_ROOT:-$PACK}"; if [ -f "$CONTENT/base/pax01.sw3z" ]; then BASE="$CONTENT/base/pax01.sw3z"; elif [ -f "$CONTENT/base/pak0.pk3" ]; then BASE="$CONTENT/base/pak0.pk3"; else echo "SKIP: set WIRED_CONTENT_ROOT"; exit 77; fi
ROOT="$(mktemp -d -t wired-demo-disappeared-XXXXXX 2>/dev/null || mktemp -d)"; HOME_DIR="$ROOT/home/q3now-preview"; RUN="$ROOT/run"; EVENTS="$ROOT/watcher.jsonl"; WPID=""
cleanup(){ [ -n "$WPID" ] && kill -TERM "$WPID" 2>/dev/null || true; [ -n "$WPID" ] && wait "$WPID" 2>/dev/null || true; [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"; }; trap cleanup EXIT INT TERM
mkdir -p "$HOME_DIR/base/demos" "$RUN"; cp "$PACK/base/pax21.sw3z" "$HOME_DIR/base/" || exit 1; cp "$BASE" "$HOME_DIR/base/" || exit 1; cp "$CFG_SOURCE" "$HOME_DIR/base/" || exit 1
python3 - "$ROOT/manifest.jsonl" "$WIRED" "$PACK/base/pax21.sw3z" "$BASE" "$0" "$WATCHER" "$CFG_SOURCE" "$TIMEOUT_RUNNER" <<'PYEOF'
import hashlib,json,os,sys
out,*paths=sys.argv[1:]
with open(out,"w",encoding="utf-8") as sink:
 for path in paths:
  digest=hashlib.sha256()
  with open(path,"rb") as source:
   for chunk in iter(lambda:source.read(1024*1024),b""):digest.update(chunk)
  sink.write(json.dumps({"kind":"provenance","name":os.path.basename(path),
   "path":os.path.abspath(path),"bytes":os.path.getsize(path),
   "sha256":digest.hexdigest()},sort_keys=True)+"\n")
PYEOF
python3 - "$HOME_DIR/base/demos/stale.loose.dm_74" "$DEMO_HEX" <<'PYEOF'
import sys
open(sys.argv[1],"wb").write(bytes.fromhex(sys.argv[2]))
PYEOF
python3 "$WATCHER" --log "$HOME_DIR/qconsole.jsonl" --file "$HOME_DIR/base/demos/stale.loose.dm_74" --events "$EVENTS" --marker Q0_DEMO_DISAPPEAR_ARMED & WPID=$!
for _ in $(seq 1 100); do [ -s "$EVENTS" ] && break; sleep .02; done
case "$(uname -s)" in Darwin) PLATFORM_ARGS=(-ApplePersistenceIgnoreState YES);; *) PLATFORM_ARGS=();; esac
python3 "$TIMEOUT_RUNNER" --timeout 90 --kill-after 10 --cwd "$RUN" --stdout "$ROOT/wired.stdout" -- "$WIRED" "${PLATFORM_ARGS[@]}" +set fs_homepath "$HOME_DIR" +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 +set r_fullscreen 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec wiredui-demo-disappeared.cfg || exit 1
wait "$WPID" || { echo "FAIL watcher"; exit 1; }; WPID=""
[ ! -e "$HOME_DIR/base/demos/stale.loose.dm_74" ] || { echo "FAIL loose demo remains"; exit 1; }
analyze_contract "$HOME_DIR/qconsole.jsonl" "$EVENTS"
echo "PASS WiredUI disappeared loose-demo gate"
