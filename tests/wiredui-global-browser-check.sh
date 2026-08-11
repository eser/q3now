#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# SPDX-License-Identifier: GPL-3.0-or-later
# Deterministic real global-browser discovery. No cache injection or Connect.

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FIXTURE="$SCRIPT_DIR/wiredui-global-server-fixture.py"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze() {
python3 - "$1" "$2" <<'PYEOF'
import json,re,sys
pp,fp=sys.argv[1:3]
def load(path, fields):
 rows=[]
 for n,line in enumerate(open(path,encoding="utf-8",errors="replace"),1):
  try:r=json.loads(line)
  except Exception as e:raise SystemExit(f"FAIL {path}:{n}: {e}")
  if not isinstance(r,dict) or any(not isinstance(r.get(k),str) for k in fields):raise SystemExit(f"FAIL {path}:{n}: schema")
  rows.append(r)
 if not rows:raise SystemExit(f"FAIL empty {path}")
 return rows
p=load(pp,("sev","cat","msg")); f=load(fp,("event",))
bad=[r for r in p if r["sev"].upper() in {"ERROR","FATAL"} or r["sev"].upper()=="WARN" and r["cat"].lower()=="ui"]
if bad:raise SystemExit("FAIL severity")
m=[r["msg"] for r in p]; ev=[r["event"] for r in f]
ready=[r for r in f if r["event"]=="ready"]
if len(ready)!=1:raise SystemExit("FAIL fixture ready")
q=ready[0]
addr={k:f"127.0.0.1:{q[k+'_port']}" for k in ("master","rogue","target","sentinel")}
def ordered(steps):
 cur=0
 for name,pat in steps:
  for i in range(cur,len(m)):
   if re.search(pat,m[i]):cur=i+1;break
  else:raise SystemExit(f"FAIL contract {name}")
E={k:re.escape(v) for k,v in addr.items()}
ordered([
 ("menu",r"WiredUI: push menu 'servers' \(depth 1\)"),
 ("source focus",r"wui_menu_nav focus: focused item 'filter_source'"),
 ("source enter",r"wui_menu_nav: K_ENTER dispatched"),
 ("refresh focus",r"wui_menu_nav focus: focused item 'btn_refresh'"),
 ("refresh enter",r"wui_menu_nav: K_ENTER dispatched"),
 ("master query",rf"Master query generation=[0-9]+ address={E['master']} extended=0 timeout=[0-9]+ms"),
 ("rogue rejected",rf"Ignored unauthorized getserversResponse from {E['rogue']}"),
 ("master accepted",rf"Accepted getserversResponse generation=[0-9]+ address={E['master']} parsed=2 total=2"),
 ("pending raw only",r"WiredUI: server roster generation=[0-9]+ source=1 displayed=0 raw=2"),
 ("roster",r"WiredUI: server roster generation=[0-9]+ source=1 displayed=2 raw=2"),
 ("sentinel",rf"WiredUI: server roster row=0 raw=1 address={E['sentinel']} name=A0 WIRED GLOBAL SENTINEL map=arena1"),
 ("target",rf"WiredUI: server roster row=1 raw=0 address={E['target']} name=Z0 WIRED GLOBAL TARGET map=arena7"),
 ("list focus",r"wui_menu_nav focus: focused item 'serverlist'"),
 ("target callback",rf"WiredUI: server selection display_row=1 raw=0 source=1 .* address={E['target']} name=Z0 WIRED GLOBAL TARGET map=arena7"),
 ("down",r"wui_menu_nav: K_DOWNARROW dispatched"),
 ("pop",r"WiredUI: pop menu \(depth 0\)"),
 ("shutdown",r"WiredUI: shutdown")])
requests={}; request_rows=[]
for msg in m:
 x=re.search(r"Ping request generation=([0-9]+) challenge=([0-9a-f]+) timeout=[0-9]+ms address=(\S+)",msg)
 if x:
  request_rows.append(x.groups());requests[x.group(3)]=(x.group(1),x.group(2))
if len(request_rows)!=2:raise SystemExit("FAIL directed ping cardinality")
identities=[requests.get(addr[r]) for r in ("target","sentinel")]
if any(not x or len(x[1])!=32 for x in identities):raise SystemExit("FAIL challenge shape")
if len({x[0] for x in identities})!=2 or len({x[1] for x in identities})!=2:raise SystemExit("FAIL challenge reuse")
master_i=next(i for i,x in enumerate(m) if "Accepted getserversResponse" in x)
first_ping_i=next(i for i,x in enumerate(m) if "Accepted ping infoResponse" in x)
if any("server roster row=" in x or "server selection" in x for x in m[master_i+1:first_ping_i]):
 raise SystemExit("FAIL pre-hydration row authority")
for role in ("sentinel","target"):
 a=addr[role]
 if a not in requests:raise SystemExit(f"FAIL no ping request {role}")
 rows=[r for r in f if r["event"]=="current_info_response" and r.get("role")==role]
 wrong=[r for r in f if r["event"]=="wrong_info_response" and r.get("role")==role]
 if len(rows)!=1 or len(wrong)!=1 or rows[0].get("challenge")!=requests[a][1] or wrong[0].get("expected")!=requests[a][1]:raise SystemExit(f"FAIL challenge correlation {role}")
 reject=rf"Ignored infoResponse challenge mismatch generation={requests[a][0]} from {re.escape(a)}"
 accept=rf"Accepted ping infoResponse generation={requests[a][0]} time=[1-9][0-9]*ms address={re.escape(a)}"
 if not any(re.search(reject,x) for x in m) or not any(re.search(accept,x) for x in m):raise SystemExit(f"FAIL ping lifecycle {role}")
if ev.count("rogue_master_response")!=1 or ev.count("authorized_master_response")!=1 or ev.count("stopped")!=1 or "decoy_ping" in ev:raise SystemExit("FAIL master authority")
master_req=[r for r in f if r["event"]=="request" and r.get("role")=="master"]
if len(master_req)!=1 or master_req[0].get("command")!="getservers q3now 74":raise SystemExit("FAIL exact getservers")
if any("BAD SENTINEL" in x or "BAD TARGET" in x for x in m):raise SystemExit("FAIL rejected metadata admitted")
if sum("Accepted getserversResponse" in x for x in m)!=1 or sum("server selection display_row=1 raw=0 source=1" in x for x in m)!=1:raise SystemExit("FAIL cardinality")
print("  PASS global discovery: authorized master -> challenge-bound pings -> keyboard row1")
PYEOF
}

if [ "${1:-}" = --self-test ]; then
 ROOT="$(mktemp -d -t wired-global-self-XXXXXX 2>/dev/null || mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
 P="$ROOT/p"; F="$ROOT/f"
 python3 - "$P" "$F" <<'PYEOF'
import json,sys
p,f=sys.argv[1:3]; a={"master":"127.0.0.1:30001","rogue":"127.0.0.1:30002","target":"127.0.0.1:30003","sentinel":"127.0.0.1:30004"}; one="1"*32; two="2"*32
msgs=["WiredUI: push menu 'servers' (depth 1)","wui_menu_nav focus: focused item 'filter_source' (top index -1)","wui_menu_nav: K_ENTER dispatched","wui_menu_nav focus: focused item 'btn_refresh' (top index -1)","wui_menu_nav: K_ENTER dispatched",f"Master query generation=7 address={a['master']} extended=0 timeout=3000ms",f"Ignored unauthorized getserversResponse from {a['rogue']}",f"Accepted getserversResponse generation=7 address={a['master']} parsed=2 total=2","WiredUI: server roster generation=3 source=1 displayed=0 raw=2",f"Ping request generation=8 challenge={one} timeout=800ms address={a['target']}",f"Ignored infoResponse challenge mismatch generation=8 from {a['target']}",f"Accepted ping infoResponse generation=8 time=17ms address={a['target']}",f"Ping request generation=9 challenge={two} timeout=800ms address={a['sentinel']}",f"Ignored infoResponse challenge mismatch generation=9 from {a['sentinel']}",f"Accepted ping infoResponse generation=9 time=19ms address={a['sentinel']}","WiredUI: server roster generation=4 source=1 displayed=2 raw=2",f"WiredUI: server roster row=0 raw=1 address={a['sentinel']} name=A0 WIRED GLOBAL SENTINEL map=arena1",f"WiredUI: server roster row=1 raw=0 address={a['target']} name=Z0 WIRED GLOBAL TARGET map=arena7","wui_menu_nav focus: focused item 'serverlist' (top index 1)",f"WiredUI: server selection display_row=1 raw=0 source=1 list_generation=4 selection_generation=2 address={a['target']} name=Z0 WIRED GLOBAL TARGET map=arena7","wui_menu_nav: K_DOWNARROW dispatched","WiredUI: pop menu (depth 0)","WiredUI: shutdown"]
with open(p,"w") as o:
 for x in msgs:o.write(json.dumps({"sev":"DEBUG","cat":"ui","msg":x})+"\n")
rows=[{"event":"ready","master_port":30001,"rogue_port":30002,"target_port":30003,"sentinel_port":30004},{"event":"request","role":"master","command":"getservers q3now 74"},{"event":"rogue_master_response"},{"event":"authorized_master_response"},{"event":"wrong_info_response","role":"target","challenge":"f"*32,"expected":one},{"event":"current_info_response","role":"target","challenge":one},{"event":"wrong_info_response","role":"sentinel","challenge":"f"*32,"expected":two},{"event":"current_info_response","role":"sentinel","challenge":two},{"event":"stopped"}]
with open(f,"w") as o:
 for x in rows:o.write(json.dumps(x)+"\n")
PYEOF
 analyze "$P" "$F" >/dev/null || { echo 'FAIL self clean'; exit 1; }
 defects='source_focus source_enter refresh_focus refresh_enter query rogue_reject master_accept parsed placeholder_visible challenge_target challenge_sentinel challenge_reuse wrong_target current_target wrong_sentinel current_sentinel decoy roster sentinel_row target_row callback down pop shutdown bad_name duplicate_master severity malformed_product malformed_fixture'
 for d in $defects; do
  D="$ROOT/$d"; mkdir "$D"
  python3 - "$P" "$F" "$D/p" "$D/f" "$d" <<'PYEOF' || { echo "FAIL self fixture $d"; exit 1; }
import json,re,sys
ps,fs,pd,fd,d=sys.argv[1:6]; p=[json.loads(x) for x in open(ps)]; f=[json.loads(x) for x in open(fs)]
keys={"source_focus":"filter_source","source_enter":"K_ENTER dispatched","refresh_focus":"btn_refresh","query":"Master query","rogue_reject":"unauthorized getservers","master_accept":"Accepted getservers","roster":"displayed=2 raw=2","sentinel_row":"GLOBAL SENTINEL","target_row":"GLOBAL TARGET","callback":"server selection display_row=1","down":"K_DOWNARROW","pop":"pop menu","shutdown":"WiredUI: shutdown"}
if d in keys:p=[r for r in p if keys[d] not in r["msg"]]
elif d=="source_enter":p.pop(next(i for i,r in enumerate(p) if "K_ENTER" in r["msg"]))
elif d=="refresh_enter":
 i=[i for i,r in enumerate(p) if "K_ENTER" in r["msg"]][1];p.pop(i)
elif d=="parsed":p[next(i for i,r in enumerate(p) if "Accepted getservers" in r["msg"])]["msg"]="Accepted getserversResponse generation=7 address=127.0.0.1:30001 parsed=1 total=1"
elif d=="placeholder_visible":
 i=next(i for i,r in enumerate(p) if "Ping request" in r["msg"]);p.insert(i,{"sev":"DEBUG","cat":"ui","msg":"WiredUI: server roster row=0 raw=0 address=127.0.0.1:30003 name=UNHYDRATED map="})
elif d in ("challenge_target","challenge_sentinel"):
 port="30003" if d.endswith("target") else "30004";p=[r for r in p if not ("Ping request" in r["msg"] and port in r["msg"])]
elif d=="challenge_reuse":
 first=next(re.search(r"challenge=([0-9a-f]+)",r["msg"]).group(1) for r in p if "Ping request" in r["msg"])
 for r in p:
  if "Ping request" in r["msg"] and "30004" in r["msg"]:r["msg"]=re.sub(r"challenge=[0-9a-f]+",f"challenge={first}",r["msg"])
 for r in f:
  if r.get("role")=="sentinel" and r.get("event")=="wrong_info_response":r["expected"]=first
  if r.get("role")=="sentinel" and r.get("event")=="current_info_response":r["challenge"]=first
elif d in ("wrong_target","current_target","wrong_sentinel","current_sentinel"):
 event,role=d.split('_'); en="wrong_info_response" if event=="wrong" else "current_info_response";f=[r for r in f if not(r.get("event")==en and r.get("role")==role)]
elif d=="decoy":f.append({"event":"decoy_ping"})
elif d=="bad_name":p.append({"sev":"DEBUG","cat":"ui","msg":"BAD TARGET"})
elif d=="duplicate_master":p.insert(8,p[7].copy())
elif d=="severity":p.append({"sev":"ERROR","cat":"renderer","msg":"synthetic"})
with open(pd,"w") as o:
 [o.write(json.dumps(r)+"\n") for r in p]
 if d=="malformed_product":o.write("{bad\n")
with open(fd,"w") as o:
 [o.write(json.dumps(r)+"\n") for r in f]
 if d=="malformed_fixture":o.write("[]\n")
PYEOF
  if analyze "$D/p" "$D/f" >/dev/null 2>&1; then echo "FAIL self $d"; exit 1; fi
 done
 echo '==> WiredUI global-browser analyzer self-test: PASS'; exit 0
fi

command -v python3 >/dev/null 2>&1 && [ -f "$FIXTURE" ] && [ -f "$TIMEOUT_RUNNER" ] || { echo 'SKIP: Python unavailable'; exit 77; }
WIRED="${1:-}"; [ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo 'SKIP: pass assembled Wired GUI binary'; exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"; WD="$(dirname "$WIRED")"
PACK=""; for c in "$WD" "$WD/../Resources" "$WD/../../.."; do [ -f "$c/base/pax21.sw3z" ] && PACK="$(cd "$c" && pwd)" && break; done
[ -n "$PACK" ] || { echo 'SKIP: pax21 missing'; exit 77; }
BASE="${WIRED_CONTENT_ROOT:-$PACK}"; if [ -f "$BASE/base/pax01.sw3z" ]; then ARCH="$BASE/base/pax01.sw3z"; elif [ -f "$BASE/base/pak0.pk3" ]; then ARCH="$BASE/base/pak0.pk3"; else echo 'SKIP: base archive missing'; exit 77; fi
ROOT="$(mktemp -d -t wired-global-XXXXXX 2>/dev/null || mktemp -d)"; HOME_DIR="$ROOT/q3now-preview"; EVENTS="$ROOT/fixture.jsonl"; STDOUT="$ROOT/stdout"; PID=""
cleanup(){ [ -n "$PID" ] && kill -TERM "$PID" 2>/dev/null || true; [ -n "$PID" ] && wait "$PID" 2>/dev/null || true; [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"; }; trap cleanup EXIT INT TERM
mkdir -p "$HOME_DIR/base"; cp "$ARCH" "$HOME_DIR/base/" || exit 1; cp "$PACK/base/pax21.sw3z" "$HOME_DIR/base/pax21.sw3z" || exit 1
python3 "$FIXTURE" --events "$EVENTS" --timeout 90 & PID=$!; for _ in $(seq 1 100); do [ -s "$EVENTS" ] && break; sleep .05; done; [ -s "$EVENTS" ] || { echo 'FAIL fixture ready'; exit 1; }
read -r MASTER_PORT <<EOF
$(python3 - "$EVENTS" <<'PYEOF'
import json,sys
print(json.loads(open(sys.argv[1]).readline())["master_port"])
PYEOF
)
EOF
cat >"$HOME_DIR/base/global-browser.cfg" <<'CFGEOF'
wait 100
wui_push servers
wait 20
wui_menu_nav focus filter_source
wui_menu_nav enter
wui_menu_nav focus btn_refresh
wui_menu_nav enter
wait 300
wui_listbox_sort 0
wait 10
wui_menu_nav focus serverlist
wui_menu_nav down
wait 20
wui_menu_nav back
wait 20
quit
CFGEOF
case "$(uname -s)" in Darwin) NATIVE="$HOME_DIR"; ARGS=(-ApplePersistenceIgnoreState YES);; MINGW*|MSYS*|CYGWIN*) NATIVE="$(cygpath -w "$HOME_DIR")"; ARGS=();; *) NATIVE="$HOME_DIR"; ARGS=();; esac
python3 "$TIMEOUT_RUNNER" --timeout 90 --kill-after 10 --cwd "$WD" --stdout "$STDOUT" -- "$WIRED" "${ARGS[@]}" +set fs_homepath "$NATIVE" +set com_automated 1 +set com_noHardReboot 1 +set net_enabled 1 +set net_ip 127.0.0.1 +set net_port 0 +set sv_master1 "127.0.0.1:$MASTER_PORT" +set s_initsound 0 +set r_fullscreen 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec global-browser.cfg
rc=$?; [ "$rc" -eq 0 ] && [ -s "$HOME_DIR/qconsole.jsonl" ] || { echo "FAIL product rc=$rc"; exit 1; }
kill -TERM "$PID" 2>/dev/null || true; wait "$PID" || { echo 'FAIL fixture exit'; exit 1; }; PID=""
analyze "$HOME_DIR/qconsole.jsonl" "$EVENTS" || exit 1
echo '==> WiredUI global-browser gate: PASS'
