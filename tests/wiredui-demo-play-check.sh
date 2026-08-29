#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Q0 contract: exercise the empty Demo browser lifecycle, record current-
# protocol demos, then reach the real browser through the authored main-menu
# keyboard route and play the selected target until natural EOF.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
PROTOCOL=74

analyze_contract() {
    python3 - "$1" "$2" "$3" "$4" "$5" <<'PYEOF'
import hashlib, json, re, struct, sys
producer_path, consumer_path, layout_path, decoy_path, target_path = sys.argv[1:]

def rows(path):
    out=[]
    with open(path, encoding="utf-8", errors="replace") as f:
        for n,line in enumerate(f,1):
            if not line.strip(): continue
            try: row=json.loads(line)
            except ValueError as e: raise SystemExit(f"FAIL {path}:{n}: invalid JSON: {e}")
            if not isinstance(row,dict): raise SystemExit(f"FAIL {path}:{n}: JSON record is not an object")
            out.append(row)
    if not out: raise SystemExit(f"FAIL: empty evidence {path}")
    return out

prod, cons, layout = rows(producer_path), rows(consumer_path), rows(layout_path)
pm=[str(r.get("msg","")) for r in prod]
cm=[str(r.get("msg","")) for r in cons]

for label,data in (("producer",prod),("consumer",cons)):
    bad=[r for r in data if str(r.get("sev","")).upper() in {"ERROR","FATAL"}
         or (str(r.get("sev","")).upper()=="WARN" and str(r.get("cat","")).lower()=="ui"
             and str(r.get("msg",""))!="WiredUI: no demo selected\n")]
    if bad: raise SystemExit(f"FAIL severity: {label} has {len(bad)} unexpected ERROR/FATAL/UI-WARN")
if sum(m=="WiredUI: no demo selected\n" for m in cm)!=1:
    raise SystemExit("FAIL: expected exactly one deliberate no-selection refusal")

def ordered(messages, steps):
    cursor=0
    for name,pat in steps:
        for i in range(cursor,len(messages)):
            if re.search(pat,messages[i]): cursor=i+1; break
        else: raise SystemExit(f"FAIL contract: missing/out-of-order {name}")

ordered(pm, [
 ("producer FIRST",r"FIRST GAMEPLAY FRAME mapname=(?:maps/)?arena7(?:\.bsp)?"),
 ("decoy record",r"recording to demos/a0_decoy\."),
 ("decoy stop",r"Stopped demo recording\."),
 ("target record",r"recording to demos/q0_target\."),
 ("target stop",r"Stopped demo recording\."),
 ("producer marker",r"Q0_DEMOS_RECORDED"),
])

ordered(cm, [
 ("ignored stale state",r"WiredUI: loaded 0 UI state entries"),
 ("main menu",r"WiredUI: push menu 'main' \(depth 1\)"),
 ("main Down 1",r"wui_menu_nav: K_DOWNARROW dispatched"),
 ("main Down 2",r"wui_menu_nav: K_DOWNARROW dispatched"),
 ("main Down 3",r"wui_menu_nav: K_DOWNARROW dispatched"),
 ("demos menu",r"WiredUI: push menu 'demos' \(depth 2\)"),
 ("inventory",r"WiredUI: demos loaded protocol=74 count=2 generation=2\b"),
 ("decoy row",r"demo feeder row=0 name=a0_decoy generation=2\b"),
 ("target row",r"demo feeder row=1 name=q0_target generation=2\b"),
 ("main Enter",r"wui_menu_nav: K_ENTER dispatched"),
 ("preselection Play",r"focused item 'btn_play_demo'"),
 ("refusal",r"WiredUI: no demo selected"),
 ("refusal Enter",r"wui_menu_nav: K_ENTER dispatched"),
 ("row0 callback",r"demo feeder selection row=0 name=a0_decoy generation=2\b"),
 ("list focus",r"focused item 'demolist'"),
 ("row1 callback",r"demo feeder selection row=1 name=q0_target generation=2\b"),
 ("real Down",r"wui_menu_nav: K_DOWNARROW dispatched"),
 ("target Play",r"focused item 'btn_play_demo'"),
 ("validated queue",r"queued validated demo playback name=q0_target\b"),
 ("close all",r"close all postcondition depth=0 active=none catcher_ui=0 paused=0"),
 ("Play Enter",r"wui_menu_nav: K_ENTER dispatched"),
 ("exact file",r"Demo file: demos/q0_target\.dm_75\b"),
 ("playback FIRST",r"FIRST GAMEPLAY FRAME mapname=(?:maps/)?arena7(?:\.bsp)?.*numEntities=[1-9][0-9]*"),
])

main_pushes=[i for i,m in enumerate(cm) if re.search(r"WiredUI: push menu 'main' \(depth 1\)",m)]
demos_pushes=[i for i,m in enumerate(cm) if re.search(r"WiredUI: push menu 'demos' \(depth 2\)",m)]
if len(main_pushes)!=1 or len(demos_pushes)!=1 or main_pushes[0]>=demos_pushes[0]:
    raise SystemExit("FAIL route: expected one ordered main-to-demos push")
route=cm[main_pushes[0]+1:demos_pushes[0]]
if sum("wui_menu_nav: K_DOWNARROW dispatched" in m for m in route)!=3:
    raise SystemExit("FAIL route: main-to-demos route must contain exactly three K_DOWN dispatches")
if any(re.search(r"focused item 'menu_demos'",m) for m in cm[:demos_pushes[0]]):
    raise SystemExit("FAIL route: named menu_demos focus bypassed the authored keyboard route")

traces=[]
rx=re.compile(r"demo playback trace state=8 demoplaying=1 name=q0_target\.dm_75 sequence=([0-9]+) serverTime=([0-9]+)")
for i,m in enumerate(cm):
    q=rx.search(m)
    if q: traces.append((i,int(q.group(1)),int(q.group(2))))
if len(traces)!=2: raise SystemExit(f"FAIL: expected two exact active playback traces, got {len(traces)}")
if not (traces[1][1]>traces[0][1] and traces[1][2]>traces[0][2]):
    raise SystemExit("FAIL: demo message sequence/serverTime did not advance")
next_i=next((i for i,m in enumerate(cm) if re.search(r"CL_NextDemo: exec q0-demo-done\.cfg",m)),None)
done_i=next((i for i,m in enumerate(cm) if "Q0_DEMO_COMPLETED" in m),None)
if next_i is None or done_i is None or not (traces[-1][0] < next_i < done_i):
    raise SystemExit("FAIL: natural EOF/nextdemo completion order missing")
if any("QUIC client: TLV ACCEPT" in m or "SV_OnPlayerConnect:" in m for m in cm):
    raise SystemExit("FAIL: playback unexpectedly used a live network server")
if any(x in "\n".join(cm) for x in ("Demo file was truncated", "Protocol 74 not supported", "couldn't open demos/")):
    raise SystemExit("FAIL: demo backend reported playback failure")

def focused_frames(menu):
    frames={}
    for row in layout:
        owner=str(row.get("menu",row.get("menuName","")))
        if owner!=menu: continue
        item=""
        if row.get("kind")=="item" and bool(row.get("focused",0)):
            item=str(row.get("region",""))
        else:
            item=str(row.get("focusedItem",""))
            if not item and isinstance(row.get("focused"),str): item=row["focused"]
        if item: frames.setdefault(int(row.get("frame",0)),[]).append(item)
    for frame,items in frames.items():
        if len(items)>1:
            raise SystemExit(f"FAIL layout: {menu} frame {frame} has {len(items)} focused items")
    return [(frame,items[0]) for frame,items in sorted(frames.items())]

def require_focus_order(menu,wanted):
    sequence=focused_frames(menu); cursor=0; matched=[]
    for item in wanted:
        for index in range(cursor,len(sequence)):
            if sequence[index][1]==item:
                matched.append(sequence[index]); cursor=index+1; break
        else: raise SystemExit(f"FAIL layout: missing ordered {menu}/{item}")
    return matched

main_focus=require_focus_order("main",("menu_campaign","menu_demos"))
route_frame=main_focus[-1][0]
expected_rail=("menu_campaign","menu_join","menu_host","menu_demos","menu_options","menu_quit")
route_rows=[r for r in layout if str(r.get("menu",""))=="main"
            and r.get("kind")=="item" and int(r.get("frame",0))==route_frame
            and str(r.get("region","")).startswith("menu_")
            and "/" not in str(r.get("region",""))]
by_name={str(r.get("region","")):r for r in route_rows}
if set(by_name)!=set(expected_rail) or len(route_rows)!=len(expected_rail):
    raise SystemExit("FAIL layout: main rail inventory is not the exact six authored rows")
ordered_rail=sorted(route_rows,key=lambda r:float(r.get("y",-1)))
if tuple(str(r.get("region","")) for r in ordered_rail)!=expected_rail:
    raise SystemExit("FAIL layout: main rail visual order does not match keyboard order")
menu_rows=[r for r in layout if str(r.get("menu",""))=="main"
           and r.get("kind")=="menu" and int(r.get("frame",0))==route_frame]
if len(menu_rows)!=1: raise SystemExit("FAIL layout: missing unique main menu bounds")
menu_row=menu_rows[0]; mx=float(menu_row.get("x",0)); my=float(menu_row.get("y",0))
mw=float(menu_row.get("w",0)); mh=float(menu_row.get("h",0)); tol=.75
if mw<=0 or mh<=0: raise SystemExit("FAIL layout: invalid main menu bounds")
for row in ordered_rail:
    x=float(row.get("x",-1)); y=float(row.get("y",-1))
    w=float(row.get("w",-1)); h=float(row.get("h",-1)); dpi=float(row.get("dpiScale",0))
    if dpi<=0 or abs(h-56.0*dpi)>tol:
        raise SystemExit(f"FAIL layout: {row.get('region')} height {h:g} != 56*dpiScale ({56*dpi:g})")
    if w<=0 or x<mx-tol or y<my-tol or x+w>mx+mw+tol or y+h>my+mh+tol:
        raise SystemExit(f"FAIL layout: {row.get('region')} escapes main menu bounds")
for previous,current in zip(ordered_rail,ordered_rail[1:]):
    if float(previous["y"])+float(previous["h"])>float(current["y"])+tol:
        raise SystemExit("FAIL layout: main rail rows overlap")
footers=[r for r in layout if str(r.get("menu",""))=="main"
         and r.get("kind")=="item" and int(r.get("frame",0))==route_frame
         and str(r.get("region",""))=="left_footer_row"]
if len(footers)!=1: raise SystemExit("FAIL layout: missing unique main footer row")
footer=footers[0]; fy=float(footer.get("y",-1)); fh=float(footer.get("h",-1))
if fh<=0 or fy+tol<float(ordered_rail[-1]["y"])+float(ordered_rail[-1]["h"]):
    raise SystemExit("FAIL layout: main footer overlaps the rail")

require_focus_order("demos",("btn_play_demo","demolist","btn_play_demo"))

def inspect_demo(path):
    data=open(path,"rb").read()
    if len(data)<1024: raise SystemExit(f"FAIL: demo too small: {path} ({len(data)})")
    pos=0; messages=0
    while pos+8<=len(data):
        seq,size=struct.unpack_from("<ii",data,pos); pos+=8
        if seq==-1 and size==-1:
            if pos!=len(data): raise SystemExit(f"FAIL: trailing bytes after demo EOF: {path}")
            if messages<2: raise SystemExit(f"FAIL: too few demo messages: {path}")
            return hashlib.sha256(data).hexdigest()
        if size<0 or pos+size>len(data): raise SystemExit(f"FAIL: malformed demo envelope: {path}")
        pos+=size; messages+=1
    raise SystemExit(f"FAIL: missing terminal -1/-1: {path}")

decoy_hash=inspect_demo(decoy_path); target_hash=inspect_demo(target_path)
if decoy_hash==target_hash: raise SystemExit("FAIL: decoy and target demo hashes unexpectedly match")
print(f"PASS demo-play contract demo_sha256={target_hash}")
PYEOF
}

analyze_empty_contract() {
    python3 - "$1" "$2" <<'PYEOF'
import json,re,sys
product_path,layout_path=sys.argv[1:]

def rows(path):
    out=[]
    with open(path,encoding="utf-8",errors="replace") as f:
        for n,line in enumerate(f,1):
            if not line.strip(): continue
            try: row=json.loads(line)
            except ValueError as e: raise SystemExit(f"FAIL {path}:{n}: invalid JSON: {e}")
            if not isinstance(row,dict): raise SystemExit(f"FAIL {path}:{n}: JSON record is not an object")
            out.append(row)
    if not out: raise SystemExit(f"FAIL: empty evidence {path}")
    return out

product,layout=rows(product_path),rows(layout_path)
messages=[str(r.get("msg","")) for r in product]
bad=[r for r in product if str(r.get("sev","")).upper() in {"ERROR","FATAL"}
     or (str(r.get("sev","")).upper()=="WARN" and str(r.get("cat","")).lower()=="ui"
         and str(r.get("msg",""))!="WiredUI: no demo selected\n")]
if bad: raise SystemExit(f"FAIL empty severity: {len(bad)} unexpected ERROR/FATAL/UI-WARN")
if sum(m=="WiredUI: no demo selected\n" for m in messages)!=2:
    raise SystemExit("FAIL empty: expected exactly two deliberate Play refusals")

def ordered(steps):
    cursor=0
    for name,pat in steps:
        for i in range(cursor,len(messages)):
            if re.search(pat,messages[i]): cursor=i+1; break
        else: raise SystemExit(f"FAIL empty contract: missing/out-of-order {name}")

ordered([
 ("main",r"push menu 'main' \(depth 1\)"),
 ("down1",r"wui_menu_nav: K_DOWNARROW dispatched"),
 ("down2",r"wui_menu_nav: K_DOWNARROW dispatched"),
 ("down3",r"wui_menu_nav: K_DOWNARROW dispatched"),
 ("demos1",r"push menu 'demos' \(depth 2\)"),
 ("empty generation2",r"demos loaded protocol=74 count=0 generation=2\b"),
 ("route enter",r"wui_menu_nav: K_ENTER dispatched"),
 ("play1",r"focused item 'btn_play_demo'"),
 ("refusal1",r"WiredUI: no demo selected"),
 ("refusal enter1",r"wui_menu_nav: K_ENTER dispatched"),
 ("escape pop",r"WiredUI: pop menu \(depth 1\)"),
 ("escape input",r"wui_menu_nav: K_ESCAPE dispatched"),
 ("demos2",r"push menu 'demos' \(depth 2\)"),
 ("empty generation3",r"demos loaded protocol=74 count=0 generation=3\b"),
 ("reopen enter1",r"wui_menu_nav: K_ENTER dispatched"),
 ("back focus",r"focused item 'btn_back'"),
 ("back pop",r"WiredUI: pop menu \(depth 1\)"),
 ("back enter",r"wui_menu_nav: K_ENTER dispatched"),
 ("demos3",r"push menu 'demos' \(depth 2\)"),
 ("empty generation4",r"demos loaded protocol=74 count=0 generation=4\b"),
 ("reopen enter2",r"wui_menu_nav: K_ENTER dispatched"),
 ("play2",r"focused item 'btn_play_demo'"),
 ("refusal2",r"WiredUI: no demo selected"),
 ("refusal enter2",r"wui_menu_nav: K_ENTER dispatched"),
 ("completion",r"Q0_EMPTY_DEMOS_COMPLETE"),
])

main_pushes=[i for i,m in enumerate(messages) if "push menu 'main' (depth 1)" in m]
demos_pushes=[i for i,m in enumerate(messages) if "push menu 'demos' (depth 2)" in m]
if len(main_pushes)!=1 or len(demos_pushes)!=3:
    raise SystemExit("FAIL empty route: expected one main push and three demos pushes")
route=messages[main_pushes[0]+1:demos_pushes[0]]
if sum("K_DOWNARROW dispatched" in m for m in route)!=3:
    raise SystemExit("FAIL empty route: expected exactly three authored main-menu Downs")
if any("focused item 'menu_demos'" in m for m in messages[:demos_pushes[0]]):
    raise SystemExit("FAIL empty route: named focus bypassed main navigation")
if sum("WiredUI: pop menu (depth 1)" in m for m in messages)!=2:
    raise SystemExit("FAIL empty lifecycle: expected ESC and Back to pop exactly once each")
if sum("K_ESCAPE dispatched" in m for m in messages)!=1:
    raise SystemExit("FAIL empty lifecycle: expected exactly one real ESC dispatch")
for forbidden in ("demo feeder row=","demo feeder selection","queued validated demo playback",
                  "Demo file:","FIRST GAMEPLAY FRAME","QUIC client: TLV ACCEPT","SV_OnPlayerConnect:"):
    if any(forbidden in m for m in messages):
        raise SystemExit(f"FAIL empty lifecycle: forbidden activity {forbidden!r}")

def focused_frames(menu):
    frames={}
    for row in layout:
        if str(row.get("menu",row.get("menuName","")))!=menu: continue
        item=""
        if row.get("kind")=="item" and bool(row.get("focused",0)): item=str(row.get("region",""))
        else:
            item=str(row.get("focusedItem",""))
            if not item and isinstance(row.get("focused"),str): item=row["focused"]
        if item: frames.setdefault(int(row.get("frame",0)),[]).append(item)
    for frame,items in frames.items():
        if len(items)>1: raise SystemExit(f"FAIL empty layout: {menu} frame {frame} has duplicate focus")
    return [(frame,items[0]) for frame,items in sorted(frames.items())]

def focus_epochs(menu):
    epochs=[]
    for frame,item in focused_frames(menu):
        if epochs and epochs[-1][2]==item and frame==epochs[-1][1]+1:
            epochs[-1]=(epochs[-1][0],frame,item)
        else:
            epochs.append((frame,frame,item))
    return epochs

def require_focus_epochs(menu,wanted):
    actual=tuple(item for _first,_last,item in focus_epochs(menu))
    if actual!=tuple(wanted):
        raise SystemExit(f"FAIL empty layout: {menu} focus epochs {actual!r} != {tuple(wanted)!r}")

require_focus_epochs("main",("menu_campaign","menu_demos","menu_demos","menu_demos"))
require_focus_epochs("demos",("btn_play_demo","btn_back","btn_play_demo"))
print("PASS demo-empty lifecycle contract")
PYEOF
}

analyze_hot_contract() {
    python3 - "$1" "$2" "$3" "$4" <<'PYEOF'
import hashlib,json,re,sys
product_path,layout_path,source_demo,copied_demo=sys.argv[1:]
def rows(path):
    out=[]
    with open(path,encoding="utf-8",errors="replace") as f:
        for n,line in enumerate(f,1):
            if not line.strip(): continue
            try: row=json.loads(line)
            except ValueError as e: raise SystemExit(f"FAIL {path}:{n}: invalid JSON: {e}")
            if not isinstance(row,dict): raise SystemExit(f"FAIL {path}:{n}: JSON record is not an object")
            out.append(row)
    if not out: raise SystemExit(f"FAIL: empty evidence {path}")
    return out
product,layout=rows(product_path),rows(layout_path)
messages=[str(r.get("msg","")) for r in product]
bad=[r for r in product if str(r.get("sev","")).upper() in {"ERROR","FATAL"}
     or (str(r.get("sev","")).upper()=="WARN" and str(r.get("cat","")).lower()=="ui"
         and str(r.get("msg",""))!="WiredUI: no demo selected\n")]
if bad: raise SystemExit(f"FAIL hot severity: {len(bad)} unexpected ERROR/FATAL/UI-WARN")
if sum(m=="WiredUI: no demo selected\n" for m in messages)!=2:
    raise SystemExit("FAIL hot: expected empty and post-reload preselection refusals")
def ordered(steps):
    cursor=0
    for name,pat in steps:
        for i in range(cursor,len(messages)):
            if re.search(pat,messages[i]): cursor=i+1; break
        else: raise SystemExit(f"FAIL hot contract: missing/out-of-order {name}")
ordered([
 ("main",r"push menu 'main' \(depth 1\)"),
 ("down1",r"wui_menu_nav: K_DOWNARROW dispatched"),
 ("down2",r"wui_menu_nav: K_DOWNARROW dispatched"),
 ("down3",r"wui_menu_nav: K_DOWNARROW dispatched"),
 ("demos empty",r"push menu 'demos' \(depth 2\)"),
 ("empty generation",r"demos loaded protocol=74 count=0 generation=2\b"),
 ("route enter",r"wui_menu_nav: K_ENTER dispatched"),
 ("empty play",r"focused item 'btn_play_demo'"),
 ("empty refusal",r"WiredUI: no demo selected"),
 ("empty refusal enter",r"wui_menu_nav: K_ENTER dispatched"),
 ("ready",r"Q0_HOT_DEMO_READY"),
 ("pop",r"WiredUI: pop menu \(depth 1\)"),
 ("escape",r"wui_menu_nav: K_ESCAPE dispatched"),
 ("demos hot",r"push menu 'demos' \(depth 2\)"),
 ("hot generation",r"demos loaded protocol=74 count=1 generation=3\b"),
 ("dotted row",r"demo feeder row=0 name=match\.final\.q0 generation=3\b"),
 ("reopen enter",r"wui_menu_nav: K_ENTER dispatched"),
 ("hot play preselection",r"focused item 'btn_play_demo'"),
 ("hot refusal",r"WiredUI: no demo selected"),
 ("hot refusal enter",r"wui_menu_nav: K_ENTER dispatched"),
 ("selection",r"demo feeder selection row=0 name=match\.final\.q0 generation=3\b"),
 ("list focus",r"focused item 'demolist'"),
 ("list down",r"wui_menu_nav: K_DOWNARROW dispatched"),
 ("final play",r"focused item 'btn_play_demo'"),
 ("queue",r"queued validated demo playback name=match\.final\.q0\b"),
 ("close",r"close all postcondition depth=0 active=none catcher_ui=0 paused=0"),
 ("play enter",r"wui_menu_nav: K_ENTER dispatched"),
 ("file",r"Demo file: demos/match\.final\.q0\.dm_75\s*$"),
 ("first",r"FIRST GAMEPLAY FRAME mapname=maps/arena7\.bsp .*numEntities=[1-9][0-9]*"),
 ("trace1",r"WiredUI: demo playback trace state=8 demoplaying=1 name=match\.final\.q0\.dm_75 sequence=([0-9]+) serverTime=([0-9]+)"),
 ("trace2",r"WiredUI: demo playback trace state=8 demoplaying=1 name=match\.final\.q0\.dm_75 sequence=([0-9]+) serverTime=([0-9]+)"),
 ("eof",r"CL_NextDemo: exec q0-hot-demo-done\.cfg"),
 ("done",r"Q0_HOT_DEMO_COMPLETED"),
])
if sum("push menu 'main' (depth 1)" in m for m in messages)!=1 or sum("push menu 'demos' (depth 2)" in m for m in messages)!=2:
    raise SystemExit("FAIL hot route: wrong push cardinality")
main_i=next(i for i,m in enumerate(messages) if "push menu 'main' (depth 1)" in m)
first_demos_i=next(i for i,m in enumerate(messages) if "push menu 'demos' (depth 2)" in m)
if sum("K_DOWNARROW dispatched" in m for m in messages[main_i+1:first_demos_i])!=3:
    raise SystemExit("FAIL hot route: expected exactly three authored main-menu Downs")
hot_load=next(i for i,m in enumerate(messages) if "count=1 generation=3" in m)
if any("demo feeder row=" in m for m in messages[:hot_load]):
    raise SystemExit("FAIL hot inventory: row appeared before generation-3 reload")
queue_i=next(i for i,m in enumerate(messages) if "queued validated demo playback" in m)
hot_rows=[m for m in messages[hot_load+1:queue_i] if "demo feeder row=" in m]
if len(hot_rows)!=1 or "demo feeder row=0 name=match.final.q0 generation=3" not in hot_rows[0]:
    raise SystemExit(f"FAIL hot inventory: reload rows {hot_rows!r}")
selections=[m for m in messages if "demo feeder selection" in m]
if len(selections)!=1 or "demo feeder selection row=0 name=match.final.q0 generation=3" not in selections[0]:
    raise SystemExit(f"FAIL hot inventory: selections {selections!r}")
if sum("queued validated demo playback" in m for m in messages)!=1:
    raise SystemExit("FAIL hot playback: expected exactly one queue")
if any("focused item 'menu_demos'" in m for m in messages):
    raise SystemExit("FAIL hot route: named main focus bypass")
for forbidden in ("QUIC client: TLV ACCEPT","SV_OnPlayerConnect:"):
    if any(forbidden in m for m in messages): raise SystemExit(f"FAIL hot lifecycle: unexpected live admission {forbidden}")
traces=[]
for m in messages:
    if "demo playback trace" not in m: continue
    x=re.search(r"demo playback trace state=8 demoplaying=1 name=match\.final\.q0\.dm_75 sequence=([0-9]+) serverTime=([0-9]+)",m)
    if not x: raise SystemExit(f"FAIL hot playback: wrong trace identity {m!r}")
    traces.append((int(x.group(1)),int(x.group(2))))
if len(traces)!=2 or not (traces[1][0]>traces[0][0] and traces[1][1]>traces[0][1]):
    raise SystemExit("FAIL hot playback: traces did not advance")
if hashlib.sha256(open(source_demo,"rb").read()).digest()!=hashlib.sha256(open(copied_demo,"rb").read()).digest():
    raise SystemExit("FAIL hot fixture: copied dotted demo differs from producer")
def focus_epochs(menu):
    frames={}
    for row in layout:
        if str(row.get("menu",row.get("menuName","")))!=menu: continue
        item=str(row.get("region","")) if row.get("kind")=="item" and bool(row.get("focused",0)) else str(row.get("focusedItem",""))
        if item: frames.setdefault(int(row.get("frame",0)),[]).append(item)
    for frame,items in frames.items():
        if len(items)>1: raise SystemExit(f"FAIL hot layout: {menu} frame {frame} duplicate focus")
    epochs=[]
    for frame in sorted(frames):
        item=frames[frame][0]
        if epochs and epochs[-1][2]==item and frame==epochs[-1][1]+1: epochs[-1]=(epochs[-1][0],frame,item)
        else: epochs.append((frame,frame,item))
    return tuple(item for _a,_b,item in epochs)
if focus_epochs("main")!=("menu_campaign","menu_demos","menu_demos"):
    raise SystemExit(f"FAIL hot layout: main epochs {focus_epochs('main')!r}")
if focus_epochs("demos")!=("btn_play_demo","btn_play_demo","demolist","btn_play_demo"):
    raise SystemExit(f"FAIL hot layout: demos epochs {focus_epochs('demos')!r}")
print("PASS demo-hot inventory contract")
PYEOF
}

write_empty_fixture() {
    python3 - "$1" "$2" "$3" <<'PYEOF'
import json,sys
mode,p,l=sys.argv[1:]
def rec(msg,sev="DEBUG",cat="ui"): return {"ts":"2026-08-12T00:00:00Z","sev":sev,"cat":cat,"msg":msg}
msgs=[
 "WiredUI: demos loaded protocol=74 count=0 generation=1\n",
 "WiredUI: push menu 'main' (depth 1)",
 "wui_menu_nav: K_DOWNARROW dispatched","wui_menu_nav: K_DOWNARROW dispatched","wui_menu_nav: K_DOWNARROW dispatched",
 "WiredUI: push menu 'demos' (depth 2)","WiredUI: demos loaded protocol=74 count=0 generation=2\n","wui_menu_nav: K_ENTER dispatched",
 "wui_menu_nav focus: focused item 'btn_play_demo'","WiredUI: no demo selected\n","wui_menu_nav: K_ENTER dispatched",
 "WiredUI: pop menu (depth 1)","wui_menu_nav: K_ESCAPE dispatched",
 "WiredUI: push menu 'demos' (depth 2)","WiredUI: demos loaded protocol=74 count=0 generation=3\n","wui_menu_nav: K_ENTER dispatched",
 "wui_menu_nav focus: focused item 'btn_back'","WiredUI: pop menu (depth 1)","wui_menu_nav: K_ENTER dispatched",
 "WiredUI: push menu 'demos' (depth 2)","WiredUI: demos loaded protocol=74 count=0 generation=4\n","wui_menu_nav: K_ENTER dispatched",
 "wui_menu_nav focus: focused item 'btn_play_demo'","WiredUI: no demo selected\n","wui_menu_nav: K_ENTER dispatched",
 "Q0_EMPTY_DEMOS_COMPLETE"]
product=[rec(m,"WARN" if m=="WiredUI: no demo selected\n" else "DEBUG") for m in msgs]
def frame(menu,number,focus):
    return [{"menu":menu,"kind":"item","region":focus,"frame":number,"focused":1},
            {"menu":menu,"kind":"item","region":focus,"frame":number+1,"focused":1}]
layout=frame("main",10,"menu_campaign")+frame("main",20,"menu_demos")+frame("demos",30,"btn_play_demo")+frame("main",40,"menu_demos")+frame("demos",50,"btn_back")+frame("main",60,"menu_demos")+frame("demos",70,"btn_play_demo")
def remove_once(token,n=0):
    hits=[i for i,r in enumerate(product) if token in r["msg"]]
    if n>=len(hits): raise RuntimeError(token)
    del product[hits[n]]
def replace_once(a,b,n=0):
    hits=[r for r in product if a in r["msg"]]
    if n>=len(hits): raise RuntimeError(a)
    hits[n]["msg"]=hits[n]["msg"].replace(a,b)
if mode=="missing-empty": remove_once("count=0 generation=2")
elif mode=="nonempty-row": product.insert(7,rec("WiredUI: demo feeder row=0 name=ghost generation=2\n"))
elif mode=="empty-queue": product.insert(10,rec("WiredUI: queued validated demo playback name=ghost\n"))
elif mode=="missing-escape": remove_once("K_ESCAPE")
elif mode=="bad-pop": replace_once("pop menu (depth 1)","pop menu (depth 0)")
elif mode=="missing-back": remove_once("focused item 'btn_back'")
elif mode=="stale-generation": replace_once("generation=3","generation=2")
elif mode=="missing-third-open": remove_once("push menu 'demos'",2)
elif mode=="named-focus": product.insert(5,rec("wui_menu_nav focus: focused item 'menu_demos'"))
elif mode=="network": product.insert(-1,rec("QUIC client: TLV ACCEPT received","INFO","client"))
elif mode=="first": product.insert(-1,rec("FIRST GAMEPLAY FRAME mapname=arena7","INFO","client"))
elif mode=="unexpected-warn": product.append(rec("unexpected\n","WARN","ui"))
elif mode=="missing-escape-return-focus": layout=[r for r in layout if not (r["menu"]=="main" and r["frame"] in (40,41))]
elif mode=="missing-back-return-focus": layout=[r for r in layout if not (r["menu"]=="main" and r["frame"] in (60,61))]
elif mode=="duplicate-focus": layout.append({"menu":"main","kind":"item","region":"menu_host","frame":40,"focused":1})
elif mode=="malformed-json": pass
for path,rows in ((p,product),(l,layout)):
    with open(path,"w") as f:
        for row in rows: f.write(json.dumps(row)+"\n")
if mode=="malformed-json": open(p,"a").write("{broken\n")
PYEOF
}

write_hot_fixture() {
    python3 - "$1" "$2" "$3" "$4" "$5" <<'PYEOF'
import json,sys
mode,p,l,src,dst=sys.argv[1:]
def rec(msg,sev="DEBUG",cat="ui"): return {"sev":sev,"cat":cat,"msg":msg}
msgs=[
 "WiredUI: push menu 'main' (depth 1)",
 "wui_menu_nav: K_DOWNARROW dispatched","wui_menu_nav: K_DOWNARROW dispatched","wui_menu_nav: K_DOWNARROW dispatched",
 "WiredUI: push menu 'demos' (depth 2)","WiredUI: demos loaded protocol=74 count=0 generation=2\n","wui_menu_nav: K_ENTER dispatched",
 "wui_menu_nav focus: focused item 'btn_play_demo'","WiredUI: no demo selected\n","wui_menu_nav: K_ENTER dispatched","Q0_HOT_DEMO_READY",
 "WiredUI: pop menu (depth 1)","wui_menu_nav: K_ESCAPE dispatched",
 "WiredUI: push menu 'demos' (depth 2)","WiredUI: demos loaded protocol=74 count=1 generation=3\n",
 "WiredUI: demo feeder row=0 name=match.final.q0 generation=3\n","wui_menu_nav: K_ENTER dispatched",
 "wui_menu_nav focus: focused item 'btn_play_demo'","WiredUI: no demo selected\n","wui_menu_nav: K_ENTER dispatched",
 "WiredUI: demo feeder selection row=0 name=match.final.q0 generation=3\n","wui_menu_nav focus: focused item 'demolist'","wui_menu_nav: K_DOWNARROW dispatched",
 "wui_menu_nav focus: focused item 'btn_play_demo'","WiredUI: queued validated demo playback name=match.final.q0",
 "WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0","wui_menu_nav: K_ENTER dispatched",
 "Demo file: demos/match.final.q0.dm_75","FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp numEntities=12",
 "WiredUI: demo playback trace state=8 demoplaying=1 name=match.final.q0.dm_75 sequence=10 serverTime=100","WiredUI: demo playback trace state=8 demoplaying=1 name=match.final.q0.dm_75 sequence=20 serverTime=200",
 "CL_NextDemo: exec q0-hot-demo-done.cfg","Q0_HOT_DEMO_COMPLETED"]
product=[rec(m,"WARN" if m=="WiredUI: no demo selected\n" else "DEBUG") for m in msgs]
def frame(menu,n,item): return [{"menu":menu,"kind":"item","region":item,"frame":n,"focused":1},{"menu":menu,"kind":"item","region":item,"frame":n+1,"focused":1}]
layout=frame("main",10,"menu_campaign")+frame("main",20,"menu_demos")+frame("demos",30,"btn_play_demo")+frame("main",40,"menu_demos")+frame("demos",50,"btn_play_demo")+frame("demos",60,"demolist")+frame("demos",70,"btn_play_demo")
def remove(token,n=0):
 h=[i for i,r in enumerate(product) if token in r["msg"]]; del product[h[n]]
def replace(a,b,n=0):
 h=[r for r in product if a in r["msg"]]; h[n]["msg"]=h[n]["msg"].replace(a,b)
if mode=="missing-ready": remove("Q0_HOT_DEMO_READY")
elif mode=="early-row": product.insert(6,rec("WiredUI: demo feeder row=0 name=match.final.q0 generation=2\n"))
elif mode=="additive-stale-row": product.insert(16,rec("WiredUI: demo feeder row=1 name=stale generation=2\n"))
elif mode=="additive-stale-selection": product.insert(21,rec("WiredUI: demo feeder selection row=0 name=stale generation=2\n"))
elif mode=="bad-count": replace("count=1 generation=3","count=0 generation=3")
elif mode=="stale-generation": replace("count=1 generation=3","count=1 generation=2")
elif mode=="collapsed-name": replace("name=match.final.q0","name=match",0)
elif mode=="stale-selection": replace("selection row=0 name=match.final.q0 generation=3","selection row=0 name=match.final.q0 generation=2")
elif mode=="wrong-queue": replace("queued validated demo playback name=match.final.q0","queued validated demo playback name=match")
elif mode=="wrong-file": replace("demos/match.final.q0.dm_75","demos/match.dm_75")
elif mode=="file-suffix": replace("demos/match.final.q0.dm_75","demos/match.final.q0.dm_75.bak")
elif mode=="wrong-map": replace("mapname=maps/arena7.bsp","mapname=maps/arena1.bsp")
elif mode=="wrong-trace-name": replace("name=match.final.q0.dm_75 sequence=10","name=other.dm_75 sequence=10")
elif mode=="wrong-trace-state": replace("state=8 demoplaying=1","state=1 demoplaying=0")
elif mode=="extra-main-down": product.insert(4,rec("wui_menu_nav: K_DOWNARROW dispatched"))
elif mode=="network": product.insert(-1,rec("QUIC client: TLV ACCEPT received","INFO","network"))
elif mode=="missing-return-focus": layout=[r for r in layout if not (r["menu"]=="main" and r["frame"] in (40,41))]
elif mode=="duplicate-row": product.insert(16,rec("WiredUI: demo feeder row=1 name=other generation=3\n"))
elif mode=="no-first": remove("FIRST GAMEPLAY FRAME")
elif mode=="stale-trace": replace("sequence=20 serverTime=200","sequence=10 serverTime=100")
elif mode=="bad-copy": open(dst,"wb").write(b"different")
for path,rows in ((p,product),(l,layout)):
 with open(path,"w") as f:
  for row in rows: f.write(json.dumps(row)+"\n")
if not __import__('os').path.exists(src): open(src,"wb").write(b"demo")
if not __import__('os').path.exists(dst): open(dst,"wb").write(open(src,"rb").read())
PYEOF
}

write_fixture() {
    python3 - "$1" "$2" "$3" "$4" "$5" "$6" <<'PYEOF'
import json,struct,sys
mode,p,c,l,d,t=sys.argv[1:]
def rec(msg,sev="DEBUG",cat="ui"): return {"ts":"2026-08-11T00:00:00Z","sev":sev,"cat":cat,"msg":msg}
prod=[rec("cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=100 numEntities=4 framecount=1)","INFO","client"),rec("recording to demos/a0_decoy.\n","INFO","client"),rec("Stopped demo recording.\n","INFO","client"),rec("recording to demos/q0_target.\n","INFO","client"),rec("Stopped demo recording.\n","INFO","client"),rec("Q0_DEMOS_RECORDED","INFO","system")]
cons_msgs=[
    "WiredUI: loaded 0 UI state entries (ignored transient server selection 0)\n",
    "WiredUI: push menu 'main' (depth 1)",
    "wui_menu_nav: K_DOWNARROW dispatched",
    "wui_menu_nav: K_DOWNARROW dispatched",
    "wui_menu_nav: K_DOWNARROW dispatched",
    "WiredUI: push menu 'demos' (depth 2)",
    "WiredUI: demos loaded protocol=74 count=2 generation=2\n",
    "WiredUI: demo feeder row=0 name=a0_decoy generation=2\n",
    "WiredUI: demo feeder row=1 name=q0_target generation=2\n",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav focus: focused item 'btn_play_demo'",
    "WiredUI: no demo selected\n",
    "wui_menu_nav: K_ENTER dispatched",
    "WiredUI: demo feeder selection row=0 name=a0_decoy generation=2\n",
    "wui_menu_nav focus: focused item 'demolist'",
    "WiredUI: demo feeder selection row=1 name=q0_target generation=2\n",
    "wui_menu_nav: K_DOWNARROW dispatched",
    "wui_menu_nav focus: focused item 'btn_play_demo'",
    "WiredUI: queued validated demo playback name=q0_target\n",
    "WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0",
    "wui_menu_nav: K_ENTER dispatched",
    "Demo file: demos/q0_target.dm_75\n",
    "cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=200 numEntities=4 framecount=2)",
    "WiredUI: demo playback trace state=8 demoplaying=1 name=q0_target.dm_75 sequence=10 serverTime=220\n",
    "WiredUI: demo playback trace state=8 demoplaying=1 name=q0_target.dm_75 sequence=14 serverTime=300\n",
    "CL_NextDemo: exec q0-demo-done.cfg\n",
    "Q0_DEMO_COMPLETED",
]
cons=[rec(m,"WARN" if m=="WiredUI: no demo selected\n" else ("INFO" if "FIRST" in m or "Demo file:" in m else "DEBUG"),"client" if "FIRST" in m or "Demo file:" in m or "CL_NextDemo" in m else "ui") for m in cons_msgs]
rail=("menu_campaign","menu_join","menu_host","menu_demos","menu_options","menu_quit")
def main_layout(frame,focused):
    out=[{"menu":"main","kind":"menu","region":"main","frame":frame,
          "x":0,"y":0,"w":1280,"h":720,"dpiScale":1.0,"focused":0}]
    for index,name in enumerate(rail):
        out.append({"menu":"main","kind":"item","region":name,"frame":frame,
                    "x":64,"y":300+57*index,"w":534,"h":56,"dpiScale":1.0,
                    "focused":int(name==focused)})
    out.append({"menu":"main","kind":"item","region":"action_menu","frame":frame,
                "x":64,"y":296,"w":534,"h":346,"dpiScale":1.0,"focused":0})
    out.append({"menu":"main","kind":"item","region":"left_footer_row","frame":frame,
                "x":64,"y":680,"w":534,"h":14,"dpiScale":1.0,"focused":0})
    return out
layout=main_layout(1,"menu_campaign")+main_layout(2,"menu_demos")+[
        {"menu":"demos","kind":"item","region":"btn_play_demo","frame":3,"focused":1},
        {"menu":"demos","kind":"item","region":"demolist","frame":4,"focused":1},
        {"menu":"demos","kind":"item","region":"btn_play_demo","frame":5,"focused":1}]
def remove(s):
    nonlocal_dummy=None
    for arr in (prod,cons): arr[:]=[r for r in arr if s not in r["msg"]]
def replace(a,b):
    for arr in (prod,cons):
        for r in arr: r["msg"]=r["msg"].replace(a,b)
def remove_nth(s,n):
    hits=[]
    for arr in (prod,cons):
        for i,r in enumerate(arr):
            if s in r["msg"]: hits.append((arr,i))
    if n>=len(hits): raise RuntimeError(f"fixture missing occurrence {n} of {s!r}")
    arr,i=hits[n]; del arr[i]
if mode=="missing-record": remove("recording to demos/q0_target")
elif mode=="missing-stop": prod=[r for i,r in enumerate(prod) if not ("Stopped demo" in r["msg"] and i>2)]
elif mode=="wrong-count": replace("count=2","count=3")
elif mode=="wrong-row": replace("row=1 name=q0_target","row=1 name=a0_decoy")
elif mode=="missing-demo-down": remove_nth("K_DOWNARROW",3)
elif mode=="missing-main-down": remove_nth("K_DOWNARROW",1)
elif mode=="extra-main-down":
    at=next(i for i,r in enumerate(cons) if "push menu 'demos'" in r["msg"])
    cons.insert(at,rec("wui_menu_nav: K_DOWNARROW dispatched"))
elif mode=="named-main-focus":
    at=next(i for i,r in enumerate(cons) if "push menu 'demos'" in r["msg"])
    cons.insert(at,rec("wui_menu_nav focus: focused item 'menu_demos'"))
elif mode=="missing-main-enter": remove_nth("K_ENTER",0)
elif mode=="early-main-enter":
    enter=next(r for r in cons if "K_ENTER" in r["msg"]); cons.remove(enter)
    at=next(i for i,r in enumerate(cons) if "demo feeder row=0" in r["msg"])
    cons.insert(at,enter)
elif mode=="wrong-demos-depth": replace("push menu 'demos' (depth 2)","push menu 'demos' (depth 1)")
elif mode=="wrong-main-focus":
    for r in layout:
        if r.get("menu")=="main" and r.get("frame")==2:
            r["focused"]=int(r.get("region")=="menu_options")
elif mode=="duplicate-main-focus":
    next(r for r in layout if r.get("menu")=="main" and r.get("frame")==2 and r.get("region")=="menu_host")["focused"]=1
elif mode=="duplicate-demos-focus":
    layout.append({"menu":"demos","kind":"item","region":"btn_back","frame":4,"focused":1})
elif mode=="missing-main-row":
    layout=[r for r in layout if not (r.get("menu")=="main" and r.get("frame")==2 and r.get("region")=="menu_options")]
elif mode=="bad-main-height":
    next(r for r in layout if r.get("menu")=="main" and r.get("frame")==2 and r.get("region")=="menu_host")["h"]=55
elif mode=="main-overlap":
    next(r for r in layout if r.get("menu")=="main" and r.get("frame")==2 and r.get("region")=="menu_demos")["y"]=450
elif mode=="main-overflow":
    next(r for r in layout if r.get("menu")=="main" and r.get("frame")==2 and r.get("region")=="menu_quit")["y"]=690
elif mode=="footer-overlap":
    next(r for r in layout if r.get("menu")=="main" and r.get("frame")==2 and r.get("region")=="left_footer_row")["y"]=630
elif mode=="missing-queue": remove("queued validated")
elif mode=="bad-close": replace("catcher_ui=0","catcher_ui=1")
elif mode=="wrong-file": replace("Demo file: demos/q0_target","Demo file: demos/a0_decoy")
elif mode=="no-first": cons=[r for r in cons if "FIRST GAMEPLAY" not in r["msg"]]
elif mode=="zero-entities": replace("numEntities=4 framecount=2","numEntities=0 framecount=2")
elif mode=="one-trace": cons=[r for r in cons if "sequence=14" not in r["msg"]]
elif mode=="stale-trace": replace("sequence=14 serverTime=300","sequence=10 serverTime=220")
elif mode=="early-eof":
    x=next(r for r in cons if "CL_NextDemo" in r["msg"]); cons.remove(x); cons.insert(18,x)
elif mode=="network": cons.insert(-1,rec("QUIC client: TLV ACCEPT received","INFO","client"))
elif mode=="ui-warn": cons.append(rec("unexpected ui warn\n","WARN","ui"))
elif mode=="renderer-error": cons.append(rec("renderer failed\n","ERROR","renderer"))
elif mode=="missing-layout": layout=layout[:-1]
elif mode=="malformed-json": pass
for path,arr in ((p,prod),(c,cons),(l,layout)):
    with open(path,"w") as f:
        for r in arr: f.write(json.dumps(r)+"\n")
if mode=="malformed-json": open(c,"a").write("{broken\n")
def demo(path,seed,bad=False):
    payload=(bytes([seed])*700)
    data=struct.pack("<ii",1,len(payload))+payload+struct.pack("<ii",2,len(payload))+payload
    if not bad: data+=struct.pack("<ii",-1,-1)
    open(path,"wb").write(data)
demo(d,1,mode=="bad-demo")
demo(t,2,mode=="bad-demo")
PYEOF
}

if [ "${1:-}" = "--self-test" ]; then
    ROOT="$(mktemp -d -t wired-demoplay-self-XXXXXX 2>/dev/null || mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
    write_empty_fixture clean "$ROOT/e.jsonl" "$ROOT/el.jsonl"
    analyze_empty_contract "$ROOT/e.jsonl" "$ROOT/el.jsonl" >/dev/null || { echo "FAIL: clean empty fixture rejected"; exit 1; }
    write_hot_fixture clean "$ROOT/h.jsonl" "$ROOT/hl.jsonl" "$ROOT/h-source.dm_75" "$ROOT/h-copy.dm_75"
    analyze_hot_contract "$ROOT/h.jsonl" "$ROOT/hl.jsonl" "$ROOT/h-source.dm_75" "$ROOT/h-copy.dm_75" >/dev/null || { echo "FAIL: clean hot fixture rejected"; exit 1; }
    write_fixture clean "$ROOT/p.jsonl" "$ROOT/c.jsonl" "$ROOT/l.jsonl" "$ROOT/a.dm_75" "$ROOT/q.dm_75"
    analyze_contract "$ROOT/p.jsonl" "$ROOT/c.jsonl" "$ROOT/l.jsonl" "$ROOT/a.dm_75" "$ROOT/q.dm_75" >/dev/null || { echo "FAIL: clean fixture rejected"; exit 1; }
    rc=0
    for defect in missing-empty nonempty-row empty-queue missing-escape bad-pop missing-back stale-generation missing-third-open named-focus network first unexpected-warn missing-escape-return-focus missing-back-return-focus duplicate-focus malformed-json; do
        write_empty_fixture "$defect" "$ROOT/e-$defect.jsonl" "$ROOT/el-$defect.jsonl"
        if analyze_empty_contract "$ROOT/e-$defect.jsonl" "$ROOT/el-$defect.jsonl" >/dev/null 2>&1; then echo "FAIL: accepted empty defect $defect"; rc=1; else echo "  PASS rejected empty $defect"; fi
    done
    for defect in missing-ready early-row additive-stale-row additive-stale-selection bad-count stale-generation collapsed-name stale-selection wrong-queue wrong-file file-suffix wrong-map wrong-trace-name wrong-trace-state extra-main-down network missing-return-focus duplicate-row no-first stale-trace bad-copy; do
        write_hot_fixture "$defect" "$ROOT/h-$defect.jsonl" "$ROOT/hl-$defect.jsonl" "$ROOT/hs-$defect.dm_75" "$ROOT/hc-$defect.dm_75"
        if analyze_hot_contract "$ROOT/h-$defect.jsonl" "$ROOT/hl-$defect.jsonl" "$ROOT/hs-$defect.dm_75" "$ROOT/hc-$defect.dm_75" >/dev/null 2>&1; then echo "FAIL: accepted hot defect $defect"; rc=1; else echo "  PASS rejected hot $defect"; fi
    done
    for defect in missing-record missing-stop wrong-count wrong-row missing-demo-down missing-main-down extra-main-down named-main-focus missing-main-enter early-main-enter wrong-demos-depth wrong-main-focus duplicate-main-focus duplicate-demos-focus missing-main-row bad-main-height main-overlap main-overflow footer-overlap missing-queue bad-close wrong-file no-first zero-entities one-trace stale-trace early-eof network ui-warn renderer-error missing-layout malformed-json bad-demo; do
        write_fixture "$defect" "$ROOT/p-$defect.jsonl" "$ROOT/c-$defect.jsonl" "$ROOT/l-$defect.jsonl" "$ROOT/a-$defect.dm_75" "$ROOT/q-$defect.dm_75"
        if analyze_contract "$ROOT/p-$defect.jsonl" "$ROOT/c-$defect.jsonl" "$ROOT/l-$defect.jsonl" "$ROOT/a-$defect.dm_75" "$ROOT/q-$defect.dm_75" >/dev/null 2>&1; then echo "FAIL: accepted defect $defect"; rc=1; else echo "  PASS rejected $defect"; fi
    done
    [ "$rc" -eq 0 ] && echo "==> SELF-TEST PASS: empty+hot+playback clean accepted; seventy defects rejected"
    exit "$rc"
fi

WIRED="${1:-}"
if [ -z "$WIRED" ]; then echo "SKIP: pass a fully assembled Wired GUI binary"; exit 77; fi
if [ ! -x "$WIRED" ] && [ -x "$WIRED.exe" ]; then WIRED="$WIRED.exe"; fi
if [ ! -x "$WIRED" ]; then echo "SKIP: binary not executable: $WIRED"; exit 77; fi
case "$WIRED" in /*) : ;; *) WIRED="$PWD/$WIRED" ;; esac
WIRED_DIR="$(cd "$(dirname "$WIRED")" && pwd)"; WIRED="$WIRED_DIR/$(basename "$WIRED")"
if [ ! -f "$TIMEOUT_RUNNER" ] || ! python3 -c 'import sys; raise SystemExit(0 if sys.version_info >= (3,8) else 1)' 2>/dev/null; then echo "SKIP: Python >=3.8 and timeout runner required"; exit 77; fi
HEADLESS="${WIRED_BINARY_HEADLESS:-}"
if [ -z "$HEADLESS" ]; then
    gui_name="$(basename "$WIRED")"; suffix="${gui_name#wired}"
    for candidate in "$WIRED_DIR/wired-headless$suffix" "$WIRED_DIR/wired-headless.arm64" "$WIRED_DIR/wired-headless.aarch64" "$WIRED_DIR/wired-headless.x86_64" "$WIRED_DIR/wired-headless.x64.exe" "$WIRED_DIR/../../../wired-headless$suffix" "$WIRED_DIR/../../../wired-headless.arm64" "$WIRED_DIR/../../../wired-headless.x86_64"; do
        [ -x "$candidate" ] && { HEADLESS="$candidate"; break; }
    done
fi
[ -n "$HEADLESS" ] && [ -x "$HEADLESS" ] || { echo "SKIP: sibling wired-headless unavailable"; exit 77; }
HEADLESS="$(cd "$(dirname "$HEADLESS")" && pwd)/$(basename "$HEADLESS")"
PACK_ROOT="$(wired_find_archive_root "$WIRED_DIR" "$WIRED_DIR/../Resources" "$WIRED_DIR/../../.." 2>/dev/null || true)"
[ -n "$PACK_ROOT" ] || { echo "SKIP: current VFS archives not found"; exit 77; }
CONTENT_ROOT="$(wired_find_archive_root "${WIRED_CONTENT_ROOT:-}" "$WIRED_HOME" "$PACK_ROOT" 2>/dev/null || true)"
[ -n "$CONTENT_ROOT" ] || { echo "SKIP: set WIRED_CONTENT_ROOT"; exit 77; }
CURRENT_ARCHIVE="$(wired_first_archive "$PACK_ROOT/base")"; BASE_ARCHIVE="$(wired_first_archive "$CONTENT_ROOT/base")"

RUN_ROOT="$(mktemp -d -t wired-demoplay-XXXXXX 2>/dev/null || mktemp -d)"; EMPTY="$RUN_ROOT/empty/q3now-preview"; HOT="$RUN_ROOT/hot/q3now-preview"; PRODUCER="$RUN_ROOT/producer/q3now-preview"; CONSUMER="$RUN_ROOT/consumer/q3now-preview"; SERVER="$RUN_ROOT/server/q3now-preview"
mkdir -p "$EMPTY/base" "$HOT/base" "$PRODUCER/base" "$CONSUMER/base" "$SERVER/base"
SERVER_PID=""; SERVER_FIFO="$RUN_ROOT/server.stdin"; SERVER_OPEN=0
stop_server(){
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then [ "$SERVER_OPEN" -eq 1 ] && printf '%s\n' quit >&9 2>/dev/null || true; for _ in $(seq 1 150); do kill -0 "$SERVER_PID" 2>/dev/null || break; sleep 0.1; done; fi
    if [ -n "$SERVER_PID" ]; then wait "$SERVER_PID" 2>/dev/null; SERVER_RC=$?; else SERVER_RC=0; fi
    [ "$SERVER_OPEN" -eq 1 ] && { exec 9>&-; SERVER_OPEN=0; }; SERVER_PID=""
}
cleanup(){ stop_server; if [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ]; then echo "    kept artifacts: $RUN_ROOT"; else rm -rf "$RUN_ROOT"; fi; }; trap cleanup EXIT INT TERM
for home in "$EMPTY" "$HOT" "$PRODUCER" "$CONSUMER" "$SERVER"; do wired_link_content_into_home "$home" "$CONTENT_ROOT/base" "$PACK_ROOT/base" || exit 1; done
PORT="$(python3 - <<'PYEOF'
import socket
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()
PYEOF
)"
case "$PORT" in ''|*[!0-9]*) echo "FAIL: loopback port allocation failed"; exit 1;; esac

cat >"$PRODUCER/base/q0-demo-producer.cfg" <<CFGEOF
set attract_enabled 0
set activeAction "exec q0-demo-record-live.cfg"
connect 127.0.0.1:$PORT
CFGEOF
cat >"$EMPTY/base/q0-demo-empty.cfg" <<'CFGEOF'
set attract_enabled 0
wait 100
wui_push main
wait 20
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav down
wait 2
wui_menu_nav down
wait 2
wui_menu_nav down
wait 10
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wait 20
wui_menu_nav focus btn_play_demo
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wait 10
wui_menu_nav back
wait 20
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wait 20
wui_menu_nav focus btn_back
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wait 20
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wait 20
wui_menu_nav focus btn_play_demo
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wait 10
echo Q0_EMPTY_DEMOS_COMPLETE
wait 10
quit
CFGEOF
cat >"$PRODUCER/base/q0-demo-record-live.cfg" <<'CFGEOF'
wait 60
+forward
record a0_decoy
wait 180
stoprecord
-forward
+moveright
record q0_target
wait 700
stoprecord
-moveright
echo Q0_DEMOS_RECORDED
wait 20
quit
CFGEOF
cat >"$HOT/base/q0-demo-hot.cfg" <<'CFGEOF'
set attract_enabled 0
set nextdemo "exec q0-hot-demo-done.cfg"
set activeAction "exec q0-hot-playback-live.cfg"
wait 100
wui_push main
wait 20
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav down
wait 2
wui_menu_nav down
wait 2
wui_menu_nav down
wait 10
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wait 20
wui_menu_nav focus btn_play_demo
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
echo Q0_HOT_DEMO_READY
wait 600
wui_menu_nav back
wait 20
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wait 20
wui_menu_nav focus btn_play_demo
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wait 10
wui_menu_nav focus demolist
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav down
wait 10
wui_menu_nav focus btn_play_demo
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
CFGEOF
cat >"$HOT/base/q0-hot-playback-live.cfg" <<'CFGEOF'
wait 30
wui_demo_trace
wait 90
wui_demo_trace
CFGEOF
cat >"$HOT/base/q0-hot-demo-done.cfg" <<'CFGEOF'
echo Q0_HOT_DEMO_COMPLETED
wait 10
quit
CFGEOF
cat >"$CONSUMER/base/q0-demo-consumer.cfg" <<'CFGEOF'
set q0_injected 0
set attract_enabled 0
set nextdemo "exec q0-demo-done.cfg"
set activeAction "exec q0-demo-playback-live.cfg"
wait 100
wui_push main
wait 20
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav down
wait 2
wui_menu_nav down
wait 2
wui_menu_nav down
wait 10
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wait 20
wui_menu_nav focus btn_play_demo
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
print q0_injected
wait 10
wui_menu_nav focus demolist
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav down
wait 10
wui_menu_nav focus btn_play_demo
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
CFGEOF
cat >"$CONSUMER/base/q0-demo-playback-live.cfg" <<'CFGEOF'
wait 30
wui_demo_trace
wait 90
wui_demo_trace
CFGEOF
cat >"$CONSUMER/base/q0-demo-done.cfg" <<'CFGEOF'
echo Q0_DEMO_COMPLETED
wait 10
quit
CFGEOF
python3 - "$CONSUMER/base/wired_ui_state.dat" <<'PYEOF'
import struct,sys
k=b"ui_selectedDemo"; v=b"stale;set q0_injected 1"
open(sys.argv[1],"wb").write(struct.pack("@iii",0x57554953,1,1)+struct.pack("@HH",len(k),len(v))+k+v)
PYEOF

case "$(uname -s)" in Darwin) E_HOME="$EMPTY"; H_HOME="$HOT"; P_HOME="$PRODUCER"; C_HOME="$CONSUMER"; S_HOME="$SERVER"; PLATFORM_ARGS=( -ApplePersistenceIgnoreState YES );; MINGW*|MSYS*|CYGWIN*) E_HOME="$(cygpath -w "$EMPTY")"; H_HOME="$(cygpath -w "$HOT")"; P_HOME="$(cygpath -w "$PRODUCER")"; C_HOME="$(cygpath -w "$CONSUMER")"; S_HOME="$(cygpath -w "$SERVER")"; PLATFORM_ARGS=();; *) E_HOME="$EMPTY"; H_HOME="$HOT"; P_HOME="$PRODUCER"; C_HOME="$CONSUMER"; S_HOME="$SERVER"; PLATFORM_ARGS=();; esac
COMMON=( +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced )

echo "==> Demo empty lifecycle: authored route, Play refusal, ESC and Back reopen"
python3 "$TIMEOUT_RUNNER" --timeout 180 --kill-after 15 --cwd "$RUN_ROOT/empty" --stdout "$RUN_ROOT/empty.stdout" -- "$WIRED" "${PLATFORM_ARGS[@]}" +set fs_homepath "$E_HOME" "${COMMON[@]}" +set r_layoutDump 0 +exec q0-demo-empty.cfg
[ "$?" -eq 0 ] || { echo "FAIL: empty demo lifecycle did not exit cleanly"; exit 1; }
ELOG="$EMPTY/qconsole.jsonl"; ELAYOUT="$RUN_ROOT/empty/layoutdump.jsonl"
for f in "$ELOG" "$ELAYOUT"; do [ -s "$f" ] || { echo "FAIL: missing empty lifecycle evidence $f"; exit 1; }; done
if [ -d "$EMPTY/base/demos" ] && find "$EMPTY/base/demos" -type f -print -quit | grep -q .; then echo "FAIL: empty lifecycle home unexpectedly contains demos"; exit 1; fi
analyze_empty_contract "$ELOG" "$ELAYOUT" || exit 1

echo "==> Demo producer server: 127.0.0.1:$PORT arena7"
mkfifo "$SERVER_FIFO"; exec 9<>"$SERVER_FIFO"; SERVER_OPEN=1
( cd "$(dirname "$HEADLESS")" && exec "$HEADLESS" +set fs_homepath "$S_HOME" +set com_automated 1 +set com_noHardReboot 1 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +set net_ip 127.0.0.1 +set net_port "$PORT" +set sv_public 0 +set sv_pure 0 +set g_autoBots 0 +map arena7 <&9 ) >"$RUN_ROOT/server.stdout" 2>&1 & SERVER_PID=$!
python3 - "$SERVER/qconsole.jsonl" "$SERVER_PID" "$PORT" <<'PYEOF' || exit 1
import json,os,re,sys,time
p,pid,port=sys.argv[1],int(sys.argv[2]),sys.argv[3]; end=time.monotonic()+45
while time.monotonic()<end:
 try: os.kill(pid,0)
 except ProcessLookupError: raise SystemExit("FAIL: demo server exited before ready")
 try:
  msgs=[json.loads(x).get("msg","") for x in open(p,errors="replace") if x.strip()]
 except (OSError,ValueError): msgs=[]
 if any(re.search(rf"WiredNet: listening on port {port}",m) for m in msgs) and any(re.search(r"Server: arena7\b",m) for m in msgs): raise SystemExit(0)
 time.sleep(.1)
raise SystemExit("FAIL: demo server readiness timeout")
PYEOF
echo "==> Demo producer client: remote current-protocol recording"
python3 "$TIMEOUT_RUNNER" --timeout 240 --kill-after 15 --cwd "$RUN_ROOT/producer" --stdout "$RUN_ROOT/producer.stdout" -- "$WIRED" "${PLATFORM_ARGS[@]}" +set fs_homepath "$P_HOME" "${COMMON[@]}" +set net_ip 127.0.0.1 +set wn_cert_verify 0 +exec q0-demo-producer.cfg
[ "$?" -eq 0 ] || { echo "FAIL: producer did not exit cleanly"; exit 1; }
stop_server; [ "${SERVER_RC:-1}" -eq 0 ] || { echo "FAIL: demo server did not exit cleanly rc=${SERVER_RC:-unset}"; exit 1; }
DECOY="$PRODUCER/base/demos/a0_decoy.dm_$PROTOCOL"; TARGET="$PRODUCER/base/demos/q0_target.dm_$PROTOCOL"
[ -s "$DECOY" ] && [ -s "$TARGET" ] || { echo "FAIL: finalized demos missing"; exit 1; }
if find "$PRODUCER/base/demos" -name '*.tmp' -print -quit | grep -q .; then echo "FAIL: temporary demo residue"; exit 1; fi

echo "==> Demo hot inventory: empty open, external dotted-name arrival, reload and playback"
python3 "$TIMEOUT_RUNNER" --timeout 240 --kill-after 15 --cwd "$RUN_ROOT/hot" --stdout "$RUN_ROOT/hot.stdout" -- "$WIRED" "${PLATFORM_ARGS[@]}" +set fs_homepath "$H_HOME" "${COMMON[@]}" +set r_layoutDump 0 +exec q0-demo-hot.cfg & HOT_RUNNER_PID=$!
python3 - "$HOT/qconsole.jsonl" "$HOT_RUNNER_PID" <<'PYEOF' || { wait "$HOT_RUNNER_PID" 2>/dev/null || true; exit 1; }
import json,os,sys,time
p,pid=sys.argv[1],int(sys.argv[2]); end=time.monotonic()+60
while time.monotonic()<end:
 try: os.kill(pid,0)
 except ProcessLookupError: raise SystemExit("FAIL: hot client exited before readiness marker")
 try:
  if any(json.loads(x).get("msg","").strip()=="Q0_HOT_DEMO_READY" for x in open(p,errors="replace") if x.strip()): raise SystemExit(0)
 except (OSError,ValueError): pass
 time.sleep(.05)
raise SystemExit("FAIL: hot client readiness timeout")
PYEOF
if [ -d "$HOT/base/demos" ] && find "$HOT/base/demos" -type f -print -quit | grep -q .; then echo "FAIL: hot home was not empty at readiness"; exit 1; fi
mkdir -p "$HOT/base/demos"; cp "$TARGET" "$HOT/base/demos/match.final.q0.dm_$PROTOCOL" || exit 1
wait "$HOT_RUNNER_PID"; [ "$?" -eq 0 ] || { echo "FAIL: hot demo lifecycle did not exit cleanly"; exit 1; }
HLOG="$HOT/qconsole.jsonl"; HLAYOUT="$RUN_ROOT/hot/layoutdump.jsonl"; HCOPY="$HOT/base/demos/match.final.q0.dm_$PROTOCOL"
for f in "$HLOG" "$HLAYOUT" "$HCOPY"; do [ -s "$f" ] || { echo "FAIL: missing hot inventory evidence $f"; exit 1; }; done
analyze_hot_contract "$HLOG" "$HLAYOUT" "$TARGET" "$HCOPY" || exit 1

mkdir -p "$CONSUMER/base/demos"; cp "$DECOY" "$CONSUMER/base/demos/a0_decoy.dm_$PROTOCOL"; cp "$TARGET" "$CONSUMER/base/demos/q0_target.dm_$PROTOCOL"

echo "==> Demo consumer: real feeder selection and natural EOF"
python3 "$TIMEOUT_RUNNER" --timeout 240 --kill-after 15 --cwd "$RUN_ROOT/consumer" --stdout "$RUN_ROOT/consumer.stdout" -- "$WIRED" "${PLATFORM_ARGS[@]}" +set fs_homepath "$C_HOME" "${COMMON[@]}" +set r_layoutDump 0 +exec q0-demo-consumer.cfg
[ "$?" -eq 0 ] || { echo "FAIL: consumer did not exit cleanly"; exit 1; }
PLOG="$PRODUCER/qconsole.jsonl"; CLOG="$CONSUMER/qconsole.jsonl"; LAYOUT="$RUN_ROOT/consumer/layoutdump.jsonl"
for f in "$PLOG" "$CLOG" "$LAYOUT"; do [ -s "$f" ] || { echo "FAIL: missing evidence $f"; exit 1; }; done
cmp -s "$TARGET" "$CONSUMER/base/demos/q0_target.dm_$PROTOCOL" || { echo "FAIL: target changed between phases"; exit 1; }
analyze_contract "$PLOG" "$CLOG" "$LAYOUT" "$CONSUMER/base/demos/a0_decoy.dm_$PROTOCOL" "$CONSUMER/base/demos/q0_target.dm_$PROTOCOL" || exit 1
python3 - "$WIRED" "$CURRENT_ARCHIVE" "$BASE_ARCHIVE" "$TARGET" "$0" <<'PYEOF'
import hashlib,sys
for label,path in zip(("binary","product-archive","base-archive","demo","harness"),sys.argv[1:]):
    print(f"    {label}_sha256={hashlib.sha256(open(path,'rb').read()).hexdigest()}")
PYEOF
echo "==> WiredUI Demo Play gate: PASS"
