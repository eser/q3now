-- SPDX-License-Identifier: GPL-3.0-or-later
-- Machinegun and shotgun deliberately share the Q1-derived ricochet palette.
local ricochetSoundRoot="weapons/machinegun/sounds/"
return {schemaVersion=1,maxActiveActions=10,maxInstances=128,duration=1.5,boundsRadius=48,actions={
 {type="particle",id="flash",particle="hitscan_impact_flash",maxInstances=1,maxParticles=1,offset={0,0,1.5},startCondition=16384,flags={"gpuLifecycle"}},
 {type="particle",id="moving-sparks",particle="hitscan_metal_sparks",maxInstances=2,maxParticles=2,offset={0,0,1.5},startCondition=16384,flags={"gpuLifecycle"}},
 {type="beam",id="line-sparks",ribbon="gfx/misc/tracer",width=.15,endWidth=.025,count=2,maxInstances=2,length={4,7},normalScale={.7,1},spread=.82,lifetime=.32,ribbonFadeOut=.22,startColor={1,.96,.72,.88},endColor={1,.40,.06,0},offset={0,0,1.5},startCondition=16384},
 {type="beam",id="side-streaks",ribbon="gfx/misc/tracer",width=.24,endWidth=.035,count=4,maxInstances=4,length={10,16},normalScale={.55,1},spread=.72,lifetime=.16,ribbonFadeOut=.112,startColor={1,.90,.56,.78},endColor={1,.36,.05,0},offset={0,0,1.5},startCondition=16384},
 {type="particle",id="smoke",particle="hitscan_dust_puff",maxInstances=1,maxParticles=1,delay={.1,.1},offset={0,0,1.5},startCondition=16384,flags={"gpuLifecycle"}},
 {type="decal",id="mark",material="gfx/damage/bullet_mrk",size=5,depth=4,duration=10},
 {type="beam",id="ricochet",ribbon="gfx/misc/tracer",width=.38,endWidth=.06,count=1,maxInstances=1,lifetime=.07,ribbonFadeOut=.04,startColor={1,.88,.52,.95},endColor={1,.32,.04,0},offset={0,0,1.75},startCondition=17408},
 {type="sound",id="ricochet-sound-0",sound=ricochetSoundRoot.."ric1.opus",channel=0,startCondition=17472},
 {type="sound",id="ricochet-sound-1",sound=ricochetSoundRoot.."ric2.opus",channel=0,startCondition=17536},
 {type="sound",id="ricochet-sound-2",sound=ricochetSoundRoot.."ric3.opus",channel=0,startCondition=17664},
}}
