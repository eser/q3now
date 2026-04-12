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
    pain25  = "sound/player/tao/pain25.opus",
    pain50  = "sound/player/tao/pain50.opus",
    pain75  = "sound/player/tao/pain75.opus",
    pain100 = "sound/player/tao/pain100.opus",
    death1  = "sound/player/tao/death1.opus",
    death2  = "sound/player/tao/death2.opus",
    death3  = "sound/player/tao/death3.opus",
    jump    = "sound/player/tao/jump1.opus",
    taunt   = "sound/player/tao/taunt.opus",
    falling = "sound/player/tao/falling1.opus",
    gasp    = "sound/player/tao/gasp.opus",
    drown   = "sound/player/tao/drown.opus",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
