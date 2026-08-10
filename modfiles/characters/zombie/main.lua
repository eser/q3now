-- zombie — the Q1 zombie, rendered as a character. A single-mesh `body` part points at
-- the Q1 zombie .mdl (re-homed to characters/zombie/models/body.mdl by the import
-- pipeline), registered MD3-shaped — no mesh conversion. Its animation comes from the
-- .mdl's own derived frame ranges: stand/walk/run/atta+attb+attc(gib throws)/paina..e,
-- all mapped by the monster alias table (atta wins as the rendered attack). Skins are
-- empty: the .mdl carries its embedded skin.
--
-- Q1-authentic note: the zombie .mdl has NO death animation — a Q1 zombie dies only by
-- GIBBING (the separate progs/zom_gib.mdl). So its MANIM_DEATH range stays empty and the
-- client resolves DEATH to STAND (frame 0). That is faithful to the original, not a bug —
-- do not invent a death range the model lacks.
return {
    archetype    = "mechanized_male",
    name         = "zombie",
    display_name = "Zombie",
    nicknames    = { "zombie" },
    role         = "rusher",

    -- Creature-only: spawnable as a monster (loaded by name), hidden from the
    -- player character-select screen. Player characters omit this key and default
    -- to selectable = true.
    selectable   = false,

    model = {
        parts = { "body" },
        skins = {},
        -- Q1 zombie collision hull (setsize -16 -16 -24 / 16 16 32) — human-ish.
        bbox  = {
            mins = { -16, -16, -24 },
            maxs = {  16,  16,  32 },
        },
    },
}
