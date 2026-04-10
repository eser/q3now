return {
  name         = "Keel",
  display_name = "Keel",
  bio          = "A cold, calculating cyborg soldier built for destruction. Methodical, heavy firepower.",
  role         = "heavy",

  model     = "models/players/keel/body",
  head      = "models/players/keel/head",
  skin      = "default",

  icon      = "gfx/characters/keel/icon_64",

  sounds = {
    pain25  = "sound/player/keel/pain25.wav",
    pain50  = "sound/player/keel/pain50.wav",
    pain75  = "sound/player/keel/pain75.wav",
    pain100 = "sound/player/keel/pain100.wav",
    death1  = "sound/player/keel/death1.wav",
    death2  = "sound/player/keel/death2.wav",
    death3  = "sound/player/keel/death3.wav",
    jump    = "sound/player/keel/jump1.wav",
    taunt   = "sound/player/keel/taunt.wav",
    falling = "sound/player/keel/falling1.wav",
    gasp    = "sound/player/keel/gasp.wav",
    drown   = "sound/player/keel/drown.wav",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
