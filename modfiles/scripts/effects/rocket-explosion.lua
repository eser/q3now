-- SPDX-License-Identifier: GPL-3.0-or-later
-- First complete WiredFX consumer: staged Quake 4-inspired rocket impact.
return {
    schemaVersion = 1,
    maxActiveActions = 10,
    maxInstances = 64,
    duration = 1.0,
    lodFar = 8192,
    boundsRadius = 192,
    actions = {
        {
            type = "particle", id = "impact-flash", particle = "hitscan_impact_flash",
            maxInstances = 1, maxParticles = 1, duration = 0.1,
            flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "hot-core", particle = "explosion_fire",
            maxInstances = 1, maxParticles = 1, offset = { 0, 0, 2 },
            flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "fire-shell", particle = "explosion_fire_shell",
            maxInstances = 1, maxParticles = 1, duration = 0.4,
            offset = { 0, 0, 2 }, flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "shrapnel", particle = "explosion_hot_shrapnel",
            maxInstances = 24, maxParticles = 24, flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "smoke", particle = "hitscan_dust_puff",
            maxInstances = 8, maxParticles = 8, delay = { 0.03, 0.06 },
            velocityScale = 0.6, flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "metal-sparks", particle = "hitscan_metal_sparks",
            maxInstances = 10, maxParticles = 10, startCondition = 2,
            flags = { "gpuLifecycle", "optionalResource" },
        },
        {
            type = "light", id = "impact-light", material = "rocketExplosion",
            radius = { 300, 300, 300 }, intensity = 1.0,
            color = { 1.0, 0.75, 0.05, 1.0 }, duration = 0.016,
            fadeOut = 0.45, flags = { "loop", "noShadows" },
        },
        {
            type = "sound", id = "impact-sound",
            sound = "sound/weapons/rocket/rocklx1a.opus", channel = 0,
        },
        {
            type = "screenShake", id = "impact-shake", duration = 0.48,
            magnitude = 0.6, controllerScale = 0.5,
            maxAngles = { 0.5, 0.5, 0.3 }, maxOffset = { 0.6, 0.6, 0.3 },
        },
        {
            type = "decal", id = "scorch", material = "gfx/damage/burn_med_mrk",
            size = 64, depth = 12, angle = 0, duration = 12,
        },
    },
}
