-- Sarge — veteran soldier. Steady aggression, railgun/rocket specialist.
-- High ground control, punishes exposed enemies.
-- Bot data: bot/main.lua (traits/aim/movement) + bot/chats.lua
return {
  archetype    = "humanoid_male",
  name         = "Sarge",
  display_name = "^1Sarge",
  bio          = "Military veteran. No-nonsense, steady, and lethal.",
  role         = "soldier",

  model = {
    skins = {
      default   = "./sarge.tga",
      paintable = { paintable = true,
                    u_torso = "./sarge_pm.tga",
                    l_legs  = "./sarge_pm.tga" },
    },
  },

  sounds = {
    footsteps = "boot",
  },
}
