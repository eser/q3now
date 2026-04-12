return {
  name         = "Sarge",
  display_name = "Sarge",
  bio          = "Military veteran. No-nonsense, steady, and lethal.",
  role         = "soldier",

  model     = "models/players/sarge/body",
  head      = "models/players/sarge/head",
  skin      = "default",
  skin_tint = "paintable",

  icon      = "gfx/characters/sarge/icon_64",

  sounds = {
    pain25  = "sound/player/sarge/pain25.opus",
    pain50  = "sound/player/sarge/pain50.opus",
    pain75  = "sound/player/sarge/pain75.opus",
    pain100 = "sound/player/sarge/pain100.opus",
    death1  = "sound/player/sarge/death1.opus",
    death2  = "sound/player/sarge/death2.opus",
    death3  = "sound/player/sarge/death3.opus",
    jump    = "sound/player/sarge/jump1.opus",
    taunt   = "sound/player/sarge/taunt.opus",
    falling = "sound/player/sarge/falling1.opus",
    gasp    = "sound/player/sarge/gasp.opus",
    drown   = "sound/player/sarge/drown.opus",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
