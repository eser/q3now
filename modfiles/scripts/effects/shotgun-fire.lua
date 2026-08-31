-- SPDX-License-Identifier: GPL-3.0-or-later
-- Preserve CG_FireWeapon exactly. The separate shotgun-smoke occurrence owns
-- the pre-existing 16u smoke puff; no additional Q4 muzzle layers belong here.
return { schemaVersion=1, maxActiveActions=2, maxInstances=128, duration=.02, boundsRadius=352, actions={
 {type="sound",id="fire",sound="sound/weapons/shotgun/sshotf1b.opus",channel=1,sourceBound=1,startCondition=4096},
 {type="light",id="muzzle-light",material="bulletExplosion",radius={300,300,300},radiusJitter=31,radiusJitterSteps=32,intensity=1,color={1,1,0,1},duration=.001,startCondition=8192,flags={"loop","noShadows"}},
} }
