-- SPDX-License-Identifier: GPL-3.0-or-later
-- WiredFX profiles are loaded in ascending handle order so inheritance stays deterministic.
return {
    { handle = 1, path = "scripts/effects/rocket-explosion.lua" },
    { handle = 2, path = "scripts/effects/rocket-detonation.lua" },
    { handle = 3, path = "scripts/effects/rocket-underwater.lua" },
    { handle = 4, path = "scripts/effects/rocket-trail.lua" },
    { handle = 5, path = "scripts/effects/grenade-trail.lua" },
    { handle = 6, path = "scripts/effects/grenade-explosion.lua" },
    -- Stable reserved alias: older schema-1 content may still carry handle 7.
    -- New events always use handle 6; both resolve to the exact same recipe.
    { handle = 7, path = "scripts/effects/grenade-explosion.lua" },
    { handle = 8, path = "scripts/effects/machinegun-impact.lua" },
    { handle = 9, path = "scripts/effects/machinegun-impact-reduced.lua" },
    { handle = 10, path = "scripts/effects/shotgun-impact.lua" },
    { handle = 11, path = "scripts/effects/shotgun-impact-reduced.lua" },
    { handle = 12, path = "scripts/effects/machinegun-tracer.lua" },
    { handle = 13, path = "scripts/effects/machinegun-fire.lua" },
    { handle = 14, path = "scripts/effects/shotgun-fire.lua" },
    { handle = 15, path = "scripts/effects/shotgun-fire-wide.lua" },
    { handle = 16, path = "scripts/effects/grenade-fire.lua" },
    { handle = 17, path = "scripts/effects/rocket-fire.lua" },
    { handle = 18, path = "scripts/effects/weapon-water-trail.lua" },
    { handle = 19, path = "scripts/effects/weapon-water-splash.lua" },
    { handle = 20, path = "scripts/effects/rocket-flight.lua" },
    { handle = 21, path = "scripts/effects/grenade-bounce.lua" },
    { handle = 22, path = "scripts/effects/shotgun-smoke.lua" },
    { handle = 23, path = "scripts/effects/shotgun-smoke-wide.lua" },
    { handle = 24, path = "scripts/effects/world-earthquake.lua" },
}
