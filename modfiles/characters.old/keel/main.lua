-- Keel — cold cyborg heavy. Methodical destruction, heavy firepower.
-- High firethrottle, prefers splash and sustained fire over precise shots.
-- Multi-surface skins: explicit per-surface table form generates a .skin file.
-- Bot data: bot/main.lua (traits/aim/movement); no bot/chats.lua — inherits base.
return {
  archetype    = "mechanized",
  name         = "Keel",
  display_name = "^5Keel",
  bio          = "A cold, calculating cyborg soldier built for destruction. Methodical, heavy firepower.",
  role         = "heavy",

  model = {
    skins = {
      default = { u_torso = "./keel_torso.tga",
                  l_legs  = "./keel_legs.tga",
                  h_head  = "./keel_head.tga" },
      chrome  = { u_torso = "./keel_chrome_torso.tga",
                  l_legs  = "./keel_chrome_legs.tga",
                  h_head  = "./keel_chrome_head.tga" },
    },
  },

  sounds = {
    footsteps = "mech",
  },
}
