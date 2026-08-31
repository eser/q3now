-- SPDX-License-Identifier: GPL-3.0-or-later
-- Canonical view-shake owner for server-authored target_earthquake events.
return {
    schemaVersion = 1,
    maxActiveActions = 1,
    maxInstances = 64,
    duration = 10,
    actions = {
        {
            type = "screenShake", id = "world-quake", duration = 10,
            fadeIn = 1, fadeOut = 1, magnitude = 1,
            decayExponent = 1, mode = 0,
            maxAngles = { 0.2, 0.2, 0.2 }, maxOffset = { 0.2, 0.2, 0.2 },
        },
    },
}
