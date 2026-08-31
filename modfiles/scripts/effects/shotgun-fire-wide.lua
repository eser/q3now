-- SPDX-License-Identifier: GPL-3.0-or-later
-- HEAD's secondary-fire event starts the regular shotgun report and then the
-- sawed-off report (currently the same asset) on the same entity/channel. Keep
-- both authored occurrences: resource deduplication must not erase semantics.
return { schemaVersion=1, maxActiveActions=4, maxInstances=128, duration=.02, boundsRadius=320, actions={
 {type="sound",id="fire-primary",sound="sound/weapons/shotgun/sshotf1b.opus",channel=1,sourceBound=1,startCondition=4096},
 {type="sound",id="fire-secondary",sound="sound/weapons/shotgun/sshotf1b.opus",channel=1,sourceBound=1,startCondition=4096},
 {type="screenShake",id="kick",duration=.2,magnitude=1,decayExponent=2,mode=1,maxAngles={3,0,1.5},maxOffset={0,0,0},startCondition=6144},
 {type="light",id="muzzle-light",material="bulletExplosion",radius={300,300,300},radiusJitter=31,radiusJitterSteps=32,intensity=1,color={1,1,0,1},duration=.001,startCondition=8192,flags={"loop","noShadows"}},
} }
