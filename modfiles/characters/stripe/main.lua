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
    pain25  = "sound/player/stripe/pain25.wav",
    pain50  = "sound/player/stripe/pain50.wav",
    pain75  = "sound/player/stripe/pain75.wav",
    pain100 = "sound/player/stripe/pain100.wav",
    death1  = "sound/player/stripe/death1.wav",
    death2  = "sound/player/stripe/death2.wav",
    death3  = "sound/player/stripe/death3.wav",
    jump    = "sound/player/stripe/jump1.wav",
    taunt   = "sound/player/stripe/taunt.wav",
    falling = "sound/player/stripe/falling1.wav",
    gasp    = "sound/player/stripe/gasp.wav",
    drown   = "sound/player/stripe/drown.wav",
  },

  stats = {
    base_health = 125,
    base_armor  = 0,
    max_health  = 200,
    max_armor   = 200,
    speed       = 320,
  },
}
