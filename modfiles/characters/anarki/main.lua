return {
  name         = "Anarki",
  display_name = "Anarki",
  bio          = "A rebellious cyberpunk with a built-in health injector. Chaotic and unpredictable.",
  role         = "skirmisher",

  model     = "models/players/anarki/body",
  head      = "models/players/anarki/head",
  skin      = "default",

  icon      = "gfx/characters/anarki/icon_64",

  sounds = {
    pain25  = "sound/player/anarki/pain25.opus",
    pain50  = "sound/player/anarki/pain50.opus",
    pain75  = "sound/player/anarki/pain75.opus",
    pain100 = "sound/player/anarki/pain100.opus",
    death1  = "sound/player/anarki/death1.opus",
    death2  = "sound/player/anarki/death2.opus",
    death3  = "sound/player/anarki/death3.opus",
    jump    = "sound/player/anarki/jump1.opus",
    taunt   = "sound/player/anarki/taunt.opus",
    falling = "sound/player/anarki/falling1.opus",
    gasp    = "sound/player/anarki/gasp.opus",
    drown   = "sound/player/anarki/drown.opus",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
