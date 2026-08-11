-- grunt — aggressive rusher proof monster (behavior-layer Rule-3 demo).
-- Reuses the mechanized_male archetype for model/anims; its behavior lives
-- entirely in bot/main.lua's decide() + traits. Paired with "skulk" (a cautious
-- holder) to prove two monsters behave differently on the SAME shared native
-- states — the difference is in this .lua data, zero new per-monster C.
return {
    archetype    = "mechanized_male",
    name         = "Grunt",
    display_name = "^1Grunt",
    nicknames    = { "grunt" },
    role         = "rusher",
    selectable   = true,
    bot_eligible = true,

    -- Grunt is a distinct behavior/personality profile that intentionally
    -- shares Visor's licensed mechanized-male render assets.  The explicit
    -- VFS root avoids duplicating those bytes into pax21.
    model = {
        root = "characters/visor/models",
        icon = "characters/visor/models/icon_default.png",
        skins = {
            default = {
                paintable = true,
                u_torso = "characters/visor/models/skin_default.png",
                l_legs  = "characters/visor/models/skin_default.png",
                h_head  = "characters/visor/models/skin_default.png",
            },
        },
    },
}
