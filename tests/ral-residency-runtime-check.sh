#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Exact-process texture adapter authority for the backend-neutral RAL residency policy.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze_contract() {
python3 - "$1" "$2" <<'PYEOF'
import hashlib,json,os,re,sys
log_path,manifest_path=sys.argv[1:]
rows=[]
for number,line in enumerate(open(log_path,encoding="utf-8",errors="strict"),1):
 if not line.strip():continue
 try:row=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL ral-residency JSON {number}: {exc}")
 if not isinstance(row,dict) or not all(isinstance(row.get(k),str) for k in ("ts","sev","cat","msg")):raise SystemExit("FAIL ral-residency log schema")
 rows.append(row)
if not rows:raise SystemExit("FAIL ral-residency empty evidence")
def norm(value):return value[:-1] if value.endswith("\n") and not value.endswith("\n\n") else value
vals=[norm(row["msg"]) for row in rows]
claimed=("RAL residency material:","RAL residency material plane:","RAL residency mip test:","RAL residency page state:","RAL residency select:","r_texEvictForce:","vk_ral_drain_reregisters:","r_texResidencyBudgetTest:","Q0_RAL_RESIDENCY_","----- Server Shutdown ","==== ShutdownGame ====","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME")
for value in vals:
 parts=re.split(r"[\r\n]",value)
 if len(parts)>1 and any(part.startswith(claimed) for part in parts):raise SystemExit("FAIL ral-residency claimed logical-line smuggling")
if any(row["sev"].upper() in ("ERROR","FATAL") for row in rows):raise SystemExit("FAIL ral-residency severity")
for value in vals:
 if ("VUID-" in value or "pinned image" in value or "victim-selection bug" in value or
     value.startswith("vk_ral_reregister_image:") or value.startswith("Ral_CreateTexture failed")):
  raise SystemExit(f"FAIL ral-residency forbidden marker: {value}")
def exact(prefix,pattern,sev,cat,count=1):
 found=[(i,row,value) for i,(row,value) in enumerate(zip(rows,vals)) if value.startswith(prefix)]
 if len(found)!=count:raise SystemExit(f"FAIL ral-residency {prefix} cardinality {len(found)}")
 for _,row,value in found:
  if row["sev"].upper()!=sev or row["cat"].lower()!=cat or re.fullmatch(pattern,value) is None:
   raise SystemExit(f"FAIL ral-residency {prefix} body/metadata: {value}")
 return found
first=exact("cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME",r"cls\.state: -> CA_ACTIVE \(FIRST GAMEPLAY FRAME mapname=maps/arena7\.bsp serverTime=[0-9]+ numEntities=[0-9]+ framecount=[0-9]+\)","INFO","client")[0]
material=exact("RAL residency material:",r"RAL residency material: action=(?:hold material=[0-9]+ level=0 planeMask=5 state=stale completedMask=0 baseResource=[0-9]+ baseSlot=[0-9]+ ormResource=[0-9]+ ormSlot=[0-9]+ baseMip=1 atomic=1|upload-start material=[0-9]+ level=0 planeMask=5 state=in-flight completedMask=0 parentBound=1 heldFrames=[1-9][0-9]* atomic=1|promote material=[0-9]+ level=0 planeMask=5 state=resident completedMask=5 baseSlot=[0-9]+ ormSlot=[0-9]+ heldFrames=[1-9][0-9]* atomic=1)","WARN","renderer.ral.texture",3)
mat_hold=re.fullmatch(r"RAL residency material: action=hold material=([0-9]+) level=0 planeMask=5 state=stale completedMask=0 baseResource=([0-9]+) baseSlot=([0-9]+) ormResource=([0-9]+) ormSlot=([0-9]+) baseMip=1 atomic=1",material[0][2])
mat_start=re.fullmatch(r"RAL residency material: action=upload-start material=([0-9]+) level=0 planeMask=5 state=in-flight completedMask=0 parentBound=1 heldFrames=([1-9][0-9]*) atomic=1",material[1][2])
mat_promote=re.fullmatch(r"RAL residency material: action=promote material=([0-9]+) level=0 planeMask=5 state=resident completedMask=5 baseSlot=([0-9]+) ormSlot=([0-9]+) heldFrames=([1-9][0-9]*) atomic=1",material[2][2])
if not mat_hold or not mat_start or not mat_promote or len({mat_hold.group(1),mat_start.group(1),mat_promote.group(1)})!=1 or mat_hold.group(1)!=mat_hold.group(2) or mat_hold.group(3)!=mat_promote.group(2) or mat_hold.group(5)!=mat_promote.group(3) or mat_hold.group(2)==mat_hold.group(4):raise SystemExit("FAIL ral-residency material identity/order")
material_planes=exact("RAL residency material plane:",r"RAL residency material plane: action=complete material=[0-9]+ plane=(?:base|orm) bit=(?:1|4) resource=[0-9]+ slot=[0-9]+ completedMask=(?:1|4|5)","WARN","renderer.ral.texture",2)
plane_re=re.compile(r"RAL residency material plane: action=complete material=([0-9]+) plane=(base|orm) bit=([14]) resource=([0-9]+) slot=([0-9]+) completedMask=([145])")
plane_rows=[plane_re.fullmatch(item[2]).groups() for item in material_planes]
if {item[1] for item in plane_rows}!={"base","orm"} or any(item[0]!=mat_hold.group(1) for item in plane_rows):raise SystemExit("FAIL ral-residency material plane identity")
expected={"base":(1,int(mat_hold.group(2)),int(mat_hold.group(3))),"orm":(4,int(mat_hold.group(4)),int(mat_hold.group(5)))}
running=0
for item in plane_rows:
 bit,resource,slot=map(int,(item[2],item[3],item[4]));running|=bit
 if (bit,resource,slot)!=expected[item[1]] or int(item[5])!=running:raise SystemExit("FAIL ral-residency material plane completion")
if running!=5:raise SystemExit("FAIL ral-residency material plane mask")
mip_views=exact("RAL residency mip test:",r"RAL residency mip test: action=(?:hold class=texture resource=[0-9]+ slot=[0-9]+ childLevel=0 parentLevel=1 baseMip=1 levelCount=[1-9][0-9]* sampleAge=[0-9]+ fallback=parent name=[^\r\n]+|upload-start class=texture resource=[0-9]+ slot=[0-9]+ childLevel=0 parentLevel=1 baseMip=1 bytes=[1-9][0-9]* synchronous=0 parentBound=1 heldFrames=[1-9][0-9]* sampleAge=[0-9]+ fallback=parent source=decoded name=[^\r\n]+|upload-promote class=texture resource=[0-9]+ slot=[0-9]+ childLevel=0 parentLevel=1 baseMip=0 levelCount=[1-9][0-9]* bytes=[1-9][0-9]* synchronous=0 fenceSignaled=1 heldFrames=[1-9][0-9]* uploadFrames=[1-9][0-9]* sampleAge=[0-9]+ fallback=parent source=decoded name=[^\r\n]+)","WARN","renderer.ral.texture",3)
hold_re=re.compile(r"RAL residency mip test: action=hold class=texture resource=([0-9]+) slot=([0-9]+) childLevel=0 parentLevel=1 baseMip=1 levelCount=([1-9][0-9]*) sampleAge=([0-9]+) fallback=parent name=([^\r\n]+)")
start_re=re.compile(r"RAL residency mip test: action=upload-start class=texture resource=([0-9]+) slot=([0-9]+) childLevel=0 parentLevel=1 baseMip=1 bytes=([1-9][0-9]*) synchronous=0 parentBound=1 heldFrames=([1-9][0-9]*) sampleAge=([0-9]+) fallback=parent source=decoded name=([^\r\n]+)")
promote_re=re.compile(r"RAL residency mip test: action=upload-promote class=texture resource=([0-9]+) slot=([0-9]+) childLevel=0 parentLevel=1 baseMip=0 levelCount=([1-9][0-9]*) bytes=([1-9][0-9]*) synchronous=0 fenceSignaled=1 heldFrames=([1-9][0-9]*) uploadFrames=([1-9][0-9]*) sampleAge=([0-9]+) fallback=parent source=decoded name=([^\r\n]+)")
hold_m=hold_re.fullmatch(mip_views[0][2]);start_m=start_re.fullmatch(mip_views[1][2]);promote_m=promote_re.fullmatch(mip_views[2][2])
if not hold_m or not start_m or not promote_m:raise SystemExit("FAIL ral-residency mip action order")
identity={(hold_m.group(1),hold_m.group(2),hold_m.group(5)),(start_m.group(1),start_m.group(2),start_m.group(6)),(promote_m.group(1),promote_m.group(2),promote_m.group(8))}
if len(identity)!=1 or start_m.group(3)!=promote_m.group(4):raise SystemExit("FAIL ral-residency mip identity/byte drift")
if int(promote_m.group(3))!=int(hold_m.group(3))+1 or int(hold_m.group(4))>1 or int(start_m.group(3))<4 or int(start_m.group(4))<20 or int(start_m.group(5))>1 or int(promote_m.group(5))<int(start_m.group(4)) or int(promote_m.group(6))<1 or int(promote_m.group(7))>1:raise SystemExit("FAIL ral-residency mip range/upload/continuity")
page_states=exact("RAL residency page state:",r"RAL residency page state: action=(?:hold class=texture resource=[0-9]+ level=0 x=0 y=0 planeMask=1 state=stale completedMask=0 parentReady=1 serial=[0-9]+ name=[^\r\n]+|upload-start class=texture resource=[0-9]+ level=0 x=0 y=0 planeMask=1 state=in-flight completedMask=0 parentReady=1 serial=[0-9]+ name=[^\r\n]+|upload-promote class=texture resource=[0-9]+ level=0 x=0 y=0 planeMask=1 state=resident completedMask=1 parentReady=1 serial=[0-9]+ name=[^\r\n]+)","WARN","renderer.ral.texture",3)
state_re=re.compile(r"RAL residency page state: action=(hold|upload-start|upload-promote) class=texture resource=([0-9]+) level=0 x=0 y=0 planeMask=1 state=(stale|in-flight|resident) completedMask=([01]) parentReady=1 serial=([0-9]+) name=([^\r\n]+)")
state_parsed=[state_re.fullmatch(item[2]).groups() for item in page_states]
if [item[0] for item in state_parsed] != ["hold","upload-start","upload-promote"] or [item[2] for item in state_parsed] != ["stale","in-flight","resident"]:raise SystemExit("FAIL ral-residency page lifecycle")
if len({(item[1],item[5]) for item in state_parsed})!=1 or state_parsed[0][1]!=hold_m.group(1) or state_parsed[0][5]!=hold_m.group(5):raise SystemExit("FAIL ral-residency page address drift")
serials=[int(item[4]) for item in state_parsed]
if any(a>b for a,b in zip(serials,serials[1:])):raise SystemExit("FAIL ral-residency page serial regression")
select=exact("RAL residency select:",r"RAL residency select: action=(?:evict|request) class=texture resource=[0-9]+ tier=(?:background|visible) age=[0-9]+ screenAge=[0-9]+ samples=[0-9]+ motion=0 explicit=0 cost=[0-9]+ total=[0-9]+ fallback=white","DEBUG","renderer.assets",4)
evict=[x for x in select if "action=evict" in x[2]]
request=[x for x in select if "action=request" in x[2]]
if len(evict)!=2 or len(request)!=2:raise SystemExit("FAIL ral-residency select action cardinality")
selection_re=re.compile(r"RAL residency select: action=(evict|request) class=texture resource=([0-9]+) tier=(background|visible) age=([0-9]+) screenAge=([0-9]+) samples=([0-9]+) motion=0 explicit=0 cost=([0-9]+) total=([0-9]+) fallback=white")
parsed=[]
for item in select:
 m=selection_re.fullmatch(item[2]);action,resource,tier,age,screen,samples,cost,total=m.groups()
 resource,age,screen,samples,cost,total=map(int,(resource,age,screen,samples,cost,total))
 if action=="evict":
  if tier!="background" or screen!=0 or samples!=0 or total!=0:raise SystemExit("FAIL ral-residency eviction components")
 else:
  expected=256*(30+min(age,120))//30
  if tier!="visible" or screen!=expected or samples!=64 or total!=max(0,screen+samples-cost):raise SystemExit("FAIL ral-residency request components")
 parsed.append((action,resource))
evict_ids=[resource for action,resource in parsed if action=="evict"]
request_ids=[resource for action,resource in parsed if action=="request"]
if len(set(evict_ids))!=2 or request_ids!=evict_ids:raise SystemExit(f"FAIL ral-residency resource replay {evict_ids}/{request_ids}")
details=exact("r_texEvictForce: evicting ",r"r_texEvictForce: evicting '[^'\r\n]+' \(frameUsed [0-9]+, age [0-9]+\)","DEBUG","renderer.assets",2)
evict_summary=exact("r_texEvictForce: evicted ",r"r_texEvictForce: evicted 2 image\(s\) \(free is frame-deferred; device-local ([0-9]+) MiB at enqueue, drops after the defer-destroy ring drains\)","WARN","renderer.assets")[0]
drain=exact("vk_ral_drain_reregisters:",r"vk_ral_drain_reregisters: auto-restreamed 2 sampled-evicted texture\(s\), ([0-9]+) KiB within 64-page/32-MiB frame budget","WARN","renderer.assets")[0]
restore=exact("r_texResidencyBudgetTest:",r"r_texResidencyBudgetTest: requested 2, restored 2, pending 0; device-local ([0-9]+) MiB \(post-evict/pre-restore\) -> ([0-9]+) MiB \(post-restore\); budget 64 pages/32 MiB","WARN","renderer.assets")[0]
rm=re.fullmatch(r"r_texResidencyBudgetTest: requested 2, restored 2, pending 0; device-local ([0-9]+) MiB \(post-evict/pre-restore\) -> ([0-9]+) MiB \(post-restore\); budget 64 pages/32 MiB",restore[2])
if int(rm.group(2))<=int(rm.group(1)):raise SystemExit("FAIL ral-residency memory restore")
complete=exact("Q0_RAL_RESIDENCY_",r"Q0_RAL_RESIDENCY_COMPLETE","INFO","system")[0]
shutdown=exact("----- Server Shutdown ",r"----- Server Shutdown \(Server quit\) -----","INFO","server")[0]
shutdown_game=exact("==== ShutdownGame ====",r"==== ShutdownGame ====","INFO","game")[0]
order=(first[0],material[0][0],material[1][0],material_planes[0][0],material_planes[1][0],material[2][0],mip_views[0][0],page_states[0][0],mip_views[1][0],page_states[1][0],mip_views[2][0],page_states[2][0],evict[0][0],details[0][0],evict[1][0],details[1][0],evict_summary[0],request[0][0],request[1][0],drain[0],restore[0],complete[0],shutdown[0],shutdown_game[0])
if any(a>=b for a,b in zip(order,order[1:])):raise SystemExit(f"FAIL ral-residency causal order {order}")
manifest=[]
for number,line in enumerate(open(manifest_path,encoding="utf-8",errors="strict"),1):
 try:item=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL ral-residency manifest JSON {number}: {exc}")
 if not isinstance(item,dict):raise SystemExit("FAIL ral-residency manifest schema")
 manifest.append(item)
scenario={"kind":"scenario","schema":3,"name":"ral-residency-runtime","map":"arena7","backend":"vulkan","parent_base_mip":1,"child_upload":"decoded-mip0-async-graphics","page_records":"persistent-per-mip","material_planes":"base+orm-atomic","evict_count":2,"fallback":"white"}
if not manifest or manifest[0]!=scenario:raise SystemExit("FAIL ral-residency scenario")
if [x.get("role") for x in manifest[1:-1]] != ["gui","renderer","moltenvk","pax21","base","harness","bootstrap-cfg"]:raise SystemExit("FAIL ral-residency provenance roles")
for item in manifest[1:-1]:
 if set(item)!={"kind","role","path","bytes","sha256"} or item.get("kind")!="provenance" or not isinstance(item.get("bytes"),int) or item["bytes"]<=0 or re.fullmatch(r"[0-9a-f]{64}",str(item.get("sha256",""))) is None:raise SystemExit("FAIL ral-residency provenance schema")
 data=open(item["path"],"rb").read()
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:raise SystemExit("FAIL ral-residency provenance rehash")
result=manifest[-1]
if set(result)!={"kind","controller_pid","rc","timeout","forced"} or result.get("kind")!="result" or not isinstance(result.get("controller_pid"),int) or result["controller_pid"]<=0 or result.get("rc")!=0 or result.get("timeout") is not False or result.get("forced") is not False:raise SystemExit("FAIL ral-residency result")
print("PASS exact RAL async decoded child-mip upload after parent hold + eviction/re-register policy roundtrip")
PYEOF
}

write_self() {
python3 - "$1" "$2" "$3" <<'PYEOF'
import hashlib,json,os,sys
log_path,manifest_path,mode=sys.argv[1:]
def row(sev,cat,msg):return {"ts":"2026-08-13T12:00:00.000+03:00","sev":sev,"cat":cat,"msg":msg+"\n"}
R=[row("INFO","client","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=1 numEntities=2 framecount=3)"),row("WARN","renderer.ral.texture","RAL residency mip test: action=hold class=texture resource=210 slot=210 childLevel=0 parentLevel=1 baseMip=1 levelCount=8 sampleAge=0 fallback=parent name=textures/base_wall/concrete.tga"),row("WARN","renderer.ral.texture","RAL residency page state: action=hold class=texture resource=210 level=0 x=0 y=0 planeMask=1 state=stale completedMask=0 parentReady=1 serial=100 name=textures/base_wall/concrete.tga"),row("WARN","renderer.ral.texture","RAL residency mip test: action=upload-start class=texture resource=210 slot=210 childLevel=0 parentLevel=1 baseMip=1 bytes=65536 synchronous=0 parentBound=1 heldFrames=30 sampleAge=0 fallback=parent source=decoded name=textures/base_wall/concrete.tga"),row("WARN","renderer.ral.texture","RAL residency page state: action=upload-start class=texture resource=210 level=0 x=0 y=0 planeMask=1 state=in-flight completedMask=0 parentReady=1 serial=130 name=textures/base_wall/concrete.tga"),row("WARN","renderer.ral.texture","RAL residency mip test: action=upload-promote class=texture resource=210 slot=210 childLevel=0 parentLevel=1 baseMip=0 levelCount=9 bytes=65536 synchronous=0 fenceSignaled=1 heldFrames=31 uploadFrames=1 sampleAge=0 fallback=parent source=decoded name=textures/base_wall/concrete.tga"),row("WARN","renderer.ral.texture","RAL residency page state: action=upload-promote class=texture resource=210 level=0 x=0 y=0 planeMask=1 state=resident completedMask=1 parentReady=1 serial=131 name=textures/base_wall/concrete.tga"),row("DEBUG","renderer.assets","RAL residency select: action=evict class=texture resource=241 tier=background age=45 screenAge=0 samples=0 motion=0 explicit=0 cost=1 total=0 fallback=white"),row("DEBUG","renderer.assets","r_texEvictForce: evicting 'a.tga' (frameUsed 0, age 45)"),row("DEBUG","renderer.assets","RAL residency select: action=evict class=texture resource=242 tier=background age=45 screenAge=0 samples=0 motion=0 explicit=0 cost=2 total=0 fallback=white"),row("DEBUG","renderer.assets","r_texEvictForce: evicting 'b.tga' (frameUsed 0, age 45)"),row("WARN","renderer.assets","r_texEvictForce: evicted 2 image(s) (free is frame-deferred; device-local 764 MiB at enqueue, drops after the defer-destroy ring drains)"),row("DEBUG","renderer.assets","RAL residency select: action=request class=texture resource=241 tier=visible age=75 screenAge=896 samples=64 motion=0 explicit=0 cost=1 total=959 fallback=white"),row("DEBUG","renderer.assets","RAL residency select: action=request class=texture resource=242 tier=visible age=75 screenAge=896 samples=64 motion=0 explicit=0 cost=2 total=958 fallback=white"),row("WARN","renderer.assets","vk_ral_drain_reregisters: auto-restreamed 2 sampled-evicted texture(s), 192 KiB within 64-page/32-MiB frame budget"),row("WARN","renderer.assets","r_texResidencyBudgetTest: requested 2, restored 2, pending 0; device-local 753 MiB (post-evict/pre-restore) -> 772 MiB (post-restore); budget 64 pages/32 MiB"),row("INFO","system","Q0_RAL_RESIDENCY_COMPLETE"),row("INFO","server","----- Server Shutdown (Server quit) -----"),row("INFO","game","==== ShutdownGame ====")]
R[1:1]=[row("WARN","renderer.ral.texture","RAL residency material: action=hold material=201 level=0 planeMask=5 state=stale completedMask=0 baseResource=201 baseSlot=201 ormResource=202 ormSlot=202 baseMip=1 atomic=1"),row("WARN","renderer.ral.texture","RAL residency material: action=upload-start material=201 level=0 planeMask=5 state=in-flight completedMask=0 parentBound=1 heldFrames=30 atomic=1"),row("WARN","renderer.ral.texture","RAL residency material plane: action=complete material=201 plane=base bit=1 resource=201 slot=201 completedMask=1"),row("WARN","renderer.ral.texture","RAL residency material plane: action=complete material=201 plane=orm bit=4 resource=202 slot=202 completedMask=5"),row("WARN","renderer.ral.texture","RAL residency material: action=promote material=201 level=0 planeMask=5 state=resident completedMask=5 baseSlot=201 ormSlot=202 heldFrames=31 atomic=1")]
def find(text,n=0):return [i for i,x in enumerate(R) if text in x["msg"]][n]
if mode=="missing":R.pop(find("action=request",1))
elif mode=="material-missing":R.pop(find("RAL residency material: action=upload-start"))
elif mode=="material-partial":R[find("RAL residency material: action=promote")]["msg"]=R[find("RAL residency material: action=promote")]["msg"].replace("completedMask=5","completedMask=1")
elif mode=="material-nonatomic":R[find("RAL residency material: action=promote")]["msg"]=R[find("RAL residency material: action=promote")]["msg"].replace("atomic=1","atomic=0")
elif mode=="material-slot":R[find("RAL residency material: action=promote")]["msg"]=R[find("RAL residency material: action=promote")]["msg"].replace("ormSlot=202","ormSlot=203")
elif mode=="material-plane-missing":R.pop(find("RAL residency material plane: action=complete",1))
elif mode=="material-plane-bit":R[find("plane=orm bit=4")]["msg"]=R[find("plane=orm bit=4")]["msg"].replace("bit=4","bit=1")
elif mode=="material-plane-order":a=find("RAL residency material plane: action=complete",1);b=find("RAL residency material: action=promote");R[a],R[b]=R[b],R[a]
elif mode=="mip-missing":R.pop(find("RAL residency mip test: action=hold"))
elif mode=="mip-resource":R[find("RAL residency mip test: action=upload-promote")]["msg"]=R[find("RAL residency mip test: action=upload-promote")]["msg"].replace("resource=210","resource=211")
elif mode=="mip-range":R[find("RAL residency mip test: action=upload-promote")]["msg"]=R[find("RAL residency mip test: action=upload-promote")]["msg"].replace("levelCount=9","levelCount=8")
elif mode=="mip-stale":R[find("RAL residency mip test: action=hold")]["msg"]=R[find("RAL residency mip test: action=hold")]["msg"].replace("sampleAge=0","sampleAge=2")
elif mode=="mip-short":R[find("RAL residency mip test: action=upload-start")]["msg"]=R[find("RAL residency mip test: action=upload-start")]["msg"].replace("heldFrames=30","heldFrames=10")
elif mode=="mip-promote-stale":R[find("RAL residency mip test: action=upload-promote")]["msg"]=R[find("RAL residency mip test: action=upload-promote")]["msg"].replace("sampleAge=0","sampleAge=2")
elif mode=="mip-upload-missing":R.pop(find("RAL residency mip test: action=upload-start"))
elif mode=="mip-promote-missing":R.pop(find("RAL residency mip test: action=upload-promote"))
elif mode=="mip-upload-bytes":R[find("RAL residency mip test: action=upload-promote")]["msg"]=R[find("RAL residency mip test: action=upload-promote")]["msg"].replace("bytes=65536","bytes=32768")
elif mode=="mip-upload-sync":R[find("RAL residency mip test: action=upload-start")]["msg"]=R[find("RAL residency mip test: action=upload-start")]["msg"].replace("synchronous=0","synchronous=1")
elif mode=="mip-upload-fence":R[find("RAL residency mip test: action=upload-promote")]["msg"]=R[find("RAL residency mip test: action=upload-promote")]["msg"].replace("fenceSignaled=1","fenceSignaled=0")
elif mode=="mip-upload-source":R[find("RAL residency mip test: action=upload-start")]["msg"]=R[find("RAL residency mip test: action=upload-start")]["msg"].replace("source=decoded","source=resident")
elif mode=="mip-parent-unbound":R[find("RAL residency mip test: action=upload-start")]["msg"]=R[find("RAL residency mip test: action=upload-start")]["msg"].replace("parentBound=1","parentBound=0")
elif mode=="mip-upload-zero-frames":R[find("RAL residency mip test: action=upload-promote")]["msg"]=R[find("RAL residency mip test: action=upload-promote")]["msg"].replace("uploadFrames=1","uploadFrames=0")
elif mode=="mip-refused":R[find("RAL residency mip test: action=hold")]["msg"]="RAL residency mip test: hold refused (no live mipmapped texture)\n"
elif mode=="page-missing":R.pop(find("RAL residency page state: action=upload-start"))
elif mode=="page-address":R[find("RAL residency page state: action=upload-promote")]["msg"]=R[find("RAL residency page state: action=upload-promote")]["msg"].replace("resource=210","resource=211")
elif mode=="page-state":R[find("RAL residency page state: action=upload-start")]["msg"]=R[find("RAL residency page state: action=upload-start")]["msg"].replace("state=in-flight","state=resident")
elif mode=="page-mask":R[find("RAL residency page state: action=upload-promote")]["msg"]=R[find("RAL residency page state: action=upload-promote")]["msg"].replace("completedMask=1","completedMask=0")
elif mode=="page-serial":R[find("RAL residency page state: action=upload-promote")]["msg"]=R[find("RAL residency page state: action=upload-promote")]["msg"].replace("serial=131","serial=99")
elif mode=="extra":R.insert(find("action=request"),dict(R[find("action=request")]))
elif mode=="order":a=find("action=request");b=find("r_texEvictForce: evicted");R[a],R[b]=R[b],R[a]
elif mode=="resource":R[find("action=request",1)]["msg"]=R[find("action=request",1)]["msg"].replace("resource=242","resource=999")
elif mode=="score":R[find("action=request")]["msg"]=R[find("action=request")]["msg"].replace("total=959","total=960")
elif mode=="tier":R[find("action=evict")]["msg"]=R[find("action=evict")]["msg"].replace("tier=background","tier=visible")
elif mode=="fallback":R[find("action=evict")]["msg"]=R[find("action=evict")]["msg"].replace("fallback=white","fallback=none")
elif mode=="metadata":R[find("action=request")]["cat"]="renderer.ral"
elif mode=="suffix":R[find("Q0_RAL_RESIDENCY_COMPLETE")]["msg"]="Q0_RAL_RESIDENCY_COMPLETE extra\n"
elif mode=="smuggle":R.append(row("INFO","system","benign\rRAL residency select: action=evict class=texture resource=1 tier=background age=1 screenAge=0 samples=0 motion=0 explicit=0 cost=0 total=0 fallback=white"))
elif mode=="count":R[find("r_texEvictForce: evicted")]["msg"]=R[find("r_texEvictForce: evicted")]["msg"].replace("evicted 2","evicted 1")
elif mode=="memory":R[find("r_texResidencyBudgetTest:")]["msg"]=R[find("r_texResidencyBudgetTest:")]["msg"].replace("753 MiB (post-evict/pre-restore) -> 772","753 MiB (post-evict/pre-restore) -> 753")
elif mode=="error":R.append(row("ERROR","renderer.assets","synthetic"))
with open(log_path,"w") as out:
 for item in R:out.write(json.dumps(item)+"\n")
roles=("gui","renderer","moltenvk","pax21","base","harness","bootstrap-cfg")
M=[{"kind":"scenario","schema":3,"name":"ral-residency-runtime","map":"arena7","backend":"vulkan","parent_base_mip":1,"child_upload":"decoded-mip0-async-graphics","page_records":"persistent-per-mip","material_planes":"base+orm-atomic","evict_count":2,"fallback":"white"}]
for role in roles:
 path=manifest_path+"."+role;data=("fixture-"+role).encode();open(path,"wb").write(data);M.append({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
M.append({"kind":"result","controller_pid":101,"rc":0,"timeout":False,"forced":False})
if mode=="manifest":M[2]["sha256"]="0"*64
elif mode=="result":M[-1]["rc"]=1
with open(manifest_path,"w") as out:
 for item in M:out.write(json.dumps(item,sort_keys=True)+"\n")
PYEOF
}

if [ "${1:-}" = --analyze ];then
 [ "$#" -eq 3 ] || { echo "usage: $0 --analyze <qconsole.jsonl> <manifest.jsonl>";exit 64; }
 analyze_contract "$2" "$3";exit $?
fi
if [ "${1:-}" = --self-test ];then
 ROOT="$(mktemp -d -t ral-residency-self-XXXXXX 2>/dev/null || mktemp -d)";trap 'rm -rf "$ROOT"' EXIT
 write_self "$ROOT/clean.log" "$ROOT/clean.manifest" clean || exit 1
 analyze_contract "$ROOT/clean.log" "$ROOT/clean.manifest" >/dev/null || exit 1
 defects=(missing material-missing material-partial material-nonatomic material-slot material-plane-missing material-plane-bit material-plane-order mip-missing mip-resource mip-range mip-stale mip-short mip-promote-stale mip-upload-missing mip-promote-missing mip-upload-bytes mip-upload-sync mip-upload-fence mip-upload-source mip-parent-unbound mip-upload-zero-frames mip-refused page-missing page-address page-state page-mask page-serial extra order resource score tier fallback metadata suffix smuggle count memory error manifest result)
 for defect in "${defects[@]}";do
  write_self "$ROOT/$defect.log" "$ROOT/$defect.manifest" "$defect" || exit 1
  if analyze_contract "$ROOT/$defect.log" "$ROOT/$defect.manifest" >/dev/null 2>&1;then echo "FAIL accepted $defect";exit 1;fi
 done
 echo "PASS ral-residency-runtime analyzer self-test (${#defects[@]} mutations)";exit 0
fi

WIRED="${1:-}";[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired";exit 64; }
[ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: missing timeout runner";exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")";WD="$(dirname "$WIRED")"
find_required(){ local name="$1" candidate;shift;for candidate in "$@";do [ -f "$candidate/$name" ] && { printf '%s\n' "$candidate/$name";return 0;};done;return 1;}
RENDERER="$(find_required wired_vulkan_arm64.dylib "$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS")" || { echo "SKIP: current Vulkan renderer unavailable";exit 77; }
MOLTEN="$(find_required libMoltenVK.dylib "$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS")" || { echo "SKIP: current MoltenVK unavailable";exit 77; }
PACK="";for candidate in "$WD" "$WD/../Resources" "$WD/../../.." "$WD/q3now-preview.arm64.app/Contents/Resources";do [ -f "$candidate/base/pax21.sw3z" ] && PACK="$(cd "$candidate" && pwd)" && break;done;[ -n "$PACK" ] || { echo "SKIP: current pax21 unavailable";exit 77; }
CONTENT="${WIRED_CONTENT_ROOT:-$PACK}";if [ -f "$CONTENT/base/pax01.sw3z" ];then BASE="$CONTENT/base/pax01.sw3z";elif [ -f "$CONTENT/base/pak0.pk3" ];then BASE="$CONTENT/base/pak0.pk3";else echo "SKIP: set WIRED_CONTENT_ROOT";exit 77;fi
ROOT="$(mktemp -d -t ral-residency-runtime-XXXXXX 2>/dev/null || mktemp -d)";HOME_DIR="$ROOT/home";RUN="$ROOT/runtime";FORCED=0
cleanup(){ local status=$?;trap - EXIT INT TERM;[ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT";exit "$status";};trap cleanup EXIT;trap 'exit 130' INT;trap 'exit 143' TERM
mkdir -p "$HOME_DIR/base" "$RUN/Contents/MacOS";cp "$PACK/base/pax21.sw3z" "$HOME_DIR/base/" || exit 1;cp "$BASE" "$HOME_DIR/base/" || exit 1;cp "$WIRED" "$RUN/wired" || exit 1;chmod +x "$RUN/wired";cp "$RENDERER" "$MOLTEN" "$RUN/Contents/MacOS/" || exit 1
# Anything the binary resolves through @executable_path must sit beside the copy
# we just made, NOT in Contents/MacOS: the renderer and MoltenVK are dlopen'd on
# a relative search path, but hard-linked dependencies are looked up by dyld
# against the executable's own directory. Ask the binary which ones those are
# instead of hardcoding a list — a shipped bundle links libSDL3/libcrypto, a
# plain devel build may link neither, and guessing gets it wrong in both
# directions. Missing files are left alone; the run then fails loudly on its own.
if command -v otool >/dev/null 2>&1;then
	otool -L "$WIRED" 2>/dev/null | sed -n 's|^[[:space:]]*@executable_path/\([^ ]*\).*|\1|p' | while read -r dep;do
		[ -n "$dep" ] || continue
		for candidate in "$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS";do
			[ -f "$candidate/$dep" ] && { cp "$candidate/$dep" "$RUN/";break; }
		done
	done
fi

BOOT="$HOME_DIR/base/ral-residency-runtime.cfg";printf '%s\n' 'log renderer.assets debug' 'set activeAction "r_texResidencyMaterialTest hold ; wait 30 ; r_texResidencyMaterialTest upload ; wait 30 ; r_texResidencyMipTest hold ; wait 30 ; r_texResidencyMipTest upload ; wait 30 ; r_texEvictForce 2 ; wait 30 ; r_texResidencyBudgetTest ; wait 30 ; echo Q0_RAL_RESIDENCY_COMPLETE ; quit"' 'map arena7' >"$BOOT"
MANIFEST="$ROOT/manifest.jsonl";python3 - "$MANIFEST" "$WIRED" "$RENDERER" "$MOLTEN" "$PACK/base/pax21.sw3z" "$BASE" "$0" "$BOOT" <<'PYEOF'
import hashlib,json,os,sys
with open(sys.argv[1],"w") as out:
 out.write(json.dumps({"kind":"scenario","schema":3,"name":"ral-residency-runtime","map":"arena7","backend":"vulkan","parent_base_mip":1,"child_upload":"decoded-mip0-async-graphics","page_records":"persistent-per-mip","material_planes":"base+orm-atomic","evict_count":2,"fallback":"white"},sort_keys=True)+"\n")
 for role,path in zip(("gui","renderer","moltenvk","pax21","base","harness","bootstrap-cfg"),sys.argv[2:]):
  data=open(path,"rb").read();out.write(json.dumps({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()},sort_keys=True)+"\n")
PYEOF
QCONSOLE="$HOME_DIR/qconsole.jsonl";STDOUT="$ROOT/stdout.log"
python3 "$TIMEOUT_RUNNER" --timeout 90 --kill-after 15 --cwd "$RUN" --stdout "$STDOUT" -- "$RUN/wired" +set fs_basepath "$HOME_DIR" +set fs_homepath "$HOME_DIR" +set fs_game base +set sv_pure 0 +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 +set r_fullscreen 0 +set r_mode 3 +set r_vkValidate 1 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite +exec ral-residency-runtime.cfg
RC=$?;TIMEOUT=false;[ "$RC" -eq 124 ] && TIMEOUT=true;python3 - "$MANIFEST" "$$" "$RC" "$TIMEOUT" "$FORCED" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a") as out:out.write(json.dumps({"kind":"result","controller_pid":int(sys.argv[2]),"rc":int(sys.argv[3]),"timeout":sys.argv[4]=="true","forced":sys.argv[5]=="1"},sort_keys=True)+"\n")
PYEOF
[ -f "$QCONSOLE" ] || { echo "FAIL missing qconsole: $ROOT";exit 1; };analyze_contract "$QCONSOLE" "$MANIFEST" || { echo "FAIL retained root: $ROOT";exit 1; }
echo "PASS retained root: $ROOT"
