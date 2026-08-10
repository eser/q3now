-- boss — Chthon, the Q1 lava lord, rendered as a character. A single-mesh `body` part
-- points at the Q1 boss .mdl (re-homed to characters/boss/models/body.mdl by the import
-- pipeline), registered MD3-shaped — no mesh conversion. Its animation comes from the
-- .mdl's own derived frame ranges: walk/attack(lavaball throw)/shocka-c(shockwave)/death,
-- all mapped by the monster alias table (attack and shock* both -> the attack code; the
-- rise* frames stay unmapped since Chthon spawns already-risen). Skins are empty: the
-- .mdl carries its embedded skin.
--
-- attack = "ranged": Chthon does not melee — he lobs an aimed lavaball at his enemy in 3D
-- from a distance (the ranged-attack behavior branch). He is stationary (no `movement`
-- key, no nav goal) — a scripted encounter drives him.
return {
    archetype    = "mechanized_male",
    name         = "boss",
    display_name = "Chthon",
    nicknames    = { "boss", "chthon" },
    role         = "boss",

    -- Creature-only: spawnable as a monster (loaded by name), hidden from the
    -- player character-select screen.
    selectable   = false,

    -- Ranged attack: fires an aimed lavaball instead of a melee swing.
    attack       = "ranged",

    model = {
        parts = { "body" },
        skins = {},
        -- Q1 Chthon collision hull (setsize -128 -128 -24 / 128 128 256) — a large boss.
        bbox  = {
            mins = { -128, -128, -24 },
            maxs = {  128,  128, 256 },
        },
    },
}
