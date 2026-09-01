-- SPDX-License-Identifier: GPL-3.0-or-later
-- Backend-neutral cinematic lens composition. Cgame supplies one already
-- occlusion-smoothed semantic source; this recipe owns every optical layer.
-- Layer positions follow the source-to-optical-centre axis. Small camera-plane
-- offsets and split RGB layers model imperfect coated optics without noise.
return {
    schemaVersion = 1,
    maxActiveActions = 16,
    maxInstances = 32,
    -- One semantic event per rendered frame: no overlapping envelopes and no
    -- frame-cadence flicker at the 250 Hz gameplay target.
    duration = 0,
    lodFar = 1800,
    boundsRadius = 320,
    actions = {
        -- Source assembly: broad sensor veil, horizontal anamorphic streak and
        -- a compact core. Different response powers keep the veil alive while
        -- preventing the white core from blooming at weak visibility.
        {
            type = "flare", id = "cool-veiling-glare", flare = "lfWarmGlow",
            autosprite = 1, screenSpace = 1, position = { 1.0, 0, 0 }, size = 96,
            intensityPower = 0.55,
            color = { 0.40, 0.58, 1.0, 0.060 },
        },
        {
            type = "flare", id = "wide-blue-streak", flare = "lfWarmGlow",
            autosprite = 1, screenSpace = 1, position = { 1.0, 0, 0 }, size = 3.4, aspect = 128,
            intensityPower = 0.70,
            color = { 0.38, 0.61, 1.0, 0.18 },
        },
        {
            type = "flare", id = "hot-horizontal-line", flare = "lfWarmGlow",
            autosprite = 1, screenSpace = 1, position = { 1.0, 0, 0 }, size = 0.75, aspect = 256,
            intensityPower = 1.35,
            color = { 0.90, 0.96, 1.0, 0.32 },
        },
        {
            type = "flare", id = "source-bloom", flare = "lfWarmGlow",
            autosprite = 1, screenSpace = 1, position = { 1.0, 0, 0 }, size = 38,
            intensityPower = 0.82,
            color = { 0.60, 0.76, 1.0, 0.19 },
        },
        {
            type = "flare", id = "white-source-core", flare = "lfWarmGlow",
            autosprite = 1, screenSpace = 1, position = { 1.0, 0, 0 }, size = 7,
            intensityPower = 1.65,
            color = { 1.0, 0.94, 0.82, 0.64 },
        },

        -- Near-source coated-lens reflections. The paired ellipses are offset
        -- by fractions of their own size, producing restrained RGB fringing.
        {
            type = "flare", id = "near-prism-blue", flare = "lfWarmGlow",
            autosprite = 1, screenSpace = 1, position = { 0.64, 0.08, 0.02 }, size = 34,
            aspect = 1.75, screenRotation = 90, intensityPower = 0.76,
            color = { 0.24, 0.50, 1.0, 0.160 },
        },
        {
            type = "flare", id = "near-prism-warm", flare = "lfWarmGlow",
            autosprite = 1, screenSpace = 1, position = { 0.61, -0.07, -0.01 }, size = 30,
            aspect = 1.65, screenRotation = 90, intensityPower = 0.80,
            color = { 1.0, 0.36, 0.18, 0.080 },
        },
        {
            type = "flare", id = "near-cyan-disc", flare = "lfWarmGlow",
            autosprite = 1, screenSpace = 1, position = { 0.27, 0.03, 0 }, size = 15,
            aspect = 1.30, screenRotation = 90, intensityPower = 0.72,
            color = { 0.22, 0.78, 1.0, 0.100 },
        },

        -- Ghost train across the optical centre. These remain deliberately
        -- faint: motion should reveal the glass, not paste opaque decals over it.
        {
            type = "flare", id = "central-hot-ghost", flare = "lfWarmGlow",
            autosprite = 1, screenSpace = 1, position = { -0.08, 0, 0 }, size = 6,
            intensityPower = 1.10,
            color = { 1.0, 0.80, 0.54, 0.160 },
        },
        {
            type = "flare", id = "coated-iris-ghost", flare = "lfGhostRing",
            autosprite = 1, screenSpace = 1, position = { -0.42, 0.02, 0 }, size = 40,
            intensityPower = 0.62,
            color = { 0.28, 0.62, 1.0, 0.060 },
        },
        {
            type = "flare", id = "iris-red-fringe", flare = "lfWarmGlow",
            autosprite = 1, screenSpace = 1, position = { -0.46, -0.03, 0 }, size = 39,
            intensityPower = 0.64,
            color = { 1.0, 0.22, 0.12, 0.030 },
        },
        {
            type = "flare", id = "vertical-caustic", flare = "lfWarmGlow",
            autosprite = 1, screenSpace = 1, position = { -0.63, 0.04, 0.02 }, size = 8,
            aspect = 4.5, screenRotation = 90, intensityPower = 0.78,
            color = { 1.0, 0.52, 0.22, 0.080 },
        },
        {
            type = "flare", id = "far-blue-ellipse", flare = "lfWarmGlow",
            autosprite = 1, screenSpace = 1, position = { -0.94, -0.03, 0 }, size = 22,
            aspect = 1.55, screenRotation = 90, intensityPower = 0.68,
            color = { 0.20, 0.54, 1.0, 0.080 },
        },
        {
            type = "flare", id = "far-warm-edge", flare = "lfWarmGlow",
            autosprite = 1, screenSpace = 1, position = { -1.18, 0.02, 0 }, size = 15,
            aspect = 1.45, screenRotation = 90, intensityPower = 0.74,
            color = { 1.0, 0.58, 0.28, 0.050 },
        },
        {
            type = "flare", id = "outer-iris-haze", flare = "lfWarmGlow",
            autosprite = 1, screenSpace = 1, position = { 0.02, 0, 0 }, size = 145,
            intensityPower = 0.48,
            color = { 0.22, 0.48, 1.0, 0.020 },
        },
        {
            type = "flare", id = "frame-veil", flare = "lfWarmGlow",
            autosprite = 1, screenSpace = 1, position = { 0.72, 0, 0 }, size = 260,
            intensityPower = 0.45,
            color = { 0.34, 0.52, 1.0, 0.015 },
        },
    },
}
