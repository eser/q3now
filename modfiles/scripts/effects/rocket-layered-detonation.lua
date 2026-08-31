-- SPDX-License-Identifier: GPL-3.0-or-later
-- Q4 detonate_mp.fx-inspired free-air composition using Wired resources.
return {
    schemaVersion = 1,
    maxActiveActions = 9,
    maxInstances = 80,
    duration = 1.75,
    lodFar = 8192,
    boundsRadius = 360,
    actions = {
        {
            type = "sprite", id = "air-flash", material = "rocketExplosionFlash",
            radius = { 52, 15 }, spriteLifetime = 0.72, randomRotation = 1,
            color = { 1.0, 0.70, 0.34, 0.90 },
            duration = 0.001, fadeOut = 0.72, flags = { "loop" },
        },
        {
            type = "particle", id = "air-fire-sphere", particle = "rocket_layered_fire_sphere",
            maxInstances = 40, maxParticles = 40, flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "air-fire-lobes", particle = "rocket_layered_fire_lobe",
            maxInstances = 4, maxParticles = 4,
            flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "air-streaks", particle = "rocket_layered_air_streak",
            maxInstances = 15, maxParticles = 15, delay = { 0.20, 0.20 },
            flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "air-smoke", particle = "rocket_layered_smoke",
            maxInstances = 7, maxParticles = 7, delay = { 0.20, 0.20 },
            flags = { "gpuLifecycle" },
        },
        {
            type = "light", id = "air-light", material = "rocketExplosion",
            radius = { 44, 44, 44 }, radiusEnd = { 380, 380, 380 }, intensity = 1.0,
            color = { 1.0, 0.58, 0.06, 1.0 }, lightLifetime = 1.0,
            duration = 0.001, fadeOut = 0.72, flags = { "loop", "noShadows" },
        },
        {
            type = "sound", id = "air-sound",
            sound = "sound/weapons/rocket/rocklx1a.opus", channel = 0,
        },
        {
            type = "screenShake", id = "air-shake", duration = 0.70,
            magnitude = 0.65, controllerScale = 0.5,
            radius = 600, decayExponent = 1, mode = 0,
            maxAngles = { 0.55, 0.55, 0.35 }, maxOffset = { 0.7, 0.7, 0.35 },
        },
        {
            type = "radialBlur", id = "air-pressure", duration = 0.12,
            maxScale = 0.08,
        },
    },
}
