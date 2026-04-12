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
    pain25  = "sound/player/major/pain25.opus",
    pain50  = "sound/player/major/pain50.opus",
    pain75  = "sound/player/major/pain75.opus",
    pain100 = "sound/player/major/pain100.opus",
    death1  = "sound/player/major/death1.opus",
    death2  = "sound/player/major/death2.opus",
    death3  = "sound/player/major/death3.opus",
    jump    = "sound/player/major/jump1.opus",
    taunt   = "sound/player/major/taunt.opus",
    falling = "sound/player/major/falling1.opus",
    gasp    = "sound/player/major/gasp.opus",
    drown   = "sound/player/major/drown.opus",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
