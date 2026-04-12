return {
  name         = "taa",
  display_name = "Taa",
  bio          = "A silent berserker. Maximum aggression, no conversation.",
  role         = "berserker",

  model     = "models/players/taa/body",
  head      = "models/players/taa/head",
  skin      = "default",
  skin_tint = "paintable",

  icon      = "gfx/characters/taa/icon_64",

  sounds = {
    pain25  = "sound/player/taa/pain25.opus",
    pain50  = "sound/player/taa/pain50.opus",
    pain75  = "sound/player/taa/pain75.opus",
    pain100 = "sound/player/taa/pain100.opus",
    death1  = "sound/player/taa/death1.opus",
    death2  = "sound/player/taa/death2.opus",
    death3  = "sound/player/taa/death3.opus",
    jump    = "sound/player/taa/jump1.opus",
    taunt   = "sound/player/taa/taunt.opus",
    falling = "sound/player/taa/falling1.opus",
    gasp    = "sound/player/taa/gasp.opus",
    drown   = "sound/player/taa/drown.opus",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
