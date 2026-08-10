-- SPDX-License-Identifier: GPL-3.0-or-later
-- SPDX-FileCopyrightText: 2024-present Wired Engine contributors
--
-- Default procedural crosshair (mandatory fallback). Every field has a sane
-- value; this is also the base every weapon's crosshair.lua is deep-merged
-- onto, and the reticle used by weapons with no script. See the procedural-
-- crosshair design spec §10.1.

return {
  dynamic = false,
  base = {
    visible = true,
    scale = 1.0,
    color = { 1.0, 1.0, 1.0, 1.0 },
    gap = 3,
    arms = {
      top    = { enabled = true, length = 6, thickness = 1 },
      right  = { enabled = true, length = 6, thickness = 1 },
      bottom = { enabled = true, length = 6, thickness = 1 },
      left   = { enabled = true, length = 6, thickness = 1 },
    },
    dot     = { enabled = false },
    ring    = { enabled = false },
    corners = { enabled = false },
    outline = { thickness = 1, alpha = 0.8 },
  },
}
