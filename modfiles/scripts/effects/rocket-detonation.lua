-- SPDX-License-Identifier: GPL-3.0-or-later
-- Preserve the pre-migration free-air recipe exactly.
return {
    schemaVersion = 1,
    maxActiveActions = 9,
    maxInstances = 64,
    duration = 1.0,
    lodFar = 8192,
    boundsRadius = 224,
    actions = {
        {
            type = "particle", id = "air-flash", particle = "hitscan_impact_flash",
            maxInstances = 1, maxParticles = 1, duration = 0.1,
            flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "air-hot-core", particle = "explosion_fire",
            maxInstances = 1, maxParticles = 1,
            flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "air-fire-shell", particle = "explosion_fire_shell",
            maxInstances = 1, maxParticles = 1, duration = 0.4,
            flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "air-shrapnel", particle = "explosion_hot_shrapnel",
            maxInstances = 28, maxParticles = 28, flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "air-smoke", particle = "hitscan_dust_puff",
            maxInstances = 10, maxParticles = 10, delay = { 0.03, 0.07 },
            velocityScale = 0.75, flags = { "gpuLifecycle" },
        },
        {
            type = "light", id = "air-light", material = "rocketExplosion",
            radius = { 340, 340, 340 }, intensity = 1.0,
            color = { 1.0, 0.68, 0.04, 1.0 }, duration = 0.016,
            fadeOut = 0.5, flags = { "loop", "noShadows" },
        },
        {
            type = "sound", id = "air-sound",
            sound = "sound/weapons/rocket/rocklx1a.opus", channel = 0,
        },
        {
            type = "screenShake", id = "air-shake", duration = 0.5,
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
