#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Default-off Phase 7.10 native-gamecl temporal continuity process contract.

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
IQM_FIXTURE="$SCRIPT_DIR/make-temporal-iqm-fixture.py"
BOOT_CONTENT=$'set r_fullscreen 0\nset r_mode -1\nset r_customwidth 1280\nset r_customheight 720\nlog cgame info\nlog renderer.assets debug\nlog renderer.temporal info\nlog renderer.ral info\nset vm_game 0\nset vm_cgame 0\nset char h5a_fixture\nset skin default\nset sv_cheats 1\nset sv_pure 0\nset g_spawnProtect 0\nset activeAction "cg_thirdPerson 1; cg_thirdPersonAlpha 255; cg_thirdPersonRange 100; sv_fps 20; timescale 0.025; fixedtime 1; cl_run 1; print sv_fps; print timescale; print fixedtime; print cl_run; print g_spawnProtect; wait 512; noclip; wait 64; echo Q3_RAL_TEMPORAL_REQUESTED; r_temporalInputTest 1; wait 1; wait 1; ral_dump live temporal; ral_dump live temporal-motion-arm; ral_dump live temporal-history-arm; ral_dump live temporal-resolve-arm; wait 1; ral_dump live temporal; wait 1; ral_dump live temporal; wait 1; wait 1; wait 1; wait 1; wait 1; wait 1; echo Q3_RAL_TEMPORAL_ENABLED; r_temporalInputTest 0; wait 1; ral_dump live temporal; echo Q3_RAL_TEMPORAL_DISABLED; fixedtime 0; timescale 1; print fixedtime; print timescale; echo Q3_RAL_TEMPORAL_COMPLETE; quit"\nmap arena7\n'

analyze_contract() {
python3 - "$1" "$2" "$BOOT_CONTENT" <<'PYEOF'
import hashlib,json,math,re,struct,sys
log_path,manifest_path,boot_expected=sys.argv[1:]
rows=[]
for number,line in enumerate(open(log_path,encoding="utf-8",errors="strict"),1):
 if not line.strip():continue
 try: row=json.loads(line)
 except Exception as exc: raise SystemExit(f"FAIL temporal JSON {number}: {exc}")
 if not isinstance(row,dict) or not all(isinstance(row.get(k),str) for k in ("ts","sev","cat","msg")):raise SystemExit("FAIL temporal log schema")
 rows.append(row)
if not rows:raise SystemExit("FAIL temporal empty evidence")
def norm(v):return v[:-1] if v.endswith("\n") and not v.endswith("\n\n") else v
vals=[norm(r["msg"]) for r in rows]
claimed=("window-extent ","temporal-projection ","temporal-continuity ","temporal-entity-","temporal-main-activation ","temporal-iqm-activation ","temporal-iqm-payload ","temporal-resolve-sample ","temporal-motion-content ","temporal-history-consume ","temporal-history-content ","temporal-history-feedback ","temporal-resolve-readback ","temporal-resolve-content ","temporal-resolved-hdr ","VM_Create policy module=gamecl ","Q3_RAL_TEMPORAL_","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME","CG_LoadCharacter: loaded profile=h5a_fixture ","IQM GPU skinning VBO: 3 verts, 2 tris (characters/h5a_fixture/models/body.iqm)",'"sv_fps" is:','"timescale" is:','"fixedtime" is:','"cl_run" is:','"g_spawnProtect" is:')
for value in vals:
 if len(re.split(r"[\r\n]",value))>1 and any(part.startswith(claimed) for part in re.split(r"[\r\n]",value)):raise SystemExit("FAIL temporal logical-line smuggling")
if any(r["sev"].upper() in ("ERROR","FATAL") for r in rows):raise SystemExit("FAIL temporal severity")
if any("VUID-" in v for v in vals):raise SystemExit("FAIL temporal VUID")
def exact(prefix,pattern,sev,cat,count=1):
 found=[(i,r,v) for i,(r,v) in enumerate(zip(rows,vals)) if v.startswith(prefix)]
 if len(found)!=count:raise SystemExit(f"FAIL temporal {prefix} cardinality {len(found)}")
 for _,row,value in found:
  if row["sev"].upper()!=sev or row["cat"].lower()!=cat or re.fullmatch(pattern,value) is None:raise SystemExit(f"FAIL temporal {prefix} body/metadata: {value}")
 return found
window=exact("window-extent ",r"window-extent schema=2 requested=1280x720 logical=1280x720 pixels=([1-9][0-9]*)x([1-9][0-9]*) exact16x9=1 publish-ready=1","INFO","client")[0]
wm=re.fullmatch(r"window-extent schema=2 requested=1280x720 logical=1280x720 pixels=([1-9][0-9]*)x([1-9][0-9]*) exact16x9=1 publish-ready=1",window[2])
pixel_w,pixel_h=map(int,wm.groups())
if pixel_w<1280 or pixel_h<720 or pixel_w*9!=pixel_h*16:raise SystemExit("FAIL temporal actual window is not widescreen 16:9 at or above 1280x720")
policy=exact("VM_Create policy module=gamecl ",r"VM_Create policy module=gamecl requested=0 effective=0 backend=native","DEBUG","system")[0]
gamesv_load=exact("Sys_LoadLibrary(gamesvarm64.dylib): ",r"Sys_LoadLibrary\(gamesvarm64\.dylib\): loaded","INFO","filesystem")[0]
gamecl_load=exact("Sys_LoadLibrary(gameclarm64.dylib): ",r"Sys_LoadLibrary\(gameclarm64\.dylib\): loaded","INFO","filesystem")[0]
gamecl_vm=exact("VM_LoadDll(gamecl): ",r"VM_LoadDll\(gamecl\): loaded, vmMain @ 0x[0-9a-fA-F]+","INFO","system")[0]
fixture_character=exact("CG_LoadCharacter: loaded profile=h5a_fixture ",r"CG_LoadCharacter: loaded profile=h5a_fixture parts=1 legs=[1-9][0-9]* torso=[1-9][0-9]* head=[1-9][0-9]* icon=0 skin=0","DEBUG","cgame")[0]
fixture_animations=exact("IQM h5a_fixture: mapped ",r"IQM h5a_fixture: mapped 36/36 animations from embedded data","INFO","cgame")[0]
fixture_vbo=exact("IQM GPU skinning VBO: ",r"IQM GPU skinning VBO: 3 verts, 2 tris \(characters/h5a_fixture/models/body\.iqm\)","DEBUG","renderer.assets")[0]
cap_pattern=r"temporal-entity-capability glconfig-generation=([0-9]+) key=trap_R_AddRefEntityToSceneTemporal expected=232 discovered=232 route=native-syscall export=1"
cap=exact("temporal-entity-capability ",cap_pattern,"INFO","cgame")[0]
slot_pat=r"temporal-entity-slot glconfig-generation=([0-9]+) trap=232 owner=([0-9]+) entity-generation=([1-9][0-9]*) role=([1-9][0-9]*) accepted=1 export=1"
slots=[]
for i,(row,value) in enumerate(zip(rows,vals)):
 if value.startswith("temporal-entity-slot "):
  m=re.fullmatch(slot_pat,value)
  if row["sev"].upper()!="INFO" or row["cat"].lower()!="cgame" or not m:raise SystemExit("FAIL temporal slot232 metadata/body")
  slots.append((i,tuple(map(int,m.groups()))))
if not slots:raise SystemExit("FAIL temporal no validated slot232 ingress")
cap_gen=int(re.fullmatch(cap_pattern,cap[2]).group(1))
if any(s[1][0]!=cap_gen for s in slots):raise SystemExit("FAIL temporal capability/slot generation mismatch")
first=exact("cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME",r"cls\.state: -> CA_ACTIVE \(FIRST GAMEPLAY FRAME mapname=maps/arena7\.bsp serverTime=[0-9]+ numEntities=[0-9]+ framecount=[0-9]+\)","INFO","client")[0]
markers=exact("Q3_RAL_TEMPORAL_",r"Q3_RAL_TEMPORAL_(?:REQUESTED|ENABLED|DISABLED|COMPLETE)","INFO","system",4)
if [x[2] for x in markers] != ["Q3_RAL_TEMPORAL_REQUESTED","Q3_RAL_TEMPORAL_ENABLED","Q3_RAL_TEMPORAL_DISABLED","Q3_RAL_TEMPORAL_COMPLETE"]:raise SystemExit("FAIL temporal marker vector")
def cvar_receipts(name,values_expected,default_expected):
 found=[];pattern=re.compile(r'^"'+re.escape(name)+r'" is:"([^"\r\n]+)"(?: default:"([^"\r\n]+)")?$')
 for i,(row,value) in enumerate(zip(rows,vals)):
  if not value.startswith(f'"{name}" is:'):continue
  match=pattern.fullmatch(value)
  if row["sev"].upper()!="INFO" or row["cat"].lower()!="system" or not match:raise SystemExit(f"FAIL temporal {name} receipt metadata/body")
  current=re.sub(r"\^[0-9]$","",match.group(1));default=re.sub(r"\^[0-9]$","",match.group(2) or default_expected)
  if default!=default_expected:raise SystemExit(f"FAIL temporal {name} default receipt")
  found.append((i,current))
 if [value for _,value in found]!=values_expected:raise SystemExit(f"FAIL temporal {name} receipt vector")
 return found
timescale_receipts=cvar_receipts("timescale",["0.025","1"],"1")
fixedtime_receipts=cvar_receipts("fixedtime",["1","0"],"0")
clrun_receipts=cvar_receipts("cl_run",["1"],"1")
svfps_receipts=cvar_receipts("sv_fps",["20"],"20")
spawn_receipts=cvar_receipts("g_spawnProtect",["0"],"2")
if not svfps_receipts[0][0]<timescale_receipts[0][0]<fixedtime_receipts[0][0]<clrun_receipts[0][0]<spawn_receipts[0][0]<markers[0][0]:raise SystemExit("FAIL temporal synthetic timing setup order")
if not markers[2][0]<fixedtime_receipts[1][0]<timescale_receipts[1][0]<markers[3][0]:raise SystemExit("FAIL temporal synthetic timing cleanup order")
rebuilds=exact("scene-depth live rebuild ",r"scene-depth live rebuild active=([01]) deferred-drained=1 attachments-rebound=1 temporal-store-reset=1","INFO","renderer.ral",2)
if [re.search(r"active=([01])",x[2]).group(1) for x in rebuilds] != ["1","0"]:raise SystemExit("FAIL temporal rebuild vector")
num=r"-?[0-9]+\.[0-9]+"
proj_pat=(r"temporal-projection world=([0-3]) frame=([1-9][0-9]*) enabled=([01]) queued=1 recorded=([01]) previous-read=([01]) committed=1 camera-valid=([01]) camera-previous=([01]) entities=([0-9]+)/([0-9]+) previous=([0-9]+) rejected=([0-9]+) entity-commit=([01]) generation=([1-9][0-9]*) reset=0x([0-9a-f]+) history-valid=([01]) read=([01]) write=([01]) phase=([0-7]) extent=([1-9][0-9]*)x([1-9][0-9]*) resources=(ready|none) resource-generation=([1-9][0-9]*) color-slots=([02]) depth-slots=([02]) jitter-px=("+num+r"),("+num+r") jitter-uv=("+num+r"),("+num+r") ui-jitter=("+num+r"),("+num+r") ndc=("+num+r"),("+num+r") projection=("+num+r"),("+num+r")->("+num+r"),("+num+r")")
projections=exact("temporal-projection ",proj_pat,"INFO","renderer.temporal",4)
pm=[re.fullmatch(proj_pat,x[2]) for x in projections]
if any(x is None for x in pm):raise SystemExit("FAIL temporal projection parse")
target_extents=[(int(x.group(19)),int(x.group(20))) for x in pm]
if any(x!=target_extents[0] for x in target_extents):raise SystemExit("FAIL temporal render-target extent drift")
if target_extents[0][0]<1280 or target_extents[0][1]<720 or target_extents[0][0]*9!=target_extents[0][1]*16:raise SystemExit("FAIL temporal render target is not widescreen 16:9 at or above 1280x720")
enabled_pm=pm[:3]; disabled_pm=pm[3]
frames=[int(x.group(2)) for x in enabled_pm]
if any(int(x.group(1))!=0 for x in pm):raise SystemExit("FAIL temporal world binding")
if frames[1]!=frames[0]+1 or frames[2]!=frames[1]+1:raise SystemExit("FAIL temporal dumps not adjacent")
for n,m in enumerate(enabled_pm):
 vals_i=list(map(int,m.groups()[:13]))
 if vals_i[2]!=1 or vals_i[3]!=1 or vals_i[4]!=1 or vals_i[5]!=1 or vals_i[7]<=0 or vals_i[8]<=0 or vals_i[10]!=0 or vals_i[11]!=1 or int(m.group(15))!=1:raise SystemExit("FAIL temporal enabled projection receipt")
 if int(m.group(14),16)!=0:raise SystemExit("FAIL temporal post-seed reset drift")
 if not markers[0][0]<projections[n][0]<markers[1][0]:raise SystemExit("FAIL temporal enabled projection window")
if int(disabled_pm.group(3))!=0 or int(disabled_pm.group(4))!=0:raise SystemExit("FAIL temporal disabled projection")
bits352=r"[0-9a-f]{352}";bits184=r"[0-9a-f]{184}"
cont_pat=(r"temporal-continuity schema=1 world=([0-3]) frame=([1-9][0-9]*) committed=([01]) camera-valid=([01]) camera-previous=([01]) camera-previous-frame=([0-9]+) camera-current=("+bits352+r") camera-previous-fields=("+bits352+r") entity-valid=([01]) entity-owner=([0-9]+) entity-generation=([0-9]+) entity-role=([0-9]+) entity-previous=([01]) entity-previous-frame=([0-9]+) entity-current=("+bits184+r") entity-previous-fields=("+bits184+r") entity-committed=([01]) attempts=([0-9]+) scans=([0-9]+) drawsurfs=([0-9]+) visible-temporal=([0-9]+) accepted=([0-9]+) rejected=([0-9]+)")
continuity=exact("temporal-continuity ",cont_pat,"INFO","renderer.temporal",4)
cm=[re.fullmatch(cont_pat,x[2]) for x in continuity]
if [int(x.group(2)) for x in cm] != [int(x.group(2)) for x in pm]:raise SystemExit("FAIL temporal projection/continuity frame bind")
slot_tuples={(owner,generation,role) for _,(_,owner,generation,role) in slots}
for n,m in enumerate(cm[:3]):
 frame=int(m.group(2));owner,generation,role=map(int,(m.group(10),m.group(11),m.group(12)))
 if int(m.group(1))!=0:raise SystemExit("FAIL temporal continuity world")
 if int(m.group(3))!=1 or int(m.group(4))!=1 or int(m.group(9))!=1 or int(m.group(17))!=1:raise SystemExit("FAIL temporal continuity commit/valid")
 if int(m.group(18))<1 or int(m.group(19))!=1 or int(m.group(20))<1 or int(m.group(21))<1 or int(m.group(22))<1 or int(m.group(23))!=0:raise SystemExit("FAIL temporal continuity attempt/scan counts")
 if int(m.group(22))>int(m.group(21)) or int(m.group(21))>int(m.group(20)):raise SystemExit("FAIL temporal continuity count relation")
 projection=enabled_pm[n]
 if (int(projection.group(8)),int(projection.group(9)),int(projection.group(11)))!=(int(m.group(22)),int(m.group(21)),int(m.group(23))):raise SystemExit("FAIL temporal projection/continuity counts")
 if (owner,generation,role) not in slot_tuples:raise SystemExit("FAIL temporal sampled entity not bound to slot232 ingress")
 if n==0:
  if int(m.group(5))!=1 or int(m.group(6))!=frame-1 or int(m.group(13))!=1 or int(m.group(14))!=frame-1 or set(m.group(8))=={"0"} or set(m.group(16))=={"0"}:raise SystemExit("FAIL temporal priming predecessor")
 else:
  prior=cm[n-1]
  if int(m.group(5))!=1 or int(m.group(6))!=frame-1 or m.group(8)!=prior.group(7):raise SystemExit("FAIL temporal camera prior/previous")
  if int(m.group(13))!=1 or int(m.group(14))!=frame-1 or m.group(16)!=prior.group(15):raise SystemExit("FAIL temporal entity prior/previous")
  if (m.group(10),m.group(11),m.group(12))!=(prior.group(10),prior.group(11),prior.group(12)):raise SystemExit("FAIL temporal sampled tuple drift")
if len({m.group(7) for m in cm[:3]})!=1:raise SystemExit("FAIL temporal stationary camera witness")
owner0,generation0,role0=map(int,(cm[0].group(10),cm[0].group(11),cm[0].group(12)))
if owner0!=0 or role0 not in (3,4,5,6):raise SystemExit("FAIL temporal witness is not local-player body part")
entity0=cm[0].group(15)
if int(entity0[0:8],16)==0 or int(entity0[40:48],16)==0 or int(entity0[48:56],16)==0:raise SystemExit("FAIL temporal witness model identity/topology")
matching_slot_indices=[i for i,(_,owner,generation,role) in slots if (owner,generation,role)==(owner0,generation0,role0)]
if not matching_slot_indices:raise SystemExit("FAIL temporal sampled witness has no exact slot232 ingress")
arm=exact("temporal-motion-readback ",r"temporal-motion-readback schema=1 action=armed captures=2","INFO","renderer.ral")[0]
history_arm=exact("temporal-history-consume ",r"temporal-history-consume schema=1 action=armed captures=2","INFO","renderer.ral")[0]
submit_pat=(r"temporal-main-activation schema=1 token=([1-9][0-9]*) frame=([1-9][0-9]*) world=([0-3]) extent=([1-9][0-9]*)x([1-9][0-9]*) topology=([1-9][0-9]*) plan=([1-9][0-9]*) materialization=([1-9][0-9]*) target=([1-9][0-9]*) layout=([1-9][0-9]*) table=([1-9][0-9]*) slot=([01]) serial=([1-9][0-9]*) segments=([1-9][0-9]*) written=([1-9][0-9]*) invalidated=([1-9][0-9]*) preserved=([0-9]+) sequence=([0-9a-f]{16}):([0-9a-f]{16}):([1-9][0-9]*) submit=1")
submits=exact("temporal-main-activation ",submit_pat,"INFO","renderer.ral",2)
sm=[re.fullmatch(submit_pat,x[2]) for x in submits]
if any((int(x.group(4)),int(x.group(5)))!=target_extents[0] for x in sm):raise SystemExit("FAIL temporal submitted extent does not match the widescreen target")
content_pat=(r"temporal-motion-content schema=1 token=([1-9][0-9]*) frame=([1-9][0-9]*) world=([0-3]) extent=([1-9][0-9]*)x([1-9][0-9]*) topology=([1-9][0-9]*) plan=([1-9][0-9]*) materialization=([1-9][0-9]*) target=([1-9][0-9]*) layout=([1-9][0-9]*) table=([1-9][0-9]*) slot=([01]) serial=([1-9][0-9]*) roi=([0-9]+),([0-9]+),([1-9][0-9]*)x([1-9][0-9]*) segments=([1-9][0-9]*) written=([1-9][0-9]*) invalidated=([1-9][0-9]*) sequence=([0-9a-f]{16}):([0-9a-f]{16}):([1-9][0-9]*) submit=1 fence=1 pixels=([1-9][0-9]*) validity-zero=([0-9]+) validity-full=([0-9]+) validity-other=0 finite=([1-9][0-9]*) nonfinite=0 nonzero-valid=([1-9][0-9]*) nonzero-invalid=0 velocity-hash=([0-9a-f]{16}) validity-hash=([0-9a-f]{16}) ready=1")
contents=exact("temporal-motion-content ",content_pat,"INFO","renderer.ral",2)
gm=[re.fullmatch(content_pat,x[2]) for x in contents]
if int(sm[1].group(2))!=int(sm[0].group(2))+1:raise SystemExit("FAIL temporal GPU frames not adjacent")
if {int(m.group(12)) for m in sm}!={0,1}:raise SystemExit("FAIL temporal capture slots")
if int(sm[1].group(13))!=int(sm[0].group(13))+1:raise SystemExit("FAIL temporal capture serial")
for s,g,submit,content in zip(sm,gm,submits,contents):
 if s.groups()[:13]!=g.groups()[:13]:raise SystemExit("FAIL temporal submit/content authority")
 if (s.group(14),s.group(15),s.group(16),s.group(18),s.group(19),s.group(20))!=(g.group(18),g.group(19),g.group(20),g.group(21),g.group(22),g.group(23)):raise SystemExit("FAIL temporal submit/content receipt")
 pixels=int(g.group(24));roi_w=int(g.group(16));roi_h=int(g.group(17))
 roi_x=int(g.group(14));roi_y=int(g.group(15));extent_w=int(g.group(4));extent_h=int(g.group(5))
 if roi_w!=min(256,extent_w) or roi_h!=min(256,extent_h) or roi_x!=(extent_w-roi_w)//2 or roi_y!=(extent_h-roi_h)//2:raise SystemExit("FAIL temporal centered bounded ROI")
 if int(s.group(14))!=int(s.group(15))+int(s.group(16)) or int(s.group(20))!=int(s.group(14))+int(s.group(17)):raise SystemExit("FAIL temporal submit receipt arithmetic")
 if pixels!=roi_w*roi_h or int(g.group(25))+int(g.group(26))!=pixels or int(g.group(27))!=pixels:raise SystemExit("FAIL temporal GPU pixel accounting")
 if int(g.group(29),16)==0 or int(g.group(30),16)==0:raise SystemExit("FAIL temporal zero content hash")
 if not arm[0]<submit[0]<content[0]:raise SystemExit("FAIL temporal submit/fence order")
iqm_activation_pat=(r"temporal-iqm-activation schema=1 token=([1-9][0-9]*) frame=([1-9][0-9]*) slot=([01]) serial=([1-9][0-9]*) iqm=([1-9][0-9]*):([1-9][0-9]*):([0-9]+):([1-9][0-9]*):([0-9a-f]{16}) tagged=([0-9a-f]{16}):([0-9a-f]{16}):([1-9][0-9]*):([1-9][0-9]*):([1-9][0-9]*) factory=([1-9][0-9]*):([1-9][0-9]*):([1-9][0-9]*) payload-layout=([1-9][0-9]*) scene-format=([1-9][0-9]*) depth-format=([1-9][0-9]*) reversed=([01]) submit=1")
iqm_activations=exact("temporal-iqm-activation ",iqm_activation_pat,"INFO","renderer.ral",2)
iam=[re.fullmatch(iqm_activation_pat,x[2]) for x in iqm_activations]
iqm_payload_pat=(r"temporal-iqm-payload schema=1 token=([1-9][0-9]*) frame=([1-9][0-9]*) slot=([01]) serial=([1-9][0-9]*) records=([1-9][0-9]*) owner=([1-9][0-9]*) slot-generation=([1-9][0-9]*) prepare=([1-9][0-9]*) content=([0-9a-f]{16}) current=([0-9a-f]{16}) previous=([0-9a-f]{16}) raster="+r":".join([r"([0-9a-f]{8})"]*16)+r" submit=1 fence=1")
iqm_payloads=exact("temporal-iqm-payload ",iqm_payload_pat,"INFO","renderer.ral",2)
ipm=[re.fullmatch(iqm_payload_pat,x[2]) for x in iqm_payloads]
sample_pat=(r"temporal-resolve-sample schema=1 token=([1-9][0-9]*) frame=([1-9][0-9]*) slot=([01]) serial=([1-9][0-9]*) pixel=([0-9]+),([0-9]+) color=([0-9a-f]{4}):([0-9a-f]{4}):([0-9a-f]{4}):([0-9a-f]{4}) depth=([1-6]):([0-9a-f]{8}) velocity=([0-9a-f]{4}):([0-9a-f]{4}) validity=255 resolved=([0-9a-f]{4}):([0-9a-f]{4}):([0-9a-f]{4}):([0-9a-f]{4}) submit=1 fence=1")
samples=exact("temporal-resolve-sample ",sample_pat,"INFO","renderer.ral",2)
sam=[re.fullmatch(sample_pat,x[2]) for x in samples]
for n,(s,a,p,z) in enumerate(zip(sm,iam,ipm,sam)):
 authority=(s.group(1),s.group(2),s.group(12),s.group(13))
 if authority!=a.groups()[:4] or authority!=p.groups()[:4] or authority!=z.groups()[:4]:raise SystemExit("FAIL temporal IQM capture authority join")
 prepared,written,invalidated,entities=map(int,a.groups()[4:8]);tagged_count,generic_count,iqm_count=map(int,a.groups()[11:14])
 if (prepared,written,invalidated,entities)!=(1,1,0,1):raise SystemExit("FAIL temporal IQM activation summary")
 if tagged_count!=generic_count+iqm_count or iqm_count!=1 or generic_count<1:raise SystemExit("FAIL temporal tagged sequence arithmetic")
 factory_gens=tuple(map(int,a.groups()[14:18]))
 if any(v<=0 or v>=0xffffffff for v in factory_gens):raise SystemExit("FAIL temporal IQM factory generation")
 if int(p.group(5))!=1 or any(int(p.group(i))<=0 or int(p.group(i))>=0xffffffff for i in (6,7,8)) or any(int(p.group(i),16)==0 for i in (9,10,11)):raise SystemExit("FAIL temporal IQM payload generation/content/hash")
 raster=[struct.unpack("<f",struct.pack("<I",int(p.group(i),16)))[0] for i in range(12,28)]
 if not all(math.isfinite(v) for v in raster) or not any(v!=0.0 for v in raster):raise SystemExit("FAIL temporal IQM raster MVP")
 color=tuple(int(z.group(i),16) for i in range(7,11));velocity=tuple(struct.unpack("<e",struct.pack("<H",int(z.group(i),16)))[0] for i in (13,14))
 color_f=tuple(struct.unpack("<e",struct.pack("<H",v))[0] for v in color)
 if not all(math.isfinite(v) for v in color_f) or max(abs(v) for v in color_f)<=0.0:raise SystemExit("FAIL temporal IQM finite scene sample")
 if not all(math.isfinite(v) for v in velocity) or max(abs(v) for v in velocity)<=0.01:raise SystemExit("FAIL temporal IQM center velocity")
 depth_bits=int(z.group(12),16);depth_encoding=int(z.group(11))
 depth=(struct.unpack("<f",struct.pack("<I",depth_bits))[0] if depth_encoding==4 else depth_bits)
 if not math.isfinite(depth) or depth<=0:raise SystemExit("FAIL temporal IQM center depth")
 if not submits[n][0]<iqm_activations[n][0]<contents[n][0]<=iqm_payloads[n][0] or not submits[n][0]<samples[n][0]:raise SystemExit("FAIL temporal IQM submit/fence order")
if ipm[1].group(11)!=ipm[0].group(10):raise SystemExit("FAIL temporal IQM previousPalette2/currentPalette1 chain")
if any(m.group(10)==m.group(11) for m in ipm):raise SystemExit("FAIL temporal IQM current/previous palette equality")
if iam[1].groups()[14:18]!=iam[0].groups()[14:18]:raise SystemExit("FAIL temporal IQM factory generation drift")
if ipm[1].group(6)!=ipm[0].group(6) or (ipm[1].group(3),ipm[1].group(7),ipm[1].group(8))==(ipm[0].group(3),ipm[0].group(7),ipm[0].group(8)):raise SystemExit("FAIL temporal IQM payload generation continuity")
history_pat=(r"temporal-history-content schema=1 world=([0-3]) frame=([1-9][0-9]*) plan=([1-9][0-9]*) allocation=([1-9][0-9]*) read=([01]) write=([01]) extent=([1-9][0-9]*)x([1-9][0-9]*) topology=([1-9][0-9]*) slot=([01]) serial=([1-9][0-9]*) history-valid=1 current=([0-9a-f]{8}):([0-9a-f]{8}):([0-9a-f]{8}) previous=([0-9a-f]{8}):([0-9a-f]{8}):([0-9a-f]{8}) submit=1 fence=1 ready=1")
history_contents=exact("temporal-history-content ",history_pat,"INFO","renderer.ral",2)
hm=[re.fullmatch(history_pat,x[2]) for x in history_contents]
if int(hm[1].group(2))!=int(hm[0].group(2))+1:raise SystemExit("FAIL temporal history frames not adjacent")
if any(m.group(5)==m.group(6) for m in hm):raise SystemExit("FAIL temporal history read/write alias")
if {int(m.group(10)) for m in hm}!={0,1} or int(hm[1].group(11))!=int(hm[0].group(11))+1:raise SystemExit("FAIL temporal history slot/serial")
def history_sample(m,start):
 color=(int(m.group(start),16),int(m.group(start+1),16));depth_bits=int(m.group(start+2),16)
 halves=[]
 for word in color:halves.extend(struct.unpack("<ee",struct.pack("<I",word)))
 depth=struct.unpack("<f",struct.pack("<I",depth_bits))[0]
 if not all(math.isfinite(v) for v in halves) or not math.isfinite(depth) or depth<=0:raise SystemExit("FAIL temporal history nonfinite/nonpositive sample")
 return color+(depth_bits,)
for m in hm:
 history_sample(m,12);history_sample(m,15)
current1=history_sample(hm[0],12);current2=history_sample(hm[1],12);previous2=history_sample(hm[1],15)
if current1[0]==0 and current1[1]==0:raise SystemExit("FAIL temporal history zero current witness")
if previous2[0]==0 and previous2[1]==0:raise SystemExit("FAIL temporal history zero chained prior witness")
if current2==current1:raise SystemExit("FAIL temporal history static/current-substitution witness")
if not history_arm[0] < history_contents[0][0] < history_contents[1][0]:raise SystemExit("FAIL temporal history arm/fence order")
resolve_arm=exact("temporal-resolve-readback schema=1 action=armed",r"temporal-resolve-readback schema=1 action=armed captures=2","INFO","renderer.ral")[0]
feedback_pat=(r"temporal-history-feedback schema=1 token=([1-9][0-9]*) frame=([1-9][0-9]*) world=([0-3]) extent=([1-9][0-9]*)x([1-9][0-9]*) topology=([1-9][0-9]*) plan=([1-9][0-9]*) producer=(current-seed|resolved-feedback) content=([1-9][0-9]*) scene=([1-9][0-9]*) target=([1-9][0-9]*) resolve-owner=([0-9]+) store-owner=([1-9][0-9]*) history=([1-9][0-9]*):([01]) slot=([0-3]) frame-count=([1-4]) commit=1")
feedback=[]
for i,(row,value) in enumerate(zip(rows,vals)):
 if value.startswith("temporal-history-feedback "):
  match=re.fullmatch(feedback_pat,value)
  if row["sev"].upper()!="INFO" or row["cat"].lower()!="renderer.ral" or not match:raise SystemExit("FAIL temporal history feedback metadata/body")
  if int(match.group(17))!=2 or int(match.group(16))>=int(match.group(17)) or not markers[0][0]<i<markers[1][0]:raise SystemExit("FAIL temporal history feedback slot/window")
  feedback.append((i,row,value,match))
if not feedback:raise SystemExit("FAIL temporal no committed history feedback")
prior_fields=r"prior-producer=[12] prior-token=[1-9][0-9]* prior-frame=[1-9][0-9]* prior-content=[1-9][0-9]* prior-scene=[1-9][0-9]* prior-target=[1-9][0-9]* prior-resolve-owner=[0-9]+ prior-store-owner=[1-9][0-9]* prior-slot=[0-3] prior-frame-count=[1-4]"
prior_pat=re.compile(r"prior-producer=([12]) prior-token=([1-9][0-9]*) prior-frame=([1-9][0-9]*) prior-content=([1-9][0-9]*) prior-scene=([1-9][0-9]*) prior-target=([1-9][0-9]*) prior-resolve-owner=([0-9]+) prior-store-owner=([1-9][0-9]*) prior-slot=([0-3]) prior-frame-count=([1-4])")
resolve_submit_pat=(r"temporal-resolve-readback schema=1 action=submitted token=([1-9][0-9]*) frame=([1-9][0-9]*) previous=([1-9][0-9]*) content=([1-9][0-9]*) world=([0-3]) extent=([1-9][0-9]*)x([1-9][0-9]*) topology=([1-9][0-9]*) plan=([1-9][0-9]*) scene=([1-9][0-9]*) history=([1-9][0-9]*):([01]) motion-materialization=([1-9][0-9]*) motion-target=([1-9][0-9]*) motion-layout=([1-9][0-9]*) table=([1-9][0-9]*) "+prior_fields+r" target=([1-9][0-9]*) owner=([1-9][0-9]*) slot=([01]) serial=([1-9][0-9]*) depth=([1-6]) segments=([1-9][0-9]*) written=([1-9][0-9]*) invalidated=([1-9][0-9]*) sequence=([0-9a-f]{16}):([0-9a-f]{16}):([1-9][0-9]*) outer-submit=1 content-submit=1")
resolve_submits=exact("temporal-resolve-readback schema=1 action=submitted ",resolve_submit_pat,"INFO","renderer.ral",2)
rsm=[re.fullmatch(resolve_submit_pat,x[2]) for x in resolve_submits]
resolve_content_pat=(r"temporal-resolve-content schema=1 token=([1-9][0-9]*) frame=([1-9][0-9]*) previous=([1-9][0-9]*) content=([1-9][0-9]*) world=([0-3]) extent=([1-9][0-9]*)x([1-9][0-9]*) topology=([1-9][0-9]*) plan=([1-9][0-9]*) scene=([1-9][0-9]*) history=([1-9][0-9]*):([01]) motion-materialization=([1-9][0-9]*) motion-target=([1-9][0-9]*) motion-layout=([1-9][0-9]*) table=([1-9][0-9]*) "+prior_fields+r" target=([1-9][0-9]*) owner=([1-9][0-9]*) slot=([01]) serial=([1-9][0-9]*) depth=([1-6]) segments=([1-9][0-9]*) written=([1-9][0-9]*) invalidated=([1-9][0-9]*) sequence=([0-9a-f]{16}):([0-9a-f]{16}):([1-9][0-9]*) capture=([0-9]+),([0-9]+),([1-9][0-9]*)x([1-9][0-9]*) core=([0-9]+),([0-9]+),([1-9][0-9]*)x([1-9][0-9]*) outer-submit=1 content-submit=1 fence=1 core-pixels=([1-9][0-9]*) eligible=([1-9][0-9]*) accepted=([1-9][0-9]*) accepted-match=([1-9][0-9]*) accepted-influence=([0-9]+) accepted-motion=([1-9][0-9]*) fallback=([0-9]+)/([0-9]+) invalid=([0-9]+)/([0-9]+) zero=([0-9]+)/([0-9]+) unsupported=([0-9]+) nonfinite=0 validity=([0-9]+):([1-9][0-9]*):0 planes=1 mismatches=0 hashes=([0-9a-f]{16}):([0-9a-f]{16}):([0-9a-f]{16}):([0-9a-f]{16}):([0-9a-f]{16}):([0-9a-f]{16}):([0-9a-f]{16}) ready=1")
resolve_contents=exact("temporal-resolve-content ",resolve_content_pat,"INFO","renderer.ral",2)
rcm=[re.fullmatch(resolve_content_pat,x[2]) for x in resolve_contents]
resolved_hdr_pat=(r"temporal-resolved-hdr schema=3 token=([1-9][0-9]*) frame=([1-9][0-9]*) previous=([1-9][0-9]*) world=([0-3]) extent=([1-9][0-9]*)x([1-9][0-9]*) topology=([1-9][0-9]*) plan=([1-9][0-9]*) scene=([1-9][0-9]*) target=([1-9][0-9]*) resolve-owner=([1-9][0-9]*) slot=([01]) serial=([1-9][0-9]*) history=([1-9][0-9]*):([01]) "+prior_fields+r" producer=resolve submit=1")
copied_hdr_pat=(r"temporal-resolved-hdr schema=2 token=([1-9][0-9]*) frame=([1-9][0-9]*) world=([0-3]) extent=([1-9][0-9]*)x([1-9][0-9]*) topology=([1-9][0-9]*) plan=([1-9][0-9]*) scene=([1-9][0-9]*) target=([1-9][0-9]*) slot=([01]) serial=([1-9][0-9]*) producer=copy submit=1")
resolved_hdr=[];copied_hdr=[]
for i,(row,value) in enumerate(zip(rows,vals)):
 if value.startswith("temporal-resolved-hdr "):
  match=re.fullmatch(resolved_hdr_pat,value)
  copied=re.fullmatch(copied_hdr_pat,value)
  if row["sev"].upper()!="INFO" or row["cat"].lower()!="renderer.ral" or (not match and not copied):raise SystemExit("FAIL temporal routed/copied HDR metadata/body")
  if match:resolved_hdr.append((i,row,value,match))
  if copied:copied_hdr.append((i,row,value,copied))
if len(resolved_hdr)<2:raise SystemExit("FAIL temporal routed HDR evidence")
if int(rsm[1].group(2))!=int(rsm[0].group(2))+1 or {int(x.group(19)) for x in rsm}!={0,1}:raise SystemExit("FAIL temporal resolve frame/slot sequence")
if int(rsm[1].group(20))!=int(rsm[0].group(20))+1:raise SystemExit("FAIL temporal resolve serial sequence")
def prior_tuple(value):
 matches=prior_pat.findall(value)
 if len(matches)!=1:raise SystemExit("FAIL temporal resolve prior source parse")
 p=matches[0]
 return (("current-seed" if p[0]=="1" else "resolved-feedback"),)+tuple(map(int,p[1:]))
def feedback_source_tuple(m):
 return (m.group(8),int(m.group(1)),int(m.group(2)),int(m.group(9)),int(m.group(10)),int(m.group(11)),int(m.group(12)),int(m.group(13)),int(m.group(16)),int(m.group(17)))
prior_sources=[];committed_feedback=[]
for capture_n,(s,c,main,motion,history,submit,content) in enumerate(zip(rsm,rcm,sm,gm,hm,resolve_submits,resolve_contents)):
 if s.groups()!=c.groups()[:27]:raise SystemExit("FAIL temporal resolve submit/content ticket join")
 prior_submit=prior_tuple(submit[2]);prior_content=prior_tuple(content[2])
 if prior_submit!=prior_content:raise SystemExit("FAIL temporal resolve prior submit/content join")
 prior_matches=[x for x in feedback if feedback_source_tuple(x[3])==prior_submit]
 if len(prior_matches)!=1 or prior_matches[0][0]>=submit[0]:raise SystemExit("FAIL temporal resolve prior/committed feedback join")
 pf=prior_matches[0][3]
 if int(prior_submit[2])!=int(c.group(3)):raise SystemExit("FAIL temporal resolve prior frame adjacency")
 if (pf.group(3),pf.group(4),pf.group(5),pf.group(6),pf.group(7),pf.group(14),pf.group(15))!=(c.group(5),c.group(6),c.group(7),c.group(8),c.group(9),c.group(11),c.group(12)):raise SystemExit("FAIL temporal resolve prior feedback cohort/history join")
 prior_sources.append(prior_submit)
 if (c.group(1),c.group(2),c.group(5),c.group(6),c.group(7),c.group(8),c.group(9),c.group(13),c.group(14),c.group(15),c.group(16),c.group(19),c.group(22),c.group(23),c.group(24),c.group(25),c.group(26),c.group(27))!=(main.group(1),main.group(2),main.group(3),main.group(4),main.group(5),main.group(6),main.group(7),main.group(8),main.group(9),main.group(10),main.group(11),main.group(12),main.group(14),main.group(15),main.group(16),main.group(18),main.group(19),main.group(20)):raise SystemExit("FAIL temporal resolve/main activation join")
 if (c.group(1),c.group(2),c.group(5),c.group(6),c.group(7),c.group(8),c.group(9),c.group(13),c.group(14),c.group(15),c.group(16),c.group(19),c.group(22),c.group(23),c.group(24),c.group(25),c.group(26),c.group(27))!=(motion.group(1),motion.group(2),motion.group(3),motion.group(4),motion.group(5),motion.group(6),motion.group(7),motion.group(8),motion.group(9),motion.group(10),motion.group(11),motion.group(12),motion.group(18),motion.group(19),motion.group(20),motion.group(21),motion.group(22),motion.group(23)):raise SystemExit("FAIL temporal resolve/motion content join")
 if (c.group(5),c.group(2),c.group(9),c.group(11),c.group(12),c.group(6),c.group(7),c.group(8),c.group(19))!=(history.group(1),history.group(2),history.group(3),history.group(4),history.group(5),history.group(7),history.group(8),history.group(9),history.group(10)):raise SystemExit("FAIL temporal resolve/history content join")
 routed_tuple=(c.group(1),c.group(2),c.group(3),c.group(5),c.group(6),c.group(7),c.group(8),c.group(9),c.group(10),c.group(17),c.group(18),c.group(19),c.group(4))
 routed_matches=[x for x in resolved_hdr if x[3].groups()[:13]==routed_tuple]
 if len(routed_matches)!=1:raise SystemExit("FAIL temporal resolve/routed HDR join")
 current_feedback=[x for x in feedback if x[3].group(1)==c.group(1) and x[3].group(2)==c.group(2) and x[3].group(3)==c.group(5) and x[3].group(4)==c.group(6) and x[3].group(5)==c.group(7) and x[3].group(6)==c.group(8) and x[3].group(7)==c.group(9) and x[3].group(8)=="resolved-feedback" and x[3].group(9)==c.group(4) and x[3].group(10)==c.group(10) and x[3].group(11)==c.group(17) and x[3].group(12)==c.group(18) and x[3].group(14)==c.group(11) and int(x[3].group(15))!=int(c.group(12)) and x[3].group(16)==c.group(19)]
 if len(current_feedback)!=1:raise SystemExit("FAIL temporal resolve/feedback commit join")
 if not routed_matches[0][0]<current_feedback[0][0]<content[0]:raise SystemExit("FAIL temporal routed/feedback/fence order")
 committed_feedback.append(current_feedback[0])
 if int(c.group(3))!=int(c.group(2))-1:raise SystemExit("FAIL temporal resolve previous frame")
 capture_x,capture_y,capture_w,capture_h=map(int,c.groups()[27:31]);core_x,core_y,core_w,core_h=map(int,c.groups()[31:35]);extent_w,extent_h=map(int,c.groups()[5:7])
 if core_w!=min(256,extent_w) or core_h!=min(256,extent_h) or core_x!=(extent_w-core_w)//2 or core_y!=(extent_h-core_h)//2:raise SystemExit("FAIL temporal resolve core geometry")
 apron_w=min(512,max(4,extent_w//4));apron_h=min(512,max(4,extent_h//4))
 if capture_w!=min(extent_w,core_w+2*apron_w) or capture_h!=min(extent_h,core_h+2*apron_h) or capture_x!=(extent_w-capture_w)//2 or capture_y!=(extent_h-capture_h)//2:raise SystemExit("FAIL temporal resolve apron geometry")
 if int(c.group(36))!=core_w*core_h or int(c.group(37))!=int(c.group(38)) or c.group(38)!=c.group(39) or int(c.group(41))<=0 or int(c.group(40))>int(c.group(39)) or int(c.group(41))>int(c.group(38)):raise SystemExit("FAIL temporal resolve oracle accounting")
 if c.group(42)!=c.group(43) or c.group(44)!=c.group(45) or c.group(46)!=c.group(47) or int(c.group(43))+int(c.group(47))+int(c.group(48))+int(c.group(38))!=int(c.group(36)) or int(c.group(45))>int(c.group(43)) or int(c.group(49))+int(c.group(50))!=capture_w*capture_h:raise SystemExit("FAIL temporal resolve fallback/unsupported/validity accounting")
 if any(int(c.group(n),16)==0 for n in range(51,58)):raise SystemExit("FAIL temporal resolve zero plane hash")
 if not resolve_arm[0]<submit[0]<routed_matches[0][0]<content[0]:raise SystemExit("FAIL temporal resolve arm/submit/routed/fence order")
priming_feedback=[x for x in feedback if feedback_source_tuple(x[3])==prior_sources[0]]
if prior_sources[0][0]!="resolved-feedback" or len(priming_feedback)!=1 or int(priming_feedback[0][3].group(12))==0:raise SystemExit("FAIL temporal missing exact adjacent resolved bootstrap prime")
pf=priming_feedback[0][3]
priming_routes=[x for x in resolved_hdr if x[3].group(1)==pf.group(1) and x[3].group(2)==pf.group(2) and int(x[3].group(3))==int(pf.group(2))-1 and x[3].group(4)==pf.group(3) and x[3].group(5)==pf.group(4) and x[3].group(6)==pf.group(5) and x[3].group(7)==pf.group(6) and x[3].group(8)==pf.group(7) and x[3].group(9)==pf.group(10) and x[3].group(10)==pf.group(11) and x[3].group(11)==pf.group(12) and x[3].group(12)==pf.group(16) and x[3].group(13)==pf.group(9)]
if len(priming_routes)!=1:raise SystemExit("FAIL temporal missing exact unarmed priming H3 route")
priming_route=priming_routes[0]
priming_prior=prior_tuple(priming_route[2])
if int(priming_prior[2])!=int(priming_route[3].group(3)):raise SystemExit("FAIL temporal priming prior frame adjacency")
seed_feedback=[x for x in feedback if feedback_source_tuple(x[3])==priming_prior]
if priming_prior[0]!="current-seed" or len(seed_feedback)!=1 or int(seed_feedback[0][3].group(12))!=0:raise SystemExit("FAIL temporal missing exact bootstrap current seed")
sf=seed_feedback[0][3]
if (priming_route[3].group(14),priming_route[3].group(15))!=(sf.group(14),sf.group(15)):raise SystemExit("FAIL temporal seed/priming history read join")
seed_copy_tuple=(sf.group(1),sf.group(2),sf.group(3),sf.group(4),sf.group(5),sf.group(6),sf.group(7),sf.group(10),sf.group(11),sf.group(16),sf.group(9))
seed_copies=[x for x in copied_hdr if x[3].groups()==seed_copy_tuple]
if len(seed_copies)!=1 or not seed_copies[0][0]<seed_feedback[0][0]<priming_route[0]<priming_feedback[0][0]<resolve_submits[0][0]:raise SystemExit("FAIL temporal bootstrap copy/seed/prime/F2 join")
chain_frames=[int(sf.group(2)),int(pf.group(2)),int(rcm[0].group(2)),int(rcm[1].group(2))]
if any(chain_frames[i+1]!=chain_frames[i]+1 for i in range(3)):raise SystemExit("FAIL temporal F0/F1/F2/F3 adjacency")
if [int(m.group(2)) for m in enabled_pm]!=chain_frames[1:]:raise SystemExit("FAIL temporal projection/prime/capture frame bind")
if (enabled_pm[0].group(16),enabled_pm[0].group(17))!=(priming_route[3].group(15),pf.group(15)):raise SystemExit("FAIL temporal priming projection history join")
for n in range(2):
 if (enabled_pm[n+1].group(16),enabled_pm[n+1].group(17))!=(rcm[n].group(12),committed_feedback[n][3].group(15)):raise SystemExit("FAIL temporal capture projection history join")
if not priming_feedback[0][0]<arm[0]<=history_arm[0]<=resolve_arm[0]<resolve_submits[0][0]:raise SystemExit("FAIL temporal priming/arm/F2 order")
if any(int(x.group(2))==chain_frames[1] for x in rsm):raise SystemExit("FAIL temporal priming frame was H3c-captured")
chain_serials=[int(sf.group(9)),int(pf.group(9)),int(rcm[0].group(4)),int(rcm[1].group(4))]
if any(chain_serials[i+1]!=chain_serials[i]+1 for i in range(3)):raise SystemExit("FAIL temporal F0/F1/F2/F3 content serial progression")
chain_slots=[int(sf.group(16)),int(pf.group(16)),int(rcm[0].group(19)),int(rcm[1].group(19))]
if any(chain_slots[i+1]==chain_slots[i] for i in range(3)):raise SystemExit("FAIL temporal F0/F1/F2/F3 slot alternation")
if feedback_source_tuple(committed_feedback[0][3])!=prior_sources[1]:raise SystemExit("FAIL temporal recursive feedback source chain")
if rcm[0].group(57)!=rcm[1].group(53):raise SystemExit("FAIL temporal recursive resolved/previous hash chain")
if rcm[0].group(57)==rcm[0].group(51):raise SystemExit("FAIL temporal recursive feedback is raw-current substitution")
shutdown=exact("----- Server Shutdown ",r"----- Server Shutdown \(Server quit\) -----","INFO","server")[0]
shutdown_game=exact("==== ShutdownGame ====",r"==== ShutdownGame ====","INFO","game")[0]
if not gamesv_load[0]<gamecl_load[0]<gamecl_vm[0]<policy[0]<first[0]<markers[0][0]:raise SystemExit("FAIL temporal native load order")
if not policy[0]<cap[0]<min(matching_slot_indices)<markers[0][0]:raise SystemExit("FAIL temporal authority order")
if not markers[0][0]<rebuilds[0][0]<arm[0]<=history_arm[0]<=resolve_arm[0]<submits[0][0]<submits[1][0]:raise SystemExit("FAIL temporal causal prefix")
if not max(contents[0][0],contents[1][0],history_contents[0][0],history_contents[1][0],resolve_contents[0][0],resolve_contents[1][0])<markers[1][0]<rebuilds[1][0]<projections[3][0]<continuity[3][0]<markers[2][0]<markers[3][0]<shutdown[0]<shutdown_game[0]:raise SystemExit("FAIL temporal causal suffix")
manifest=[json.loads(line) for line in open(manifest_path,encoding="utf-8",errors="strict") if line.strip()]
scenario={"kind":"scenario","schema":11,"name":"ral-temporal-projection","map":"arena7","backend":"vulkan","cgame":"native-vm_cgame-0","toggle":"latched-r_entitySSBO=1;isolated-r_temporalInputTest:0->1->0;g_spawnProtect=0;sv_fps=20;fixedtime:0->1->0;timescale:1->0.025->1;cl_run=1;stationary-third-person-camera;h5a-fixture-root-y-animation","consumer":"synthetic-fixed-1ms-per-render-tick-stationary-camera-primary-world-projection-history-IQM-palette-chain-tagged-activation-fenced-motion-history-h3-resolve-center-sample-and-recursive-feedback-commit","nonclaims":["validation-backed-vuid-cleanliness","full-frame-resolve-proof","visual-quality-proof","whole-image-motion-coverage","reference-correct-vector-field","first-recursive-frame-pixel-influence","view-invariant-depth-disocclusion","arbitrary-camera-translation","exact-0.025-physical-motion-ratio","ordinary-default-wall-clock-timing","per-frame-snapshot-cadence","production-timing-quality","single-frame-bootstrap-fence-readback","more-than-one-recursive-link","long-run-stability","camera-cuts","letterboxed-or-multiview-history","screenmap-replay-runtime","split-screen-runtime","wasm-cgame-runtime","future-normal-map-IQM-semantics"]}
if not manifest or manifest[0]!=scenario:raise SystemExit("FAIL temporal scenario")
roles=["gui","renderer","gamecl-native","gamesv-native","moltenvk","pax21","base","iqm-generator","iqm-model","iqm-character-manifest","iqm-texture","harness","bootstrap-cfg"]
if [x.get("role") for x in manifest[1:-1]]!=roles:raise SystemExit("FAIL temporal provenance roles")
for item in manifest[1:-1]:
 if set(item)!={"kind","role","path","bytes","sha256"} or item.get("kind")!="provenance" or not isinstance(item.get("bytes"),int) or item["bytes"]<=0 or re.fullmatch(r"[0-9a-f]{64}",str(item.get("sha256",""))) is None:raise SystemExit("FAIL temporal provenance schema")
 data=open(item["path"],"rb").read()
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:raise SystemExit("FAIL temporal provenance rehash")
 if item["role"]=="bootstrap-cfg" and data.decode()!=boot_expected:raise SystemExit("FAIL temporal bootstrap content")
result=manifest[-1]
if set(result)!={"kind","controller_pid","rc","timeout","forced"} or result.get("kind")!="result" or not isinstance(result.get("controller_pid"),int) or result["controller_pid"]<=0 or result.get("rc")!=0 or result.get("timeout") is not False or result.get("forced") is not False:raise SystemExit("FAIL temporal result")
print("PASS native temporal continuity, fenced motion/history, bounded H3 resolve ROI and recursive feedback commit")
PYEOF
}

write_self() {
python3 - "$1" "$2" "$3" "$BOOT_CONTENT" <<'PYEOF'
import hashlib,json,os,sys
log_path,manifest_path,mode,boot=sys.argv[1:]
def row(sev,cat,msg):return {"ts":"2026-08-14T12:00:00.000+03:00","sev":sev,"cat":cat,"msg":msg+"\n"}
def proj(frame,enabled,reset,history,read,write,phase,cam_prev):
 rec=1 if enabled else 0;ready="ready" if enabled else "none";slots=2 if enabled else 0;accepted=2 if enabled else 0;visible=3 if enabled else 0;commit=1 if enabled else 0
 return row("INFO","renderer.temporal",f"temporal-projection world=0 frame={frame} enabled={enabled} queued=1 recorded={rec} previous-read={1 if enabled else 0} committed=1 camera-valid=1 camera-previous={cam_prev} entities={accepted}/{visible} previous={accepted if cam_prev else 0} rejected=0 entity-commit={commit} generation={1 if enabled else 2} reset=0x{reset:x} history-valid={history} read={read} write={write} phase={phase} extent=1280x720 resources={ready} resource-generation=1 color-slots={slots} depth-slots={slots} jitter-px=0.000000,-0.166667 jitter-uv=0.000000000,-0.000231481 ui-jitter=0.0,0.0 ndc=0.000000000,0.000462963 projection=0.100000000,-0.200000000->0.100000000,-0.200462963")
def cont(frame,n,enabled=True):
 cam="2"*352;ent=("%x"%(n+3))*184;pcam="2"*352;pent=("0"*184 if n==1 else ("%x"%(n+2))*184);prev=0 if n==1 else 1;pf=0 if n==1 else frame-1
 return row("INFO","renderer.temporal",f"temporal-continuity schema=1 world=0 frame={frame} committed=1 camera-valid=1 camera-previous={prev} camera-previous-frame={pf} camera-current={cam} camera-previous-fields={pcam} entity-valid={1 if enabled else 0} entity-owner=0 entity-generation=1 entity-role=3 entity-previous={prev if enabled else 0} entity-previous-frame={pf if enabled else 0} entity-current={ent if enabled else '0'*184} entity-previous-fields={pent if enabled else '0'*184} entity-committed={1 if enabled else 0} attempts=1 scans=1 drawsurfs=40 visible-temporal={3 if enabled else 0} accepted={2 if enabled else 0} rejected=0")
R=[row("INFO","filesystem","Sys_LoadLibrary(gamesvarm64.dylib): loaded"),row("INFO","filesystem","Sys_LoadLibrary(gameclarm64.dylib): loaded"),row("INFO","system","VM_LoadDll(gamecl): loaded, vmMain @ 0x1234"),row("DEBUG","system","VM_Create policy module=gamecl requested=0 effective=0 backend=native"),row("INFO","client","window-extent schema=2 requested=1280x720 logical=1280x720 pixels=2560x1440 exact16x9=1 publish-ready=1"),row("INFO","cgame","IQM h5a_fixture: mapped 36/36 animations from embedded data"),row("DEBUG","cgame","CG_LoadCharacter: loaded profile=h5a_fixture parts=1 legs=77 torso=77 head=77 icon=0 skin=0"),row("DEBUG","renderer.assets","IQM GPU skinning VBO: 3 verts, 2 tris (characters/h5a_fixture/models/body.iqm)"),row("INFO","client","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=1 numEntities=2 framecount=3)"),row("INFO","cgame","temporal-entity-capability glconfig-generation=7 key=trap_R_AddRefEntityToSceneTemporal expected=232 discovered=232 route=native-syscall export=1"),row("INFO","cgame","temporal-entity-slot glconfig-generation=7 trap=232 owner=0 entity-generation=1 role=3 accepted=1 export=1"),row("INFO","system","Q3_RAL_TEMPORAL_REQUESTED"),row("INFO","renderer.ral","scene-depth live rebuild active=1 deferred-drained=1 attachments-rebound=1 temporal-store-reset=1")]
R[7:7]=[row("INFO","system",'"sv_fps" is:"20^7" default:"20^7"'),row("INFO","system",'"timescale" is:"0.025^7" default:"1^7"'),row("INFO","system",'"fixedtime" is:"1^7" default:"0^7"'),row("INFO","system",'"cl_run" is:"1^7" default:"1^7"'),row("INFO","system",'"g_spawnProtect" is:"0^7" default:"2^7"')]
for f,n in ((20,2),(21,3),(22,4)):R += [proj(f,1,0,1,n&1,(n+1)&1,n-1,1),cont(f,n)]
R += [row("INFO","renderer.ral","temporal-resolved-hdr schema=2 token=98 frame=17 world=0 extent=1280x720 topology=3 plan=4 scene=5 target=8 slot=1 serial=498 producer=copy submit=1")]
R += [row("INFO","renderer.ral","temporal-resolved-hdr schema=2 token=100 frame=19 world=0 extent=1280x720 topology=3 plan=4 scene=5 target=8 slot=1 serial=500 producer=copy submit=1")]
R += [row("INFO","renderer.ral","temporal-history-feedback schema=1 token=100 frame=19 world=0 extent=1280x720 topology=3 plan=4 producer=current-seed content=500 scene=5 target=8 resolve-owner=0 store-owner=13 history=3:0 slot=1 frame-count=2 commit=1")]
R += [row("INFO","renderer.ral","temporal-resolved-hdr schema=3 token=101 frame=20 previous=19 world=0 extent=1280x720 topology=3 plan=4 scene=5 target=8 resolve-owner=12 slot=0 serial=501 history=3:0 prior-producer=1 prior-token=100 prior-frame=19 prior-content=500 prior-scene=5 prior-target=8 prior-resolve-owner=0 prior-store-owner=13 prior-slot=1 prior-frame-count=2 producer=resolve submit=1")]
R += [row("INFO","renderer.ral","temporal-history-feedback schema=1 token=101 frame=20 world=0 extent=1280x720 topology=3 plan=4 producer=resolved-feedback content=501 scene=5 target=8 resolve-owner=12 store-owner=13 history=3:1 slot=0 frame-count=2 commit=1")]
R += [row("INFO","renderer.ral","temporal-motion-readback schema=1 action=armed captures=2"),row("INFO","renderer.ral","temporal-history-consume schema=1 action=armed captures=2"),row("INFO","renderer.ral","temporal-resolve-readback schema=1 action=armed captures=2")]
gpu=((21,102,1,1,"1111111111111111","2222222222222222"),(22,103,0,2,"3333333333333333","4444444444444444"))
for frame,token,slot,serial,l0,l1 in gpu:
 R += [row("INFO","renderer.ral",f"temporal-main-activation schema=1 token={token} frame={frame} world=0 extent=1280x720 topology=3 plan=4 materialization=10 target=8 layout=9 table=11 slot={slot} serial={serial} segments=2 written=1 invalidated=1 preserved=1 sequence={l0}:{l1}:3 submit=1")]
 R += [row("INFO","renderer.ral",f"temporal-iqm-activation schema=1 token={token} frame={frame} slot={slot} serial={serial} iqm=1:1:0:1:5555555555555555 tagged={l0}:{l1}:3:2:1 factory=17:18:3 payload-layout=19 scene-format=97 depth-format=126 reversed=1 submit=1")]
for n,(frame,token,slot,serial,l0,l1) in enumerate(gpu):
 prior_producer,prior_token,prior_frame,prior_content,prior_owner,prior_slot=(2,101,20,501,12,0) if n==0 else (2,102,21,502,12,1)
 R += [row("INFO","renderer.ral",f"temporal-resolve-readback schema=1 action=submitted token={token} frame={frame} previous={frame-1} content={502+n} world=0 extent=1280x720 topology=3 plan=4 scene=5 history=3:{1-n} motion-materialization=10 motion-target=8 motion-layout=9 table=11 prior-producer={prior_producer} prior-token={prior_token} prior-frame={prior_frame} prior-content={prior_content} prior-scene=5 prior-target=8 prior-resolve-owner={prior_owner} prior-store-owner=13 prior-slot={prior_slot} prior-frame-count=2 target=8 owner=12 slot={slot} serial={serial} depth=4 segments=2 written=1 invalidated=1 sequence={l0}:{l1}:3 outer-submit=1 content-submit=1")]
 R += [row("INFO","renderer.ral",f"temporal-resolved-hdr schema=3 token={token} frame={frame} previous={frame-1} world=0 extent=1280x720 topology=3 plan=4 scene=5 target=8 resolve-owner=12 slot={slot} serial={501+n+1} history=3:{1-n} prior-producer={prior_producer} prior-token={prior_token} prior-frame={prior_frame} prior-content={prior_content} prior-scene=5 prior-target=8 prior-resolve-owner={prior_owner} prior-store-owner=13 prior-slot={prior_slot} prior-frame-count=2 producer=resolve submit=1")]
 R += [row("INFO","renderer.ral",f"temporal-history-feedback schema=1 token={token} frame={frame} world=0 extent=1280x720 topology=3 plan=4 producer=resolved-feedback content={501+n+1} scene=5 target=8 resolve-owner=12 store-owner=13 history=3:{n} slot={slot} frame-count=2 commit=1")]
for frame,token,slot,serial,l0,l1 in gpu:
 R += [row("INFO","renderer.ral",f"temporal-motion-content schema=1 token={token} frame={frame} world=0 extent=1280x720 topology=3 plan=4 materialization=10 target=8 layout=9 table=11 slot={slot} serial={serial} roi=512,232,256x256 segments=2 written=1 invalidated=1 sequence={l0}:{l1}:3 submit=1 fence=1 pixels=65536 validity-zero=60000 validity-full=5536 validity-other=0 finite=65536 nonfinite=0 nonzero-valid=2048 nonzero-invalid=0 velocity-hash=aaaaaaaaaaaaaaaa validity-hash=bbbbbbbbbbbbbbbb ready=1")]
 current,previous=(("1111111111111111","aaaaaaaaaaaaaaaa") if frame==21 else ("2222222222222222","1111111111111111"))
 raster=":".join(("3f800000","00000000","00000000","00000000","00000000","3f800000","00000000","00000000","00000000","00000000","3f800000","00000000","00000000","00000000","3f000000","3f800000"))
 R += [row("INFO","renderer.ral",f"temporal-iqm-payload schema=1 token={token} frame={frame} slot={slot} serial={serial} records=1 owner=19 slot-generation={serial} prepare={serial} content=cccccccccccccccc current={current} previous={previous} raster={raster} submit=1 fence=1")]
for n,(frame,token,slot,serial,l0,l1) in enumerate(gpu):
 prior_producer,prior_token,prior_frame,prior_content,prior_owner,prior_slot=(2,101,20,501,12,0) if n==0 else (2,102,21,502,12,1)
 hashes=("1111111111111111:2222222222222222:3333333333333333:4444444444444444:5555555555555555:6666666666666666:7777777777777777" if n==0 else "8888888888888888:2222222222222222:7777777777777777:4444444444444444:5555555555555555:6666666666666666:9999999999999999")
 R += [row("INFO","renderer.ral",f"temporal-resolve-content schema=1 token={token} frame={frame} previous={frame-1} content={502+n} world=0 extent=1280x720 topology=3 plan=4 scene=5 history=3:{1-n} motion-materialization=10 motion-target=8 motion-layout=9 table=11 prior-producer={prior_producer} prior-token={prior_token} prior-frame={prior_frame} prior-content={prior_content} prior-scene=5 prior-target=8 prior-resolve-owner={prior_owner} prior-store-owner=13 prior-slot={prior_slot} prior-frame-count=2 target=8 owner=12 slot={slot} serial={serial} depth=4 segments=2 written=1 invalidated=1 sequence={l0}:{l1}:3 capture=192,52,896x616 core=512,232,256x256 outer-submit=1 content-submit=1 fence=1 core-pixels=65536 eligible=5536 accepted=5536 accepted-match=5536 accepted-influence=4096 accepted-motion=2048 fallback=59477/59477 invalid=59477/59477 zero=0/0 unsupported=523 nonfinite=0 validity=546400:5536:0 planes=1 mismatches=0 hashes={hashes} ready=1")]
 R += [row("INFO","renderer.ral",f"temporal-resolve-sample schema=1 token={token} frame={frame} slot={slot} serial={serial} pixel=640,360 color=0000:3c00:0000:3c00 depth=4:3f000000 velocity=2c00:0000 validity=255 resolved=0000:3c00:0000:3c00 submit=1 fence=1")]
R += [row("INFO","renderer.ral","temporal-resolved-hdr schema=3 token=104 frame=23 previous=22 world=0 extent=1280x720 topology=3 plan=4 scene=5 target=8 resolve-owner=12 slot=1 serial=504 history=3:1 prior-producer=2 prior-token=103 prior-frame=22 prior-content=503 prior-scene=5 prior-target=8 prior-resolve-owner=12 prior-store-owner=13 prior-slot=0 prior-frame-count=2 producer=resolve submit=1")]
R += [row("INFO","renderer.ral","temporal-history-content schema=1 world=0 frame=21 plan=4 allocation=3 read=1 write=0 extent=1280x720 topology=3 slot=1 serial=1 history-valid=1 current=11111111:22222222:33333333 previous=aaaaaaaa:bbbbbbbb:3f800000 submit=1 fence=1 ready=1"),row("INFO","renderer.ral","temporal-history-content schema=1 world=0 frame=22 plan=4 allocation=3 read=0 write=1 extent=1280x720 topology=3 slot=0 serial=2 history-valid=1 current=44444444:55555555:66666666 previous=11111111:22222222:33333333 submit=1 fence=1 ready=1")]
R += [row("INFO","system","Q3_RAL_TEMPORAL_ENABLED"),row("INFO","renderer.ral","scene-depth live rebuild active=0 deferred-drained=1 attachments-rebound=1 temporal-store-reset=1"),proj(13,0,2,0,0,0,0,1),cont(13,4,False),row("INFO","system","Q3_RAL_TEMPORAL_DISABLED"),row("INFO","system","Q3_RAL_TEMPORAL_COMPLETE"),row("INFO","server","----- Server Shutdown (Server quit) -----"),row("INFO","game","==== ShutdownGame ====")]
R[-3:-3]=[row("INFO","system",'"fixedtime" is:"0^7" default:"0^7"'),row("INFO","system",'"timescale" is:"1^7" default:"1^7"')]
def find(prefix,n=1):return [i for i,x in enumerate(R) if x["msg"].startswith(prefix)][n-1]
if mode=="cap-missing":R.pop(find("temporal-entity-capability"))
elif mode=="cap-trap":R[find("temporal-entity-capability")]["msg"]=R[find("temporal-entity-capability")]["msg"].replace("discovered=232","discovered=231")
elif mode=="slot-missing":R.pop(find("temporal-entity-slot"))
elif mode=="slot-export":R[find("temporal-entity-slot")]["msg"]=R[find("temporal-entity-slot")]["msg"].replace("export=1","export=0")
elif mode=="vm":R[3]["msg"]=R[3]["msg"].replace("requested=0 effective=0 backend=native","requested=1 effective=1 backend=wasm-interpreter")
elif mode=="continuity-missing":R.pop(find("temporal-continuity",2))
elif mode=="adjacent":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("frame=21","frame=24")
elif mode=="camera-previous":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("camera-previous=1","camera-previous=0")
elif mode=="camera-blob":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("camera-previous-fields="+"2"*352,"camera-previous-fields="+"f"*352)
elif mode=="entity-previous":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("entity-previous=1","entity-previous=0")
elif mode=="entity-blob":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("entity-previous-fields="+"5"*184,"entity-previous-fields="+"f"*184)
elif mode=="tuple":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("entity-role=3","entity-role=4")
elif mode=="camera-drift":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("camera-current="+"2"*352,"camera-current="+"3"*352)
elif mode=="commit":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("entity-committed=1","entity-committed=0")
elif mode=="counts":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("scans=1","scans=0")
elif mode=="world":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("world=0","world=1")
elif mode=="load":R.pop(find("Sys_LoadLibrary(gameclarm64.dylib):"))
elif mode=="late-slot":R.append(R.pop(find("temporal-entity-slot")))
elif mode=="non-player":R[find("temporal-continuity",1)]["msg"]=R[find("temporal-continuity",1)]["msg"].replace("entity-role=3","entity-role=7")
elif mode=="zero-hmodel":R[find("temporal-continuity",1)]["msg"]=R[find("temporal-continuity",1)]["msg"].replace("entity-current="+"5"*184,"entity-current="+"0"*8+"5"*176)
elif mode=="zero-topology":R[find("temporal-continuity",1)]["msg"]=R[find("temporal-continuity",1)]["msg"].replace("entity-current="+"5"*184,"entity-current="+"5"*48+"0"*8+"5"*128)
elif mode=="severity":R.append(row("ERROR","renderer.temporal","synthetic"))
elif mode=="smuggle":R.append(row("INFO","system","benign\rtemporal-continuity schema=1"))
elif mode=="resolve-smuggle":R.append(row("INFO","system","benign\rtemporal-resolved-hdr schema=2"))
elif mode=="feedback-smuggle":R.append(row("INFO","system","benign\rtemporal-history-feedback schema=1"))
elif mode=="prime-missing":R.pop(find("temporal-resolved-hdr",3))
elif mode=="prime-duplicate":R.insert(find("temporal-resolved-hdr",3),R[find("temporal-resolved-hdr",3)].copy())
elif mode=="prime-prior-token":R[find("temporal-resolved-hdr",3)]["msg"]=R[find("temporal-resolved-hdr",3)]["msg"].replace("prior-token=100","prior-token=109")
elif mode=="prime-prior-frame":R[find("temporal-resolved-hdr",3)]["msg"]=R[find("temporal-resolved-hdr",3)]["msg"].replace("prior-frame=19","prior-frame=18")
elif mode=="prime-history":R[find("temporal-resolved-hdr",3)]["msg"]=R[find("temporal-resolved-hdr",3)]["msg"].replace("history=3:0","history=3:1")
elif mode=="prime-submit":R[find("temporal-resolved-hdr",3)]["msg"]=R[find("temporal-resolved-hdr",3)]["msg"].replace("submit=1","submit=0")
elif mode=="prime-cohort":R[find("temporal-resolved-hdr",3)]["msg"]=R[find("temporal-resolved-hdr",3)]["msg"].replace("world=0","world=1")
elif mode=="prime-feedback-missing":R.pop(find("temporal-history-feedback",2))
elif mode=="prime-feedback-duplicate":R.insert(find("temporal-history-feedback",2),R[find("temporal-history-feedback",2)].copy())
elif mode=="prime-reorder":
 item=R.pop(find("temporal-resolved-hdr",3));R.insert(find("temporal-history-feedback",2)+1,item)
elif mode=="arm-before-prime":
 items=[R.pop(find("temporal-motion-readback")),R.pop(find("temporal-history-consume schema=1 action=armed")),R.pop(find("temporal-resolve-readback schema=1 action=armed"))]
 at=find("temporal-history-feedback",2)
 for item in reversed(items):R.insert(at,item)
elif mode=="projection-history":R[find("temporal-projection",1)]["msg"]=R[find("temporal-projection",1)]["msg"].replace("read=0 write=1","read=1 write=0")
elif mode=="capture-frame-gap":R[find("temporal-resolve-content",2)]["msg"]=R[find("temporal-resolve-content",2)]["msg"].replace("frame=22","frame=23")
elif mode=="capture-feedback-ambiguous":R.insert(find("temporal-history-feedback",3),R[find("temporal-history-feedback",3)].copy())
elif mode=="f2-zero":R[find("temporal-resolve-content",1)]["msg"]=R[find("temporal-resolve-content",1)]["msg"].replace("eligible=5536 accepted=5536 accepted-match=5536 accepted-influence=4096 accepted-motion=2048","eligible=0 accepted=0 accepted-match=0 accepted-influence=0 accepted-motion=0")
elif mode=="gpu-missing":R.pop(find("temporal-motion-content",2))
elif mode=="gpu-submit":R[find("temporal-main-activation",2)]["msg"]=R[find("temporal-main-activation",2)]["msg"].replace("submit=1","submit=0")
elif mode=="gpu-fence":R[find("temporal-motion-content",2)]["msg"]=R[find("temporal-motion-content",2)]["msg"].replace("fence=1","fence=0")
elif mode=="gpu-authority":R[find("temporal-motion-content",2)]["msg"]=R[find("temporal-motion-content",2)]["msg"].replace("target=8","target=9")
elif mode=="gpu-validity":R[find("temporal-motion-content",2)]["msg"]=R[find("temporal-motion-content",2)]["msg"].replace("validity-other=0","validity-other=1")
elif mode=="gpu-nonfinite":R[find("temporal-motion-content",2)]["msg"]=R[find("temporal-motion-content",2)]["msg"].replace("nonfinite=0","nonfinite=1")
elif mode=="gpu-nonzero":R[find("temporal-motion-content",2)]["msg"]=R[find("temporal-motion-content",2)]["msg"].replace("nonzero-valid=2048","nonzero-valid=0")
elif mode=="gpu-slot":R[find("temporal-main-activation",2)]["msg"]=R[find("temporal-main-activation",2)]["msg"].replace("slot=0","slot=1")
elif mode=="gpu-serial-gap":
 R[find("temporal-main-activation",2)]["msg"]=R[find("temporal-main-activation",2)]["msg"].replace("serial=2","serial=3")
 R[find("temporal-motion-content",2)]["msg"]=R[find("temporal-motion-content",2)]["msg"].replace("serial=2","serial=3")
elif mode=="gpu-roi":R[find("temporal-motion-content",2)]["msg"]=R[find("temporal-motion-content",2)]["msg"].replace("roi=512,232,256x256","roi=511,232,256x256")
elif mode=="gpu-hash":R[find("temporal-motion-content",2)]["msg"]=R[find("temporal-motion-content",2)]["msg"].replace("velocity-hash=aaaaaaaaaaaaaaaa","velocity-hash=0000000000000000")
elif mode=="iqm-activation-missing":R.pop(find("temporal-iqm-activation",2))
elif mode=="iqm-count":R[find("temporal-iqm-activation",2)]["msg"]=R[find("temporal-iqm-activation",2)]["msg"].replace("iqm=1:1:0:1:","iqm=1:1:0:2:")
elif mode=="iqm-tagged":R[find("temporal-iqm-activation",2)]["msg"]=R[find("temporal-iqm-activation",2)]["msg"].replace(":3:2:1 factory=",":4:2:1 factory=")
elif mode=="iqm-factory":R[find("temporal-iqm-activation",2)]["msg"]=R[find("temporal-iqm-activation",2)]["msg"].replace("factory=17:18:3","factory=17:18:4")
elif mode=="iqm-factory-max":R[find("temporal-iqm-activation",2)]["msg"]=R[find("temporal-iqm-activation",2)]["msg"].replace("factory=17:18:3","factory=4294967295:18:3")
elif mode=="iqm-payload-missing":R.pop(find("temporal-iqm-payload",2))
elif mode=="iqm-records":R[find("temporal-iqm-payload",2)]["msg"]=R[find("temporal-iqm-payload",2)]["msg"].replace("records=1","records=2")
elif mode=="iqm-owner":R[find("temporal-iqm-payload",2)]["msg"]=R[find("temporal-iqm-payload",2)]["msg"].replace("owner=19","owner=20")
elif mode=="iqm-prepare-zero":R[find("temporal-iqm-payload",2)]["msg"]=R[find("temporal-iqm-payload",2)]["msg"].replace("prepare=2","prepare=0")
elif mode=="iqm-content-zero":R[find("temporal-iqm-payload",2)]["msg"]=R[find("temporal-iqm-payload",2)]["msg"].replace("content=cccccccccccccccc","content=0000000000000000")
elif mode=="iqm-palette-chain":R[find("temporal-iqm-payload",2)]["msg"]=R[find("temporal-iqm-payload",2)]["msg"].replace("previous=1111111111111111","previous=3333333333333333")
elif mode=="iqm-palette-static":R[find("temporal-iqm-payload",2)]["msg"]=R[find("temporal-iqm-payload",2)]["msg"].replace("previous=1111111111111111","previous=2222222222222222")
elif mode=="iqm-raster-nonfinite":R[find("temporal-iqm-payload",2)]["msg"]=R[find("temporal-iqm-payload",2)]["msg"].replace("raster=3f800000","raster=7fc00000",1)
elif mode=="iqm-sample-missing":R.pop(find("temporal-resolve-sample",2))
elif mode=="iqm-sample-color":R[find("temporal-resolve-sample",2)]["msg"]=R[find("temporal-resolve-sample",2)]["msg"].replace("color=0000:3c00:0000:3c00","color=0000:0000:0000:0000")
elif mode=="iqm-sample-depth":R[find("temporal-resolve-sample",2)]["msg"]=R[find("temporal-resolve-sample",2)]["msg"].replace("depth=4:3f000000","depth=4:00000000")
elif mode=="iqm-sample-velocity":R[find("temporal-resolve-sample",2)]["msg"]=R[find("temporal-resolve-sample",2)]["msg"].replace("velocity=2c00:0000","velocity=0000:0000")
elif mode=="iqm-sample-validity":R[find("temporal-resolve-sample",2)]["msg"]=R[find("temporal-resolve-sample",2)]["msg"].replace("validity=255","validity=0")
elif mode=="fixture-animation":R.pop(find("IQM h5a_fixture: mapped"))
elif mode=="fixture-profile":R.pop(find("CG_LoadCharacter: loaded profile=h5a_fixture"))
elif mode=="fixture-vbo":R.pop(find("IQM GPU skinning VBO:"))
elif mode=="resolve-missing":R.pop(find("temporal-resolve-content",2))
elif mode=="resolve-content-serial":R[find("temporal-resolve-content",2)]["msg"]=R[find("temporal-resolve-content",2)]["msg"].replace("content=502","content=503")
elif mode=="resolve-scene":R[find("temporal-resolve-content",2)]["msg"]=R[find("temporal-resolve-content",2)]["msg"].replace("scene=5","scene=6")
elif mode=="resolve-depth":R[find("temporal-resolve-content",2)]["msg"]=R[find("temporal-resolve-content",2)]["msg"].replace("depth=4","depth=3")
elif mode=="resolve-submit":R[find("temporal-resolve-readback schema=1 action=submitted",2)]["msg"]=R[find("temporal-resolve-readback schema=1 action=submitted",2)]["msg"].replace("content-submit=1","content-submit=0")
elif mode=="resolve-influence":R[find("temporal-resolve-content",2)]["msg"]=R[find("temporal-resolve-content",2)]["msg"].replace("accepted-influence=4096","accepted-influence=5537")
elif mode=="resolve-overcount":R[find("temporal-resolve-content",2)]["msg"]=R[find("temporal-resolve-content",2)]["msg"].replace("fallback=59477/59477","fallback=59478/59478")
elif mode=="resolve-unsupported-partition":R[find("temporal-resolve-content",2)]["msg"]=R[find("temporal-resolve-content",2)]["msg"].replace("unsupported=523","unsupported=524")
elif mode=="resolve-unsupported-count":R[find("temporal-resolve-content",2)]["msg"]=R[find("temporal-resolve-content",2)]["msg"].replace("unsupported=523","unsupported=-1")
elif mode=="resolve-motion-bound":R[find("temporal-resolve-content",2)]["msg"]=R[find("temporal-resolve-content",2)]["msg"].replace("accepted-motion=2048","accepted-motion=6000")
elif mode=="resolve-validity":R[find("temporal-resolve-content",2)]["msg"]=R[find("temporal-resolve-content",2)]["msg"].replace("validity=546400:5536:0","validity=546399:5536:1")
elif mode=="resolve-hash":R[find("temporal-resolve-content",2)]["msg"]=R[find("temporal-resolve-content",2)]["msg"].replace("7777777777777777","0000000000000000")
elif mode=="resolve-cross-motion":
 for prefix in ("temporal-resolve-readback schema=1 action=submitted","temporal-resolve-content"):
  R[find(prefix,2)]["msg"]=R[find(prefix,2)]["msg"].replace("motion-target=8","motion-target=9")
elif mode=="resolve-cross-history":
 for prefix in ("temporal-resolve-readback schema=1 action=submitted","temporal-resolve-content"):
  R[find(prefix,2)]["msg"]=R[find(prefix,2)]["msg"].replace("history=3:0","history=4:0")
elif mode=="resolve-cross-token":
 for prefix in ("temporal-resolve-readback schema=1 action=submitted","temporal-resolve-content"):
  R[find(prefix,2)]["msg"]=R[find(prefix,2)]["msg"].replace("token=102","token=103")
elif mode=="resolve-routed":R[find("temporal-resolved-hdr",4)]["msg"]=R[find("temporal-resolved-hdr",4)]["msg"].replace("serial=502","serial=503")
elif mode=="resolved-copy":R[find("temporal-resolved-hdr",1)]["msg"]=R[find("temporal-resolved-hdr",1)]["msg"].replace("producer=copy submit=1","producer=copy submit=0")
elif mode=="feedback-copy-token":R[find("temporal-resolved-hdr",2)]["msg"]=R[find("temporal-resolved-hdr",2)]["msg"].replace("token=100","token=109")
elif mode=="feedback-copy-content":R[find("temporal-resolved-hdr",2)]["msg"]=R[find("temporal-resolved-hdr",2)]["msg"].replace("serial=500","serial=509")
elif mode=="feedback-copy-slot":R[find("temporal-resolved-hdr",2)]["msg"]=R[find("temporal-resolved-hdr",2)]["msg"].replace("slot=1","slot=0")
elif mode=="feedback-seed-missing":R.pop(find("temporal-history-feedback",1))
elif mode=="feedback-source":R[find("temporal-history-feedback",2)]["msg"]=R[find("temporal-history-feedback",2)]["msg"].replace("content=501","content=509")
elif mode=="feedback-prior":
 for prefix in ("temporal-resolve-readback schema=1 action=submitted","temporal-resolve-content"):
  R[find(prefix,2)]["msg"]=R[find(prefix,2)]["msg"].replace("prior-store-owner=13","prior-store-owner=14")
elif mode=="feedback-prior-token":
 for prefix in ("temporal-resolve-readback schema=1 action=submitted","temporal-resolve-content"):
  R[find(prefix,2)]["msg"]=R[find(prefix,2)]["msg"].replace("prior-token=102","prior-token=109")
elif mode=="feedback-prior-frame":
 substitute=row("INFO","renderer.ral","temporal-history-feedback schema=1 token=109 frame=20 world=0 extent=1280x720 topology=3 plan=4 producer=resolved-feedback content=509 scene=5 target=8 resolve-owner=12 store-owner=13 history=3:1 slot=0 frame-count=2 commit=1")
 R.insert(find("temporal-resolve-readback schema=1 action=submitted",2),substitute)
 for prefix in ("temporal-resolve-readback schema=1 action=submitted","temporal-resolve-content"):
  R[find(prefix,2)]["msg"]=R[find(prefix,2)]["msg"].replace("prior-token=102 prior-frame=21 prior-content=502","prior-token=109 prior-frame=20 prior-content=509")
elif mode=="feedback-substitute":
 substitute=row("INFO","renderer.ral","temporal-history-feedback schema=1 token=109 frame=20 world=0 extent=1280x720 topology=3 plan=4 producer=resolved-feedback content=509 scene=5 target=8 resolve-owner=12 store-owner=13 history=3:1 slot=0 frame-count=2 commit=1")
 R.insert(find("temporal-resolve-readback schema=1 action=submitted",1),substitute)
 for prefix in ("temporal-resolve-readback schema=1 action=submitted","temporal-resolve-content"):
  R[find(prefix,1)]["msg"]=R[find(prefix,1)]["msg"].replace("prior-producer=2 prior-token=101 prior-frame=20 prior-content=501 prior-scene=5 prior-target=8 prior-resolve-owner=12","prior-producer=2 prior-token=109 prior-frame=20 prior-content=509 prior-scene=5 prior-target=8 prior-resolve-owner=12")
elif mode=="feedback-world":R[find("temporal-history-feedback",2)]["msg"]=R[find("temporal-history-feedback",2)]["msg"].replace("world=0","world=1")
elif mode=="feedback-extent":R[find("temporal-history-feedback",2)]["msg"]=R[find("temporal-history-feedback",2)]["msg"].replace("extent=1280x720","extent=1279x720")
elif mode=="feedback-topology":R[find("temporal-history-feedback",2)]["msg"]=R[find("temporal-history-feedback",2)]["msg"].replace("topology=3","topology=4")
elif mode=="feedback-plan":R[find("temporal-history-feedback",2)]["msg"]=R[find("temporal-history-feedback",2)]["msg"].replace("plan=4","plan=5")
elif mode=="feedback-history":R[find("temporal-history-feedback",2)]["msg"]=R[find("temporal-history-feedback",2)]["msg"].replace("history=3:1","history=4:1")
elif mode=="feedback-index":R[find("temporal-history-feedback",2)]["msg"]=R[find("temporal-history-feedback",2)]["msg"].replace("history=3:1","history=3:0")
elif mode=="seed-world":R[find("temporal-history-feedback",1)]["msg"]=R[find("temporal-history-feedback",1)]["msg"].replace("world=0","world=1")
elif mode=="seed-extent":R[find("temporal-history-feedback",1)]["msg"]=R[find("temporal-history-feedback",1)]["msg"].replace("extent=1280x720","extent=1279x720")
elif mode=="seed-topology":R[find("temporal-history-feedback",1)]["msg"]=R[find("temporal-history-feedback",1)]["msg"].replace("topology=3","topology=4")
elif mode=="seed-plan":R[find("temporal-history-feedback",1)]["msg"]=R[find("temporal-history-feedback",1)]["msg"].replace("plan=4","plan=5")
elif mode=="seed-history":R[find("temporal-history-feedback",1)]["msg"]=R[find("temporal-history-feedback",1)]["msg"].replace("history=3:0","history=4:0")
elif mode=="seed-index":R[find("temporal-history-feedback",1)]["msg"]=R[find("temporal-history-feedback",1)]["msg"].replace("history=3:0","history=3:1")
elif mode=="feedback-before":
 item=R.pop(find("temporal-history-feedback",1));R.insert(find("Q3_RAL_TEMPORAL_REQUESTED"),item)
elif mode=="feedback-after":
 item=R.pop(find("temporal-history-feedback",1));R.insert(find("Q3_RAL_TEMPORAL_ENABLED")+1,item)
elif mode=="feedback-slot":R[find("temporal-history-feedback",1)]["msg"]=R[find("temporal-history-feedback",1)]["msg"].replace("frame-count=2","frame-count=1")
elif mode=="feedback-hash":R[find("temporal-resolve-content",2)]["msg"]=R[find("temporal-resolve-content",2)]["msg"].replace("7777777777777777:4444444444444444","aaaaaaaaaaaaaaaa:4444444444444444")
elif mode=="feedback-raw-current":
 R[find("temporal-resolve-content",1)]["msg"]=R[find("temporal-resolve-content",1)]["msg"].replace("7777777777777777 ready=1","1111111111111111 ready=1")
 R[find("temporal-resolve-content",2)]["msg"]=R[find("temporal-resolve-content",2)]["msg"].replace("7777777777777777:4444444444444444","1111111111111111:4444444444444444")
elif mode=="history-missing":R.pop(find("temporal-history-content",2))
elif mode=="history-bootstrap":R[find("temporal-history-content",1)]["msg"]=R[find("temporal-history-content",1)]["msg"].replace("history-valid=1","history-valid=0")
elif mode=="history-alias":R[find("temporal-history-content",2)]["msg"]=R[find("temporal-history-content",2)]["msg"].replace("read=0 write=1","read=0 write=0")
elif mode=="history-zero":
 R[find("temporal-history-content",1)]["msg"]=R[find("temporal-history-content",1)]["msg"].replace("current=11111111:22222222:33333333","current=00000000:00000000:3f800000")
 R[find("temporal-history-content",2)]["msg"]=R[find("temporal-history-content",2)]["msg"].replace("previous=11111111:22222222:33333333","previous=00000000:00000000:3f800000")
elif mode=="history-static":R[find("temporal-history-content",2)]["msg"]=R[find("temporal-history-content",2)]["msg"].replace("current=44444444:55555555:66666666","current=11111111:22222222:33333333")
elif mode=="timing-missing":R.pop(find('"timescale" is:',1))
elif mode=="timing-value":R[find('"timescale" is:',1)]["msg"]=R[find('"timescale" is:',1)]["msg"].replace("0.025","0.1")
elif mode=="timing-fixed":R[find('"fixedtime" is:',1)]["msg"]=R[find('"fixedtime" is:',1)]["msg"].replace('is:"1^7"','is:"0^7"')
elif mode=="timing-clrun":R[find('"cl_run" is:',1)]["msg"]=R[find('"cl_run" is:',1)]["msg"].replace('is:"1^7"','is:"0^7"')
elif mode=="timing-order":
 a=find('"timescale" is:',1);b=find('"fixedtime" is:',1);R[a],R[b]=R[b],R[a]
elif mode=="timing-cleanup":R.pop(find('"timescale" is:',2))
elif mode=="timing-default":R[find('"timescale" is:',1)]["msg"]=R[find('"timescale" is:',1)]["msg"].replace('default:"1^7"','default:"0^7"')
elif mode=="timing-svfps":R[find('"sv_fps" is:',1)]["msg"]=R[find('"sv_fps" is:',1)]["msg"].replace('is:"20^7"','is:"30^7"')
elif mode=="spawn-protect":R[find('"g_spawnProtect" is:',1)]["msg"]=R[find('"g_spawnProtect" is:',1)]["msg"].replace('is:"0^7"','is:"2^7"')
elif mode=="window-extent":R[find("window-extent",1)]["msg"]=R[find("window-extent",1)]["msg"].replace("logical=1280x720","logical=640x480")
with open(log_path,"w") as out:
 for x in R:out.write(json.dumps(x)+"\n")
scenario={"kind":"scenario","schema":11,"name":"ral-temporal-projection","map":"arena7","backend":"vulkan","cgame":"native-vm_cgame-0","toggle":"latched-r_entitySSBO=1;isolated-r_temporalInputTest:0->1->0;g_spawnProtect=0;sv_fps=20;fixedtime:0->1->0;timescale:1->0.025->1;cl_run=1;stationary-third-person-camera;h5a-fixture-root-y-animation","consumer":"synthetic-fixed-1ms-per-render-tick-stationary-camera-primary-world-projection-history-IQM-palette-chain-tagged-activation-fenced-motion-history-h3-resolve-center-sample-and-recursive-feedback-commit","nonclaims":["validation-backed-vuid-cleanliness","full-frame-resolve-proof","visual-quality-proof","whole-image-motion-coverage","reference-correct-vector-field","first-recursive-frame-pixel-influence","view-invariant-depth-disocclusion","arbitrary-camera-translation","exact-0.025-physical-motion-ratio","ordinary-default-wall-clock-timing","per-frame-snapshot-cadence","production-timing-quality","single-frame-bootstrap-fence-readback","more-than-one-recursive-link","long-run-stability","camera-cuts","letterboxed-or-multiview-history","screenmap-replay-runtime","split-screen-runtime","wasm-cgame-runtime","future-normal-map-IQM-semantics"]}
M=[scenario]
for role in ("gui","renderer","gamecl-native","gamesv-native","moltenvk","pax21","base","iqm-generator","iqm-model","iqm-character-manifest","iqm-texture","harness","bootstrap-cfg"):
 path=manifest_path+"."+role;data=boot.encode() if role=="bootstrap-cfg" else ("fixture-"+role).encode();open(path,"wb").write(data);M.append({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
M.append({"kind":"result","controller_pid":101,"rc":0,"timeout":False,"forced":False})
if mode=="manifest":M[2]["sha256"]="0"*64
elif mode=="scenario":M[0]["schema"]=7
elif mode=="result":M[-1]["rc"]=1
elif mode=="gamecl-role":M[3]["role"]="gamecl"
elif mode=="bootstrap":
 data=b"wrong\n";open(M[-2]["path"],"wb").write(data);M[-2]["bytes"]=len(data);M[-2]["sha256"]=hashlib.sha256(data).hexdigest()
elif mode=="camera-input":
 data=boot.replace("noclip; wait 64; echo Q3_RAL_TEMPORAL_REQUESTED","noclip; wait 64; +forward; echo Q3_RAL_TEMPORAL_REQUESTED").encode();open(M[-2]["path"],"wb").write(data);M[-2]["bytes"]=len(data);M[-2]["sha256"]=hashlib.sha256(data).hexdigest()
elif mode=="assets-log":
 data=boot.replace("log renderer.assets debug\n","").encode();open(M[-2]["path"],"wb").write(data);M[-2]["bytes"]=len(data);M[-2]["sha256"]=hashlib.sha256(data).hexdigest()
elif mode=="fixture-provenance":M[9]["sha256"]="0"*64
elif mode=="generator-provenance":M[8]["sha256"]="0"*64
with open(manifest_path,"w") as out:
 for x in M:out.write(json.dumps(x,sort_keys=True)+"\n")
PYEOF
}

if [ "${1:-}" = --analyze ];then
 [ -f "${2:-}" ] && [ -f "${3:-}" ] || { echo "usage: $0 --analyze qconsole.jsonl manifest.jsonl";exit 64; }
 analyze_contract "$2" "$3"
 exit $?
fi

if [ "${1:-}" = --self-test ];then
 ROOT="$(mktemp -d -t ral-temporal-self-XXXXXX 2>/dev/null || mktemp -d)";trap 'rm -rf "$ROOT"' EXIT
 write_self "$ROOT/clean.log" "$ROOT/clean.manifest" clean || exit 1;analyze_contract "$ROOT/clean.log" "$ROOT/clean.manifest" >/dev/null || exit 1
 defects=(cap-missing cap-trap slot-missing slot-export vm continuity-missing adjacent camera-previous camera-blob entity-previous entity-blob tuple camera-drift commit counts world load late-slot non-player zero-hmodel zero-topology severity smuggle resolve-smuggle feedback-smuggle prime-missing prime-duplicate prime-prior-token prime-prior-frame prime-history prime-submit prime-cohort prime-feedback-missing prime-feedback-duplicate prime-reorder arm-before-prime projection-history capture-frame-gap capture-feedback-ambiguous f2-zero resolved-copy gpu-missing gpu-submit gpu-fence gpu-authority gpu-validity gpu-nonfinite gpu-nonzero gpu-slot gpu-serial-gap gpu-roi gpu-hash iqm-activation-missing iqm-count iqm-tagged iqm-factory iqm-factory-max iqm-payload-missing iqm-records iqm-owner iqm-prepare-zero iqm-content-zero iqm-palette-chain iqm-palette-static iqm-raster-nonfinite iqm-sample-missing iqm-sample-color iqm-sample-depth iqm-sample-velocity iqm-sample-validity fixture-animation fixture-profile fixture-vbo resolve-missing resolve-content-serial resolve-scene resolve-depth resolve-submit resolve-influence resolve-overcount resolve-unsupported-partition resolve-unsupported-count resolve-motion-bound resolve-validity resolve-hash resolve-cross-motion resolve-cross-history resolve-cross-token resolve-routed feedback-copy-token feedback-copy-content feedback-copy-slot feedback-seed-missing feedback-source feedback-prior feedback-prior-token feedback-prior-frame feedback-substitute feedback-world feedback-extent feedback-topology feedback-plan feedback-history feedback-index seed-world seed-extent seed-topology seed-plan seed-history seed-index feedback-before feedback-after feedback-slot feedback-hash feedback-raw-current history-missing history-bootstrap history-alias history-zero history-static manifest scenario result gamecl-role bootstrap camera-input assets-log fixture-provenance generator-provenance timing-missing timing-value timing-fixed timing-clrun timing-order timing-cleanup timing-default timing-svfps spawn-protect window-extent)
 for d in "${defects[@]}";do write_self "$ROOT/$d.log" "$ROOT/$d.manifest" "$d" || exit 1;if analyze_contract "$ROOT/$d.log" "$ROOT/$d.manifest" >/dev/null 2>&1;then echo "FAIL accepted $d";exit 1;fi;done
 echo "PASS ral-temporal analyzer self-test (${#defects[@]} mutations)";exit 0
fi

WIRED="${1:-}";[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired";exit 64; }
[ "$(uname -s)" = Darwin ] || { echo "SKIP: temporal runtime currently requires macOS/MoltenVK";exit 77; }
[ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: missing timeout runner";exit 77; }
[ -f "$IQM_FIXTURE" ] || { echo "SKIP: missing temporal IQM fixture builder";exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")";WD="$(dirname "$WIRED")"
# A stale client binary can accept the explicit 1280x720 arguments above yet
# still fall through its legacy mode-3 recovery and publish a 640x480 SDL
# window.  Refuse such artifacts before SDL/video initialization: native
# evidence may run only with the hidden-until-validated runtime guard compiled
# into the client.
if ! grep -aFq "Automated window extent fell below 1280x720" "$WIRED";then
	echo "SKIP: wired binary predates the mandatory widescreen runtime guard"
	exit 77
fi
find_required(){ local name="$1" candidate;shift;for candidate in "$@";do [ -f "$candidate/$name" ] && { printf '%s\n' "$candidate/$name";return 0;};done;return 1;}
RENDERER="${WIRED_RENDERER:-}";[ -f "$RENDERER" ] || RENDERER="$(find_required wired_vulkan_arm64.dylib "$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS")" || { echo "SKIP: set WIRED_RENDERER to the current Vulkan renderer";exit 77; }
GAMECL="${WIRED_GAMECL:-}";[ -f "$GAMECL" ] || GAMECL="$(find_required gameclarm64.dylib "$WD/base" "$WD/Contents/Resources/base" "$WD/../Resources/base" "$WD/q3now-preview.arm64.app/Contents/Resources/base")" || { echo "SKIP: set WIRED_GAMECL to current native gameclarm64.dylib";exit 77; }
GAMESV="${WIRED_GAMESV:-}";[ -f "$GAMESV" ] || GAMESV="$(find_required gamesvarm64.dylib "$WD/base" "$WD/Contents/Resources/base" "$WD/../Resources/base" "$WD/q3now-preview.arm64.app/Contents/Resources/base")" || { echo "SKIP: set WIRED_GAMESV to current native gamesvarm64.dylib";exit 77; }
MOLTEN="$(find_required libMoltenVK.dylib "$WD" "$WD/.." "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS")" || { echo "SKIP: current MoltenVK unavailable";exit 77; }
PACK="";for candidate in "${WIRED_CONTENT_ROOT:-}" "$WD" "$WD/../Resources" "$WD/../../.." "$WD/q3now-preview.arm64.app/Contents/Resources";do [ -n "$candidate" ] && [ -f "$candidate/base/pax21.sw3z" ] && PACK="$(cd "$candidate" && pwd)" && break;done;[ -n "$PACK" ] || { echo "SKIP: current pax21 unavailable";exit 77; }
CONTENT="${WIRED_CONTENT_ROOT:-$PACK}";if [ -f "$CONTENT/base/pax01.sw3z" ];then BASE="$CONTENT/base/pax01.sw3z";elif [ -f "$CONTENT/base/pak0.pk3" ];then BASE="$CONTENT/base/pak0.pk3";else echo "SKIP: set WIRED_CONTENT_ROOT";exit 77;fi
ROOT="$(mktemp -d -t ral-temporal-projection-XXXXXX 2>/dev/null || mktemp -d)";HOME_DIR="$ROOT/home";RUN="$ROOT/runtime";FORCED=0
cleanup(){ local status=$?;trap - EXIT INT TERM;[ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT";exit "$status";};trap cleanup EXIT;trap 'exit 130' INT;trap 'exit 143' TERM
mkdir -p "$HOME_DIR/base" "$RUN/Contents/MacOS";python3 "$IQM_FIXTURE" "$HOME_DIR/base" || exit 1;cp "$PACK/base/pax21.sw3z" "$HOME_DIR/base/" || exit 1;cp "$BASE" "$HOME_DIR/base/" || exit 1;cp "$GAMECL" "$HOME_DIR/base/gameclarm64.dylib" || exit 1;cp "$GAMESV" "$HOME_DIR/base/gamesvarm64.dylib" || exit 1;cp "$WIRED" "$RUN/wired" || exit 1;chmod +x "$RUN/wired";cp "$RENDERER" "$MOLTEN" "$RUN/Contents/MacOS/" || exit 1
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

BOOT="$HOME_DIR/base/ral-temporal-projection.cfg";printf '%s' "$BOOT_CONTENT" >"$BOOT"
MANIFEST="$ROOT/manifest.jsonl";python3 - "$MANIFEST" "$RUN/wired" "$RUN/Contents/MacOS/wired_vulkan_arm64.dylib" "$HOME_DIR/base/gameclarm64.dylib" "$HOME_DIR/base/gamesvarm64.dylib" "$RUN/Contents/MacOS/libMoltenVK.dylib" "$HOME_DIR/base/pax21.sw3z" "$HOME_DIR/base/$(basename "$BASE")" "$IQM_FIXTURE" "$HOME_DIR/base/characters/h5a_fixture/models/body.iqm" "$HOME_DIR/base/characters/h5a_fixture/main.lua" "$HOME_DIR/base/textures/h5a/fixture.tga" "$0" "$BOOT" <<'PYEOF'
import hashlib,json,os,sys
scenario={"kind":"scenario","schema":11,"name":"ral-temporal-projection","map":"arena7","backend":"vulkan","cgame":"native-vm_cgame-0","toggle":"latched-r_entitySSBO=1;isolated-r_temporalInputTest:0->1->0;g_spawnProtect=0;sv_fps=20;fixedtime:0->1->0;timescale:1->0.025->1;cl_run=1;stationary-third-person-camera;h5a-fixture-root-y-animation","consumer":"synthetic-fixed-1ms-per-render-tick-stationary-camera-primary-world-projection-history-IQM-palette-chain-tagged-activation-fenced-motion-history-h3-resolve-center-sample-and-recursive-feedback-commit","nonclaims":["validation-backed-vuid-cleanliness","full-frame-resolve-proof","visual-quality-proof","whole-image-motion-coverage","reference-correct-vector-field","first-recursive-frame-pixel-influence","view-invariant-depth-disocclusion","arbitrary-camera-translation","exact-0.025-physical-motion-ratio","ordinary-default-wall-clock-timing","per-frame-snapshot-cadence","production-timing-quality","single-frame-bootstrap-fence-readback","more-than-one-recursive-link","long-run-stability","camera-cuts","letterboxed-or-multiview-history","screenmap-replay-runtime","split-screen-runtime","wasm-cgame-runtime","future-normal-map-IQM-semantics"]}
with open(sys.argv[1],"w") as out:
 out.write(json.dumps(scenario,sort_keys=True)+"\n")
 for role,path in zip(("gui","renderer","gamecl-native","gamesv-native","moltenvk","pax21","base","iqm-generator","iqm-model","iqm-character-manifest","iqm-texture","harness","bootstrap-cfg"),sys.argv[2:]):
  data=open(path,"rb").read();out.write(json.dumps({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()},sort_keys=True)+"\n")
PYEOF
QCONSOLE="$HOME_DIR/qconsole.jsonl";STDOUT="$ROOT/stdout.log"
python3 "$TIMEOUT_RUNNER" --timeout 120 --kill-after 15 --cwd "$RUN" --stdout "$STDOUT" -- "$RUN/wired" +set fs_basepath "$HOME_DIR" +set fs_homepath "$HOME_DIR" +set fs_game base +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set r_vkValidate 0 +set r_entitySSBO 1 +set r_temporalInputTest 0 +set r_bloom 0 +set r_ssao 0 +set r_smaa 0 +set r_forwardPlus 0 +set r_drawSunRays 0 +set r_shadows 0 +set r_depthFade 0 +set r_lens 0 +set r_gpuDecals 0 +set r_particles 0 +set r_atmosphericGPU 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec ral-temporal-projection.cfg
RC=$?;TIMEOUT=false;[ "$RC" -eq 124 ] && TIMEOUT=true;python3 - "$MANIFEST" "$$" "$RC" "$TIMEOUT" "$FORCED" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a") as out:out.write(json.dumps({"kind":"result","controller_pid":int(sys.argv[2]),"rc":int(sys.argv[3]),"timeout":sys.argv[4]=="true","forced":sys.argv[5]=="1"},sort_keys=True)+"\n")
PYEOF
[ -f "$QCONSOLE" ] || { echo "FAIL missing qconsole; retained root: $ROOT";WIRED_KEEP_ARTIFACTS=1;exit 1; };analyze_contract "$QCONSOLE" "$MANIFEST" || { echo "FAIL retained root: $ROOT";WIRED_KEEP_ARTIFACTS=1;exit 1; }
echo "PASS retained root: $ROOT"
