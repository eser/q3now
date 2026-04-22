-- Major — female all-rounder. Methodical, patient, camps effectively at high skill.
-- Transitions from cautious to oppressive aggressor as skill rises.
-- Bot data: bot/main.lua (traits/aim/movement) + bot/chats.lua
return {
  archetype    = "humanoid_female",
  name         = "Major",
  display_name = "^6Major",
  bio          = "Female all-rounder. Methodical, camps effectively at high skill.",
  role         = "soldier",

  model = {
    skins = {
      default   = "./major.tga",
      paintable = { paintable = true,
                    u_torso = "./major_pm.tga",
                    l_legs  = "./major_pm.tga" },
    },
  },
}
