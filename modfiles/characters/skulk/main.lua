-- skulk — cautious holder/ambusher proof monster (behavior-layer Rule-3 demo).
-- Reuses the mechanized_male archetype; its behavior lives entirely in
-- bot/main.lua's decide() + traits. Paired with "grunt" (an aggressive rusher)
-- to prove two monsters behave differently on the SAME shared native states —
-- the difference is in this .lua data, zero new per-monster C.
return {
    archetype    = "mechanized_male",
    name         = "Skulk",
    display_name = "^4Skulk",
    nicknames    = { "skulk" },
    role         = "holder",
}
