return {
  name         = "Daemia",
  display_name = "Daemia",
  bio          = "Bounty hunter. Always jumping, balanced stats, Spanish flair.",
  role         = "hunter",

  model     = "models/players/daemia/body",
  head      = "models/players/daemia/head",
  skin      = "default",
  skin_tint = "paintable",

  icon      = "gfx/characters/daemia/icon_64",

  sounds = {
    pain25  = "sound/player/daemia/pain25.wav",
    pain50  = "sound/player/daemia/pain50.wav",
    pain75  = "sound/player/daemia/pain75.wav",
    pain100 = "sound/player/daemia/pain100.wav",
    death1  = "sound/player/daemia/death1.wav",
    death2  = "sound/player/daemia/death2.wav",
    death3  = "sound/player/daemia/death3.wav",
    jump    = "sound/player/daemia/jump1.wav",
    taunt   = "sound/player/daemia/taunt.wav",
    falling = "sound/player/daemia/falling1.wav",
    gasp    = "sound/player/daemia/gasp.wav",
    drown   = "sound/player/daemia/drown.wav",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
