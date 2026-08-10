-- SPDX-License-Identifier: GPL-3.0-or-later
-- SPDX-FileCopyrightText: 2024-present Wired Engine contributors
--
-- Machinegun procedural crosshair. Dynamic: arms spread with movement speed
-- and the reticle tints to the target's armor tier. See the procedural-
-- crosshair design spec §10.2.
--
-- Fire-spread (RS-5): the reticle opens with the live bullet cone and closes as
-- it decays. state.recoil is the REAL deterministic 0..1 cone size from the
-- shared BG_CalcWeaponSpreadNormalized (recomputed client-side from the synced
-- fire-ramp anchor), staged via cg.predictedPlayerState — NOT a fake proxy, and
-- it matches the server cone (the bullets). Display-only; gameplay is unchanged.

local MOVE_SPREAD_MAX = 8
local FIRE_SPREAD_MAX = 14   -- px gap added at the cone ceiling (recoil == 1.0)

return {
  dynamic = true,
  base = {
    gap = 4,
    arms = {
      top    = { length = 5, thickness = 1 },
      right  = { length = 5, thickness = 1 },
      bottom = { length = 5, thickness = 1 },
      left   = { length = 5, thickness = 1 },
    },
    dot = { enabled = true, radius = 1, filled = true },
  },
  update = function(state)
    local move_t      = math.min(state.speed / q3.RUN_SPEED, 1.0)
    local move_spread = move_t * MOVE_SPREAD_MAX
    local fire_spread = state.recoil * FIRE_SPREAD_MAX   -- state.recoil is the real 0..1 cone
    local color       = q3.helpers.armor_tint(state.target)

    -- sub-pixel dead-zone: at rest (both spreads ~0, no tint) keep the base reticle
    -- so float noise doesn't jitter the gap.
    if move_spread < 0.5 and fire_spread < 0.5 and color == nil then return nil end

    return {
      gap = 4 + move_spread + fire_spread,
      color = color,
    }
  end,
}
