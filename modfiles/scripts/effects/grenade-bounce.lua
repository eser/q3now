-- SPDX-License-Identifier: GPL-3.0-or-later
-- Grenade collision response remains gameplay-owned; WiredFX owns the
-- authored audible bounce variant at the projectile entity and hit position.
return { schemaVersion=1, maxActiveActions=2, maxInstances=128, duration=.1, boundsRadius=64, actions={
 {type="sound",id="bounce-0",sound="sound/weapons/grenade/hgrenb1a.opus",channel=0,sourceBound=1,startCondition=64},
 {type="sound",id="bounce-1",sound="sound/weapons/grenade/hgrenb2a.opus",channel=0,sourceBound=1,startCondition=128},
} }
