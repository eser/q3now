-- shambler — the Q1 shambler, rendered as a character. A single-mesh `body` part points
-- at the Q1 shambler .mdl (re-homed to characters/shambler/models/body.mdl by the import
-- pipeline), registered MD3-shaped — no mesh conversion. Its animation comes from the
-- .mdl's own derived frame ranges: stand/walk/run/smash+swingr+swingl+magic(attacks)/
-- pain/death, all mapped by the monster alias table (smash wins as the rendered melee;
-- the other attack beats collapse to the one attack code for now). Skins are empty: the
-- .mdl carries its embedded skin. The bbox is the shambler's own large hull.
return {
    archetype    = "mechanized_male",
    name         = "shambler",
    display_name = "Shambler",
    nicknames    = { "shambler", "sham" },
    role         = "rusher",

    -- Creature-only: spawnable as a monster (loaded by name), hidden from the
    -- player character-select screen. Player characters omit this key and default
    -- to selectable = true.
    selectable   = false,

    model = {
        parts = { "body" },
        skins = {},
        -- Q1 shambler collision hull (setsize -32 -32 -24 / 32 32 64).
        bbox  = {
            mins = { -32, -32, -24 },
            maxs = {  32,  32,  64 },
        },
    },
}
