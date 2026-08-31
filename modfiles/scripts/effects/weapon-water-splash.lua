-- SPDX-License-Identifier: GPL-3.0-or-later
return { schemaVersion=1, maxActiveActions=2, maxInstances=128, duration=.5, boundsRadius=96, actions={
 {type="particle",id="crown",particle="weapon_water_splash",maxInstances=6,maxParticles=6,flags={"gpuLifecycle"}},
 {type="sound",id="sound",sound="sound/misc/h2ohit1.wav",channel=0,flags={"optionalResource"}},
} }
