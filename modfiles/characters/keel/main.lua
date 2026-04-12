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
    pain25  = "sound/player/keel/pain25.opus",
    pain50  = "sound/player/keel/pain50.opus",
    pain75  = "sound/player/keel/pain75.opus",
    pain100 = "sound/player/keel/pain100.opus",
    death1  = "sound/player/keel/death1.opus",
    death2  = "sound/player/keel/death2.opus",
    death3  = "sound/player/keel/death3.opus",
    jump    = "sound/player/keel/jump1.opus",
    taunt   = "sound/player/keel/taunt.opus",
    falling = "sound/player/keel/falling1.opus",
    gasp    = "sound/player/keel/gasp.opus",
    drown   = "sound/player/keel/drown.opus",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
