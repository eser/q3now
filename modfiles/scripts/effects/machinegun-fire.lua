-- SPDX-License-Identifier: GPL-3.0-or-later
-- Exact presentation parity with the pre-migration CG_FireWeapon path: one of
-- four fire sounds plus the existing tag_flash dynamic light. The weapon model
-- still supplies the authored flash geometry; this profile must not invent an
-- extra smoke, streak or sprite layer.
return { schemaVersion=1, maxActiveActions=5, maxInstances=128, duration=.02, boundsRadius=352, actions={
 {type="sound",id="fire-0",sound="sound/weapons/machinegun/machgf1b.opus",channel=1,sourceBound=1,startCondition=4160},
 {type="sound",id="fire-1",sound="sound/weapons/machinegun/machgf2b.opus",channel=1,sourceBound=1,startCondition=4224},
 {type="sound",id="fire-2",sound="sound/weapons/machinegun/machgf3b.opus",channel=1,sourceBound=1,startCondition=4352},
 {type="sound",id="fire-3",sound="sound/weapons/machinegun/machgf4b.opus",channel=1,sourceBound=1,startCondition=4608},
 {type="light",id="muzzle-light",material="bulletExplosion",radius={300,300,300},radiusJitter=31,radiusJitterSteps=32,intensity=1,color={1,1,0,1},duration=.001,startCondition=8192,flags={"loop","noShadows"}},
} }
