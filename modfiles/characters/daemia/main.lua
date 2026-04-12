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
    pain25  = "sound/player/daemia/pain25.opus",
    pain50  = "sound/player/daemia/pain50.opus",
    pain75  = "sound/player/daemia/pain75.opus",
    pain100 = "sound/player/daemia/pain100.opus",
    death1  = "sound/player/daemia/death1.opus",
    death2  = "sound/player/daemia/death2.opus",
    death3  = "sound/player/daemia/death3.opus",
    jump    = "sound/player/daemia/jump1.opus",
    taunt   = "sound/player/daemia/taunt.opus",
    falling = "sound/player/daemia/falling1.opus",
    gasp    = "sound/player/daemia/gasp.opus",
    drown   = "sound/player/daemia/drown.opus",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
