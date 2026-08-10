-- Cinematic-director fixture (DOOM-3-style third-person cutscene).
--
-- Unlike arena1.lua (a WORLD-space POV fly-through for the render golden), this
-- one exercises the full cinematic director: cameraSpace='player' so every knot
-- is an OFFSET from the local player, rotated by the player's yaw and added to
-- the player origin (orbital / over-the-shoulder). The camera detaches, the
-- player MODEL is shown (thirdperson), the HUD + view-weapon are suppressed,
-- local input is frozen, and a caption is timed in — then it all reverts.
--
-- All of this is CLIENT-VIEW-ONLY: no server round-trip, no pm_type change. The
-- freeze is a null-move usercmd built client-side; prediction replays it so the
-- player never snaps.
--
-- Orbit: the eye starts behind the player (-X offset in player space = behind
-- the facing) at chest+ height, sweeps around to the side and rises, framing the
-- player model throughout. The look-at target sits at the player's chest
-- (offset {0,0,40}), so the camera always faces the model as it orbits.
--
-- Timeline: 6 s. t=0 detach (thirdperson on / hud off / freeze on). t=2 s a
-- caption fires (l10n key, logged as a stub). t=5 s everything reverts (freeze
-- off, thirdperson off) so the final second shows the clean hand-back. STOP at
-- t=6 s ends the cutscene and the normal player view + HUD resume.
return {
  time = 6.0,
  cameraSpace = 'player',        -- knots are player-relative offsets (orbital)
  eyePath = {
    type = 'catmullrom',
    boundary = 'clamped',
    knots = {
      -- a tight over-the-shoulder orbit (radius ~85, +45 height) — small enough
      -- to stay clear of arena1's cramped spawn geometry while keeping the player
      -- model framed. Offsets are player-relative; yaw rotates them each frame.
      { t = 0.0, pos = {  -85,   0,  45 } },   -- behind the player
      { t = 1.5, pos = {  -60, -60,  55 } },   -- swinging to the left
      { t = 3.0, pos = {    0, -85,  60 } },   -- out to the side
      { t = 4.5, pos = {   60, -55,  55 } },   -- coming around the front-left
      { t = 6.0, pos = {   85,   0,  45 } },   -- front, settling
    },
  },
  targets = {
    -- a static look-at at the player's chest (offset {0,0,40}); in player space
    -- this transforms to the SAME frame as the eye, so the camera tracks the
    -- model as it orbits.
    hero = {
      type = 'catmullrom',
      knots = {
        { t = 0.0, pos = { 0, 0, 40 } },
        { t = 6.0, pos = { 0, 0, 40 } },
      },
    },
  },
  fov = { start = 80, ['end'] = 65, length = 3 },
  events = {
    { verb = 'thirdperson',  time = 0,    on = 1 },              -- show the player model
    { verb = 'hud',          time = 0,    on = 0 },              -- hide the HUD + weapon
    { verb = 'playerfreeze', time = 0,    on = 1 },              -- suppress local input
    { verb = 'target',       time = 0,    name = 'hero' },       -- face the player model
    { verb = 'caption',      time = 2000, key = 'scene/cinematic/intro' },
    { verb = 'playerfreeze', time = 5000, on = 0 },              -- restore input (clean hand-back)
    { verb = 'thirdperson',  time = 5000, on = 0 },              -- back to first person
    { verb = 'stop',         time = 6000 },                      -- end the cutscene
  },
}
