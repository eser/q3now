-- SPDX-License-Identifier: GPL-3.0-or-later
-- WiredFX profiles are loaded in ascending handle order so inheritance stays deterministic.
return {
    { handle = 1, path = "scripts/effects/rocket-explosion.lua" },
    { handle = 2, path = "scripts/effects/rocket-detonation.lua" },
    { handle = 3, path = "scripts/effects/rocket-underwater.lua" },
}
