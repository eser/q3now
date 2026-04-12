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
    pain25  = "sound/player/crash/pain25.opus",
    pain50  = "sound/player/crash/pain50.opus",
    pain75  = "sound/player/crash/pain75.opus",
    pain100 = "sound/player/crash/pain100.opus",
    death1  = "sound/player/crash/death1.opus",
    death2  = "sound/player/crash/death2.opus",
    death3  = "sound/player/crash/death3.opus",
    jump    = "sound/player/crash/jump1.opus",
    taunt   = "sound/player/crash/taunt.opus",
    falling = "sound/player/crash/falling1.opus",
    gasp    = "sound/player/crash/gasp.opus",
    drown   = "sound/player/crash/drown.opus",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
