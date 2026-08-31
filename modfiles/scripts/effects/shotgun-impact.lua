-- SPDX-License-Identifier: GPL-3.0-or-later
-- Preserve pellet-local fan-out in cgame. Counts here are intentionally below
-- Quake 4's single-impact recipe because every shotgun pellet owns an occurrence.
-- Machinegun and shotgun deliberately share the Q1-derived ricochet palette.
local ricochetSoundRoot="weapons/machinegun/sounds/"
local function lineSparks(id,count,condition)
 return {type="beam",id=id,ribbon="gfx/misc/tracer",width=.14,endWidth=.02,
  count=count,maxInstances=count,length={3,6},normalScale={.7,1},spread=.86,
  lifetime=.38,ribbonFadeOut=.27,startColor={1,.97,.76,.92},endColor={1,.42,.06,0},
  offset={0,0,1.5},startCondition=condition}
end
local function sideStreaks(id,count,condition)
 return {type="beam",id=id,ribbon="gfx/misc/tracer",width=.23,endWidth=.032,
  count=count,maxInstances=count,length={8,14},normalScale={.55,1},spread=.74,
  lifetime=.16,ribbonFadeOut=.112,startColor={1,.90,.56,.82},endColor={1,.34,.04,0},
  offset={0,0,1.5},startCondition=condition}
end
return {schemaVersion=1,maxActiveActions=21,maxInstances=128,duration=1.5,boundsRadius=56,actions={
 {type="particle",id="default-flash",particle="shotgun_impact_flash",maxInstances=1,maxParticles=1,offset={0,0,1.5},startCondition=16385,flags={"gpuLifecycle"}},
 {type="particle",id="default-moving-sparks",particle="hitscan_metal_sparks",maxInstances=2,maxParticles=2,offset={0,0,1.5},startCondition=16385,flags={"gpuLifecycle"}},
 lineSparks("default-line-sparks",2,16385),
 sideStreaks("default-side-streaks",3,16385),
 {type="particle",id="default-smoke",particle="hitscan_dust_puff",maxInstances=1,maxParticles=1,delay={.1,.1},offset={0,0,1.5},startCondition=16385,flags={"gpuLifecycle"}},
 {type="particle",id="metal-flash",particle="shotgun_impact_flash",maxInstances=1,maxParticles=1,offset={0,0,1.5},startCondition=16386,flags={"gpuLifecycle"}},
 {type="particle",id="metal-moving-sparks",particle="hitscan_metal_sparks",maxInstances=3,maxParticles=3,offset={0,0,1.5},startCondition=16386,flags={"gpuLifecycle"}},
 lineSparks("metal-line-sparks",2,16386),
 sideStreaks("metal-side-streaks",4,16386),
 {type="particle",id="metal-smoke",particle="hitscan_dust_puff",maxInstances=1,maxParticles=1,delay={.1,.1},offset={0,0,1.5},startCondition=16386,flags={"gpuLifecycle"}},
 {type="particle",id="dust-flash",particle="shotgun_impact_flash",maxInstances=1,maxParticles=1,offset={0,0,1.5},startCondition=16416,flags={"gpuLifecycle"}},
 {type="particle",id="dust-moving-sparks",particle="hitscan_metal_sparks",maxInstances=1,maxParticles=1,offset={0,0,1.5},startCondition=16416,flags={"gpuLifecycle"}},
 lineSparks("dust-line-sparks",1,16416),
 sideStreaks("dust-side-streaks",2,16416),
 {type="particle",id="dust-smoke",particle="hitscan_dust_puff",maxInstances=2,maxParticles=2,delay={.1,.1},offset={0,0,1.5},startCondition=16416,flags={"gpuLifecycle"}},
 {type="particle",id="dust-chips",particle="hitscan_surface_chips",maxInstances=3,maxParticles=3,delay={.04,.04},offset={0,0,1.5},startCondition=16416,flags={"gpuLifecycle"}},
 {type="decal",id="mark",material="gfx/damage/bullet_mrk",size=2.5,depth=3,duration=10},
 {type="beam",id="ricochet",ribbon="gfx/misc/tracer",width=.38,endWidth=.06,count=1,maxInstances=1,lifetime=.07,ribbonFadeOut=.04,startColor={1,.88,.52,.95},endColor={1,.32,.04,0},offset={0,0,1.75},startCondition=17408},
 {type="sound",id="ricochet-sound-0",sound=ricochetSoundRoot.."ric1.opus",channel=0,startCondition=17472},
 {type="sound",id="ricochet-sound-1",sound=ricochetSoundRoot.."ric2.opus",channel=0,startCondition=17536},
 {type="sound",id="ricochet-sound-2",sound=ricochetSoundRoot.."ric3.opus",channel=0,startCondition=17664},
}}
