-- SPDX-License-Identifier: GPL-3.0-or-later
-- Flipbook-free underwater branch retained for complete feature-flag isolation.
return {
    schemaVersion = 1,
    maxActiveActions = 7,
    maxInstances = 64,
    duration = 1.4,
    lodFar = 6144,
    boundsRadius = 256,
    actions = {
        {
            type = "sprite", id = "water-pressure-flash", material = "rocketExhaustGlow",
            radius = { 38, 9 }, spriteLifetime = 0.14, randomRotation = 1,
            color = { 0.45, 0.72, 0.92, 0.70 },
            duration = 0.001, fadeOut = 0.14, flags = { "loop" },
        },
        {
            type = "particle", id = "water-bubbles", particle = "explosion_water_bubbles",
            maxInstances = 40, maxParticles = 40, flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "water-debris", particle = "explosion_hot_shrapnel",
            maxInstances = 10, maxParticles = 10, velocityScale = 0.32,
            color = { 0.45, 0.62, 0.72, 0.50 }, flags = { "gpuLifecycle" },
        },
        {
            type = "light", id = "water-light", material = "rocketExplosion",
            radius = { 240, 240, 240 }, intensity = 0.65,
            color = { 0.35, 0.62, 0.8, 1.0 }, lightLifetime = 0.40,
            duration = 0.001, fadeOut = 0.40, flags = { "loop", "noShadows" },
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
        {
            type = "radialBlur", id = "water-pressure", duration = 0.10,
            maxScale = 0.045,
        },
    },
}
