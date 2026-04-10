-- characters/_base/bot/main.lua
-- Default bot profile — moderate all-rounder.
-- Character files return a table with the same sections; missing sections fall
-- back to these defaults. The engine (sv_lua.c) merges per-section.
--
-- Trait/aim values:    number = constant | {low, high} = lerp by skill (0..1)
-- Movement thresholds: number = minimum skill to enable | true/false = always
-- attacks:             ordered list — first attack whose weapon is carried wins
-- items:               ordered list — bot prioritizes picking items in this order
-- chats.file:          CHARACTERISTIC_CHAT_FILE passthrough (CHAT_NAME comes from nicknames[1] in main.lua)

return {

  -- ── Traits ─────────────────────────────────────────────────────────────────
  traits = {
    attack_skill     = {0.40, 0.80},
    view_factor      = {0.35, 0.75},
    view_maxchange   = {120.00, 240.00},
    reaction_time    = {2.50, 0.20},
    croucher         = 0.10,
    jumper           = {0.40, 0.80},
    weaponjumping    = {0.20, 0.60},
    grapple_user     = {0.20, 0.50},
    aggression       = {0.40, 0.65},
    selfpreservation = {0.60, 0.40},
    vengefulness     = {0.35, 0.60},
    camper           = 0.25,
    easy_fragger     = {0.40, 0.70},
    alertness        = {0.45, 0.80},
    firethrottle     = {0.00, 0.60},
    walker           = 0.05,
  },

  -- ── Aim ────────────────────────────────────────────────────────────────────
  aim = {
    accuracy                  = {0.30, 0.75},
    accuracy_machinegun       = {0.20, 0.55},
    accuracy_shotgun          = {0.25, 0.65},
    accuracy_grenade_launcher = {0.20, 0.60},
    accuracy_rocket_launcher  = {0.25, 0.65},
    accuracy_lightning_gun    = {0.20, 0.60},
    accuracy_railgun          = {0.25, 0.65},
    accuracy_plasma_rifle     = {0.20, 0.55},
    skill                     = {0.30, 0.75},
    skill_machinegun          = {0.20, 0.55},
    skill_shotgun             = {0.20, 0.60},
    skill_grenade_launcher    = {0.20, 0.60},
    skill_rocket_launcher     = {0.25, 0.65},
    skill_lightning_gun       = {0.20, 0.60},
    skill_railgun             = {0.25, 0.65},
    skill_plasma_rifle        = {0.20, 0.55},
  },

  -- ── Movement ───────────────────────────────────────────────────────────────
  -- boolean  → always on/off
  -- number   → enabled when skill >= value
  -- {lo, hi} → numeric lerp
  movement = {
    strafe_jump   = 0.25,          -- enabled at skill 0.25+
    rocket_jump   = 0.65,          -- enabled at skill 0.65+
    bunny_hop     = 0.30,          -- enabled at skill 0.30+
    dodge_on_fire = {0.15, 0.60},
    use_jumppads  = true,
    swim          = true,
  },

  -- ── Attack priority ────────────────────────────────────────────────────────
  -- Ordered list of attack shortnames.  Bot walks the list and fires the first
  -- attack whose parent weapon is currently held (and has ammo).
  attacks = {
    "rg1",  -- railgun: precision finisher at range
    "lg1",  -- lightning: close-range DPS
    "rl1",  -- rocket: medium-range splash
    "gl1",  -- grenades: area denial
    "sg1",  -- shotgun: backup
    "pr1",  -- plasma: suppression
    "mg1",  -- machinegun: always available
  },

  -- ── Item priority ──────────────────────────────────────────────────────────
  -- Ordered list of item type strings.  Bot scores each visible/reachable item
  -- and pursues the highest-scoring one proportional to distance.
  items = {
    "powerup_quad",
    "powerup_haste",
    "powerup_invis",
    "armor_red",
    "health_mega",
    "weapon_rl",
    "weapon_lg",
    "weapon_rg",
    "armor_yellow",
    "health_large",
    "weapon_gl",
    "weapon_sg",
    "weapon_pr",
    "powerup_regen",
    "powerup_battlesuit",
    "health_medium",
    "ammo_rl",
    "ammo_lg",
    "ammo_rg",
    "ammo_gl",
    "ammo_pr",
    "ammo_sg",
    "ammo_mg",
    "health_small",
  },

  -- ── Chats ──────────────────────────────────────────────────────────────────
  chats = {
    file          = "characters/_base/bot/chats",
    insult        = 0.25,
    misc          = 0.40,
    startendlevel = 0.45,
    enterexitgame = 0.45,
    kill          = 0.45,
    death         = 0.35,
    enemysuicide  = 0.35,
    hittalking    = 0.10,
    hitnodeath    = 0.35,
    hitnokill     = 0.35,
    random        = 0.35,
    reply         = {0.35, 0.10},
  },
}
