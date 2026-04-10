return {
  name         = "Major",
  display_name = "Major",
  bio          = "Female all-rounder. Methodical, camps effectively at high skill.",
  role         = "soldier",

  model     = "models/players/major/body",
  head      = "models/players/major/head",
  skin      = "default",
  skin_tint = "paintable",

  icon      = "gfx/characters/major/icon_64",

  sounds = {
    pain25  = "sound/player/major/pain25.wav",
    pain50  = "sound/player/major/pain50.wav",
    pain75  = "sound/player/major/pain75.wav",
    pain100 = "sound/player/major/pain100.wav",
    death1  = "sound/player/major/death1.wav",
    death2  = "sound/player/major/death2.wav",
    death3  = "sound/player/major/death3.wav",
    jump    = "sound/player/major/jump1.wav",
    taunt   = "sound/player/major/taunt.wav",
    falling = "sound/player/major/falling1.wav",
    gasp    = "sound/player/major/gasp.wav",
    drown   = "sound/player/major/drown.wav",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
