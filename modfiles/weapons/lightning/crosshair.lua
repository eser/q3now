-- SPDX-License-Identifier: GPL-3.0-or-later
-- SPDX-FileCopyrightText: 2024-present Wired Engine contributors
--
-- Lightning gun procedural crosshair. Cyan base with a dot + outline ring;
-- dynamically tints to the target's armor tier when the beam is on an armored
-- enemy. See the procedural-crosshair design spec §10.3.

return {
  dynamic = true,
  base = {
    color = { 0.6, 1.0, 1.0, 1.0 },
    gap = 4,
    arms = {
      top    = { length = 4, thickness = 1 },
      right  = { length = 4, thickness = 1 },
      bottom = { length = 4, thickness = 1 },
      left   = { length = 4, thickness = 1 },
    },
    dot  = { enabled = true, radius = 1, filled = true },
    ring = { enabled = true, radius = 10, thickness = 1, filled = false },
  },
  update = function(state)
    local tint = q3.helpers.armor_tint(state.target)
    if tint == nil then return nil end
    return { color = tint }
  end,
}
