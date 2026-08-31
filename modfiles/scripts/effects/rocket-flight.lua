-- SPDX-License-Identifier: GPL-3.0-or-later
-- Per-frame semantic flight occurrence. The looping sound is entity-bound;
-- the client audio mixer owns continuity and Doppler across occurrences.
return { schemaVersion=1, maxActiveActions=2, maxInstances=128, duration=.02, boundsRadius=224, actions={
 {type="light",id="flight-light",material="rocketExplosion",radius={200,200,200},intensity=1,color={1,.75,0,1},flags={"noShadows"}},
 {type="sound",id="flight-loop",sound="sound/weapons/rocket/rockfly.opus",channel=0,looping=1},
} }
