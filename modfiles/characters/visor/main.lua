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
    pain25  = "sound/player/visor/pain25.opus",
    pain50  = "sound/player/visor/pain50.opus",
    pain75  = "sound/player/visor/pain75.opus",
    pain100 = "sound/player/visor/pain100.opus",
    death1  = "sound/player/visor/death1.opus",
    death2  = "sound/player/visor/death2.opus",
    death3  = "sound/player/visor/death3.opus",
    jump    = "sound/player/visor/jump1.opus",
    taunt   = "sound/player/visor/taunt.opus",
    falling = "sound/player/visor/falling1.opus",
    gasp    = "sound/player/visor/gasp.opus",
    drown   = "sound/player/visor/drown.opus",
  },

  stats = {
    health      = 100,
    speed       = 320,
  },
}
