-- knight — the Q1 sword knight, rendered as a character. Like the soldier and dog, a
-- single-mesh `body` part points at the Q1 knight .mdl (re-homed to
-- characters/knight/models/body.mdl by the import pipeline), registered MD3-shaped by
-- the character loader — no mesh conversion. Its animation comes from the .mdl's own
-- derived frame ranges: stand/walk/runb(run)/runattack/attackb(swing)/pain/death, all
-- mapped by the monster alias table. Skins are empty: the .mdl carries its embedded skin.
-- The bbox is the knight's own collision hull (a human-sized melee fighter).
return {
    archetype    = "mechanized_male",
    name         = "knight",
    display_name = "Knight",
    nicknames    = { "knight", "swordknight" },
    role         = "rusher",

    -- Creature-only: spawnable as a monster (loaded by name), hidden from the
    -- player character-select screen. Player characters omit this key and default
    -- to selectable = true.
    selectable   = false,

    model = {
        parts = { "body" },
        skins = {},
        -- Q1 knight collision hull (setsize -16 -16 -24 / 16 16 40) — human-sized.
        bbox  = {
            mins = { -16, -16, -24 },
            maxs = {  16,  16,  40 },
        },
    },
}
