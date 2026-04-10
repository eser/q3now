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
    pain25  = "sound/player/grunt/pain25.wav",
    pain50  = "sound/player/grunt/pain50.wav",
    pain75  = "sound/player/grunt/pain75.wav",
    pain100 = "sound/player/grunt/pain100.wav",
    death1  = "sound/player/grunt/death1.wav",
    death2  = "sound/player/grunt/death2.wav",
    death3  = "sound/player/grunt/death3.wav",
    jump    = "sound/player/grunt/jump1.wav",
    taunt   = "sound/player/grunt/taunt.wav",
    falling = "sound/player/grunt/falling1.wav",
    gasp    = "sound/player/grunt/gasp.wav",
    drown   = "sound/player/grunt/drown.wav",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
