-- warden_intro.lua — the boss-intro cutscene for the arena1 warden encounter.
-- The camera arcs across the encounter space while looking at the "warden"
-- target. Played via `playscene warden_intro self`, target slot 0 ("warden") is
-- bound to the live boss entity, so the camera tracks the real warden as it moves
-- (the authored spline below is the fallback used only when no actor is bound).
--
-- A caption at t=0 names the encounter (l10n key scene/warden/intro). Timeline:
-- 4 s, STOP at the end. Anchored in arena1's lit playable space (~1052 1432).
return {
  time = 4.0,
  eyePath = {
    type = 'catmullrom',
    boundary = 'clamped',
    knots = {
      { t = 0.0, pos = {  820, 1320, 130 } },
      { t = 2.0, pos = {  980, 1420, 170 } },
      { t = 4.0, pos = { 1160, 1500, 140 } },
    },
  },
  targets = {
    -- fallback spline for "warden" (used only when no actor is bound): the boss's
    -- spawn point, so an unbound control shot still frames the encounter.
    warden = {
      type = 'catmullrom',
      boundary = 'clamped',
      knots = {
        { t = 0.0, pos = { 1052, 1432, 100 } },
        { t = 4.0, pos = { 1052, 1432, 100 } },
      },
    },
  },
  events = {
    { verb = 'target',  time = 0,    name = 'warden' },
    { verb = 'caption', time = 0,    name = 'scene/warden/intro' },
    { verb = 'stop',    time = 4000 },
  },
}
