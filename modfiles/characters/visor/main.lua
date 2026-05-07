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

    model        = {
        -- String-form skin: one texture applied to every surface via customShader.
        -- Table-form skin with paintable=true: per-surface .skin file via customSkin.
        skins = {
            default   = {
                paintable = true,
                u_torso   = "./skin_default.png",
                l_legs    = "./skin_default.png",
                h_head    = "./skin_default.png"
            },
            original  = "./skin_original.jpg",
            deadlight = "./skin_deadlight.jpg",
            shade     = "./skin_shade.jpg",
        },
    },
}
