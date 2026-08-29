#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# SPDX-License-Identifier: GPL-3.0-or-later
# reload_wasm is a fail-closed diagnostic, never a live VM hot swap.

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze_contract() {
python3 - "$1" "$2" "$3" "$4" <<'PYEOF'
import hashlib,json,os,re,sys
no_path,head_path,gui_path,manifest_path=sys.argv[1:]
def read(path,label):
 rows=[]
 for number,line in enumerate(open(path,encoding="utf-8",errors="strict"),1):
  if not line.strip():continue
  try:row=json.loads(line)
  except Exception as exc:raise SystemExit(f"FAIL reload-wasm {label} JSON {number}: {exc}")
  if not isinstance(row,dict) or not all(isinstance(row.get(k),str) for k in ("sev","cat","msg")):raise SystemExit(f"FAIL reload-wasm {label} schema")
  rows.append(row)
 if not rows:raise SystemExit(f"FAIL reload-wasm empty {label}")
 return rows
def norm(value):return value[:-1] if value.endswith("\n") and not value.endswith("\n\n") else value
claimed=("reload_wasm:","Q0_RELOAD_WASM_","VM_Create policy module=gamesv ","VM_Create policy module=gamecl ","vm/gamesv.","vm/gamecl.","InitGame:","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME","==== ShutdownGame ====","ShutdownGame:","----- Server Shutdown ")
viewpos_re=r"-?[0-9]+ -?[0-9]+ -?[0-9]+ -?[0-9]+ -?[0-9]+"
def prepare(rows,label):
 vals=[norm(r["msg"]) for r in rows]
 for value in vals:
  parts=re.split(r"[\r\n]",value)
  if len(parts)>1 and any(part.startswith(claimed) or re.fullmatch(viewpos_re,part) for part in parts):raise SystemExit(f"FAIL reload-wasm {label} logical-line smuggling")
 if any(r["sev"].upper() in ("ERROR","FATAL") for r in rows):raise SystemExit(f"FAIL reload-wasm {label} severity")
 for value in vals:
  if value.startswith("Reloading ") or value.startswith("WASM modules unloaded") or value.startswith("forcefully unloading "):
   raise SystemExit(f"FAIL reload-wasm legacy destructive marker: {value}")
 return vals
no,head,gui=read(no_path,"no-live"),read(head_path,"headless"),read(gui_path,"gui")
nv,hv,gv=prepare(no,"no-live"),prepare(head,"headless"),prepare(gui,"gui")
def exact_family(rows,vals,prefix,pattern,sev,cat,label,count):
 found=[(i,r,v) for i,(r,v) in enumerate(zip(rows,vals)) if v.startswith(prefix)]
 if len(found)!=count:raise SystemExit(f"FAIL reload-wasm {label} cardinality {len(found)}")
 for _,row,value in found:
  if row["sev"].upper()!=sev or row["cat"].lower()!=cat or re.fullmatch(pattern,value) is None:raise SystemExit(f"FAIL reload-wasm {label} body/metadata: {value}")
 return [item[0] for item in found]
def exact(rows,vals,prefix,pattern,sev,cat,label):
 return exact_family(rows,vals,prefix,pattern,sev,cat,label,1)[0]
def absent(vals,prefix,label):
 if any(value.startswith(prefix) for value in vals):raise SystemExit(f"FAIL reload-wasm unexpected {label}")
def marker_vector(rows,vals,expected,label):
 found=[(i,row,value) for i,(row,value) in enumerate(zip(rows,vals)) if value.startswith("Q0_RELOAD_WASM_")]
 if [value for _,_,value in found]!=expected:raise SystemExit(f"FAIL reload-wasm {label} marker vector")
 if any(row["sev"].upper()!="INFO" or row["cat"].lower()!="system" for _,row,_ in found):raise SystemExit(f"FAIL reload-wasm {label} marker metadata")
 return [i for i,_,_ in found]
no_ops=exact_family(no,nv,"reload_wasm:",r"reload_wasm: no-op live_wasm_count=0","INFO","system","no-live decision",2)
no_markers=marker_vector(no,nv,["Q0_RELOAD_WASM_NO_LIVE_ONE_REQUESTED","Q0_RELOAD_WASM_NO_LIVE_ONE_COMPLETE","Q0_RELOAD_WASM_NO_LIVE_TWO_REQUESTED","Q0_RELOAD_WASM_NO_LIVE_TWO_COMPLETE","Q0_RELOAD_WASM_NO_LIVE_DONE"],"no-live")
absent(nv,"==== ShutdownGame ====","no-live ShutdownGame")
absent(nv,"ShutdownGame:","no-live ShutdownGame detail")
if not no_markers[0]<no_ops[0]<no_markers[1]<no_markers[2]<no_ops[1]<no_markers[3]<no_markers[4]:raise SystemExit("FAIL reload-wasm no-live order")
h_load=exact(head,hv,"vm/gamesv.",r"vm/gamesv\.wasm loaded as WASM interpreter \(\d+ KB memory, \d+ ms\)","INFO","system","headless load")
h_policy=exact(head,hv,"VM_Create policy module=gamesv ",r"VM_Create policy module=gamesv requested=1 effective=1 backend=wasm-interpreter","DEBUG","system","headless policy")
h_init=exact(head,hv,"InitGame:",r"InitGame: .*\\mapname\\arena7(?:\\.*)?","INFO","game","headless InitGame")
h_refuse=exact_family(head,hv,"reload_wasm:",r"reload_wasm: refused live_wasm_count=1 identities=gamesv\[game:0:interpreter\]","WARN","system","headless refusal",2)
h_markers=marker_vector(head,hv,["Q0_RELOAD_WASM_HEADLESS_ONE_REQUESTED","Q0_RELOAD_WASM_HEADLESS_ONE_COMPLETE","Q0_RELOAD_WASM_HEADLESS_TWO_REQUESTED","Q0_RELOAD_WASM_HEADLESS_TWO_COMPLETE","Q0_RELOAD_WASM_HEADLESS_SURVIVED"],"headless")
h_game_shutdown=exact(head,hv,"==== ShutdownGame ====",r"==== ShutdownGame ====","INFO","game","headless ShutdownGame")
h_shutdown_detail=exact(head,hv,"ShutdownGame:",r"ShutdownGame:","INFO","game","headless ShutdownGame detail")
h_shutdown=exact(head,hv,"----- Server Shutdown ",r"----- Server Shutdown \(Server quit\) -----","INFO","server","headless shutdown")
if not h_load<h_policy<h_init<h_markers[0]<h_refuse[0]<h_markers[1]<h_markers[2]<h_refuse[1]<h_markers[3]<h_markers[4]<h_shutdown<h_game_shutdown<h_shutdown_detail:raise SystemExit("FAIL reload-wasm headless order")
g_gload=exact(gui,gv,"vm/gamesv.",r"vm/gamesv\.wasm loaded as WASM interpreter \(\d+ KB memory, \d+ ms\)","INFO","system","gui gamesv load")
g_gpolicy=exact(gui,gv,"VM_Create policy module=gamesv ",r"VM_Create policy module=gamesv requested=1 effective=1 backend=wasm-interpreter","DEBUG","system","gui gamesv policy")
g_init=exact(gui,gv,"InitGame:",r"InitGame: .*\\mapname\\arena7(?:\\.*)?","INFO","game","gui InitGame")
g_cload=exact(gui,gv,"vm/gamecl.",r"vm/gamecl\.wasm loaded as WASM interpreter \(\d+ KB memory, \d+ ms\)","INFO","system","gui gamecl load")
g_cpolicy=exact(gui,gv,"VM_Create policy module=gamecl ",r"VM_Create policy module=gamecl requested=1 effective=1 backend=wasm-interpreter","DEBUG","system","gui gamecl policy")
g_first=exact(gui,gv,"cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME",r"cls\.state: -> CA_ACTIVE \(FIRST GAMEPLAY FRAME mapname=maps/arena7\.bsp serverTime=[0-9]+ numEntities=[0-9]+ framecount=[0-9]+\)","INFO","client","gui FIRST")
g_refuse=exact_family(gui,gv,"reload_wasm:",r"reload_wasm: refused live_wasm_count=2 identities=gamesv\[game:0:interpreter\],gamecl\[cgame:0:interpreter\]","WARN","system","gui refusal",2)
g_viewpos=[i for i,(row,value) in enumerate(zip(gui,gv)) if re.fullmatch(viewpos_re,value)]
if len(g_viewpos)!=2:raise SystemExit(f"FAIL reload-wasm gui viewpos cardinality {len(g_viewpos)}")
if any(gui[i]["sev"].upper()!="INFO" or gui[i]["cat"].lower()!="cgame" for i in g_viewpos):raise SystemExit("FAIL reload-wasm gui viewpos metadata")
g_done=marker_vector(gui,gv,["Q0_RELOAD_WASM_GUI_SURVIVED"],"gui")[0]
g_game_shutdown=exact(gui,gv,"==== ShutdownGame ====",r"==== ShutdownGame ====","INFO","game","gui ShutdownGame")
absent(gv,"ShutdownGame:","gui ShutdownGame detail")
g_shutdown=exact(gui,gv,"----- Server Shutdown ",r"----- Server Shutdown \(Server quit\) -----","INFO","server","gui shutdown")
if not g_gload<g_gpolicy<g_init<g_first or not g_cload<g_cpolicy<g_first or not g_first<g_refuse[0]<g_viewpos[0]<g_refuse[1]<g_viewpos[1]<g_done<g_shutdown<g_game_shutdown:raise SystemExit("FAIL reload-wasm gui order")
manifest=[]
for number,line in enumerate(open(manifest_path,encoding="utf-8",errors="strict"),1):
 try:item=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL reload-wasm manifest JSON {number}: {exc}")
 if not isinstance(item,dict):raise SystemExit("FAIL reload-wasm manifest schema")
 manifest.append(item)
if not manifest or manifest[0]!={"kind":"scenario","schema":1,"name":"reload-wasm-refusal","map":"arena7"}:raise SystemExit("FAIL reload-wasm scenario")
if [x.get("role") for x in manifest[1:-1]] != ["gui","headless","current-archive","content-archive","harness"]:raise SystemExit("FAIL reload-wasm provenance roles")
for item in manifest[1:-1]:
 if set(item)!={"kind","role","path","bytes","sha256"} or item.get("kind")!="provenance" or not isinstance(item.get("bytes"),int) or item["bytes"]<=0 or re.fullmatch(r"[0-9a-f]{64}",str(item.get("sha256",""))) is None:raise SystemExit("FAIL reload-wasm provenance schema")
 data=open(item["path"],"rb").read()
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:raise SystemExit("FAIL reload-wasm provenance rehash")
result=manifest[-1]
keys={"kind","no_live_rc","headless_rc","gui_rc","headless_pid","gui_controller_pid","timeout","forced"}
if set(result)!=keys or result.get("kind")!="result" or any(result.get(k)!=0 for k in ("no_live_rc","headless_rc","gui_rc")) or not all(isinstance(result.get(k),int) and result[k]>0 for k in ("headless_pid","gui_controller_pid")) or result["headless_pid"]==result["gui_controller_pid"] or result.get("timeout") is not False or result.get("forced") is not False:raise SystemExit("FAIL reload-wasm result")
print("PASS reload_wasm refuses live gamesv/gamecl atomically and no-ops with no live VM")
PYEOF
}

write_self() {
python3 - "$1" "$2" "$3" "$4" "$5" <<'PYEOF'
import hashlib,json,os,sys
no_path,head_path,gui_path,manifest_path,mode=sys.argv[1:]
def row(sev,cat,msg):return {"sev":sev,"cat":cat,"msg":msg+"\n"}
no=[row("INFO","system","Q0_RELOAD_WASM_NO_LIVE_ONE_REQUESTED"),row("INFO","system","reload_wasm: no-op live_wasm_count=0"),row("INFO","system","Q0_RELOAD_WASM_NO_LIVE_ONE_COMPLETE"),row("INFO","system","Q0_RELOAD_WASM_NO_LIVE_TWO_REQUESTED"),row("INFO","system","reload_wasm: no-op live_wasm_count=0"),row("INFO","system","Q0_RELOAD_WASM_NO_LIVE_TWO_COMPLETE"),row("INFO","system","Q0_RELOAD_WASM_NO_LIVE_DONE")]
head=[row("INFO","system","vm/gamesv.wasm loaded as WASM interpreter (4096 KB memory, 2 ms)"),row("DEBUG","system","VM_Create policy module=gamesv requested=1 effective=1 backend=wasm-interpreter"),row("INFO","game","InitGame: \\mapname\\arena7\\protocol\\74"),row("INFO","system","Q0_RELOAD_WASM_HEADLESS_ONE_REQUESTED"),row("WARN","system","reload_wasm: refused live_wasm_count=1 identities=gamesv[game:0:interpreter]"),row("INFO","system","Q0_RELOAD_WASM_HEADLESS_ONE_COMPLETE"),row("INFO","system","Q0_RELOAD_WASM_HEADLESS_TWO_REQUESTED"),row("WARN","system","reload_wasm: refused live_wasm_count=1 identities=gamesv[game:0:interpreter]"),row("INFO","system","Q0_RELOAD_WASM_HEADLESS_TWO_COMPLETE"),row("INFO","system","Q0_RELOAD_WASM_HEADLESS_SURVIVED"),row("INFO","server","----- Server Shutdown (Server quit) -----"),row("INFO","game","==== ShutdownGame ===="),row("INFO","game","ShutdownGame:")]
gui=[row("INFO","system","vm/gamesv.wasm loaded as WASM interpreter (4096 KB memory, 2 ms)"),row("DEBUG","system","VM_Create policy module=gamesv requested=1 effective=1 backend=wasm-interpreter"),row("INFO","game","InitGame: \\mapname\\arena7\\protocol\\74"),row("INFO","system","vm/gamecl.wasm loaded as WASM interpreter (4096 KB memory, 2 ms)"),row("DEBUG","system","VM_Create policy module=gamecl requested=1 effective=1 backend=wasm-interpreter"),row("INFO","client","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=100 numEntities=2 framecount=3)"),row("WARN","system","reload_wasm: refused live_wasm_count=2 identities=gamesv[game:0:interpreter],gamecl[cgame:0:interpreter]"),row("INFO","cgame","1 2 3 4 5"),row("WARN","system","reload_wasm: refused live_wasm_count=2 identities=gamesv[game:0:interpreter],gamecl[cgame:0:interpreter]"),row("INFO","cgame","1 2 3 4 5"),row("INFO","system","Q0_RELOAD_WASM_GUI_SURVIVED"),row("INFO","server","----- Server Shutdown (Server quit) -----"),row("INFO","game","==== ShutdownGame ====")]
def find(rows,text,which=0):return [i for i,r in enumerate(rows) if text in r["msg"]][which]
if mode=="no-live-refused":no[1]["msg"]="reload_wasm: refused live_wasm_count=1 identities=gamesv[game:0:interpreter]\n"
elif mode=="headless-noop":head[4]["msg"]="reload_wasm: no-op live_wasm_count=0\n"
elif mode=="gui-partial":gui[6]["msg"]="reload_wasm: refused live_wasm_count=1 identities=gamecl[cgame:0:interpreter]\n"
elif mode=="gui-reversed":gui[6]["msg"]="reload_wasm: refused live_wasm_count=2 identities=gamecl[cgame:0:interpreter],gamesv[game:0:interpreter]\n"
elif mode=="native-label":gui[6]["msg"]="reload_wasm: refused live_wasm_count=2 identities=gamesv[game:0:aot],gamecl[cgame:0:interpreter]\n"
elif mode=="destructive":gui.insert(7,row("INFO","system","forcefully unloading gamesv vm"))
elif mode=="legacy":head.insert(5,row("INFO","system","WASM modules unloaded. They will reload on next map."))
elif mode=="missing-completion":head.pop(find(head,"HEADLESS_SURVIVED"))
elif mode=="early-completion":
 a=find(head,"HEADLESS_TWO_COMPLETE");b=find(head,"reload_wasm: refused",1);head[a],head[b]=head[b],head[a]
elif mode=="refuse-after-shutdown":gui[6],gui[11]=gui[11],gui[6]
elif mode=="wrong-metadata":gui[6]["sev"]="INFO"
elif mode=="additive":gui.insert(7,dict(gui[6]))
elif mode=="suffix":head[4]["msg"]="reload_wasm: refused live_wasm_count=1 identities=gamesv[game:0:interpreter] extra\n"
elif mode=="smuggle":gui.append(row("INFO","system","benign\rreload_wasm: no-op live_wasm_count=0"))
elif mode=="wrong-map":gui[5]["msg"]=gui[5]["msg"].replace("arena7","arena1")
elif mode=="error":head.append(row("ERROR","system","synthetic error"))
elif mode=="single-decision":head.pop(find(head,"reload_wasm: refused",1))
elif mode=="decision-drift":gui[8]["msg"]="reload_wasm: refused live_wasm_count=1 identities=gamesv[game:0:interpreter]\n"
elif mode=="early-shutdown":head.insert(2,row("INFO","game","==== ShutdownGame ===="))
elif mode=="additive-shutdown":gui.append(row("INFO","game","==== ShutdownGame ===="))
elif mode=="missing-viewpos":gui.pop(7)
elif mode=="reordered-viewpos":gui[7],gui[8]=gui[8],gui[7]
elif mode=="extra-viewpos":gui.insert(9,row("INFO","cgame","1 2 3 4 5"))
elif mode=="viewpos-metadata":gui[7]["cat"]="client"
elif mode=="duplicate-first-epoch":head.insert(find(head,"HEADLESS_ONE_COMPLETE"),dict(head[find(head,"HEADLESS_ONE_REQUESTED")]))
elif mode=="skip-second-epoch":no.pop(find(no,"NO_LIVE_TWO_REQUESTED"))
elif mode=="swapped-request-epoch":
 a=find(head,"HEADLESS_ONE_REQUESTED");b=find(head,"HEADLESS_TWO_REQUESTED");head[a],head[b]=head[b],head[a]
for path,rows in ((no_path,no),(head_path,head),(gui_path,gui)):
 with open(path,"w") as out:
  for item in rows:out.write(json.dumps(item)+"\n")
items=[{"kind":"scenario","schema":1,"name":"reload-wasm-refusal","map":"arena7"}]
for role in ("gui","headless","current-archive","content-archive","harness"):
 path=manifest_path+"."+role;data=("fixture-"+role).encode();open(path,"wb").write(data);items.append({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
items.append({"kind":"result","no_live_rc":0,"headless_rc":0,"gui_rc":0,"headless_pid":101,"gui_controller_pid":102,"timeout":False,"forced":False})
if mode=="manifest":items[2]["sha256"]="0"*64
elif mode=="result":items[-1]["gui_rc"]=1
with open(manifest_path,"w") as out:
 for item in items:out.write(json.dumps(item,sort_keys=True)+"\n")
PYEOF
}

if [ "${1:-}" = --analyze ]; then
 [ "$#" -eq 5 ] || { echo "usage: $0 --analyze <no-live> <headless> <gui> <manifest>"; exit 64; }
 analyze_contract "$2" "$3" "$4" "$5";exit $?
fi
if [ "${1:-}" = --self-test ]; then
 ROOT="$(mktemp -d -t reload-wasm-self-XXXXXX 2>/dev/null || mktemp -d)";trap 'rm -rf "$ROOT"' EXIT
 write_self "$ROOT/no" "$ROOT/head" "$ROOT/gui" "$ROOT/manifest" clean;analyze_contract "$ROOT/no" "$ROOT/head" "$ROOT/gui" "$ROOT/manifest" >/dev/null || exit 1
 defects=(no-live-refused headless-noop gui-partial gui-reversed native-label destructive legacy missing-completion early-completion refuse-after-shutdown wrong-metadata additive suffix smuggle wrong-map error single-decision decision-drift early-shutdown additive-shutdown missing-viewpos reordered-viewpos extra-viewpos viewpos-metadata duplicate-first-epoch skip-second-epoch swapped-request-epoch manifest result)
 for defect in "${defects[@]}";do write_self "$ROOT/$defect.no" "$ROOT/$defect.head" "$ROOT/$defect.gui" "$ROOT/$defect.manifest" "$defect";if analyze_contract "$ROOT/$defect.no" "$ROOT/$defect.head" "$ROOT/$defect.gui" "$ROOT/$defect.manifest" >/dev/null 2>&1;then echo "FAIL accepted $defect";exit 1;fi;done
 echo "PASS reload-wasm analyzer self-test (${#defects[@]} mutations)";exit 0
fi

WIRED="${1:-}";[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired";exit 64; }
[ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: timeout runner unavailable";exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")";WD="$(dirname "$WIRED")"
HEADLESS="${WIRED_BINARY_HEADLESS:-}";if [ -z "$HEADLESS" ];then suffix="$(basename "$WIRED")";suffix="${suffix#wired}";for candidate in "$WD/wired-headless$suffix" "$WD/wired-headless.arm64" "$WD/wired-headless.x86_64" "$WD/../../../wired-headless$suffix";do [ -x "$candidate" ] && HEADLESS="$candidate" && break;done;fi
[ -n "$HEADLESS" ] && [ -x "$HEADLESS" ] || { echo "SKIP: sibling wired-headless unavailable";exit 77; };HEADLESS="$(cd "$(dirname "$HEADLESS")" && pwd)/$(basename "$HEADLESS")"
PACK="$(wired_find_archive_root "$WD" "$WD/../Resources" "$WD/../../.." 2>/dev/null || true)";[ -n "$PACK" ] || { echo "SKIP: current VFS archives unavailable";exit 77; }
CONTENT="$(wired_find_archive_root "${WIRED_CONTENT_ROOT:-}" "$WIRED_HOME" "$PACK" 2>/dev/null || true)";[ -n "$CONTENT" ] || { echo "SKIP: set WIRED_CONTENT_ROOT";exit 77; }
CURRENT_ARCHIVE="$(wired_first_archive "$PACK/base")" || exit 77
CONTENT_ARCHIVE="$(wired_first_archive "$CONTENT/base")" || exit 77
ROOT="$(mktemp -d -t reload-wasm-XXXXXX 2>/dev/null || mktemp -d)";NO_HOME="$ROOT/no/q3now-preview";HEAD_HOME="$ROOT/head/q3now-preview";GUI_HOME="$ROOT/gui/q3now-preview";PID="";OPEN=0;FORCED=0
cleanup(){ local status=$?;trap - EXIT INT TERM;[ "$OPEN" -eq 1 ] && exec 9>&- || true;[ -n "$PID" ] && kill -TERM "$PID" 2>/dev/null || true;[ -n "$PID" ] && wait "$PID" 2>/dev/null || true;[ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT";exit "$status"; };trap cleanup EXIT;trap 'exit 130' INT;trap 'exit 143' TERM
for home in "$NO_HOME" "$HEAD_HOME" "$GUI_HOME";do mkdir -p "$home/base";wired_link_content_into_home "$home" "$CONTENT/base" "$PACK/base" || exit 1;done
MANIFEST="$ROOT/manifest.jsonl";python3 - "$MANIFEST" "$WIRED" "$HEADLESS" "$CURRENT_ARCHIVE" "$CONTENT_ARCHIVE" "$0" <<'PYEOF'
import hashlib,json,os,sys
with open(sys.argv[1],"w") as out:
 out.write(json.dumps({"kind":"scenario","schema":1,"name":"reload-wasm-refusal","map":"arena7"},sort_keys=True)+"\n")
 for role,path in zip(("gui","headless","current-archive","content-archive","harness"),sys.argv[2:]):
  data=open(path,"rb").read();out.write(json.dumps({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()},sort_keys=True)+"\n")
PYEOF
printf 'echo Q0_RELOAD_WASM_NO_LIVE_ONE_REQUESTED\nreload_wasm\necho Q0_RELOAD_WASM_NO_LIVE_ONE_COMPLETE\necho Q0_RELOAD_WASM_NO_LIVE_TWO_REQUESTED\nreload_wasm\necho Q0_RELOAD_WASM_NO_LIVE_TWO_COMPLETE\necho Q0_RELOAD_WASM_NO_LIVE_DONE\nquit\n' >"$NO_HOME/base/no-live.cfg"
python3 "$TIMEOUT_RUNNER" --timeout 30 --kill-after 5 --cwd "$(dirname "$HEADLESS")" --stdout "$ROOT/no.stdout" -- "$HEADLESS" +set fs_homepath "$NO_HOME" +set com_automated 1 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec no-live.cfg;NO_RC=$?
NO_JSON="$NO_HOME/qconsole.jsonl";[ "$NO_RC" -eq 0 ] && [ -s "$NO_JSON" ] || { echo "FAIL no-live phase";exit 1; }
PORTS="$(python3 - <<'PYEOF'
import socket
sockets=[socket.socket(socket.AF_INET,socket.SOCK_DGRAM) for _ in range(2)]
for sock in sockets:sock.bind(("127.0.0.1",0))
print(*(sock.getsockname()[1] for sock in sockets))
for sock in sockets:sock.close()
PYEOF
)";HEAD_PORT="${PORTS%% *}";GUI_PORT="${PORTS##* }";FIFO="$ROOT/head.stdin";mkfifo "$FIFO";exec 9<>"$FIFO";OPEN=1
(cd "$(dirname "$HEADLESS")" && exec "$HEADLESS" +set fs_homepath "$HEAD_HOME" +set com_automated 1 +set net_ip 127.0.0.1 +set net_port "$HEAD_PORT" +set vm_game 1 +set sv_pure 0 +set g_autoBots 0 +set g_minPlayers 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +map arena7 <&9) >"$ROOT/head.stdout" 2>&1 &PID=$!;HEAD_PID=$PID;HEAD_JSON="$HEAD_HOME/qconsole.jsonl"
for _ in $(seq 1 900);do python3 - "$HEAD_JSON" <<'PYEOF' >/dev/null 2>&1 && break
import json,sys
msgs=[json.loads(x).get("msg","") for x in open(sys.argv[1]) if x.strip()]
raise SystemExit(0 if any(x.startswith("VM_Create policy module=gamesv requested=1 effective=1") for x in msgs) and any(x.startswith("InitGame:") for x in msgs) else 1)
PYEOF
 sleep .1;kill -0 "$PID" 2>/dev/null || { echo "FAIL headless exited";exit 1; };done
printf 'echo Q0_RELOAD_WASM_HEADLESS_ONE_REQUESTED; reload_wasm; wait 10; echo Q0_RELOAD_WASM_HEADLESS_ONE_COMPLETE; echo Q0_RELOAD_WASM_HEADLESS_TWO_REQUESTED; reload_wasm; wait 10; echo Q0_RELOAD_WASM_HEADLESS_TWO_COMPLETE; echo Q0_RELOAD_WASM_HEADLESS_SURVIVED; quit\n' >&9
for _ in $(seq 1 300);do kill -0 "$PID" 2>/dev/null || break;sleep .1;done
if kill -0 "$PID" 2>/dev/null;then FORCED=1;kill -TERM "$PID" 2>/dev/null || true;fi;wait "$PID";HEAD_RC=$?;PID="";exec 9>&-;OPEN=0
printf 'set activeAction "reload_wasm ; viewpos ; wait 10 ; reload_wasm ; viewpos ; wait 10 ; echo Q0_RELOAD_WASM_GUI_SURVIVED ; wait 30 ; quit"\nmap arena7\n' >"$GUI_HOME/base/gui.cfg"
case "$(uname -s)" in Darwin) PLATFORM=(-ApplePersistenceIgnoreState YES);;*) PLATFORM=();;esac
python3 "$TIMEOUT_RUNNER" --timeout 150 --kill-after 15 --cwd "$WD" --stdout "$ROOT/gui.stdout" -- "$WIRED" "${PLATFORM[@]}" +set fs_homepath "$GUI_HOME" +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set net_ip 127.0.0.1 +set net_port "$GUI_PORT" +set vm_game 1 +set vm_cgame 1 +set sv_pure 0 +set g_autoBots 0 +set g_minPlayers 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec gui.cfg &GUI_CONTROLLER_PID=$!;wait "$GUI_CONTROLLER_PID";GUI_RC=$?;GUI_JSON="$GUI_HOME/qconsole.jsonl"
python3 - "$MANIFEST" "$NO_RC" "$HEAD_RC" "$GUI_RC" "$HEAD_PID" "$GUI_CONTROLLER_PID" "$FORCED" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a") as out:out.write(json.dumps({"kind":"result","no_live_rc":int(sys.argv[2]),"headless_rc":int(sys.argv[3]),"gui_rc":int(sys.argv[4]),"headless_pid":int(sys.argv[5]),"gui_controller_pid":int(sys.argv[6]),"timeout":False,"forced":bool(int(sys.argv[7]))},sort_keys=True)+"\n")
PYEOF
[ "$HEAD_RC" -eq 0 ] && [ "$GUI_RC" -eq 0 ] && [ "$FORCED" -eq 0 ] || { echo "FAIL process result";exit 1; }
analyze_contract "$NO_JSON" "$HEAD_JSON" "$GUI_JSON" "$MANIFEST" || exit 1
echo "PASS reload_wasm refusal gate"
