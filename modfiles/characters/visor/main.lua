-- Visor — cybernetic duelist. Enhanced optics: railgun specialist.
-- Clinical precision, reads enemy movement, rewards patience.
-- Bot data: bot/main.lua (traits/aim/movement) + bot/chats.lua
return {
  archetype    = "mechanized_male",
  name         = "Visor",
  display_name = "^3Visor",
  nicknames    = { "visor", "goggles" },
  bio          = "A cybernetic gladiator with enhanced optics. Sees everything.",
  role         = "duelist",

  model = {
    -- String-form skin: one texture applied to every surface via customShader.
    -- Table-form skin with paintable=true: per-surface .skin file via customSkin.
    skins = {
      default           = "./visor_default.jpg",
      default_paintable = { paintable = true,
                            u_torso = "./visor_paintable.tga",
                            l_legs  = "./visor_paintable.tga",
                            h_head  = "./visor_paintable.tga" },
    },
  },
}
