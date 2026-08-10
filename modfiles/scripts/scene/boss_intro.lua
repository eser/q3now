-- boss_intro.lua — CINEMATIC set-piece scene with a named look-at target for
-- runtime actor-binding (Fork-1 Faz-7c). The camera arcs across the arena while
-- looking at the "boss" target. Played WITHOUT an actor binding, the look-at
-- follows the authored `boss` spline below (static). Played via
-- `playscene boss_intro <boss-entityNum>` (or `... self`), target slot 0 ("boss")
-- is bound to that live entity, so the camera tracks the real boss as it moves —
-- the spline below is then ignored for that target.
--
-- Timeline: 6 s. A TARGET event at t=0 makes "boss" the active look-at for the
-- whole shot; STOP at t=6 s ends it. Anchored in arena1's lit playable space.
return {
  time = 6.0,
  eyePath = {
    type = 'catmullrom',
    boundary = 'clamped',
    knots = {
      { t = 0.0, pos = {  700, 1200, 120 } },
      { t = 3.0, pos = { 1000, 1400, 160 } },
      { t = 6.0, pos = { 1300, 1600, 120 } },
    },
  },
  targets = {
    -- the authored fallback spline for "boss" (used only when NO actor is bound).
    -- A fixed point near the arena center so an unbound control shot is stable.
    boss = {
      type = 'catmullrom',
      boundary = 'clamped',
      knots = {
        { t = 0.0, pos = { 1052, 1432, 100 } },
        { t = 6.0, pos = { 1052, 1432, 100 } },
      },
    },
  },
  events = {
    { verb = 'target', time = 0,    name = 'boss' },
    { verb = 'stop',   time = 6000 },
  },
}
