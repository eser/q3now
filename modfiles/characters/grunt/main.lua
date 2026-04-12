return {
  name         = "Grunt",
  display_name = "Grunt",
  bio          = "Camper and defender. High self-preservation, steady aggression.",
  role         = "soldier",

  model     = "models/players/grunt/body",
  head      = "models/players/grunt/head",
  skin      = "default",
  skin_tint = "paintable",

  icon      = "gfx/characters/grunt/icon_64",

  sounds = {
    pain25  = "sound/player/grunt/pain25.opus",
    pain50  = "sound/player/grunt/pain50.opus",
    pain75  = "sound/player/grunt/pain75.opus",
    pain100 = "sound/player/grunt/pain100.opus",
    death1  = "sound/player/grunt/death1.opus",
    death2  = "sound/player/grunt/death2.opus",
    death3  = "sound/player/grunt/death3.opus",
    jump    = "sound/player/grunt/jump1.opus",
    taunt   = "sound/player/grunt/taunt.opus",
    falling = "sound/player/grunt/falling1.opus",
    gasp    = "sound/player/grunt/gasp.opus",
    drown   = "sound/player/grunt/drown.opus",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
