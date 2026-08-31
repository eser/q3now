-- SPDX-License-Identifier: GPL-3.0-or-later
-- Q4 impact_mp.fx-inspired composition using only Wired-owned materials/classes.
-- The original q4base assets are reference data and are never loaded or shipped.
return {
    schemaVersion = 1,
    maxActiveActions = 11,
    maxInstances = 96,
    duration = 1.75,
    lodFar = 8192,
    boundsRadius = 320,
    actions = {
        {
            type = "sprite", id = "impact-flash", material = "rocketExplosionFlash",
            radius = { 48, 14 }, spriteLifetime = 0.72, randomRotation = 1,
            offset = { 0, 0, 4 }, color = { 1.0, 0.74, 0.40, 0.88 },
            duration = 0.001, fadeOut = 0.72, flags = { "loop" },
        },
        {
            type = "particle", id = "fire-sphere", particle = "rocket_layered_fire_sphere",
            maxInstances = 40, maxParticles = 40, offset = { 0, 0, 5 },
            flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "fire-lobes", particle = "rocket_layered_fire_lobe",
            maxInstances = 4, maxParticles = 4, offset = { 0, 0, 5 },
            flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "expanding-ring", particle = "rocket_layered_pressure_ring",
            maxInstances = 1, maxParticles = 1, duration = 0.75,
            offset = { 0, 0, 3 }, flags = { "gpuLifecycle" },
        },
        {
            type = "beam", id = "line-burst", ribbon = "gfx/misc/tracer",
            width = 0.9, endWidth = 0.08, count = 10, maxInstances = 10,
            length = { 40, 72 }, normalScale = { 0.45, 1.0 }, spread = 0.96,
            lifetime = 0.15, ribbonFadeOut = 0.13,
            startColor = { 1.0, 0.92, 0.66, 0.90 },
            endColor = { 0.96, 0.36, 0.04, 0.0 }, offset = { 0, 0, 3 },
        },
        {
            type = "particle", id = "impact-streaks", particle = "rocket_layered_impact_streak",
            maxInstances = 15, maxParticles = 15, delay = { 0.20, 0.20 },
            offset = { 0, 0, 3 }, flags = { "gpuLifecycle" },
        },
        {
            type = "particle", id = "hanging-smoke", particle = "rocket_layered_smoke",
            maxInstances = 7, maxParticles = 7, delay = { 0.20, 0.20 },
            offset = { 0, 0, 6 }, flags = { "gpuLifecycle" },
        },
        {
            type = "light", id = "impact-light", material = "rocketExplosion",
            -- Keep the point light on the visible side of the impact plane.
            -- An origin exactly on (or one trace epsilon behind) the BSP plane
            -- contributes no forward-facing Lambert light to that surface.
            radius = { 96, 96, 96 }, radiusEnd = { 360, 360, 360 }, intensity = 1.25,
            color = { 1.0, 0.62, 0.08, 1.0 }, lightLifetime = 1.0,
            offset = { 0, 0, 12 },
            duration = 0.001, fadeOut = 0.72, flags = { "loop", "noShadows" },
        },
        {
            type = "sound", id = "impact-sound",
            sound = "sound/weapons/rocket/rocklx1a.opus", channel = 0,
        },
        {
            type = "screenShake", id = "impact-shake", duration = 0.70,
            magnitude = 0.6, controllerScale = 0.5,
            radius = 600, decayExponent = 1, mode = 0,
            maxAngles = { 0.5, 0.5, 0.3 }, maxOffset = { 0.6, 0.6, 0.3 },
        },
        {
            type = "decal", id = "scorch", material = "gfx/damage/burn_med_mrk",
            size = 64, depth = 12, angle = 0, duration = 12,
        },
    },
}
