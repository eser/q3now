-- soldier — the Q1 grunt, rendered as a character. This is the first creature that
-- renders through the character path (CG_Creature → CG_CharacterMesh) instead of the
-- raw-model path: a single-mesh `body` part points at the Q1 soldier .mdl (re-homed to
-- characters/soldier/models/body.mdl by the import pipeline), which the character loader
-- registers MD3-shaped — no mesh conversion. Its animation comes from the .mdl's own
-- derived frame ranges (driven server-side by the behavior FSM's MANIM code), NOT from a
-- character animation table, so the `animations` inherited from the archetype is inert
-- here. Skins are empty on purpose: the .mdl carries its own embedded skin, so the render
-- leaves customShader/customSkin unset and the model's own texture shows.
return {
    archetype    = "mechanized_male",
    name         = "soldier",
    display_name = "Soldier",
    nicknames    = { "soldier", "grunt_q1" },
    role         = "rusher",

    -- Creature-only: spawnable as a monster (loaded by name), hidden from the
    -- player character-select screen. Player characters omit this key and default
    -- to selectable = true.
    selectable   = false,

    model = {
        -- Single-mesh body: overrides the archetype's 3-part head/upper/lower default.
        -- Resolves to characters/soldier/models/body.mdl (the re-homed Q1 soldier.mdl).
        parts = { "body" },
        -- Empty: the .mdl's embedded skin renders; no manifest skin is applied.
        skins = {},
        -- The soldier's collision hull (the human-ish default the spawn used before the
        -- bbox became manifest-driven — carried here so every monster is sized uniformly
        -- from its own manifest).
        bbox  = {
            mins = { -15, -15, -24 },
            maxs = {  15,  15,  32 },
        },
    },
}
