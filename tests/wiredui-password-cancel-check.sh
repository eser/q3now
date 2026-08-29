#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Password Cancel/ESC secret disposal and underlying Server Info preservation.

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"
FIXTURE="$SCRIPT_DIR/wiredui-server-fixture.py"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze() {
python3 - "$1" "$2" "$3" "$4" "$5" "$6" <<'PYEOF'
import json,re,sys
pp,fp,lp,target_port,client_port,sentinel_port=sys.argv[1:]
target=f"127.0.0.1:{target_port}"; et=re.escape(target)
def load(path):
 rows=[]
 for n,line in enumerate(open(path,encoding="utf-8",errors="replace"),1):
  try:r=json.loads(line)
  except Exception as e:raise SystemExit(f"FAIL {path}:{n}: {e}")
  if not isinstance(r,dict):raise SystemExit(f"FAIL {path}:{n}: schema")
  rows.append(r)
 return rows
p=load(pp);f=load(fp);layout=load(lp)
if not p or not f or not layout:raise SystemExit("FAIL empty evidence")
for path in (pp,fp,lp):
 evidence=open(path,"rb").read()
 if any(secret in evidence for secret in (b"WipeA",b"WipeBB",b"WipeCCC")):raise SystemExit("FAIL secret evidence")
if any(not all(isinstance(r.get(k),str) for k in ("sev","cat","msg")) for r in p):raise SystemExit("FAIL product schema")
if any(not isinstance(r.get("event"),str) for r in f):raise SystemExit("FAIL fixture schema")
if any(r["sev"].upper() in ("ERROR","FATAL") or r["sev"].upper()=="WARN" and r["cat"].lower()=="ui" for r in p):raise SystemExit("FAIL severity")
def norm(v):
 if v.endswith("\n"):v=v[:-1]
 return v
vals=[norm(r["msg"]) for r in p]
claimed=("WiredUI: password required ","WiredUI: password state ","WiredUI: password render trace ","WiredUI: password cancel postcondition ","WiredUI: server status request ","WiredUI: server status state=", "WiredUI: server status loaded ","WiredUI: server status failed ","WiredUI: server status refused ","WiredUI: server status row=", "WiredUI: server status snapshot ","WiredUI: server status registry ","WiredUI: server status cancelled ","WiredUI: server status dispose ","WiredUI: server selection ","Browser connect attempt armed ","WiredUI: started validated connect ","Browser connect credential disposed ","WiredUI: password submit ","WiredUI: authentication retry ","Connect failed ","QUIC client:","SV_OnPlayerConnect:","FIRST GAMEPLAY FRAME","WiredUI: close all ","WiredUI: push menu 'error_popup'","WiredUI: push menu 'popup_message'","wui_menu_nav: K_","wui_menu_nav focus:","wui_menu_nav: typed ","WiredUI: pointer phase=", "WiredUI: push menu 'password'", "WiredUI: pop menu ")
for value in vals:
 lines=re.split(r"[\r\n]",value)
 if len(lines)>1 and (any(line.startswith(claimed) for line in lines) or any(" resolved to " in line for line in lines)):raise SystemExit("FAIL claimed multiline")
def exact(prefix,patterns,sev="DEBUG",cat="ui"):
 rows=[(i,p[i],v) for i,v in enumerate(vals) if v.startswith(prefix)]
 if len(rows)!=len(patterns):raise SystemExit(f"FAIL {prefix} cardinality")
 for (_,r,v),pat in zip(rows,patterns):
  if r["sev"].upper()!=sev or r["cat"].lower()!=cat or re.fullmatch(pat,v) is None:raise SystemExit(f"FAIL {prefix} metadata")
 return rows
ready=[r for r in f if r["event"]=="ready"]
expected_ready={"event":"ready","client_port":int(client_port),"sentinel_port":int(sentinel_port),"target_port":int(target_port),"protocol":74}
if ready!=[expected_ready]:raise SystemExit("FAIL fixture ready")
events=[r["event"] for r in f]
if events != ["ready","request","challenge_seen","status_response","stopped"]:raise SystemExit("FAIL fixture events")
if f[1]!={"event":"request","role":"target","command":"getstatus","peer_port":int(client_port)}:raise SystemExit("FAIL fixture request")
if set(f[2])!={"event","ordinal","challenge"} or f[2].get("ordinal")!=1 or re.fullmatch(r"[0-9a-f]{16}",f[2].get("challenge","") ) is None:raise SystemExit("FAIL fixture challenge")
if f[3]!={"event":"status_response","role":"target","peer_port":int(client_port)} or f[4]!={"event":"stopped","reason":"signal"}:raise SystemExit("FAIL fixture lifecycle")
sel=exact("WiredUI: server selection ",[r"WiredUI: server selection display_row=0 raw=1 source=0 list_generation=([1-9][0-9]*) selection_generation=([1-9][0-9]*) address=127\.0\.0\.1:[0-9]+ name=A0 WIRED Q0 SENTINEL map=arena1",rf"WiredUI: server selection display_row=1 raw=0 source=0 list_generation=([1-9][0-9]*) selection_generation=([1-9][0-9]*) address={et} name=Z0 WIRED Q0 TARGET map=arena7"])
sm0=re.fullmatch(r"WiredUI: server selection display_row=0 raw=1 source=0 list_generation=([1-9][0-9]*) selection_generation=([1-9][0-9]*) address=127\.0\.0\.1:[0-9]+ name=A0 WIRED Q0 SENTINEL map=arena1",sel[0][2]);sm=re.fullmatch(rf"WiredUI: server selection display_row=1 raw=0 source=0 list_generation=([1-9][0-9]*) selection_generation=([1-9][0-9]*) address={et} name=Z0 WIRED Q0 TARGET map=arena7",sel[1][2]);sg=sm.group(2)
if sm0.group(1)!=sm.group(1) or int(sm0.group(2))+1!=int(sg):raise SystemExit("FAIL selection epochs")
request=exact("WiredUI: server status request ",[rf"WiredUI: server status request generation=([1-9][0-9]*) selection_generation={sg} address={et}"])
rm=re.fullmatch(rf"WiredUI: server status request generation=([1-9][0-9]*) selection_generation={sg} address={et}",request[0][2]);status_gen=rm.group(1)
exact("WiredUI: server status state=",[rf"WiredUI: server status state=pending generation={status_gen} selection_generation={sg} address={et} rows=1"])
loaded=exact("WiredUI: server status loaded ",[rf"WiredUI: server status loaded generation={status_gen} selection_generation={sg} address={et} rows=9"])
status_patterns=[
 rf"WiredUI: server status row=0 key=Address value={et}",r"WiredUI: server status row=1 key=Server value=Z0 WIRED Q0 TARGET",r"WiredUI: server status row=2 key=Map value=arena7",r"WiredUI: server status row=3 key=Players value=1/8",r"WiredUI: server status row=4 key=Game type value=0",r"WiredUI: server status row=5 key=Game value=q3now",r"WiredUI: server status row=6 key=Protocol value=74",rf"WiredUI: server status row=7 key=Version value=Q0 fixture {target_port}",r"WiredUI: server status row=8 key=Player 1 value=StatusBot — score 7, ping 23"]
status_rows=exact("WiredUI: server status row=",status_patterns)
registry_patterns=[]
for _ in range(5):
 registry_patterns.append(r"WiredUI: server status registry feeder=13 count=9")
 registry_patterns += [pat.replace("server status row=","server status registry row=") for pat in status_patterns]
registry=exact("WiredUI: server status registry ",registry_patterns)
snapshot=exact("WiredUI: server status snapshot ",[rf"WiredUI: server status snapshot state=ready generation={status_gen} selection_generation={sg} address={et} count=9"]*5)
pre=exact("WiredUI: password required ",[rf"WiredUI: password required origin=browser address={et} selection_generation={sg}"]*4)
push=exact("WiredUI: push menu 'password'",[r"WiredUI: push menu 'password' \(depth 3\)"]*4)
states=exact("WiredUI: password state ",[
 rf"WiredUI: password state valid=1 secret_length=0 editing=0 selection_generation={sg} address={et} target_empty=0 error_present=0 top=password depth=3",
 r"WiredUI: password state valid=0 secret_length=0 editing=0 selection_generation=0 address=none target_empty=1 error_present=0 top=serverinfo depth=2",
 rf"WiredUI: password state valid=1 secret_length=0 editing=0 selection_generation={sg} address={et} target_empty=0 error_present=0 top=password depth=3",
 r"WiredUI: password state valid=0 secret_length=0 editing=0 selection_generation=0 address=none target_empty=1 error_present=0 top=serverinfo depth=2",
 rf"WiredUI: password state valid=1 secret_length=0 editing=0 selection_generation={sg} address={et} target_empty=0 error_present=0 top=password depth=3",
 r"WiredUI: password state valid=0 secret_length=0 editing=0 selection_generation=0 address=none target_empty=1 error_present=0 top=serverinfo depth=2",
 rf"WiredUI: password state valid=1 secret_length=0 editing=0 selection_generation={sg} address={et} target_empty=0 error_present=0 top=password depth=3"])
render=exact("WiredUI: password render trace ",[r"WiredUI: password render trace length=0 masked=0",r"WiredUI: password render trace length=6 masked=0",r"WiredUI: password render trace length=0 masked=0",r"WiredUI: password render trace length=6 masked=1",r"WiredUI: password render trace length=0 masked=0",r"WiredUI: password render trace length=8 masked=0",r"WiredUI: password render trace length=0 masked=0"])
cancel=exact("WiredUI: password cancel postcondition ",[
 r"WiredUI: password cancel postcondition origin=button valid=0 secret_length=0 editing=0 target_empty=1 error_empty=1 top=serverinfo depth=2",
 r"WiredUI: password cancel postcondition origin=menu-escape valid=0 secret_length=0 editing=0 target_empty=1 error_empty=1 top=serverinfo depth=2",
 r"WiredUI: password cancel postcondition origin=edit-escape valid=0 secret_length=0 editing=0 target_empty=1 error_empty=1 top=serverinfo depth=2"])
pop=exact("WiredUI: pop menu ",[r"WiredUI: pop menu \(depth 2\)"]*3)
typed=exact("wui_menu_nav: typed ",[r"wui_menu_nav: typed 5 printable character\(s\)",r"wui_menu_nav: typed 6 printable character\(s\)",r"wui_menu_nav: typed 7 printable character\(s\)"])
esc=exact("wui_menu_nav: K_ESCAPE",[r"wui_menu_nav: K_ESCAPE dispatched"]*2)
nav=exact("wui_menu_nav: K_",[r"wui_menu_nav: K_DOWNARROW dispatched",r"wui_menu_nav: K_ENTER dispatched",r"wui_menu_nav: K_ENTER dispatched",r"wui_menu_nav: K_ENTER dispatched",r"wui_menu_nav: K_ENTER dispatched",r"wui_menu_nav: K_ENTER dispatched",r"wui_menu_nav: K_ENTER dispatched",r"wui_menu_nav: K_ESCAPE dispatched",r"wui_menu_nav: K_ENTER dispatched",r"wui_menu_nav: K_ENTER dispatched",r"wui_menu_nav: K_ESCAPE dispatched",r"wui_menu_nav: K_ENTER dispatched"])
focus=exact("wui_menu_nav focus:",[r"wui_menu_nav focus: focused item 'serverlist' \(top index -1\)",r"wui_menu_nav focus: focused item 'btn_info' \(top index -1\)",r"wui_menu_nav focus: focused item 'btn_connect' \(top index -1\)",r"wui_menu_nav focus: focused item 'row_password' \(top index -1\)",r"wui_menu_nav focus: focused item 'row_password' \(top index -1\)",r"wui_menu_nav focus: focused item 'row_password' \(top index -1\)",r"wui_menu_nav focus: focused item 'row_password' \(top index -1\)"])
pointer=exact("WiredUI: pointer phase=",[
 r"WiredUI: pointer phase=release reason=reload was_down=0 pointer_down=0",
 r"WiredUI: pointer phase=release reason=push was_down=0 pointer_down=0",
 r"WiredUI: pointer phase=release reason=push was_down=0 pointer_down=0",
 r"WiredUI: pointer phase=release reason=push was_down=0 pointer_down=0",
 r"WiredUI: pointer phase=moved ingress=CL_MouseEvent menu=password item=btn_cancel x=[0-9]+ y=[0-9]+",
 r"WiredUI: pointer phase=click ingress=CL_KeyEvent key=K_MOUSE1 x=[0-9]+ y=[0-9]+",
 r"WiredUI: pointer phase=release reason=pop was_down=1 pointer_down=0",
 r"WiredUI: pointer phase=release reason=push was_down=0 pointer_down=0",
 r"WiredUI: pointer phase=release reason=pop was_down=0 pointer_down=0",
 r"WiredUI: pointer phase=release reason=push was_down=0 pointer_down=0",
 r"WiredUI: pointer phase=release reason=pop was_down=0 pointer_down=0",
 r"WiredUI: pointer phase=release reason=push was_down=0 pointer_down=0",
 r"WiredUI: pointer phase=release reason=shutdown was_down=0 pointer_down=0"])
hover,click=pointer[4],pointer[5]
if not request[0][0]<loaded[0][0]<status_rows[-1][0]<snapshot[0][0]<registry[0][0]<pre[0][0]:raise SystemExit("FAIL initial order")
for n in range(3):
 if not pre[n][0]<push[n][0]<states[2*n][0]<render[2*n][0]<typed[n][0]<render[2*n+1][0]<pop[n][0]<cancel[n][0]<states[2*n+1][0]<snapshot[n+1][0]<registry[(n+1)*10][0]:raise SystemExit(f"FAIL cancel phase {n}")
if not registry[30][0]<pre[3][0]<push[3][0]<nav[11][0]<states[6][0]<focus[6][0]<render[6][0]<snapshot[4][0]<registry[40][0]:raise SystemExit("FAIL final natural reopen")
if not focus[1][0]<request[0][0]<nav[1][0]<loaded[0][0] or not focus[2][0]<pre[0][0]<push[0][0]<nav[2][0]<states[0][0]:raise SystemExit("FAIL authored serverinfo route")
if not focus[3][0]<nav[3][0]<typed[0][0] or not focus[4][0]<nav[5][0]<typed[1][0]<nav[6][0] or not focus[5][0]<nav[9][0]<typed[2][0]<nav[10][0]:raise SystemExit("FAIL authored password route")
for state_index,cancel_index,snapshot_index,registry_index,nav_index,pre_index in ((1,0,1,10,4,1),(3,1,2,20,8,2),(5,2,3,30,11,3)):
 if not cancel[cancel_index][0]<states[state_index][0]<snapshot[snapshot_index][0]<registry[registry_index][0]<pre[pre_index][0]<push[pre_index][0]<nav[nav_index][0]<states[2*pre_index][0]:raise SystemExit("FAIL natural return route")
for n in range(5):
 if not snapshot[n][0]<registry[n*10][0]:raise SystemExit("FAIL snapshot/registry order")
if not hover[0]<click[0]<pop[0][0]:raise SystemExit("FAIL pointer cancel order")
if not pop[1][0]<cancel[1][0]<esc[0][0] or not pop[2][0]<cancel[2][0]<esc[1][0]:raise SystemExit("FAIL escape cancel order")
if int(status_gen)<4:raise SystemExit("FAIL status generation floor")
cancel_status=exact("WiredUI: server status cancelled ",[
 rf"WiredUI: server status cancelled generation={int(status_gen)-3} rows=0",
 rf"WiredUI: server status cancelled generation={int(status_gen)-2} rows=0",
 rf"WiredUI: server status cancelled generation={int(status_gen)-1} rows=0",
 rf"WiredUI: server status cancelled generation={int(status_gen)+1} rows=0",
 rf"WiredUI: server status cancelled generation={int(status_gen)+2} rows=0"])
if not cancel_status[0][0]<cancel_status[1][0]<sel[0][0]<cancel_status[2][0]<sel[1][0]<request[0][0] or not registry[40][0]<cancel_status[3][0]<cancel_status[4][0]:raise SystemExit("FAIL status cancel lifecycle")
for prefix in ("WiredUI: server status failed ","WiredUI: server status refused ","WiredUI: server status dispose ","WiredUI: close all ","WiredUI: push menu 'error_popup'","WiredUI: push menu 'popup_message'","Browser connect attempt armed ","WiredUI: started validated connect ","Browser connect credential disposed ","WiredUI: password submit ","WiredUI: authentication retry ","Connect failed ","QUIC client:","SV_OnPlayerConnect:","FIRST GAMEPLAY FRAME"):
 exact(prefix,[],"DEBUG","ui")
if any(" resolved to " in line for value in vals for line in re.split(r"[\r\n]",value)):raise SystemExit("FAIL forbidden resolution")
password_layout=[r for r in layout if r.get("menu")=="password"]
server_layout=[r for r in layout if r.get("menu")=="serverinfo"]
for rows,regions in ((password_layout,("password","password_root","row_password","btn_cancel","btn_connect")),(server_layout,("serverinfo","statuslist","btn_connect"))):
 for region in regions:
  hit=[r for r in rows if r.get("region")==region]
  if not hit or any(not isinstance(x.get("w"),(int,float)) or not isinstance(x.get("h"),(int,float)) or x["w"]<=0 or x["h"]<=0 for x in hit):raise SystemExit(f"FAIL layout {region}")
frames={}
for r in layout:
 if isinstance(r.get("frame"),int) and r.get("menu") in ("password","serverinfo"):frames.setdefault((r["menu"],r["frame"]),[]).append(r)
if any(sum(x.get("focused")==1 for x in rows)>1 for rows in frames.values()):raise SystemExit("FAIL layout multifocus")
focused=sorted((r["frame"],r["region"],r) for r in password_layout if r.get("focused")==1)
epochs=[]
for frame,region,row in focused:
 if not epochs or region!=epochs[-1][1] or frame>epochs[-1][0]+1:epochs.append([frame,region,row])
 else:epochs[-1]=[frame,region,row]
if [x[1] for x in epochs] != ["btn_cancel","row_password"]:raise SystemExit("FAIL layout focus epochs")
server_focus=sorted(r["frame"] for r in server_layout if r.get("region")=="btn_connect" and r.get("focused")==1)
server_epochs=[]
for frame in server_focus:
 if not server_epochs or frame>server_epochs[-1]+1:server_epochs.append(frame)
 else:server_epochs[-1]=frame
if len(server_epochs)!=4:raise SystemExit("FAIL serverinfo return-focus epochs")
password_frames=[x[0] for x in epochs]
if not server_epochs[0]<password_frames[0]<server_epochs[1]<server_epochs[2]<server_epochs[3]<password_frames[1]:raise SystemExit("FAIL layout return-focus order")
hm=re.fullmatch(r"WiredUI: pointer phase=moved ingress=CL_MouseEvent menu=password item=btn_cancel x=([0-9]+) y=([0-9]+)",hover[2]);x,y=map(int,hm.groups())
cm=re.fullmatch(r"WiredUI: pointer phase=click ingress=CL_KeyEvent key=K_MOUSE1 x=([0-9]+) y=([0-9]+)",click[2])
if tuple(map(int,cm.groups()))!=(x,y):raise SystemExit("FAIL pointer coordinate drift")
# The moved marker is emitted from the compositor's authoritative rendered
# btn_cancel centre. layoutdump currently exposes the legacy resolved rect,
# which is not the compositor hit rect; bind its positive presence above and
# require the actual ingress coordinate to remain in the complete password
# modal surface. password_root is the content body and excludes its footer.
password_surfaces=[r for r in password_layout if r.get("region")=="password"]
if not password_surfaces or any(not (r.get("x")<=x<=r.get("x")+r.get("w") and r.get("y")<=y<=r.get("y")+r.get("h")) for r in password_surfaces):raise SystemExit("FAIL pointer outside password surface")
print("PASS password cancel: pointer button + menu ESC + edit ESC wipe prompt and preserve READY Server Info")
PYEOF
}

write_self() {
python3 - "$1" "$2" "$3" "$4" <<'PYEOF'
import json,sys
pp,fp,lp,mode=sys.argv[1:];t="127.0.0.1:30002";sg=4;g=7
P=[]
def add(msg,sev="DEBUG",cat="ui"):P.append({"sev":sev,"cat":cat,"msg":msg})
add("WiredUI: server status cancelled generation=4 rows=0");add("WiredUI: pointer phase=release reason=reload was_down=0 pointer_down=0");add("WiredUI: pointer phase=release reason=push was_down=0 pointer_down=0");add("wui_menu_nav focus: focused item 'serverlist' (top index -1)");add("WiredUI: server status cancelled generation=5 rows=0");add("WiredUI: server selection display_row=0 raw=1 source=0 list_generation=3 selection_generation=3 address=127.0.0.1:30001 name=A0 WIRED Q0 SENTINEL map=arena1");add("WiredUI: server status cancelled generation=6 rows=0");add(f"WiredUI: server selection display_row=1 raw=0 source=0 list_generation=3 selection_generation={sg} address={t} name=Z0 WIRED Q0 TARGET map=arena7");add("wui_menu_nav: K_DOWNARROW dispatched");add("wui_menu_nav focus: focused item 'btn_info' (top index -1)")
add(f"WiredUI: server status request generation={g} selection_generation={sg} address={t}");add(f"WiredUI: server status state=pending generation={g} selection_generation={sg} address={t} rows=1");add("WiredUI: pointer phase=release reason=push was_down=0 pointer_down=0");add("wui_menu_nav: K_ENTER dispatched");add(f"WiredUI: server status loaded generation={g} selection_generation={sg} address={t} rows=9")
rows=[f"WiredUI: server status row=0 key=Address value={t}","WiredUI: server status row=1 key=Server value=Z0 WIRED Q0 TARGET","WiredUI: server status row=2 key=Map value=arena7","WiredUI: server status row=3 key=Players value=1/8","WiredUI: server status row=4 key=Game type value=0","WiredUI: server status row=5 key=Game value=q3now","WiredUI: server status row=6 key=Protocol value=74","WiredUI: server status row=7 key=Version value=Q0 fixture 30002","WiredUI: server status row=8 key=Player 1 value=StatusBot — score 7, ping 23"]
for x in rows:add(x)
add(f"WiredUI: server status snapshot state=ready generation={g} selection_generation={sg} address={t} count=9")
add("WiredUI: server status registry feeder=13 count=9")
for x in rows:add(x.replace("server status row=","server status registry row="))
origins=("button","menu-escape","edit-escape");lengths=(5,6,7)
for n,(origin,length) in enumerate(zip(origins,lengths)):
 if n==0:add("wui_menu_nav focus: focused item 'btn_connect' (top index -1)")
 add(f"WiredUI: password required origin=browser address={t} selection_generation={sg}");add("WiredUI: pointer phase=release reason=push was_down=0 pointer_down=0");add("WiredUI: push menu 'password' (depth 3)");add("wui_menu_nav: K_ENTER dispatched");add(f"WiredUI: password state valid=1 secret_length=0 editing=0 selection_generation={sg} address={t} target_empty=0 error_present=0 top=password depth=3");add("wui_menu_nav focus: focused item 'row_password' (top index -1)");add("WiredUI: password render trace length=0 masked=0");add("wui_menu_nav: K_ENTER dispatched");add(f"wui_menu_nav: typed {length} printable character(s)");add(f"WiredUI: password render trace length={length if n==1 else length+1} masked={1 if n==1 else 0}")
 if n==0:add("WiredUI: pointer phase=moved ingress=CL_MouseEvent menu=password item=btn_cancel x=700 y=500");add("WiredUI: pointer phase=click ingress=CL_KeyEvent key=K_MOUSE1 x=700 y=500")
 if n==1:add("wui_menu_nav: K_ENTER dispatched")
 add("WiredUI: pop menu (depth 2)");add(f"WiredUI: pointer phase=release reason=pop was_down={1 if n==0 else 0} pointer_down=0");add(f"WiredUI: password cancel postcondition origin={origin} valid=0 secret_length=0 editing=0 target_empty=1 error_empty=1 top=serverinfo depth=2")
 if n>0:add("wui_menu_nav: K_ESCAPE dispatched")
 add("WiredUI: password state valid=0 secret_length=0 editing=0 selection_generation=0 address=none target_empty=1 error_present=0 top=serverinfo depth=2")
 add(f"WiredUI: server status snapshot state=ready generation={g} selection_generation={sg} address={t} count=9")
 add("WiredUI: server status registry feeder=13 count=9")
 for x in rows:add(x.replace("server status row=","server status registry row="))
add(f"WiredUI: password required origin=browser address={t} selection_generation={sg}");add("WiredUI: pointer phase=release reason=push was_down=0 pointer_down=0");add("WiredUI: push menu 'password' (depth 3)");add("wui_menu_nav: K_ENTER dispatched");add(f"WiredUI: password state valid=1 secret_length=0 editing=0 selection_generation={sg} address={t} target_empty=0 error_present=0 top=password depth=3");add("wui_menu_nav focus: focused item 'row_password' (top index -1)");add("WiredUI: password render trace length=0 masked=0");add(f"WiredUI: server status snapshot state=ready generation={g} selection_generation={sg} address={t} count=9");add("WiredUI: server status registry feeder=13 count=9")
for x in rows:add(x.replace("server status row=","server status registry row="))
add("WiredUI: pointer phase=release reason=shutdown was_down=0 pointer_down=0");add("WiredUI: server status cancelled generation=8 rows=0");add("WiredUI: server status cancelled generation=9 rows=0")
F=[{"event":"ready","client_port":40000,"sentinel_port":30001,"target_port":30002,"protocol":74},{"event":"request","role":"target","command":"getstatus","peer_port":40000},{"event":"challenge_seen","ordinal":1,"challenge":"1"*16},{"event":"status_response","role":"target","peer_port":40000},{"event":"stopped","reason":"signal"}]
L=[]
for frame,menu,regions,focus in ((1,"serverinfo",("serverinfo","statuslist","btn_connect"),"btn_connect"),(2,"password",("password","password_root","row_password","btn_cancel","btn_connect"),"btn_cancel"),(4,"serverinfo",("serverinfo","statuslist","btn_connect"),"btn_connect"),(5,"password",("password","password_root","row_password","btn_cancel","btn_connect"),None),(6,"serverinfo",("serverinfo","statuslist","btn_connect"),"btn_connect"),(7,"password",("password","password_root","row_password","btn_cancel","btn_connect"),None),(8,"serverinfo",("serverinfo","statuslist","btn_connect"),"btn_connect"),(9,"password",("password","password_root","row_password","btn_cancel","btn_connect"),"row_password")):
 for i,r in enumerate(regions):
  if menu=="password" and r in ("password","password_root"):x,y,w,h=(0,0,1280,720)
  elif r=="btn_cancel":x,y,w,h=(650,480,100,40)
  else:x,y,w,h=(10,10+i*50,500,40)
  L.append({"menu":menu,"region":r,"frame":frame,"focused":1 if r==focus else 0,"x":x,"y":y,"w":w,"h":h})
def drop(token):
 global P;P=[r for r in P if token not in r["msg"]]
if mode=="missing_button":drop("origin=button")
elif mode=="missing_menu_esc":drop("origin=menu-escape")
elif mode=="missing_edit_esc":drop("origin=edit-escape")
elif mode=="two_press_edit":P.insert(next(i for i,r in enumerate(P) if "origin=edit-escape" in r["msg"]),{"sev":"DEBUG","cat":"ui","msg":"wui_menu_nav: K_ESCAPE dispatched"})
elif mode=="secret_retained":next(r for r in P if "origin=button" in r["msg"])["msg"]=next(r for r in P if "origin=button" in r["msg"])["msg"].replace("secret_length=0","secret_length=5")
elif mode=="target_retained":next(r for r in P if "origin=button" in r["msg"])["msg"]=next(r for r in P if "origin=button" in r["msg"])["msg"].replace("target_empty=1","target_empty=0")
elif mode=="editing_retained":next(r for r in P if "origin=edit-escape" in r["msg"])["msg"]=next(r for r in P if "origin=edit-escape" in r["msg"])["msg"].replace("editing=0","editing=1")
elif mode=="stale_generation":next(r for r in P if "password required" in r["msg"])["msg"]=next(r for r in P if "password required" in r["msg"])["msg"].replace("generation=4","generation=5")
elif mode=="status_cancel":P.insert(next(i for i,r in enumerate(P) if "origin=edit-escape" in r["msg"]),{"sev":"DEBUG","cat":"ui","msg":"WiredUI: server status cancelled generation=8 rows=0"})
elif mode=="status_rerequest":P.insert(-1,next(r for r in P if "server status request generation=" in r["msg"]).copy())
elif mode=="status_row":drop("registry row=8")
elif mode=="connect":P.append({"sev":"DEBUG","cat":"client","msg":f"Browser connect attempt armed target={t} selection_generation=4 credential_present=1"})
elif mode=="suffix":next(r for r in P if "origin=button" in r["msg"])["msg"]+=" trailing"
elif mode=="wrong_category":next(r for r in P if "origin=button" in r["msg"])["cat"]="client"
elif mode=="multiline":next(r for r in P if "origin=button" in r["msg"])["msg"]="benign\r"+next(r for r in P if "origin=button" in r["msg"])["msg"]
elif mode=="fixture_event":F.insert(3,F[3].copy())
elif mode=="protocol":F[0]["protocol"]=75
elif mode=="layout_missing":L=[r for r in L if r["region"]!="btn_cancel"]
elif mode=="layout_multifocus":L[1]["focused"]=1
elif mode=="nav_missing":drop("K_DOWNARROW")
elif mode=="nav_suffix":next(r for r in P if "K_DOWNARROW" in r["msg"])["msg"]+=" trailing"
elif mode=="focus_missing":drop("focused item 'btn_info'")
elif mode=="pointer_ingress":next(r for r in P if "item=btn_cancel x=" in r["msg"])["msg"]=next(r for r in P if "item=btn_cancel x=" in r["msg"])["msg"].replace("CL_MouseEvent","direct")
elif mode=="pointer_outside":next(r for r in P if "item=btn_cancel x=" in r["msg"])["msg"]=next(r for r in P if "item=btn_cancel x=" in r["msg"])["msg"].replace("x=700 y=500","x=100 y=100")
elif mode=="snapshot_missing":drop("server status snapshot state=ready")
elif mode=="snapshot_state":next(r for r in P if "server status snapshot" in r["msg"])["msg"]=next(r for r in P if "server status snapshot" in r["msg"])["msg"].replace("state=ready","state=pending")
elif mode=="snapshot_generation":next(r for r in P if "server status snapshot" in r["msg"])["msg"]=next(r for r in P if "server status snapshot" in r["msg"])["msg"].replace("generation=7","generation=99")
elif mode=="snapshot_selection":next(r for r in P if "server status snapshot" in r["msg"])["msg"]=next(r for r in P if "server status snapshot" in r["msg"])["msg"].replace("selection_generation=4","selection_generation=99")
elif mode=="closeall":P.insert(-1,{"sev":"DEBUG","cat":"ui","msg":"WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0"})
elif mode=="submit":P.insert(-1,{"sev":"DEBUG","cat":"ui","msg":"WiredUI: password submit refused reason=invalid-credential"})
elif mode=="resolve":P.insert(-1,{"sev":"INFO","cat":"client","msg":f"{t} resolved to {t}"})
elif mode=="secret_fixture":F[0]["leak"]="WipeA"
elif mode=="enter_missing":
 for i,r in enumerate(P):
  if r["msg"]=="wui_menu_nav: K_ENTER dispatched":P.pop(i);break
elif mode=="enter_extra":P.insert(-1,{"sev":"DEBUG","cat":"ui","msg":"wui_menu_nav: K_ENTER dispatched"})
elif mode=="return_focus_override":P.insert(next(i for i,r in enumerate(P) if "origin=menu-escape" in r["msg"])+2,{"sev":"DEBUG","cat":"ui","msg":"wui_menu_nav focus: focused item 'btn_connect' (top index -1)"})
elif mode=="click_missing":drop("pointer phase=click")
elif mode=="click_target":next(r for r in P if "pointer phase=moved" in r["msg"])["msg"]=next(r for r in P if "pointer phase=moved" in r["msg"])["msg"].replace("item=btn_cancel","item=btn_connect")
elif mode=="click_coordinates":next(r for r in P if "pointer phase=click" in r["msg"])["msg"]=next(r for r in P if "pointer phase=click" in r["msg"])["msg"].replace("x=700 y=500","x=701 y=500")
elif mode=="smuggle_pointer":P.insert(-1,{"sev":"INFO","cat":"system","msg":"benign\rWiredUI: pointer phase=click ingress=CL_KeyEvent key=K_MOUSE1 x=700 y=500"})
elif mode=="smuggle_quic":P.insert(-1,{"sev":"INFO","cat":"system","msg":"benign\rQUIC client: TLV ACCEPT received"})
elif mode=="status_failed":P.insert(-1,{"sev":"DEBUG","cat":"ui","msg":f"WiredUI: server status failed generation={g} selection_generation={sg} address={t} state=no-response reason=timeout rows=2"})
elif mode=="status_refused":P.insert(-1,{"sev":"DEBUG","cat":"ui","msg":"WiredUI: server status refused without current selection"})
elif mode=="smuggle_status_failed":P.insert(-1,{"sev":"INFO","cat":"system","msg":f"benign\rWiredUI: server status failed generation={g} selection_generation={sg} address={t} state=no-response reason=timeout rows=2"})
elif mode=="smuggle_status_refused":P.insert(-1,{"sev":"INFO","cat":"system","msg":"benign\rWiredUI: server status refused without current selection"})
elif mode=="smuggle_status_retry":P.insert(-1,{"sev":"INFO","cat":"system","msg":f"benign\rWiredUI: server status request generation={g+1} selection_generation={sg} address={t}"})
elif mode=="pointer_extra":P.insert(-1,{"sev":"DEBUG","cat":"ui","msg":"WiredUI: pointer phase=moved ingress=CL_MouseEvent menu=password item=row_password x=700 y=500"})
elif mode=="error_popup":P.insert(-1,{"sev":"DEBUG","cat":"ui","msg":"WiredUI: push menu 'error_popup' (depth 4)"})
elif mode=="return_layout_missing":
 frame=next(r["frame"] for r in L if r["menu"]=="serverinfo" and r["frame"]>1)
 L=[r for r in L if not (r["menu"]=="serverinfo" and r["frame"]==frame)]
for path,rows2 in ((pp,P),(fp,F),(lp,L)):
 with open(path,"w") as out:
  for row in rows2:out.write(json.dumps(row)+"\n")
PYEOF
}

if [ "${1:-}" = --self-test ]; then
 ROOT="$(mktemp -d -t password-cancel-self-XXXXXX 2>/dev/null || mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
 write_self "$ROOT/p" "$ROOT/f" "$ROOT/l" clean; analyze "$ROOT/p" "$ROOT/f" "$ROOT/l" 30002 40000 30001 >/dev/null || exit 1
 for d in missing_button missing_menu_esc missing_edit_esc two_press_edit secret_retained target_retained editing_retained stale_generation status_cancel status_rerequest status_row connect suffix wrong_category multiline fixture_event protocol layout_missing layout_multifocus nav_missing nav_suffix focus_missing pointer_ingress pointer_outside snapshot_missing snapshot_state snapshot_generation snapshot_selection closeall submit resolve secret_fixture enter_missing enter_extra return_focus_override click_missing click_target click_coordinates smuggle_pointer smuggle_quic status_failed status_refused smuggle_status_failed smuggle_status_refused smuggle_status_retry pointer_extra error_popup return_layout_missing; do
  write_self "$ROOT/p-$d" "$ROOT/f-$d" "$ROOT/l-$d" "$d"
  if analyze "$ROOT/p-$d" "$ROOT/f-$d" "$ROOT/l-$d" 30002 40000 30001 >/dev/null 2>&1; then echo "FAIL self $d"; exit 1; fi
 done
 echo '==> password-cancel analyzer self-test: PASS (48 defects rejected)'; exit 0
fi

if [ "${1:-}" = --analyze ]; then
 [ "$#" -eq 7 ] || { echo "usage: $0 --analyze PRODUCT FIXTURE LAYOUT TARGET_PORT CLIENT_PORT SENTINEL_PORT"; exit 64; }
 analyze "$2" "$3" "$4" "$5" "$6" "$7"
 exit $?
fi

command -v python3 >/dev/null 2>&1 && [ -f "$FIXTURE" ] && [ -f "$TIMEOUT_RUNNER" ] || { echo 'SKIP: Python unavailable'; exit 77; }
WIRED="${1:-}"; [ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo 'SKIP: pass assembled Wired GUI binary'; exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"; WD="$(dirname "$WIRED")"
PACK="$(wired_find_archive_root "$WD" "$WD/../Resources" "$WD/q3now-preview.arm64.app/Contents/Resources" 2>/dev/null || true)"
[ -n "$PACK" ] || { echo 'SKIP: current VFS archives unavailable'; exit 77; }
CONTENT="$(wired_find_archive_root "${WIRED_CONTENT_ROOT:-}" "$WIRED_HOME" "$PACK" 2>/dev/null || true)"
[ -n "$CONTENT" ] || { echo 'SKIP: set WIRED_CONTENT_ROOT'; exit 77; }
ROOT="$(mktemp -d -t password-cancel-XXXXXX 2>/dev/null || mktemp -d)"; EVENTS="$ROOT/fixture.jsonl"; HOME_DIR="$ROOT/q3now-preview"; RUN_DIR="$ROOT/run"; LAYOUT="$RUN_DIR/layoutdump.jsonl"; PID=""
cleanup(){ [ -n "$PID" ] && kill -TERM "$PID" 2>/dev/null || true; [ -n "$PID" ] && wait "$PID" 2>/dev/null || true; [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"; }; trap cleanup EXIT INT TERM
mkdir -p "$RUN_DIR"; wired_link_content_into_home "$HOME_DIR" "$CONTENT/base" "$PACK/base" || exit 1
CLIENT_PORT="$(python3 - <<'PY'
import socket
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',0));print(s.getsockname()[1]);s.close()
PY
)"; SENTINEL_PORT=$((CLIENT_PORT+1)); TARGET_PORT=$((CLIENT_PORT+2))
python3 "$FIXTURE" --client-port "$CLIENT_PORT" --sentinel-port "$SENTINEL_PORT" --target-port "$TARGET_PORT" --protocol 75 --events "$EVENTS" --timeout 30 & PID=$!
for _ in $(seq 1 100); do [ -s "$EVENTS" ] && break; sleep .05; done
cat >"$HOME_DIR/base/password-cancel.cfg" <<CFGEOF
set com_maxfps 60
wait 100
wui_server_fixture $SENTINEL_PORT $TARGET_PORT 1
wui_push servers
wait 20
wui_listbox_sort 0
wui_menu_nav focus serverlist
wui_menu_nav down
wui_menu_nav focus btn_info
wui_menu_nav enter
wait 20
set r_layoutDump 1
wui_serverstatus_trace
wui_menu_nav focus btn_connect
wait 2
wui_menu_nav enter
wait 3
wui_password_trace state
wui_menu_nav focus row_password
wui_password_trace
wui_menu_nav enter
wui_menu_nav type "WipeA"
wui_password_trace
wui_pointer_item btn_cancel
wait 2
wui_pointer_click
wui_password_trace state
wui_serverstatus_trace
wait 2
wui_menu_nav enter
wait 2
wui_password_trace state
wui_menu_nav focus row_password
wui_password_trace
wui_menu_nav enter
wui_menu_nav type "WipeBB"
wui_menu_nav enter
wui_password_trace
wui_menu_nav back
wui_password_trace state
wui_serverstatus_trace
wait 2
wui_menu_nav enter
wait 2
wui_password_trace state
wui_menu_nav focus row_password
wui_password_trace
wui_menu_nav enter
wui_menu_nav type "WipeCCC"
wui_password_trace
wui_menu_nav back
wui_password_trace state
wui_serverstatus_trace
wait 2
wui_menu_nav enter
wait 2
wui_password_trace state
wui_menu_nav focus row_password
wui_password_trace
wui_serverstatus_trace
wait 5
quit
CFGEOF
case "$(uname -s)" in Darwin) NATIVE="$HOME_DIR"; ARGS=(-ApplePersistenceIgnoreState YES);; *) NATIVE="$HOME_DIR"; ARGS=();; esac
python3 "$TIMEOUT_RUNNER" --timeout 30 --kill-after 10 --cwd "$RUN_DIR" --stdout "$ROOT/stdout" -- "$WIRED" "${ARGS[@]}" +set fs_homepath "$NATIVE" +set com_automated 1 +set com_noHardReboot 1 +set net_enabled 1 +set net_ip 127.0.0.1 +set net_port "$CLIENT_PORT" +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec password-cancel.cfg
rc=$?; [ "$rc" -eq 0 ] && [ -s "$HOME_DIR/qconsole.jsonl" ] && [ -s "$LAYOUT" ] || { echo "FAIL product rc=$rc"; exit 1; }
kill -TERM "$PID" 2>/dev/null || true; wait "$PID" || exit 1; PID=""
for secret in WipeA WipeBB WipeCCC; do grep -Fq "$secret" "$EVENTS" "$HOME_DIR/qconsole.jsonl" "$LAYOUT" "$ROOT/stdout" && { echo 'FAIL secret leak'; exit 1; }; done
analyze "$HOME_DIR/qconsole.jsonl" "$EVENTS" "$LAYOUT" "$TARGET_PORT" "$CLIENT_PORT" "$SENTINEL_PORT"
