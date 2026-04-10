return {
  name         = "Crash",
  display_name = "Crash",
  bio          = "A tough-as-nails combat veteran who plays it smart. Precision, patience, survival.",
  role         = "defender",

  model     = "models/players/crash/body",
  head      = "models/players/crash/head",
  skin      = "default",

  icon      = "gfx/characters/crash/icon_64",

  sounds = {
    pain25  = "sound/player/crash/pain25.wav",
    pain50  = "sound/player/crash/pain50.wav",
    pain75  = "sound/player/crash/pain75.wav",
    pain100 = "sound/player/crash/pain100.wav",
    death1  = "sound/player/crash/death1.wav",
    death2  = "sound/player/crash/death2.wav",
    death3  = "sound/player/crash/death3.wav",
    jump    = "sound/player/crash/jump1.wav",
    taunt   = "sound/player/crash/taunt.wav",
    falling = "sound/player/crash/falling1.wav",
    gasp    = "sound/player/crash/gasp.wav",
    drown   = "sound/player/crash/drown.wav",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
