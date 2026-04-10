return {
  name         = "Visor",
  display_name = "Visor",
  nicknames    = {"visor", "gogles"},
  bio          = "A cybernetic gladiator with enhanced optics. Sees everything.",
  role         = "duelist",

  model     = "models/players/visor/body",
  head      = "models/players/visor/head",
  icon      = "gfx/characters/visor/icon_64",
  skin      = "default",
  skin_tint = "paintable",

  sounds = {
    pain25  = "sound/player/visor/pain25.wav",
    pain50  = "sound/player/visor/pain50.wav",
    pain75  = "sound/player/visor/pain75.wav",
    pain100 = "sound/player/visor/pain100.wav",
    death1  = "sound/player/visor/death1.wav",
    death2  = "sound/player/visor/death2.wav",
    death3  = "sound/player/visor/death3.wav",
    jump    = "sound/player/visor/jump1.wav",
    taunt   = "sound/player/visor/taunt.wav",
    falling = "sound/player/visor/falling1.wav",
    gasp    = "sound/player/visor/gasp.wav",
    drown   = "sound/player/visor/drown.wav",
  },

  stats = {
    health      = 100,
    speed       = 320,
  },
}
