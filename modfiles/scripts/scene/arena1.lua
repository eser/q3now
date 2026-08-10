-- Cinematic-camera fixture for the arena1 visual-regression golden.
--
-- A distinctive eye-path arc anchored in arena1's lit playable space (near the
-- viewport gate's known-good camera at ~1052 1432 100), sweeping laterally and
-- rising so the view frames real geometry throughout. No look-at target: the
-- facing follows the path tangent, so the camera looks along its travel.
--
-- Timeline: 8 s total. An FOV event at t=3 s narrows to 55 deg over 2 s (so the
-- mid-arc sample at ~4-4.5 s is inside the tightened FOV, visually distinct from
-- the 90->70 baseline). STOP at t=8 s ends the cutscene. The golden samples a
-- mid-segment point (between the t=4 and t=6 knots), where per-ms view change is
-- smallest -> least run-to-run jitter.
return {
  time = 8.0,
  eyePath = {
    type = 'catmullrom',
    boundary = 'clamped',
    knots = {
      { t = 0.0, pos = {  700, 1200,  80 } },
      { t = 2.0, pos = {  900, 1350, 140 } },
      { t = 4.0, pos = { 1052, 1432, 180 } },
      { t = 6.0, pos = { 1200, 1520, 160 } },
      { t = 8.0, pos = { 1350, 1600, 120 } },
    },
  },
  fov = { start = 90, ['end'] = 70, length = 4 },
  events = {
    { verb = 'fov',  time = 3000, fov = 55, length = 2 },
    { verb = 'stop', time = 8000 },
  },
}
