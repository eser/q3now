return {
  name         = "Stripe",
  display_name = "Stripe",
  bio          = "Cautious southern fighter. Defensive playstyle, great rocket jumper.",
  role         = "soldier",

  model     = "models/players/stripe/body",
  head      = "models/players/stripe/head",
  skin      = "default",
  skin_tint = "paintable",

  icon      = "gfx/characters/stripe/icon_64",

  sounds = {
    pain25  = "sound/player/stripe/pain25.opus",
    pain50  = "sound/player/stripe/pain50.opus",
    pain75  = "sound/player/stripe/pain75.opus",
    pain100 = "sound/player/stripe/pain100.opus",
    death1  = "sound/player/stripe/death1.opus",
    death2  = "sound/player/stripe/death2.opus",
    death3  = "sound/player/stripe/death3.opus",
    jump    = "sound/player/stripe/jump1.opus",
    taunt   = "sound/player/stripe/taunt.opus",
    falling = "sound/player/stripe/falling1.opus",
    gasp    = "sound/player/stripe/gasp.opus",
    drown   = "sound/player/stripe/drown.opus",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
