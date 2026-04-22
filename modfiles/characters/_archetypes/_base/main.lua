-- characters/_archetypes/_base/main.lua
-- Absolute minimum defaults. First layer merged into every character.
-- Required fields (name, display_name) are NOT present here.
return {
  archetype  = nil,          -- characters supply this; absent defaults to "humanoid_male"
  nicknames  = {},
  bio        = "",
  role       = nil,

  model = {
    parts      = { "head", "upper", "lower" }, -- relative base names under models/
    icon       = "icon_64.tga",                -- relative to characters/{name}/
    headoffset = { 0, 0, 0 },
    skins      = {},                           -- each character provides their own skins
  },

  sounds = {
    footsteps = "normal",    -- enum: normal | boot | flesh | mech | energy
  },

  stats = {
    health = 100,
    speed  = 320,
  },
}
