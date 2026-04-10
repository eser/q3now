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
    pain25  = "sound/player/sarge/pain25.wav",
    pain50  = "sound/player/sarge/pain50.wav",
    pain75  = "sound/player/sarge/pain75.wav",
    pain100 = "sound/player/sarge/pain100.wav",
    death1  = "sound/player/sarge/death1.wav",
    death2  = "sound/player/sarge/death2.wav",
    death3  = "sound/player/sarge/death3.wav",
    jump    = "sound/player/sarge/jump1.wav",
    taunt   = "sound/player/sarge/taunt.wav",
    falling = "sound/player/sarge/falling1.wav",
    gasp    = "sound/player/sarge/gasp.wav",
    drown   = "sound/player/sarge/drown.wav",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
