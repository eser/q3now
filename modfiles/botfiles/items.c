// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Bot item configuration (botlib BotSetupGoalAI / LoadItemConfig).
//
// Each `iteminfo "<classname>" { ... }` entry maps a pickup entity to the data
// the bot goal AI needs. The fields:
//   name         display name (matches the pickup string)
//   model        primary world model path
//   modelindex   MUST equal the item's index in code/game/bg_misc.c bg_itemlist
//                (the engine sets ent->s.modelindex = item - bg_itemlist, and
//                BotUpdateEntityItems matches level items by this index). The
//                indices below are the FEAT_HARVESTER=1 + FEAT_PW_PORTAL=1
//                build order (all items compiled in).
//   type         informational item class (IT_* family)
//   index        informational tag (WP_* / PW_* / ARM_* / HI_*)
//   respawntime  seconds the bot avoids the spot after pickup (0 → default)
//   mins / maxs  item world bounds (standard Q3 pickup box)
//
// classnames are the goal-AI match key; keep them in sync with bg_itemlist.

#define ARMOR_RESPAWN       25
#define HEALTH_RESPAWN      35
#define WEAPON_RESPAWN      5
#define AMMO_RESPAWN        40
#define POWERUP_RESPAWN     120
#define HOLDABLE_RESPAWN    60

#define ITEM_MINS   { -15, -15, -15 }
#define ITEM_MAXS   { 15, 15, 15 }

//
// ARMOR
//
iteminfo "item_armor_jacket"
{
	name        "Jacket Armor"
	model       "models/powerups/armor/armor_grn.md3"
	modelindex  1
	type        1
	index       1
	respawntime ARMOR_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "item_armor_combat"
{
	name        "Combat Armor"
	model       "models/powerups/armor/armor_yel.md3"
	modelindex  2
	type        1
	index       2
	respawntime ARMOR_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "item_armor_body"
{
	name        "Heavy Armor"
	model       "models/powerups/armor/armor_red.md3"
	modelindex  3
	type        1
	index       3
	respawntime ARMOR_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

//
// HEALTH
//
iteminfo "item_health_5"
{
	name        "5 Health"
	model       "models/powerups/health/small_cross.md3"
	modelindex  4
	type        2
	index       0
	respawntime HEALTH_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "item_health_25"
{
	name        "25 Health"
	model       "models/powerups/health/medium_cross.md3"
	modelindex  5
	type        2
	index       0
	respawntime HEALTH_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "item_health_50"
{
	name        "50 Health"
	model       "models/powerups/health/large_cross.md3"
	modelindex  6
	type        2
	index       0
	respawntime HEALTH_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

//
// WEAPONS
//
iteminfo "weapon_gauntlet"
{
	name        "Gauntlet"
	model       "models/weapons2/gauntlet/gauntlet.md3"
	modelindex  7
	type        4
	index       1
	respawntime WEAPON_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "weapon_shotgun"
{
	name        "Shotgun"
	model       "models/weapons2/shotgun/shotgun.md3"
	modelindex  8
	type        4
	index       2
	respawntime WEAPON_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "weapon_machinegun"
{
	name        "Machinegun"
	model       "models/weapons2/machinegun/machinegun.md3"
	modelindex  9
	type        4
	index       3
	respawntime WEAPON_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "weapon_grenadelauncher"
{
	name        "Grenade Launcher"
	model       "models/weapons2/grenadel/grenadel.md3"
	modelindex  10
	type        4
	index       4
	respawntime WEAPON_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "weapon_rocketlauncher"
{
	name        "Rocket Launcher"
	model       "models/weapons2/rocketl/rocketl.md3"
	modelindex  11
	type        4
	index       5
	respawntime WEAPON_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "weapon_lightning"
{
	name        "Lightning Gun"
	model       "models/weapons2/lightning/lightning.md3"
	modelindex  12
	type        4
	index       6
	respawntime WEAPON_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "weapon_railgun"
{
	name        "Railgun"
	model       "models/weapons2/railgun/railgun.md3"
	modelindex  13
	type        4
	index       7
	respawntime WEAPON_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "weapon_plasmagun"
{
	name        "Plasma Rifle"
	model       "models/weapons2/plasma/plasma.md3"
	modelindex  14
	type        4
	index       8
	respawntime WEAPON_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

//
// AMMO
//
iteminfo "ammo_shells"
{
	name        "Shells"
	model       "models/powerups/ammo/shotgunam.md3"
	modelindex  15
	type        8
	index       2
	respawntime AMMO_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "ammo_bullets"
{
	name        "Bullets"
	model       "models/powerups/ammo/machinegunam.md3"
	modelindex  16
	type        8
	index       3
	respawntime AMMO_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "ammo_grenades"
{
	name        "Grenades"
	model       "models/powerups/ammo/grenadeam.md3"
	modelindex  17
	type        8
	index       4
	respawntime AMMO_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "ammo_cells"
{
	name        "Plasma Cells"
	model       "models/powerups/ammo/plasmaam.md3"
	modelindex  18
	type        8
	index       8
	respawntime AMMO_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "ammo_lightning"
{
	name        "Lightning Cells"
	model       "models/powerups/ammo/lightningam.md3"
	modelindex  19
	type        8
	index       6
	respawntime AMMO_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "ammo_rockets"
{
	name        "Rockets"
	model       "models/powerups/ammo/rocketam.md3"
	modelindex  20
	type        8
	index       5
	respawntime AMMO_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "ammo_slugs"
{
	name        "Slugs"
	model       "models/powerups/ammo/railgunam.md3"
	modelindex  21
	type        8
	index       7
	respawntime AMMO_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

//
// HOLDABLE
//
iteminfo "holdable_teleporter"
{
	name        "Teleporter"
	model       "models/powerups/holdable/teleporter.md3"
	modelindex  22
	type        32
	index       1
	respawntime HOLDABLE_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "holdable_medkit"
{
	name        "Medkit"
	model       "models/powerups/holdable/medkit.md3"
	modelindex  23
	type        32
	index       2
	respawntime HOLDABLE_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

//
// POWERUPS
//
iteminfo "item_quad"
{
	name        "Quad Damage"
	model       "models/powerups/instant/quad.md3"
	modelindex  24
	type        16
	index       1
	respawntime POWERUP_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "item_berserk"
{
	name        "Berserk"
	model       "models/powerups/instant/quad.md3"
	modelindex  25
	type        16
	index       2
	respawntime POWERUP_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "item_enviro"
{
	name        "Battle Suit"
	model       "models/powerups/instant/enviro.md3"
	modelindex  26
	type        16
	index       3
	respawntime POWERUP_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "item_haste"
{
	name        "Haste"
	model       "models/powerups/instant/haste.md3"
	modelindex  27
	type        16
	index       4
	respawntime POWERUP_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "item_invis"
{
	name        "Invisibility"
	model       "models/powerups/instant/invis.md3"
	modelindex  28
	type        16
	index       5
	respawntime POWERUP_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "item_regen"
{
	name        "Regeneration"
	model       "models/powerups/instant/regen.md3"
	modelindex  29
	type        16
	index       6
	respawntime POWERUP_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "item_flight"
{
	name        "Flight"
	model       "models/powerups/instant/flight.md3"
	modelindex  30
	type        16
	index       7
	respawntime POWERUP_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

//
// TEAM (CTF flags) — goal items in team modes
//
iteminfo "team_CTF_redflag"
{
	name        "Red Flag"
	model       "models/flags/r_flag.md3"
	modelindex  31
	type        64
	index       0
	respawntime 0
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "team_CTF_blueflag"
{
	name        "Blue Flag"
	model       "models/flags/b_flag.md3"
	modelindex  32
	type        64
	index       0
	respawntime 0
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

//
// HOLDABLE / TEAM (Team Arena additions)
//
iteminfo "holdable_kamikaze"
{
	name        "Kamikaze"
	model       "models/powerups/kamikazi.md3"
	modelindex  33
	type        32
	index       3
	respawntime HOLDABLE_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "holdable_portal"
{
	name        "Portal"
	model       "models/powerups/holdable/porter.md3"
	modelindex  34
	type        32
	index       4
	respawntime HOLDABLE_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "holdable_deflector"
{
	name        "Deflector"
	model       "models/powerups/holdable/invulnerability.md3"
	modelindex  35
	type        32
	index       5
	respawntime HOLDABLE_RESPAWN
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "team_CTF_neutralflag"
{
	name        "Neutral Flag"
	model       "models/flags/n_flag.md3"
	modelindex  36
	type        64
	index       0
	respawntime 0
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "item_redcube"
{
	name        "Red Cube"
	model       "models/powerups/orb/r_orb.md3"
	modelindex  37
	type        64
	index       0
	respawntime 0
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}

iteminfo "item_bluecube"
{
	name        "Blue Cube"
	model       "models/powerups/orb/b_orb.md3"
	modelindex  38
	type        64
	index       0
	respawntime 0
	mins        ITEM_MINS
	maxs        ITEM_MAXS
}
