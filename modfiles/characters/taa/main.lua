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
    pain25  = "sound/player/taa/pain25.wav",
    pain50  = "sound/player/taa/pain50.wav",
    pain75  = "sound/player/taa/pain75.wav",
    pain100 = "sound/player/taa/pain100.wav",
    death1  = "sound/player/taa/death1.wav",
    death2  = "sound/player/taa/death2.wav",
    death3  = "sound/player/taa/death3.wav",
    jump    = "sound/player/taa/jump1.wav",
    taunt   = "sound/player/taa/taunt.wav",
    falling = "sound/player/taa/falling1.wav",
    gasp    = "sound/player/taa/gasp.wav",
    drown   = "sound/player/taa/drown.wav",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
