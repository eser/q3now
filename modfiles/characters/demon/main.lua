-- demon — the Q1 fiend, rendered as a character. A single-mesh `body` part points at
-- the Q1 demon .mdl (re-homed to characters/demon/models/body.mdl by the import
-- pipeline), registered MD3-shaped — no mesh conversion. Its animation comes from the
-- .mdl's own derived frame ranges: stand/walk/run/leap/attacka(melee)/pain/death, all
-- mapped by the monster alias table. Skins are empty: the .mdl carries its embedded skin.
-- The bbox is the fiend's own hull — a large leaping beast, wider and taller than a human.
return {
    archetype    = "mechanized_male",
    name         = "demon",
    display_name = "Fiend",
    nicknames    = { "demon", "fiend" },
    role         = "rusher",

    -- Creature-only: spawnable as a monster (loaded by name), hidden from the
    -- player character-select screen. Player characters omit this key and default
    -- to selectable = true.
    selectable   = false,

    model = {
        parts = { "body" },
        skins = {},
        -- Q1 fiend collision hull (setsize -32 -32 -24 / 32 32 64).
        bbox  = {
            mins = { -32, -32, -24 },
            maxs = {  32,  32,  64 },
        },
    },
}
