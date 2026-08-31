-- SPDX-License-Identifier: GPL-3.0-or-later
-- Exact semantic translation of c2a440ff6's grenade CG_MissileHitWall branch.
return {schemaVersion=1,maxActiveActions=6,maxInstances=128,duration=.6,boundsRadius=600,actions={
 {type="sprite",id="dish-flash",material="grenadeExplosion",radius={30,42},startTimeJitter=.063,startTimeJitterSteps=64,randomRotation=1,offset={0,0,16},color={1,1,1,.33},duration=.001,fadeOut=.6,flags={"loop"}},
 {type="particle",id="shrapnel",particle="explosion_hot_shrapnel",maxInstances=12,maxParticles=12,offset={0,0,2},flags={"gpuLifecycle"}},
 {type="light",id="light",material="grenadeExplosion",radius={300,300,300},intensity=1,color={1,1,0,1},offset={0,0,16},startTimeJitter=.063,startTimeJitterSteps=64,duration=.001,fadeOut=.3,flags={"loop","noShadows"}},
 {type="sound",id="sound",sound="sound/weapons/rocket/rocklx1a.opus",channel=0},
 {type="screenShake",id="quake",duration=.6,fadeOut=.5,magnitude=1,radius=600,decayExponent=1,mode=0,maxAngles={.6,.6,.6},maxOffset={.6,.6,.6}},
 {type="decal",id="scorch",material="gfx/damage/burn_med_mrk",size=64,depth=12,duration=10},
}}
