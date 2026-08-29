#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Native two-map proof for portable dynamic offsets in procedural-effect draws.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze() {
	python3 - "$@" <<'PYEOF'
import json,re,sys
rows=[]
for path in sys.argv[1:]:
 for number,line in enumerate(open(path,encoding="utf-8",errors="strict"),1):
  if not line.strip():continue
  try:row=json.loads(line)
  except Exception as exc:raise SystemExit(f"FAIL effects smoke JSON {path}:{number}: {exc}")
  if not isinstance(row,dict):raise SystemExit("FAIL effects smoke log schema")
  rows.append(row)
messages=[str(row.get("msg","")).rstrip("\n") for row in rows]
if any(str(row.get("sev","")).upper() in ("ERROR","FATAL") for row in rows):raise SystemExit("FAIL effects smoke error severity")
if any("VUID-" in msg or "died on signal" in msg for msg in messages):raise SystemExit("FAIL effects smoke VUID/crash")
maps=("arena1","arena17");families=("ribbon","rail-ribbon","beam","sprite","particle","decal","atmospheric")
pat=re.compile(r"ral-effects schema=1 map=([^ ]+) family=([^ ]+) draws=([0-9]+) dynamic-offsets=([01])")
compute_pat=re.compile(r"ral-particle-compute schema=1 map=([^ ]+) dispatches=([0-9]+) barrier=([^ ]+)")
graph_pat=re.compile(r"ral-atmosphere-effect-graph schema=1 map=([^ ]+) profile=64 stages=3 root-requests=([1-9][0-9]*) root-particles=([1-9][0-9]*) collision=1 death=1 class=64 max-particles=24 compute-dispatches=4 barrier=compute-to-graphics")
full_atmosphere_pat=re.compile(r"ral-atmosphere-full schema=1 map=([^ ]+) tier=full froxels=([1-9][0-9]*) volumes=1 lights=([0-9]+) dispatches=5 clouds=1 coverage-milli=650 composite=1 history=(rejected|reused) scene-hdr=copy-back")
weather_pat=re.compile(r"ral-atmosphere-weather schema=1 map=([^ ]+) family-mask=31 active-particles=6144 exposure-milli=750 heightgrid=1 roof=split depth-soft=1")
fixture_pat=re.compile(r"ral-atmosphere-fixture schema=1 map=([^ ]+) fixture=(clear|rain|snow|cold|fog|storm) tier=([0-3]) family-mask=([0-9]+) weather-particles=([0-9]+) froxels=([0-9]+) dispatches=([0-9]+) clouds=([0-9]+) semantic-particles=([0-9]+)")
impact_pat=re.compile(r"ral-atmosphere-impact schema=1 map=([^ ]+) gpu-events=([1-9][0-9]*) gpu-spawned=([1-9][0-9]*) dropped-events=([0-9]+) dropped-particles=([0-9]+) readback-bytes=64")
atmospheric_compute_pat=re.compile(r"ral-atmospheric-compute schema=1 map=([^ ]+) dispatches=([0-9]+) barrier=([^ ]+)")
atmospheric_frame_pat=re.compile(r"ral-atmospheric-frame schema=1 map=([^ ]+) slot=([01]) owner-generation=([1-9][0-9]*) slot-generation=([1-9][0-9]*) compute-write=([1-9][0-9]*) final-write=([1-9][0-9]*) content-hash=([1-9][0-9]*)")
frame_uniform_pat=re.compile(r"ral-frame-uniform schema=1 map=([^ ]+) family=(particle|decal) slot=([01]) owner-generation=([1-9][0-9]*) slot-generation=([1-9][0-9]*) partial-write=([0-9]+) final-write=([1-9][0-9]*) content-hash=([1-9][0-9]*)")
shadow_pat=re.compile(r"ral-shadow-storage schema=1 map=([^ ]+) family=(particle-pool|particle-classes|decal-pool) buffer=([01]) owner-generation=([1-9][0-9]*) shadow-generation=([1-9][0-9]*) flush-generation=([1-9][0-9]*) dirty-elements=([0-9]+) writes=([0-9]+) content-hash=([0-9]+) write-digest=([0-9]+) wrote=([01])")
clear_pat=re.compile(r"ral-storage-clear schema=1 map=([^ ]+) family=(cull|forward-plus|hdr-histogram|particle-child-budgets) bytes=([1-9][0-9]*) visibility=transfer-to-compute")
seed_pat=re.compile(r"ral-storage-seed schema=1 map=([^ ]+) family=hdr-exposure bytes=4 bits=0x3f800000 transfer=completed graphics-visible=1")
found={}
compute_found={}
graph_found={}
full_atmosphere_found={}
weather_found={}
fixture_found={}
impact_found={}
atmospheric_compute_found={}
atmospheric_frame_found={}
frame_uniform_found={}
shadow_found={}
clear_found={}
seed_found={}
for index,msg in enumerate(messages):
 m=pat.fullmatch(msg)
 if m:
  key=(m.group(1),m.group(2));found.setdefault(key,[]).append((index,int(m.group(3)),int(m.group(4))))
 m=compute_pat.fullmatch(msg)
 if m:compute_found.setdefault(m.group(1),[]).append((index,int(m.group(2)),m.group(3)))
 m=graph_pat.fullmatch(msg)
 if m:graph_found.setdefault(m.group(1),[]).append((index,int(m.group(2)),int(m.group(3))))
 m=full_atmosphere_pat.fullmatch(msg)
 if m:full_atmosphere_found.setdefault(m.group(1),[]).append((index,int(m.group(2)),int(m.group(3)),m.group(4)))
 m=weather_pat.fullmatch(msg)
 if m:weather_found.setdefault(m.group(1),[]).append(index)
 m=fixture_pat.fullmatch(msg)
 if m:fixture_found.setdefault(m.group(1),{})[m.group(2)]=(index,)+tuple(map(int,m.groups()[2:]))
 m=impact_pat.fullmatch(msg)
 if m:impact_found.setdefault(m.group(1),[]).append((index,)+tuple(map(int,m.groups()[1:])))
 m=atmospheric_compute_pat.fullmatch(msg)
 if m:atmospheric_compute_found.setdefault(m.group(1),[]).append((index,int(m.group(2)),m.group(3)))
 m=atmospheric_frame_pat.fullmatch(msg)
 if m:atmospheric_frame_found.setdefault(m.group(1),[]).append((index,)+tuple(map(int,m.groups()[1:])))
 m=frame_uniform_pat.fullmatch(msg)
 if m:
  key=(m.group(1),m.group(2));frame_uniform_found.setdefault(key,[]).append((index,)+tuple(map(int,m.groups()[2:])))
 m=shadow_pat.fullmatch(msg)
 if m:
  key=(m.group(1),m.group(2));shadow_found.setdefault(key,[]).append((index,)+tuple(map(int,m.groups()[2:])))
 m=clear_pat.fullmatch(msg)
 if m:clear_found.setdefault((m.group(1),m.group(2)),[]).append((index,int(m.group(3))))
 m=seed_pat.fullmatch(msg)
 if m:seed_found.setdefault(m.group(1),[]).append(index)
for map_name in maps:
 first=[i for i,msg in enumerate(messages) if re.fullmatch(rf"cls\.state: -> CA_ACTIVE \(FIRST GAMEPLAY FRAME mapname=maps/{map_name}\.bsp serverTime=[0-9]+ numEntities=[0-9]+ framecount=[0-9]+\)",msg)]
 if len(first)!=1:raise SystemExit(f"FAIL effects smoke {map_name} gameplay cardinality {len(first)}")
 for family in families:
  hits=found.get((map_name,family),[])
  expected_dynamic=1 if family in ("ribbon","rail-ribbon","beam","sprite") else 0
  if len(hits)!=1 or hits[0][1]<=0 or hits[0][2]!=expected_dynamic:raise SystemExit(f"FAIL effects smoke {map_name}/{family} receipt {hits}")
  if hits[0][0]<=first[0]:raise SystemExit(f"FAIL effects smoke {map_name}/{family} causal order")
 compute=compute_found.get(map_name,[])
 if len(compute)!=1 or compute[0][1]!=1 or compute[0][2]!="compute-to-graphics":raise SystemExit(f"FAIL particle compute {map_name} receipt {compute}")
 graph=graph_found.get(map_name,[])
 if len(graph)!=1:raise SystemExit(f"FAIL atmosphere effect graph {map_name} receipt {graph}")
 if graph[0][0]<=first[0]:raise SystemExit(f"FAIL atmosphere effect graph {map_name} causal order")
 full_atmosphere=full_atmosphere_found.get(map_name,[])
 if len(full_atmosphere)!=1:raise SystemExit(f"FAIL full atmosphere {map_name} receipt {full_atmosphere}")
 if full_atmosphere[0][0]<=first[0]:raise SystemExit(f"FAIL full atmosphere {map_name} causal order")
 weather=weather_found.get(map_name,[])
 if len(weather)!=1:raise SystemExit(f"FAIL atmosphere weather matrix {map_name} receipt {weather}")
 if weather[0]<=first[0]:raise SystemExit(f"FAIL atmosphere weather matrix {map_name} causal order")
 fixtures=fixture_found.get(map_name,{})
 if set(fixtures)!={"clear","rain","snow","cold","fog","storm"}:raise SystemExit(f"FAIL atmosphere fixture matrix {map_name} {sorted(fixtures)}")
 if any(row[0]<=first[0] for row in fixtures.values()):raise SystemExit(f"FAIL atmosphere fixture matrix {map_name} causal order")
 impacts=impact_found.get(map_name,[])
 if len(impacts)!=1 or impacts[0][2]<=0:raise SystemExit(f"FAIL atmosphere impact telemetry {map_name} {impacts}")
 for family in ("particle-pool","particle-classes"):
  storage=shadow_found.get((map_name,family),[])
  if len(storage)!=1:raise SystemExit(f"FAIL shadow storage {map_name}/{family} receipt {storage}")
  if storage[0][9]:
   if min(storage[0][5:9])<=0:raise SystemExit(f"FAIL shadow storage {map_name}/{family} write receipt {storage}")
  elif any(storage[0][5:9]):raise SystemExit(f"FAIL shadow storage {map_name}/{family} clean receipt {storage}")
  if storage[0][0]>=compute[0][0]:raise SystemExit(f"FAIL shadow storage {map_name}/{family} causal order")
 particle=found[(map_name,"particle")][0]
 particle_frame=frame_uniform_found.get((map_name,"particle"),[])
 if len(particle_frame)!=1 or particle_frame[0][4]<=0 or particle_frame[0][5]<=particle_frame[0][4]:raise SystemExit(f"FAIL particle frame {map_name} receipt {particle_frame}")
 if not compute[0][0]<particle_frame[0][0]<particle[0]:raise SystemExit(f"FAIL particle frame {map_name} causal order")
 decal_frame=frame_uniform_found.get((map_name,"decal"),[])
 decal=found[(map_name,"decal")][0]
 decal_storage=shadow_found.get((map_name,"decal-pool"),[])
 if len(decal_storage)!=1:raise SystemExit(f"FAIL shadow storage {map_name}/decal-pool receipt {decal_storage}")
 if decal_storage[0][9]:
  if min(decal_storage[0][5:9])<=0:raise SystemExit(f"FAIL shadow storage {map_name}/decal-pool write receipt {decal_storage}")
 elif any(decal_storage[0][5:9]):raise SystemExit(f"FAIL shadow storage {map_name}/decal-pool clean receipt {decal_storage}")
 if not decal_storage[0][0]<decal[0]:raise SystemExit(f"FAIL decal shadow storage {map_name} causal order")
 if len(decal_frame)!=1 or decal_frame[0][4]!=0 or decal_frame[0][5]<=0:raise SystemExit(f"FAIL decal frame {map_name} receipt {decal_frame}")
 if not decal_frame[0][0]<decal[0]:raise SystemExit(f"FAIL decal frame {map_name} causal order")
 atmospheric_compute=atmospheric_compute_found.get(map_name,[])
 if len(atmospheric_compute)!=1 or atmospheric_compute[0][1]!=1 or atmospheric_compute[0][2]!="compute-to-graphics":raise SystemExit(f"FAIL atmospheric compute {map_name} receipt {atmospheric_compute}")
 atmospheric=found[(map_name,"atmospheric")][0]
 atmospheric_frame=atmospheric_frame_found.get(map_name,[])
 if len(atmospheric_frame)!=1:raise SystemExit(f"FAIL atmospheric frame {map_name} receipt {atmospheric_frame}")
 if atmospheric_frame[0][5]<=atmospheric_frame[0][4]:raise SystemExit(f"FAIL atmospheric frame {map_name} write generations")
 if not atmospheric_compute[0][0]<atmospheric_frame[0][0]<atmospheric[0]:raise SystemExit(f"FAIL atmospheric frame {map_name} causal order")
 for family,expected in (("forward-plus",None),("hdr-histogram",1024),("particle-child-budgets",2304)):
  clears=clear_found.get((map_name,family),[])
  if len(clears)!=1 or clears[0][1]%4 or (expected is not None and clears[0][1]!=expected):raise SystemExit(f"FAIL storage clear {map_name}/{family} receipt {clears}")
 cull=clear_found.get((map_name,"cull"),[])
 if cull and (len(cull)!=1 or cull[0][1]!=4):raise SystemExit(f"FAIL storage clear {map_name}/cull receipt {cull}")
 seeds=seed_found.get(map_name,[])
 if len(seeds)!=1:raise SystemExit(f"FAIL storage seed {map_name} receipt {seeds}")
if len(found)!=14:raise SystemExit(f"FAIL effects smoke unexpected receipt set {sorted(found)}")
if len(compute_found)!=2:raise SystemExit(f"FAIL particle compute unexpected receipt set {sorted(compute_found)}")
if len(graph_found)!=2:raise SystemExit(f"FAIL atmosphere effect graph unexpected receipt set {sorted(graph_found)}")
if len(full_atmosphere_found)!=2:raise SystemExit(f"FAIL full atmosphere unexpected receipt set {sorted(full_atmosphere_found)}")
if len(weather_found)!=2:raise SystemExit(f"FAIL atmosphere weather matrix unexpected receipt set {sorted(weather_found)}")
if len(fixture_found)!=2:raise SystemExit(f"FAIL atmosphere fixture matrix unexpected receipt set {sorted(fixture_found)}")
if len(impact_found)!=2:raise SystemExit(f"FAIL atmosphere impact telemetry unexpected receipt set {sorted(impact_found)}")
if len(atmospheric_compute_found)!=2:raise SystemExit(f"FAIL atmospheric compute unexpected receipt set {sorted(atmospheric_compute_found)}")
if len(atmospheric_frame_found)!=2:raise SystemExit(f"FAIL atmospheric frame unexpected receipt set {sorted(atmospheric_frame_found)}")
if len(frame_uniform_found)!=4:raise SystemExit(f"FAIL frame uniform unexpected receipt set {sorted(frame_uniform_found)}")
if len(shadow_found)!=6:raise SystemExit(f"FAIL shadow storage unexpected receipt set {sorted(shadow_found)}")
# Cull dispatch is an explicit _DEBUG-only verification pass; all three
# production storage clears remain mandatory in both configurations.
if len(clear_found) not in (6,8):raise SystemExit(f"FAIL storage clear unexpected receipt set {sorted(clear_found)}")
if len(seed_found)!=2:raise SystemExit(f"FAIL storage seed unexpected receipt set {sorted(seed_found)}")
print("PASS native RAL effects commands: arena1 + arena17, seven families each")
PYEOF
}

if [ "${1:-}" = --analyze ];then
	[ "$#" -eq 2 ] || { echo "usage: $0 --analyze <qconsole.jsonl>";exit 64; }
	analyze "$2";exit $?
fi
if [ "${1:-}" = --self-test ];then
	ROOT="$(mktemp -d -t ral-effects-smoke-self-XXXXXX 2>/dev/null || mktemp -d)";trap 'rm -rf "$ROOT"' EXIT
	python3 - "$ROOT/clean.jsonl" <<'PYEOF'
import json,sys
with open(sys.argv[1],"w") as out:
 for map_name in ("arena1","arena17"):
  rows=[("INFO","client",f"cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/{map_name}.bsp serverTime=1 numEntities=2 framecount=3)")]
  rows += [("INFO","renderer.ral",f"ral-atmosphere-effect-graph schema=1 map={map_name} profile=64 stages=3 root-requests=1 root-particles=4 collision=1 death=1 class=64 max-particles=24 compute-dispatches=4 barrier=compute-to-graphics")]
  rows += [("INFO","renderer.ral",f"ral-atmosphere-full schema=1 map={map_name} tier=full froxels=230400 volumes=1 lights=0 dispatches=5 clouds=1 coverage-milli=650 composite=1 history=rejected scene-hdr=copy-back")]
  rows += [("INFO","renderer.ral",f"ral-atmosphere-weather schema=1 map={map_name} family-mask=31 active-particles=6144 exposure-milli=750 heightgrid=1 roof=split depth-soft=1")]
  rows += [("INFO","renderer.ral",f"ral-atmosphere-fixture schema=1 map={map_name} fixture={fixture} tier={tier} family-mask={mask} weather-particles={particles} froxels={froxels} dispatches={dispatches} clouds={clouds} semantic-particles={semantic}") for fixture,tier,mask,particles,froxels,dispatches,clouds,semantic in (("clear",1,0,0,0,0,0,0),("rain",2,1,8192,0,0,0,0),("snow",2,2,8192,0,0,0,0),("cold",2,0,0,0,0,0,24),("fog",3,0,0,230400,4,0,0),("storm",3,31,8192,230400,5,1,0))]
  rows += [("INFO","renderer.ral",f"ral-atmosphere-impact schema=1 map={map_name} gpu-events=2 gpu-spawned=4 dropped-events=0 dropped-particles=0 readback-bytes=64")]
  rows += [("INFO","renderer.ral",f"ral-shadow-storage schema=1 map={map_name} family={family} buffer={0 if family != 'particle-pool' else 1} owner-generation=1 shadow-generation=2 flush-generation=2 dirty-elements={0 if family == 'particle-pool' else 3} writes={0 if family == 'particle-pool' else 2} content-hash={0 if family == 'particle-pool' else 40} write-digest={0 if family == 'particle-pool' else 41} wrote={0 if family == 'particle-pool' else 1}") for family in ("particle-pool","particle-classes")]
  rows += [("INFO","renderer.ral",f"ral-particle-compute schema=1 map={map_name} dispatches=1 barrier=compute-to-graphics")]
  rows += [("INFO","renderer.ral",f"ral-atmospheric-compute schema=1 map={map_name} dispatches=1 barrier=compute-to-graphics")]
  rows += [("INFO","renderer.ral",f"ral-atmospheric-frame schema=1 map={map_name} slot=0 owner-generation=1 slot-generation=1 compute-write=10 final-write=11 content-hash=12")]
  rows += [("INFO","renderer.ral",f"ral-frame-uniform schema=1 map={map_name} family=particle slot=0 owner-generation=1 slot-generation=1 partial-write=20 final-write=21 content-hash=22")]
  rows += [("INFO","renderer.ral",f"ral-frame-uniform schema=1 map={map_name} family=decal slot=0 owner-generation=1 slot-generation=1 partial-write=0 final-write=30 content-hash=31")]
  rows += [("INFO","renderer.ral",f"ral-shadow-storage schema=1 map={map_name} family=decal-pool buffer=0 owner-generation=1 shadow-generation=2 flush-generation=2 dirty-elements=0 writes=0 content-hash=0 write-digest=0 wrote=0")]
  rows += [("INFO","renderer.ral",f"ral-storage-clear schema=1 map={map_name} family={family} bytes={size} visibility=transfer-to-compute") for family,size in (("cull",4),("forward-plus",4096),("hdr-histogram",1024),("particle-child-budgets",2304))]
  rows += [("INFO","renderer.ral",f"ral-storage-seed schema=1 map={map_name} family=hdr-exposure bytes=4 bits=0x3f800000 transfer=completed graphics-visible=1")]
  rows += [("INFO","renderer.ral",f"ral-effects schema=1 map={map_name} family={family} draws=1 dynamic-offsets={1 if family in ('ribbon','rail-ribbon','beam','sprite') else 0}") for family in ("ribbon","rail-ribbon","beam","sprite","particle","decal","atmospheric")]
  for sev,cat,msg in rows:out.write(json.dumps({"ts":"2026-08-21T12:00:00+03:00","sev":sev,"cat":cat,"msg":msg+"\n"})+"\n")
PYEOF
	analyze "$ROOT/clean.jsonl" >/dev/null || exit 1
	python3 - "$ROOT/clean.jsonl" "$ROOT/bad.jsonl" <<'PYEOF'
import sys
data=open(sys.argv[1]).read();open(sys.argv[2],"w").write(data.replace("family=beam draws=1", "family=beam draws=0",1))
PYEOF
	if analyze "$ROOT/bad.jsonl" >/dev/null 2>&1;then echo "FAIL effects smoke accepted zero draw";exit 1;fi
	python3 - "$ROOT/clean.jsonl" "$ROOT/bad-compute.jsonl" <<'PYEOF'
import sys
data=open(sys.argv[1]).read();open(sys.argv[2],"w").write(data.replace("dispatches=1 barrier=compute-to-graphics", "dispatches=1 barrier=graphics-to-compute",1))
PYEOF
	if analyze "$ROOT/bad-compute.jsonl" >/dev/null 2>&1;then echo "FAIL effects smoke accepted wrong compute barrier";exit 1;fi
	python3 - "$ROOT/clean.jsonl" "$ROOT/bad-graph.jsonl" <<'PYEOF'
import sys
data=open(sys.argv[1]).read();open(sys.argv[2],"w").write(data.replace("collision=1 death=1", "collision=0 death=1",1))
PYEOF
	if analyze "$ROOT/bad-graph.jsonl" >/dev/null 2>&1;then echo "FAIL effects smoke accepted incomplete atmosphere graph";exit 1;fi
	python3 - "$ROOT/clean.jsonl" "$ROOT/bad-full-atmosphere.jsonl" <<'PYEOF'
import sys
data=open(sys.argv[1]).read();open(sys.argv[2],"w").write(data.replace("dispatches=5 clouds=1 coverage-milli=650", "dispatches=4 clouds=1 coverage-milli=650",1))
PYEOF
	if analyze "$ROOT/bad-full-atmosphere.jsonl" >/dev/null 2>&1;then echo "FAIL effects smoke accepted incomplete full-atmosphere dispatch";exit 1;fi
	python3 - "$ROOT/clean.jsonl" "$ROOT/bad-weather-matrix.jsonl" <<'PYEOF'
import sys
data=open(sys.argv[1]).read();open(sys.argv[2],"w").write(data.replace("family-mask=31 active-particles=6144", "family-mask=15 active-particles=6144",1))
PYEOF
	if analyze "$ROOT/bad-weather-matrix.jsonl" >/dev/null 2>&1;then echo "FAIL effects smoke accepted incomplete weather matrix";exit 1;fi
	python3 - "$ROOT/clean.jsonl" "$ROOT/bad-atmospheric-compute.jsonl" <<'PYEOF'
import sys
data=open(sys.argv[1]).read();open(sys.argv[2],"w").write(data.replace("ral-atmospheric-compute schema=1", "ral-atmospheric-compute schema=2",1))
PYEOF
	if analyze "$ROOT/bad-atmospheric-compute.jsonl" >/dev/null 2>&1;then echo "FAIL effects smoke accepted wrong atmospheric schema";exit 1;fi
	python3 - "$ROOT/clean.jsonl" "$ROOT/bad-atmospheric-frame.jsonl" <<'PYEOF'
import sys
data=open(sys.argv[1]).read();open(sys.argv[2],"w").write(data.replace("final-write=11", "final-write=9",1))
PYEOF
	if analyze "$ROOT/bad-atmospheric-frame.jsonl" >/dev/null 2>&1;then echo "FAIL effects smoke accepted stale atmospheric frame write";exit 1;fi
	python3 - "$ROOT/clean.jsonl" "$ROOT/bad-particle-frame.jsonl" <<'PYEOF'
import sys
data=open(sys.argv[1]).read();open(sys.argv[2],"w").write(data.replace("partial-write=20 final-write=21", "partial-write=22 final-write=21",1))
PYEOF
	if analyze "$ROOT/bad-particle-frame.jsonl" >/dev/null 2>&1;then echo "FAIL effects smoke accepted stale particle frame write";exit 1;fi
	python3 - "$ROOT/clean.jsonl" "$ROOT/bad-decal-frame.jsonl" <<'PYEOF'
import sys
data=open(sys.argv[1]).read();open(sys.argv[2],"w").write(data.replace("family=decal slot=0", "family=decal slot=2",1))
PYEOF
	if analyze "$ROOT/bad-decal-frame.jsonl" >/dev/null 2>&1;then echo "FAIL effects smoke accepted invalid decal frame slot";exit 1;fi
	python3 - "$ROOT/clean.jsonl" "$ROOT/bad-shadow.jsonl" <<'PYEOF'
import sys
data=open(sys.argv[1]).read();open(sys.argv[2],"w").write(data.replace("dirty-elements=3 writes=2", "dirty-elements=0 writes=0",1))
PYEOF
	if analyze "$ROOT/bad-shadow.jsonl" >/dev/null 2>&1;then echo "FAIL effects smoke accepted empty shadow upload";exit 1;fi
	python3 - "$ROOT/clean.jsonl" "$ROOT/bad-clear.jsonl" <<'PYEOF'
import sys
data=open(sys.argv[1]).read();open(sys.argv[2],"w").write(data.replace("family=cull bytes=4 visibility=transfer-to-compute", "family=cull bytes=4 visibility=compute-to-transfer",1))
PYEOF
	if analyze "$ROOT/bad-clear.jsonl" >/dev/null 2>&1;then echo "FAIL effects smoke accepted wrong storage-clear visibility";exit 1;fi
	python3 - "$ROOT/clean.jsonl" "$ROOT/bad-seed.jsonl" <<'PYEOF'
import sys
data=open(sys.argv[1]).read();open(sys.argv[2],"w").write(data.replace("bits=0x3f800000", "bits=0x00000000",1))
PYEOF
	if analyze "$ROOT/bad-seed.jsonl" >/dev/null 2>&1;then echo "FAIL effects smoke accepted wrong storage seed";exit 1;fi
	echo "PASS RAL effects smoke analyzer self-test";exit 0
fi

WIRED="${1:-}";[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired";exit 64; }
[ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: missing timeout runner";exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")";WD="$(dirname "$WIRED")"
find_required(){ local name="$1" candidate;shift;for candidate in "$@";do [ -f "$candidate/$name" ] && { printf '%s\n' "$candidate/$name";return 0;};done;return 1;}
RENDERER="${WIRED_RENDERER:-}"
[ -f "$RENDERER" ] || RENDERER="$(find_required wired_vulkan_arm64.dylib "$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS")" || { echo "SKIP: current Vulkan renderer unavailable";exit 77; }
GAMECL="${WIRED_GAMECL:-}";[ -f "$GAMECL" ] || GAMECL="$(find_required gameclarm64.dylib "$WD/base")" || { echo "SKIP: current gameclarm64.dylib unavailable";exit 77; }
GAMESV="${WIRED_GAMESV:-}";[ -f "$GAMESV" ] || GAMESV="$(find_required gamesvarm64.dylib "$WD/base")" || { echo "SKIP: current gamesvarm64.dylib unavailable";exit 77; }
MOLTEN="$(find_required libMoltenVK.dylib "$WD" "$WD/Contents/MacOS" /opt/homebrew/opt/molten-vk/lib /usr/local/lib)" || { echo "SKIP: current MoltenVK unavailable";exit 77; }
CONTENT="${WIRED_CONTENT_ROOT:-$HOME/wired/q3now-preview}";[ -f "$CONTENT/base/pax21.sw3z" ] || { echo "SKIP: set WIRED_CONTENT_ROOT to a current content root";exit 77; }
BASE="";for candidate in "$CONTENT/base/pax01.sw3z" "$CONTENT/base/pak0.pk3" "$HOME/wired/q3now-preview/base/pax01.sw3z" "$HOME/wired/q3now-preview/base/pak0.pk3";do [ -f "$candidate" ] && { BASE="$candidate";break; };done
[ -n "$BASE" ] || { echo "SKIP: base content unavailable";exit 77; }

ROOT="$(mktemp -d -t ral-effects-smoke-XXXXXX 2>/dev/null || mktemp -d)";RUN="$ROOT/runtime"
trap '[ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"' EXIT
mkdir -p "$RUN/Contents/MacOS"
cp "$WIRED" "$RUN/wired";chmod +x "$RUN/wired";cp "$RENDERER" "$MOLTEN" "$RUN/Contents/MacOS/" || exit 1

# Each map gets an isolated native process. A cold navmesh cache may still be
# baking on a worker when an in-process map transition starts; that unrelated
# navigation lifecycle is not part of this renderer receipt. Separate sessions
# keep this proof scoped while still exercising two real maps and two complete
# renderer initialisation/teardown cycles.
run_map() {
	local map_name="$1" home_dir="$ROOT/home-$1" qconsole="$ROOT/$1.jsonl" stdout="$ROOT/$1.stdout.log" rc
	mkdir -p "$home_dir/base"
	cp "$CONTENT/base/pax21.sw3z" "$BASE" "$home_dir/base/" || return 1
	cp "$GAMECL" "$home_dir/base/gameclarm64.dylib" || return 1
	cp "$GAMESV" "$home_dir/base/gamesvarm64.dylib" || return 1
	printf '%s\n' 'log renderer.ral info' 'set r_ralEffectsSmoke 1' \
		'set r_particles 1' 'set r_gpuDecals 1' 'set r_atmosphericGPU 1' \
		'set r_forwardPlus 1' 'set r_hdrAutoExposure 1' >"$home_dir/base/ral-effects-smoke.cfg"
	python3 "$TIMEOUT_RUNNER" --timeout 120 --kill-after 15 --cwd "$RUN" --stdout "$stdout" -- "$RUN/wired" +set fs_basepath "$home_dir" +set fs_homepath "$home_dir" +set fs_game base +set vm_game 0 +set vm_cgame 0 +set sv_cheats 1 +set sv_pure 0 +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set r_forwardPlus 1 +set r_hdrAutoExposure 1 +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec ral-effects-smoke.cfg +map "$map_name" +waitForMap +wait 90 +quit
	rc=$?
	[ "$rc" -eq 0 ] && [ -f "$home_dir/qconsole.jsonl" ] || return 1
	cp "$home_dir/qconsole.jsonl" "$qconsole"
}

if ! run_map arena1 || ! run_map arena17 || ! analyze "$ROOT/arena1.jsonl" "$ROOT/arena17.jsonl";then
	echo "FAIL retained root: $ROOT";WIRED_KEEP_ARTIFACTS=1;exit 1
fi
echo "PASS retained root: $ROOT"
