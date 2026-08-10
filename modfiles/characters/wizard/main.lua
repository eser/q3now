-- wizard — the Q1 scrag (flying spellcaster), rendered as a character. Like the ground
-- monsters, a single-mesh `body` part points at the Q1 wizard .mdl (re-homed to
-- characters/wizard/models/body.mdl by the import pipeline), registered MD3-shaped — no
-- mesh conversion. Its animation comes from the .mdl's own derived frame ranges:
-- hover(idle)/fly(cruise)/magatt(attack)/pain/death, all mapped by the monster alias
-- table. Skins are empty: the .mdl carries its embedded skin.
--
-- movement = "fly": the scrag has no ground path — it steers in a straight 3D line toward
-- its enemy (rising/descending to the player's height) and is never ground-clamped. Its
-- fly frames animate the cruise via the fly->WALK alias.
return {
    archetype    = "mechanized_male",
    name         = "wizard",
    display_name = "Scrag",
    nicknames    = { "wizard", "scrag" },
    role         = "rusher",

    -- Creature-only: spawnable as a monster (loaded by name), hidden from the
    -- player character-select screen. Player characters omit this key and default
    -- to selectable = true.
    selectable   = false,

    -- Flying movement: 3D straight-line steer, no ground clamp.
    movement     = "fly",

    model = {
        parts = { "body" },
        skins = {},
        -- Q1 scrag collision hull (setsize -16 -16 -24 / 16 16 40).
        bbox  = {
            mins = { -16, -16, -24 },
            maxs = {  16,  16,  40 },
        },
    },
}
