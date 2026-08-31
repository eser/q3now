-- SPDX-License-Identifier: GPL-3.0-or-later
return { schemaVersion=1, maxActiveActions=3, maxInstances=128, duration=.5, boundsRadius=96, actions={
 {type="particle",id="core",particle="rocket_exhaust_core",maxInstances=8,maxParticles=8,trailSpacing=8,pathSampling=1,flags={"gpuLifecycle"}},
 {type="particle",id="smoke",particle="rocket_smoke_wake",maxInstances=6,maxParticles=6,spawnRate=50,rateBoundaryAligned=1,pathSampling=1,flags={"gpuLifecycle"}},
 {type="particle",id="embers",particle="rocket_hot_embers",maxInstances=3,maxParticles=3,spawnRate=25,rateBoundaryAligned=1,pathSampling=1,flags={"gpuLifecycle"}},
} }
