return {
  name         = "tao",
  display_name = "Tao",
  bio          = "A suicidal berserker. Zero self-preservation. Pure destruction.",
  role         = "berserker",

  model     = "models/players/tao/body",
  head      = "models/players/tao/head",
  skin      = "default",
  skin_tint = "paintable",

  icon      = "gfx/characters/tao/icon_64",

  sounds = {
    pain25  = "sound/player/tao/pain25.wav",
    pain50  = "sound/player/tao/pain50.wav",
    pain75  = "sound/player/tao/pain75.wav",
    pain100 = "sound/player/tao/pain100.wav",
    death1  = "sound/player/tao/death1.wav",
    death2  = "sound/player/tao/death2.wav",
    death3  = "sound/player/tao/death3.wav",
    jump    = "sound/player/tao/jump1.wav",
    taunt   = "sound/player/tao/taunt.wav",
    falling = "sound/player/tao/falling1.wav",
    gasp    = "sound/player/tao/gasp.wav",
    drown   = "sound/player/tao/drown.wav",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
