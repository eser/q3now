-- SPDX-License-Identifier: GPL-3.0-or-later
-- Underwater rocket detonation: pressure flash, bubbles and muted debris; no scorch.
return {
    schemaVersion = 1,
    maxActiveActions = 7,
    maxInstances = 64,
    duration = 1.4,
    lodFar = 6144,
    boundsRadius = 256,
    actions = {
        {
            type = "particle", id = "water-flash", particle = "hitscan_impact_flash",
            maxInstances = 1, maxParticles = 1, duration = 0.08,
            color = { 0.65, 0.82, 1.0, 0.72 }, flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "water-core", particle = "explosion_fire",
            maxInstances = 1, maxParticles = 1, color = { 0.5, 0.72, 0.85, 0.55 },
            flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "water-bubbles", particle = "explosion_water_bubbles",
            maxInstances = 36, maxParticles = 36, flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "water-debris", particle = "explosion_hot_shrapnel",
            maxInstances = 10, maxParticles = 10, velocityScale = 0.35,
            color = { 0.5, 0.62, 0.68, 0.55 }, flags = { "gpuLifecycle" },
        },
        {
            type = "light", id = "water-light", material = "rocketExplosion",
            radius = { 240, 240, 240 }, intensity = 0.65,
            color = { 0.35, 0.62, 0.8, 1.0 }, duration = 0.016,
            fadeOut = 0.4, flags = { "loop", "noShadows" },
        },
        {
            type = "sound", id = "water-sound",
            sound = "sound/weapons/rocket/rocklx1a.opus", channel = 0,
        },
        {
            type = "screenShake", id = "water-shake", duration = 0.42,
            magnitude = 0.45, controllerScale = 0.4,
            radius = 600, decayExponent = 1, mode = 0,
            maxAngles = { 0.35, 0.35, 0.2 }, maxOffset = { 0.45, 0.45, 0.2 },
        },
    },
}
