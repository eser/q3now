-- SPDX-License-Identifier: GPL-3.0-or-later
-- Exact pre-migration fire sound + tag_flash dlight. The grenade model's flash
-- remains the visual source; no synthetic sprite/streak/smoke is introduced.
return { schemaVersion=1, maxActiveActions=2, maxInstances=128, duration=.02, boundsRadius=352, actions={
 {type="sound",id="fire",sound="sound/weapons/grenade/grenlf1a.opus",channel=1,sourceBound=1,startCondition=4096},
 {type="light",id="muzzle-light",material="grenadeExplosion",radius={300,300,300},radiusJitter=31,radiusJitterSteps=32,intensity=1,color={1,.70,0,1},duration=.001,startCondition=8192,flags={"loop","noShadows"}},
} }
