-- testmonster — dev-only test character for the monster behavior Lua-decide path.
-- Minimal: reuses the mechanized_male archetype for model/anims; its only reason
-- to exist is bot/main.lua's decide() function, which drives the Lua-decide opt-in
-- so the behavior layer's Lua path can be exercised and measured. Not a shipping
-- character — no bespoke model/skins.
return {
    archetype    = "mechanized_male",
    name         = "TestMonster",
    display_name = "^5TestMonster",
    nicknames    = { "testmonster" },
    role         = "duelist",
}
