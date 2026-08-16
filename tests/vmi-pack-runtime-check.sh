#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# SPDX-License-Identifier: GPL-3.0-or-later
# Shipped-pax-only provenance for the paired gamesv/gamecl runtime.

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze_contract() {
python3 - "$1" "$2" "$3" <<'PYEOF'
import hashlib,json,os,re,sys
client_path,server_path,manifest_path=sys.argv[1:]
def read(path,label):
 rows=[]
 for number,line in enumerate(open(path,encoding="utf-8",errors="strict"),1):
  if not line.strip():continue
  try:row=json.loads(line)
  except Exception as exc:raise SystemExit(f"FAIL pack-runtime {label} JSON {number}: {exc}")
  if not isinstance(row,dict) or not all(isinstance(row.get(k),str) for k in ("sev","cat","msg")):raise SystemExit(f"FAIL pack-runtime {label} schema")
  rows.append(row)
 if not rows:raise SystemExit(f"FAIL pack-runtime empty {label}")
 return rows
def norm(value):return value[:-1] if value.endswith("\n") and not value.endswith("\n\n") else value
claimed=("vm/gamesv.","vm/gamecl.","VM_Create policy module=gamesv ","VM_Create policy module=gamecl ","Connected to a pure server.","SV_VerifyPaks: ","ClientConnect: ","ClientBegin: ","say: ","QUIC client: TLV ACCEPT received","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME","  game        ","  cgame       ","Q0_VMI_PACK_","----- Server Shutdown ","==== ShutdownGame ====","ShutdownGame:")
viewpos_re=r"-?[0-9]+ -?[0-9]+ -?[0-9]+ -?[0-9]+ -?[0-9]+"
def prepare(rows,label):
 vals=[norm(row["msg"]) for row in rows]
 for value in vals:
  parts=re.split(r"[\r\n]",value)
  if len(parts)>1 and any(part.startswith(claimed) or re.fullmatch(viewpos_re,part) for part in parts):raise SystemExit(f"FAIL pack-runtime {label} logical-line smuggling")
 if any(row["sev"].upper() in ("ERROR","FATAL") for row in rows):raise SystemExit(f"FAIL pack-runtime {label} severity")
 return vals
client,server=read(client_path,"client"),read(server_path,"server")
cv,sv=prepare(client,"client"),prepare(server,"server")
def exact(rows,vals,prefix,pattern,sev,cat,label):
 found=[(i,row,value) for i,(row,value) in enumerate(zip(rows,vals)) if value.startswith(prefix)]
 if len(found)!=1:raise SystemExit(f"FAIL pack-runtime {label} cardinality {len(found)}")
 i,row,value=found[0]
 if row["sev"].upper()!=sev or row["cat"].lower()!=cat or re.fullmatch(pattern,value) is None:raise SystemExit(f"FAIL pack-runtime {label} body/metadata: {value}")
 return i,value
manifest=[]
for number,line in enumerate(open(manifest_path,encoding="utf-8",errors="strict"),1):
 try:item=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL pack-runtime manifest JSON {number}: {exc}")
 if not isinstance(item,dict):raise SystemExit("FAIL pack-runtime manifest schema")
 manifest.append(item)
if not manifest:raise SystemExit("FAIL pack-runtime empty manifest")
scenario=manifest[0]
if set(scenario)!={"kind","schema","name","map","vm_game","vm_cgame","sv_pure","endpoint","server_pax21","server_pax21_bytes","server_pax21_sha256","client_pax21","client_pax21_bytes","client_pax21_sha256","vm_members"} or any(scenario.get(k)!=v for k,v in {"kind":"scenario","schema":1,"name":"vmi-pack-runtime","map":"arena7","vm_game":2,"vm_cgame":2,"sv_pure":1}.items()) or re.fullmatch(r"127\.0\.0\.1:[1-9][0-9]*",str(scenario.get("endpoint",""))) is None:raise SystemExit("FAIL pack-runtime scenario")
for prefix in ("server","client"):
 path=scenario[prefix+"_pax21"];data=open(path,"rb").read()
 if len(data)!=scenario[prefix+"_pax21_bytes"] or hashlib.sha256(data).hexdigest()!=scenario[prefix+"_pax21_sha256"]:raise SystemExit(f"FAIL pack-runtime staged {prefix} pax21 changed")
members=scenario["vm_members"]
if not isinstance(members,list) or [x.get("path") for x in members]!=["vm/gamecl.wasm","vm/gamesv.wasm"]:raise SystemExit("FAIL pack-runtime VM inventory")
for item in members:
 if set(item)!={"path","extracted","bytes","sha256"} or not isinstance(item["bytes"],int) or item["bytes"]<=0 or re.fullmatch(r"[0-9a-f]{64}",str(item["sha256"])) is None:raise SystemExit("FAIL pack-runtime member schema")
 data=open(item["extracted"],"rb").read()
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:raise SystemExit("FAIL pack-runtime member rehash")
provenance=manifest[1:-1]
if [x.get("role") for x in provenance] != ["gui","headless","pax01","pax21","sw3z","harness"]:raise SystemExit("FAIL pack-runtime provenance roles")
for item in provenance:
 if set(item)!={"kind","role","path","bytes","sha256"} or item.get("kind")!="provenance" or not isinstance(item.get("bytes"),int) or item["bytes"]<=0 or re.fullmatch(r"[0-9a-f]{64}",str(item.get("sha256",""))) is None:raise SystemExit("FAIL pack-runtime provenance schema")
 data=open(item["path"],"rb").read()
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:raise SystemExit("FAIL pack-runtime provenance rehash")
source_pax=next(item for item in provenance if item["role"]=="pax21")
if source_pax["bytes"]!=scenario["server_pax21_bytes"] or source_pax["bytes"]!=scenario["client_pax21_bytes"] or source_pax["sha256"]!=scenario["server_pax21_sha256"] or source_pax["sha256"]!=scenario["client_pax21_sha256"]:raise SystemExit("FAIL pack-runtime source/staged pax21 identity")
result=manifest[-1]
if set(result)!={"kind","client_controller_pid","server_pid","client_rc","server_rc","timeout","forced"} or result.get("kind")!="result" or not all(isinstance(result.get(k),int) and result[k]>0 for k in ("client_controller_pid","server_pid")) or result["client_controller_pid"]==result["server_pid"] or result.get("client_rc")!=0 or result.get("server_rc")!=0 or result.get("timeout") is not False or result.get("forced") is not False:raise SystemExit("FAIL pack-runtime result")
server_pax,client_pax=scenario["server_pax21"],scenario["client_pax21"]
endpoint=scenario["endpoint"];port=endpoint.rsplit(":",1)[1]
gs,_=exact(server,sv,"vm/gamesv.",r"vm/gamesv\.wasm loaded as WASM interpreter \([0-9]+ KB memory, [0-9]+ ms\)","INFO","system","gamesv load")
gp,_=exact(server,sv,"VM_Create policy module=gamesv ",r"VM_Create policy module=gamesv requested=2 effective=2 backend=wasm-interpreter","DEBUG","system","gamesv policy")
listen,_=exact(server,sv,"WiredNet: listening on port ",r"WiredNet: listening on port "+port+r" \(IPv4\), ALPN: .+","INFO","network","listener")
sr,_=exact(server,sv,"Q0_VMI_PACK_SERVER_SYSINFO_REQUESTED",r"Q0_VMI_PACK_SERVER_SYSINFO_REQUESTED","INFO","system","server sysinfo request")
transport,_=exact(server,sv,"SV_OnPlayerConnect: conn=",r"SV_OnPlayerConnect: conn=[1-9][0-9]*","DEBUG","server","transport admission")
cc,cc_value=exact(server,sv,"ClientConnect: ",r"ClientConnect: [0-9]+","INFO","game","admission ClientConnect")
cb,cb_value=exact(server,sv,"ClientBegin: ",r"ClientBegin: [0-9]+","INFO","game","admission ClientBegin")
if cc_value.rsplit(" ",1)[1]!=cb_value.rsplit(" ",1)[1]:raise SystemExit("FAIL pack-runtime admission slot identity")
gsi,_=exact(server,sv,"  game        ",r"  game        .+  "+re.escape(server_pax+r" :: vm/gamesv.wasm"),"INFO","system","gamesv sysinfo path")
sd,_=exact(server,sv,"Q0_VMI_PACK_SERVER_SYSINFO_DONE",r"Q0_VMI_PACK_SERVER_SYSINFO_DONE","INFO","system","server completion")
pure_accepted,_=exact(server,sv,"SV_VerifyPaks: ",r"SV_VerifyPaks: accepted client=PaxWitness","DEBUG","server","pure verification accepted")
say,_=exact(server,sv,"say: ",r"say: PaxWitness: Q0_VMI_PACK_PURE_CONTINUITY","INFO","game","post-pure continuity say")
shutdown,_=exact(server,sv,"----- Server Shutdown ",r"----- Server Shutdown \(Server quit\) -----","INFO","server","clean server shutdown")
shutdown_header,_=exact(server,sv,"==== ShutdownGame ====",r"==== ShutdownGame ====","INFO","game","clean game shutdown")
shutdown_detail,_=exact(server,sv,"ShutdownGame:",r"ShutdownGame:","INFO","game","clean game shutdown detail")
cg,_=exact(client,cv,"vm/gamecl.",r"vm/gamecl\.wasm loaded as WASM interpreter \([0-9]+ KB memory, [0-9]+ ms\)","INFO","system","gamecl load")
cp,_=exact(client,cv,"VM_Create policy module=gamecl ",r"VM_Create policy module=gamecl requested=2 effective=2 backend=wasm-interpreter","DEBUG","system","gamecl policy")
resolve,_=exact(client,cv,endpoint+" resolved to ",re.escape(endpoint+r" resolved to "+endpoint),"INFO","client","endpoint resolution")
accept,_=exact(client,cv,"QUIC client: TLV ACCEPT received",r"QUIC client: TLV ACCEPT received","DEBUG","network","ACCEPT")
pure,_=exact(client,cv,"Connected to a pure server.",r"Connected to a pure server\.","DEBUG","filesystem","pure negotiation")
first,_=exact(client,cv,"cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME",r"cls\.state: -> CA_ACTIVE \(FIRST GAMEPLAY FRAME mapname=maps/arena7\.bsp serverTime=[0-9]+ numEntities=[0-9]+ framecount=[0-9]+\)","INFO","client","FIRST")
viewpos=[i for i,(row,value) in enumerate(zip(client,cv)) if re.fullmatch(viewpos_re,value)]
if len(viewpos)!=1 or client[viewpos[0]]["sev"].upper()!="INFO" or client[viewpos[0]]["cat"].lower()!="cgame":raise SystemExit("FAIL pack-runtime post-pure cgame continuity")
csi,_=exact(client,cv,"  cgame       ",r"  cgame       .+  "+re.escape(client_pax+r" :: vm/gamecl.wasm"),"INFO","system","gamecl sysinfo path")
cd,_=exact(client,cv,"Q0_VMI_PACK_CLIENT_SYSINFO_DONE",r"Q0_VMI_PACK_CLIENT_SYSINFO_DONE","INFO","system","client completion")
if not listen<gs<gp<transport<cc<pure_accepted<cb or not pure_accepted<say<shutdown or not cb<sr<gsi<sd<shutdown<shutdown_header<shutdown_detail:raise SystemExit("FAIL pack-runtime gamesv/pure/admission/sysinfo order")
if not resolve<pure<cg<cp<first<viewpos[0]<csi<cd or not resolve<accept<first:raise SystemExit("FAIL pack-runtime gamecl/pure/ACCEPT/FIRST/sysinfo order")
for label,vals in (("client",cv),("server",sv)):
 for value in vals:
  lower=value.lower()
  if any(token in lower for token in ("vm/gamesv.aot","vm/gamecl.aot","vm/gamesv.qvm","vm/gamecl.qvm","wasm aot","backend=wasm-aot")):raise SystemExit(f"FAIL pack-runtime forbidden VM backend {label}: {value}")
  if any(token in lower for token in ("sv_verifypaks: rejecting","unpure client","cannot validate pure client","pure client detected")):raise SystemExit(f"FAIL pack-runtime pure rejection {label}: {value}")
print("PASS shipped pax-only gamesv/gamecl runtime provenance")
PYEOF
}

write_self() {
python3 - "$1" "$2" "$3" "$4" <<'PYEOF'
import hashlib,json,os,sys
client_path,server_path,manifest_path,mode=sys.argv[1:]
root=os.path.dirname(manifest_path);server_pax=root+"/server/base/pax21.sw3z";client_pax=root+"/client/base/pax21.sw3z";endpoint="127.0.0.1:30123"
def row(sev,cat,msg):return {"sev":sev,"cat":cat,"msg":msg+"\n"}
server=[row("INFO","network","WiredNet: listening on port 30123 (IPv4), ALPN: q3now"),row("INFO","system","vm/gamesv.wasm loaded as WASM interpreter (4096 KB memory, 3 ms)"),row("DEBUG","system","VM_Create policy module=gamesv requested=2 effective=2 backend=wasm-interpreter"),row("DEBUG","server","SV_OnPlayerConnect: conn=17"),row("INFO","game","ClientConnect: 0"),row("DEBUG","server","SV_VerifyPaks: accepted client=PaxWitness"),row("INFO","game","ClientBegin: 0"),row("INFO","system","Q0_VMI_PACK_SERVER_SYSINFO_REQUESTED"),row("INFO","system",f"  game        Aug 12 2026         abcdef      {server_pax} :: vm/gamesv.wasm"),row("INFO","system","Q0_VMI_PACK_SERVER_SYSINFO_DONE"),row("INFO","game","say: PaxWitness: Q0_VMI_PACK_PURE_CONTINUITY"),row("INFO","server","----- Server Shutdown (Server quit) -----"),row("INFO","game","==== ShutdownGame ===="),row("INFO","game","ShutdownGame:")]
client=[row("INFO","client",f"{endpoint} resolved to {endpoint}"),row("DEBUG","filesystem","Connected to a pure server."),row("DEBUG","network","QUIC client: TLV ACCEPT received"),row("INFO","system","vm/gamecl.wasm loaded as WASM interpreter (4096 KB memory, 2 ms)"),row("DEBUG","system","VM_Create policy module=gamecl requested=2 effective=2 backend=wasm-interpreter"),row("INFO","client","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=100 numEntities=2 framecount=3)"),row("INFO","cgame","1 2 3 4 5"),row("INFO","system",f"  cgame       Aug 12 2026         abcdef      {client_pax} :: vm/gamecl.wasm"),row("INFO","system","Q0_VMI_PACK_CLIENT_SYSINFO_DONE")]
def find(rows,text):return next(i for i,r in enumerate(rows) if text in r["msg"])
if mode=="wrong-gamesv-policy":server[find(server,"VM_Create policy")]["msg"]=server[find(server,"VM_Create policy")]["msg"].replace("requested=2 effective=2","requested=1 effective=1")
elif mode=="wrong-gamecl-policy":client[find(client,"VM_Create policy")]["msg"]=client[find(client,"VM_Create policy")]["msg"].replace("effective=2","effective=1")
elif mode=="aot":client.insert(2,row("WARN","system","WASM: failed to load vm/gamecl.aot"))
elif mode=="loose-path":client[find(client,"  cgame")]["msg"]=client[find(client,"  cgame")]["msg"].replace(client_pax+" :: ",root+"/client/base/")
elif mode=="cross-path":server[find(server,"  game")]["msg"]=server[find(server,"  game")]["msg"].replace(server_pax,client_pax)
elif mode=="missing-accept":client.pop(find(client,"TLV ACCEPT"))
elif mode=="first-before-policy":
 a=find(client,"VM_Create policy");b=find(client,"FIRST GAMEPLAY");client[a],client[b]=client[b],client[a]
elif mode=="sysinfo-before-first":
 a=find(client,"FIRST GAMEPLAY");b=find(client,"  cgame");client[a],client[b]=client[b],client[a]
elif mode=="additive-sysinfo":client.insert(find(client,"CLIENT_SYSINFO_DONE"),dict(client[find(client,"  cgame")]))
elif mode=="wrong-metadata":server[find(server,"  game")]["cat"]="client"
elif mode=="suffix":server[find(server,"  game")]["msg"]=server[find(server,"  game")]["msg"].rstrip("\n")+" extra\n"
elif mode=="smuggle":client.append(row("INFO","system","benign\r  cgame       fake"))
elif mode=="no-begin":server.pop(find(server,"ClientBegin:"))
elif mode=="error":server.append(row("ERROR","system","synthetic"))
elif mode=="missing-done":client.pop(find(client,"CLIENT_SYSINFO_DONE"))
elif mode=="missing-continuity":client.pop(find(client,"1 2 3 4 5"))
elif mode=="missing-pure":client.pop(find(client,"Connected to a pure server"))
elif mode=="missing-pure-accepted":server.pop(find(server,"SV_VerifyPaks: accepted"))
elif mode=="missing-say":server.pop(find(server,"say:"))
elif mode=="duplicate-pure-accepted":server.insert(find(server,"SV_VerifyPaks: accepted"),row("DEBUG","server","SV_VerifyPaks: accepted client=PaxWitness"))
elif mode=="late-pure-accepted":
 accepted=server.pop(find(server,"SV_VerifyPaks: accepted"));server.insert(find(server,"say:")+1,accepted)
elif mode=="accepted-after-begin":
 accepted=server.pop(find(server,"SV_VerifyPaks: accepted"));server.insert(find(server,"ClientBegin:")+1,accepted)
elif mode=="wrong-pure-accepted-metadata":server[find(server,"SV_VerifyPaks: accepted")]["cat"]="network"
elif mode=="say-before-sysinfo":
 game_say=server.pop(find(server,"say:"));at=find(server,"Q0_VMI_PACK_SERVER_SYSINFO_REQUESTED");server.insert(at,game_say)
elif mode=="missing-shutdown":server.pop(find(server,"Server Shutdown"))
for path,rows in ((client_path,client),(server_path,server)):
 os.makedirs(os.path.dirname(path),exist_ok=True)
 with open(path,"w") as out:
  for item in rows:out.write(json.dumps(item)+"\n")
members=[]
for name in ("vm/gamecl.wasm","vm/gamesv.wasm"):
 path=root+"/extract/"+name;os.makedirs(os.path.dirname(path),exist_ok=True);data=("member-"+name).encode();open(path,"wb").write(data);members.append({"path":name,"extracted":path,"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
for path in (server_pax,client_pax):os.makedirs(os.path.dirname(path),exist_ok=True);open(path,"wb").write(b"staged-pax21")
items=[{"kind":"scenario","schema":1,"name":"vmi-pack-runtime","map":"arena7","vm_game":2,"vm_cgame":2,"sv_pure":1,"endpoint":endpoint,"server_pax21":server_pax,"server_pax21_bytes":12,"server_pax21_sha256":hashlib.sha256(b"staged-pax21").hexdigest(),"client_pax21":client_pax,"client_pax21_bytes":12,"client_pax21_sha256":hashlib.sha256(b"staged-pax21").hexdigest(),"vm_members":members}]
for role in ("gui","headless","pax01","pax21","sw3z","harness"):
 path=root+"/"+role;data=b"staged-pax21" if role=="pax21" else ("fixture-"+role).encode();os.makedirs(os.path.dirname(path),exist_ok=True);open(path,"wb").write(data);items.append({"kind":"provenance","role":role,"path":path,"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
items.append({"kind":"result","client_controller_pid":101,"server_pid":102,"client_rc":0,"server_rc":0,"timeout":False,"forced":False})
if mode=="inventory":items[0]["vm_members"].append(dict(members[0]))
elif mode=="member-hash":items[0]["vm_members"][0]["sha256"]="0"*64
elif mode=="manifest":items[2]["sha256"]="0"*64
elif mode=="result":items[-1]["client_rc"]=1
elif mode=="source-pax":items[0]["server_pax21_sha256"]="0"*64
with open(manifest_path,"w") as out:
 for item in items:out.write(json.dumps(item,sort_keys=True)+"\n")
PYEOF
}

if [ "${1:-}" = --analyze ];then [ "$#" -eq 4 ] || exit 64;analyze_contract "$2" "$3" "$4";exit $?;fi
if [ "${1:-}" = --self-test ];then
 ROOT="$(mktemp -d -t vmi-pack-self-XXXXXX 2>/dev/null || mktemp -d)";trap 'rm -rf "$ROOT"' EXIT
 write_self "$ROOT/clean/client.jsonl" "$ROOT/clean/server.jsonl" "$ROOT/clean/manifest" clean || exit 1;analyze_contract "$ROOT/clean/client.jsonl" "$ROOT/clean/server.jsonl" "$ROOT/clean/manifest" >/dev/null || exit 1
 write_self "$ROOT/say-before-sysinfo/client.jsonl" "$ROOT/say-before-sysinfo/server.jsonl" "$ROOT/say-before-sysinfo/manifest" say-before-sysinfo || exit 1;analyze_contract "$ROOT/say-before-sysinfo/client.jsonl" "$ROOT/say-before-sysinfo/server.jsonl" "$ROOT/say-before-sysinfo/manifest" >/dev/null || { echo "FAIL rejected valid say-before-sysinfo partial order";exit 1; }
 defects=(wrong-gamesv-policy wrong-gamecl-policy aot loose-path cross-path missing-accept first-before-policy sysinfo-before-first additive-sysinfo wrong-metadata suffix smuggle no-begin error missing-done missing-continuity missing-pure missing-pure-accepted duplicate-pure-accepted late-pure-accepted accepted-after-begin wrong-pure-accepted-metadata missing-say missing-shutdown inventory member-hash source-pax manifest result)
 for defect in "${defects[@]}";do write_self "$ROOT/$defect/client.jsonl" "$ROOT/$defect/server.jsonl" "$ROOT/$defect/manifest" "$defect" || exit 1;if analyze_contract "$ROOT/$defect/client.jsonl" "$ROOT/$defect/server.jsonl" "$ROOT/$defect/manifest" >/dev/null 2>&1;then echo "FAIL accepted $defect";exit 1;fi;done
 echo "PASS vmi-pack-runtime analyzer self-test (${#defects[@]} mutations)";exit 0
fi

WIRED="${1:-}";[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired";exit 64; }
[ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: timeout runner unavailable";exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")";WD="$(dirname "$WIRED")"
HEADLESS="${WIRED_BINARY_HEADLESS:-}";if [ -z "$HEADLESS" ];then suffix="$(basename "$WIRED")";suffix="${suffix#wired}";for candidate in "$WD/wired-headless$suffix" "$WD/wired-headless.arm64" "$WD/wired-headless.x86_64" "$WD/../../../wired-headless$suffix";do [ -x "$candidate" ] && HEADLESS="$candidate" && break;done;fi
[ -n "$HEADLESS" ] && [ -x "$HEADLESS" ] || { echo "SKIP: sibling wired-headless unavailable";exit 77; };HEADLESS="$(cd "$(dirname "$HEADLESS")" && pwd)/$(basename "$HEADLESS")"
PACK="";for candidate in "$WD" "$WD/../Resources" "$WD/../../..";do [ -f "$candidate/base/pax21.sw3z" ] && PACK="$(cd "$candidate" && pwd)" && break;done;[ -n "$PACK" ] || { echo "SKIP: pax21 unavailable beside assembled binary";exit 77; }
PAX21="$PACK/base/pax21.sw3z";PAX01="";for candidate in "${WIRED_CONTENT_ROOT:-}" "$PACK";do [ -n "$candidate" ] && [ -f "$candidate/base/pax01.sw3z" ] && PAX01="$(cd "$candidate/base" && pwd)/pax01.sw3z" && break;done;[ -n "$PAX01" ] || { echo "SKIP: shipped pax01.sw3z unavailable (set WIRED_CONTENT_ROOT)";exit 77; }
SW3Z="${SW3Z_TOOL:-$WD/../../../tools/sw3z-archiver/cmd/sw3z/sw3z}";[ -x "$SW3Z" ] || SW3Z="$SCRIPT_DIR/../tools/sw3z-archiver/cmd/sw3z/sw3z";[ -x "$SW3Z" ] || { echo "SKIP: sw3z tool unavailable";exit 77; };SW3Z="$(cd "$(dirname "$SW3Z")" && pwd)/$(basename "$SW3Z")"
ROOT="$(mktemp -d -t vmi-pack-runtime-XXXXXX 2>/dev/null || mktemp -d)";CLIENT_HOME="$ROOT/client/q3now-preview";SERVER_HOME="$ROOT/server/q3now-preview";EXTRACT="$ROOT/extract";FIFO="$ROOT/server.stdin";SERVER_PID="";OPEN=0;FORCED=0
cleanup(){ local status=$?;trap - EXIT INT TERM;[ "$OPEN" -eq 1 ] && exec 9>&- || true;[ -n "$SERVER_PID" ] && kill -TERM "$SERVER_PID" 2>/dev/null || true;[ -n "$SERVER_PID" ] && wait "$SERVER_PID" 2>/dev/null || true;[ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT";exit "$status";};trap cleanup EXIT;trap 'exit 130' INT;trap 'exit 143' TERM
mkdir -p "$CLIENT_HOME/base" "$SERVER_HOME/base" "$EXTRACT";for home in "$CLIENT_HOME" "$SERVER_HOME";do cp "$PAX01" "$home/base/pax01.sw3z" || exit 1;cp "$PAX21" "$home/base/pax21.sw3z" || exit 1;[ ! -e "$home/base/vm" ] || { echo "FAIL loose VM directory staged";exit 1; };done
for home in "$CLIENT_HOME" "$SERVER_HOME";do [ "$(find "$home/base" -type f | wc -l | tr -d ' ')" = 2 ] && [ -f "$home/base/pax01.sw3z" ] && [ -f "$home/base/pax21.sw3z" ] || { echo "FAIL non-pax file staged";exit 1; };done
"$SW3Z" t "$PAX21" >"$ROOT/sw3z-test.txt" || exit 1;"$SW3Z" l "$PAX21" >"$ROOT/sw3z-list.txt" || exit 1
python3 - "$ROOT/sw3z-list.txt" <<'PYEOF' || exit 1
import re,sys
members=[]
for line in open(sys.argv[1]):
 match=re.fullmatch(r"(.*?)\s+([0-9]+)\s+([0-9]+)\s+(?:Store|LZ4)\s+[0-9A-Fa-f]{8}\s*",line)
 if match:
  path=match.group(1).strip()
  if path.split("/",1)[0].casefold()=="vm":members.append(path)
if members != ["vm/gamecl.wasm","vm/gamesv.wasm"]:raise SystemExit(f"FAIL VM inventory {members}")
PYEOF
"$SW3Z" x "$PAX21" "$EXTRACT" >"$ROOT/sw3z-extract.txt" || exit 1
[ -f "$EXTRACT/vm/gamecl.wasm" ] && [ -f "$EXTRACT/vm/gamesv.wasm" ] || { echo "FAIL extracted VM members";exit 1; }
python3 - "$EXTRACT" <<'PYEOF' || exit 1
import os,sys
root=sys.argv[1];found=[]
for current,_,files in os.walk(root):
 for name in files:
  path=os.path.relpath(os.path.join(current,name),root).replace(os.sep,"/")
  if path.split("/",1)[0].casefold()=="vm":found.append(path)
if sorted(found)!=["vm/gamecl.wasm","vm/gamesv.wasm"]:raise SystemExit(f"FAIL extracted VM inventory {found}")
PYEOF
PORT="$(python3 - <<'PYEOF'
import socket
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(("127.0.0.1",0));print(s.getsockname()[1]);s.close()
PYEOF
)";ENDPOINT="127.0.0.1:$PORT";MANIFEST="$ROOT/manifest.jsonl"
python3 - "$MANIFEST" "$WIRED" "$HEADLESS" "$PAX01" "$PAX21" "$SW3Z" "$0" "$SERVER_HOME/base/pax21.sw3z" "$CLIENT_HOME/base/pax21.sw3z" "$EXTRACT/vm/gamecl.wasm" "$EXTRACT/vm/gamesv.wasm" "$ENDPOINT" <<'PYEOF'
import hashlib,json,os,sys
manifest,*args=sys.argv[1:];server_pax,client_pax=args[-5:-3];member_paths=args[-3:-1];endpoint=args[-1]
members=[]
for name,path in zip(("vm/gamecl.wasm","vm/gamesv.wasm"),member_paths):
 data=open(path,"rb").read();members.append({"path":name,"extracted":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
with open(manifest,"w") as out:
 server_data=open(server_pax,"rb").read();client_data=open(client_pax,"rb").read()
 out.write(json.dumps({"kind":"scenario","schema":1,"name":"vmi-pack-runtime","map":"arena7","vm_game":2,"vm_cgame":2,"sv_pure":1,"endpoint":endpoint,"server_pax21":os.path.abspath(server_pax),"server_pax21_bytes":len(server_data),"server_pax21_sha256":hashlib.sha256(server_data).hexdigest(),"client_pax21":os.path.abspath(client_pax),"client_pax21_bytes":len(client_data),"client_pax21_sha256":hashlib.sha256(client_data).hexdigest(),"vm_members":members},sort_keys=True)+"\n")
 for role,path in zip(("gui","headless","pax01","pax21","sw3z","harness"),args[:6]):
  data=open(path,"rb").read();out.write(json.dumps({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()},sort_keys=True)+"\n")
PYEOF
mkfifo "$FIFO";exec 9<>"$FIFO";OPEN=1
(cd "$(dirname "$HEADLESS")" && exec "$HEADLESS" +set fs_homepath "$SERVER_HOME" +set com_automated 1 +set com_noHardReboot 1 +set net_ip 127.0.0.1 +set net_port "$PORT" +set vm_game 2 +set sv_pure 1 +set g_autoBots 0 +set g_minPlayers 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +map arena7 <&9) >"$ROOT/server.stdout" 2>&1 &SERVER_PID=$!;RUN_SERVER_PID=$SERVER_PID;SERVER_JSON="$SERVER_HOME/qconsole.jsonl"
for _ in $(seq 1 900);do python3 - "$SERVER_JSON" <<'PYEOF' >/dev/null 2>&1 && break
import json,sys
msgs=[json.loads(x).get("msg","") for x in open(sys.argv[1]) if x.strip()]
raise SystemExit(0 if any(x.startswith("VM_Create policy module=gamesv requested=2 effective=2") for x in msgs) and any(x.startswith("InitGame:") for x in msgs) else 1)
PYEOF
 sleep .1;kill -0 "$SERVER_PID" 2>/dev/null || { echo "FAIL server exited";exit 1; };done
python3 - "$SERVER_JSON" <<'PYEOF' >/dev/null 2>&1 || { echo "FAIL server readiness";exit 1; }
import json,sys
msgs=[json.loads(x).get("msg","") for x in open(sys.argv[1]) if x.strip()]
raise SystemExit(0 if any(x.startswith("VM_Create policy module=gamesv requested=2 effective=2") for x in msgs) and any(x.startswith("InitGame:") for x in msgs) else 1)
PYEOF
case "$(uname -s)" in Darwin) PLATFORM=(-ApplePersistenceIgnoreState YES);;*) PLATFORM=();;esac
CLIENT_JSON="$CLIENT_HOME/qconsole.jsonl";python3 "$TIMEOUT_RUNNER" --timeout 150 --kill-after 15 --cwd "$WD" --stdout "$ROOT/client.stdout" -- "$WIRED" "${PLATFORM[@]}" +set fs_homepath "$CLIENT_HOME" +set com_automated 1 +set com_noHardReboot 1 +set name PaxWitness +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set net_ip 127.0.0.1 +set net_port 0 +set vm_game 2 +set vm_cgame 2 +set sv_pure 1 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +set activeAction "viewpos ; sysinfo ; echo Q0_VMI_PACK_CLIENT_SYSINFO_DONE ; wait 30 ; say Q0_VMI_PACK_PURE_CONTINUITY ; wait 30 ; quit" +wait 100 +connect "$ENDPOINT" &CLIENT_CONTROLLER_PID=$!
for _ in $(seq 1 1200);do python3 - "$CLIENT_JSON" <<'PYEOF' >/dev/null 2>&1 && break
import json,sys
msgs=[json.loads(x).get("msg","") for x in open(sys.argv[1]) if x.strip()]
raise SystemExit(0 if any(x.startswith("cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME") for x in msgs) else 1)
PYEOF
 sleep .1;kill -0 "$CLIENT_CONTROLLER_PID" 2>/dev/null || { echo "FAIL client exited before FIRST";exit 1; };done
printf 'echo Q0_VMI_PACK_SERVER_SYSINFO_REQUESTED; sysinfo; echo Q0_VMI_PACK_SERVER_SYSINFO_DONE\n' >&9
for _ in $(seq 1 300);do python3 - "$SERVER_JSON" <<'PYEOF' >/dev/null 2>&1 && break
import json,sys
msgs=[json.loads(x).get("msg","") for x in open(sys.argv[1]) if x.strip()]
raise SystemExit(0 if any(x.startswith("Q0_VMI_PACK_SERVER_SYSINFO_DONE") for x in msgs) else 1)
PYEOF
 sleep .1;kill -0 "$SERVER_PID" 2>/dev/null || { echo "FAIL server exited before sysinfo";exit 1; };done
python3 - "$SERVER_JSON" <<'PYEOF' >/dev/null 2>&1 || { echo "FAIL server sysinfo completion";exit 1; }
import json,sys
raise SystemExit(0 if any(json.loads(x).get("msg","").startswith("Q0_VMI_PACK_SERVER_SYSINFO_DONE") for x in open(sys.argv[1]) if x.strip()) else 1)
PYEOF
wait "$CLIENT_CONTROLLER_PID";CLIENT_RC=$?
printf 'quit\n' >&9;for _ in $(seq 1 300);do kill -0 "$SERVER_PID" 2>/dev/null || break;sleep .1;done;if kill -0 "$SERVER_PID" 2>/dev/null;then FORCED=1;kill -TERM "$SERVER_PID" 2>/dev/null || true;fi;wait "$SERVER_PID";SERVER_RC=$?;SERVER_PID="";exec 9>&-;OPEN=0
python3 - "$MANIFEST" "$CLIENT_RC" "$SERVER_RC" "$RUN_SERVER_PID" "$CLIENT_CONTROLLER_PID" "$FORCED" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a") as out:out.write(json.dumps({"kind":"result","client_rc":int(sys.argv[2]),"server_rc":int(sys.argv[3]),"server_pid":int(sys.argv[4]),"client_controller_pid":int(sys.argv[5]),"timeout":False,"forced":bool(int(sys.argv[6]))},sort_keys=True)+"\n")
PYEOF
[ "$CLIENT_RC" -eq 0 ] && [ "$SERVER_RC" -eq 0 ] && [ "$FORCED" -eq 0 ] || { echo "FAIL process result";exit 1; };analyze_contract "$CLIENT_JSON" "$SERVER_JSON" "$MANIFEST" || exit 1;echo "PASS vmi pack runtime gate"
