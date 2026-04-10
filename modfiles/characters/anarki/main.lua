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
    pain25  = "sound/player/anarki/pain25.wav",
    pain50  = "sound/player/anarki/pain50.wav",
    pain75  = "sound/player/anarki/pain75.wav",
    pain100 = "sound/player/anarki/pain100.wav",
    death1  = "sound/player/anarki/death1.wav",
    death2  = "sound/player/anarki/death2.wav",
    death3  = "sound/player/anarki/death3.wav",
    jump    = "sound/player/anarki/jump1.wav",
    taunt   = "sound/player/anarki/taunt.wav",
    falling = "sound/player/anarki/falling1.wav",
    gasp    = "sound/player/anarki/gasp.wav",
    drown   = "sound/player/anarki/drown.wav",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
