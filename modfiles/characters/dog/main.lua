-- dog — the Q1 rottweiler, rendered as a character. Like the soldier, a single-mesh
-- `body` part points at the Q1 dog .mdl (re-homed to characters/dog/models/body.mdl by
-- the import pipeline), which the character loader registers MD3-shaped — no mesh
-- conversion. Its animation comes from the .mdl's own derived frame ranges (the dog has
-- stand/walk/run/leap/attack/pain/death, all already mapped by the monster alias table),
-- driven server-side by the behavior FSM's MANIM code. Skins are empty: the .mdl carries
-- its own embedded skin. The bbox is the dog's own collision hull — a dog is wider and
-- lower than a soldier, so it carries its size here rather than borrowing the default.
return {
    archetype    = "mechanized_male",
    name         = "dog",
    display_name = "Dog",
    nicknames    = { "dog", "rottweiler" },
    role         = "rusher",

    -- Creature-only: spawnable as a monster (loaded by name), hidden from the
    -- player character-select screen. Player characters omit this key and default
    -- to selectable = true.
    selectable   = false,

    -- A dog cannot operate world activators (press buttons, etc.). Characters that
    -- omit this key default to being able to activate, so a plain player/bot can;
    -- an explicit false denies it. Read game-side via char:<name>:can_activate.
    can_activate = false,

    model = {
        parts = { "body" },
        skins = {},
        -- Q1 dog collision hull (setsize -32 -32 -24 / 32 32 40).
        bbox  = {
            mins = { -32, -32, -24 },
            maxs = {  32,  32,  40 },
        },
    },
}
