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
    -- Icon is auto-derived from skin keys: tries characters/{name}/icon_{skinname}.{png,tga,jpg}
    -- in skin-list order and uses the first one that resolves (see CL_Characters_RegisterIcons).
    headoffset = { 0, 0, 0 },
    skins      = {},                           -- each character provides their own skins
  },

  sounds = {
    gender = "male",
  },

  stats = {
    health = 100,
    speed  = 320,
  },
}
