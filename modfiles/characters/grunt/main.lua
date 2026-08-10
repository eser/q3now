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
}
