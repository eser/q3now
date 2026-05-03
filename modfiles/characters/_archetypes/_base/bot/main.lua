-- characters/_base/bot/main.lua
-- Default bot profile — moderate all-rounder.
-- Character files return a table with the same sections; missing sections fall
-- back to these defaults. The engine (sv_lua.c) merges per-section.
--
-- Trait/aim values:    number = constant | {low, high} = lerp by skill (0..1)
-- Movement thresholds: number = minimum skill to enable | true/false = always
-- attacks:             ordered list — first attack whose weapon is carried wins
--                      string entry: { shortname }  →  default aim_height = 28
--                      table entry:  { "shortname", aim_height = N }  where N is
--                        units above entity origin (0=feet/splash, 28=center mass, 36=upper body)
-- items:               ordered list — bot prioritizes picking items in this order
-- chats:                CHARACTERISTIC_CHAT_FILE resolved by VFS presence of bot/chats.lua
--                        (CHAT_NAME comes from nicknames[1] in main.lua)
--
-- Values aligned to Q3 Quake3AI baseline (visor_c.c / default_c.c) converted
-- to {low, high} tuples where Q3 skill-1 → low, skill-5 → high.

return {

  -- ── Traits ─────────────────────────────────────────────────────────────────
  traits = {
    attack_skill     = {0.50, 0.80},
    view_factor      = {0.70, 1.00},
    view_maxchange   = {120, 240},
    reaction_time    = {2.50, 0.80},
    grapple_user     = {0.20, 0.50},
    aggression       = {0.40, 0.80},
    selfpreservation = {0.60, 0.45},
    vengefulness     = {0.35, 0.70},
    camper           = 0.30,
    easy_fragger     = {0.10, 0.40},
    alertness        = {0.40, 0.80},
    firethrottle     = {0.30, 0.90},
    walker           = 0.05,
  },

  -- ── Aim ────────────────────────────────────────────────────────────────────
  aim = {
    accuracy                  = {0.30, 0.75},
    accuracy_machinegun_pri       = {0.20, 0.55},
    accuracy_shotgun_pri          = {0.25, 0.65},
    accuracy_grenade_launcher_pri = {0.20, 0.60},
    accuracy_rocket_launcher_pri  = {0.25, 0.65},
    accuracy_lightning_gun_pri    = {0.20, 0.60},
    accuracy_railgun_pri          = {0.25, 0.65},
    accuracy_plasma_rifle_pri     = {0.20, 0.55},
    skill                     = {0.30, 0.75},
    skill_machinegun          = {0.20, 0.55},
    skill_shotgun             = {0.20, 0.60},
    skill_grenade_launcher    = {0.20, 0.60},
    skill_rocket_launcher     = {0.25, 0.65},
    skill_lightning_gun       = {0.20, 0.60},
    skill_railgun             = {0.25, 0.65},
    skill_plasma_rifle        = {0.20, 0.55},
    weapon_bias_mg            = 1.00,
    weapon_bias_sg            = 1.00,
    weapon_bias_gl            = 1.00,
    weapon_bias_rl            = 1.00,
    weapon_bias_lg            = 1.00,
    weapon_bias_rg            = 1.00,
    weapon_bias_pg            = 1.00,
  },

  -- ── Movement ───────────────────────────────────────────────────────────────
  -- boolean  → always on/off
  -- number   → enabled when skill >= value
  -- {lo, hi} → numeric lerp
  movement = {
    strafe_jump      = 0.25,          -- enabled at skill 0.25+
    weapon_jumping   = {0.20, 0.65}, -- 0.20 at low, 0.65 at high (continuous)
    jumper           = 0.30,          -- enabled at skill 0.30+
    dodging          = {0.15, 0.60},
    use_jumppads     = true,
    swim             = true,
    navigation_skill = {0.40, 0.80},
  },

  -- ── Attack priority ────────────────────────────────────────────────────────
  -- Ordered list of attack shortnames.  Bot walks the list and fires the first
  -- attack whose parent weapon is currently held (and has ammo).
  attacks = {
    { "rg1", aim_height = 36 },  -- railgun: upper body for clean hit detection
    { "lg1", aim_height = 28 },  -- lightning: center mass hitscan
    { "rl1", aim_height = 0  },  -- rocket: aim at feet for splash
    { "gl1", aim_height = 0  },  -- grenades: aim at feet for bounce/splash
    { "sg1", aim_height = 28 },  -- shotgun: center mass hitscan
    { "pr1", aim_height = 28 },  -- plasma: center mass
    { "mg1", aim_height = 28 },  -- machinegun: center mass
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
    -- Core events — keys match WiredBots_Chat event names
    -- insult/misc: Q3-path sub-rate only; WiredBots tone is handled in Lua
    insult         = 0.25,
    misc           = 0.40,
    kill           = 0.45,
    death          = 0.35,
    random         = 0.35,
    reply          = {0.35, 0.10},  -- {low_skill_rate, high_skill_rate}
    message        = 0.35,
    enemy_suicide  = 0.35,
    hit_talking    = 0.10,
    hit_nodeath    = 0.35,
    hit_nokill     = 0.35,
    level_start    = 0.45,
    level_end      = 0.45,
    level_end_eliminated = 0.60,
    game_enter     = 0.45,
    game_exit      = 0.45,
    -- Team broadcasts (teamplay only)
    team_need_health         = 0.65,
    team_need_weapon         = 0.50,
    team_cover_me            = 0.55,
    team_follow_me           = 0.60,
    team_enemy_base_attack   = 0.55,
    team_defending_base      = 0.50,
    team_got_flag_need_support = 0.75,
    -- Powerup events
    powerup_quad          = 0.70,
    powerup_haste         = 0.50,
    powerup_invis         = 0.50,
    powerup_regen         = 0.40,
    powerup_battlesuit    = 0.40,
    powerup_enemy_quad    = 0.55,
    powerup_enemy_any     = 0.30,
    -- Kill streaks
    kill_double          = 0.45,
    kill_streak_5        = 0.75,
    kill_streak_10       = 0.85,
    kill_rampage         = 0.90,
    -- Score milestones
    score_first_place    = 0.70,
    score_falling_back   = 0.50,
    score_last_place     = 0.35,
    score_frag_milestone = 0.60,
    -- CTF events
    ctf_got_flag      = 0.70,
    ctf_enemy_got_flag = 0.40,
    ctf_returning_flag = 0.65,
    ctf_capture       = 0.85,
    ctf_flag_dropped  = 0.45,
    ctf_attack        = 0.30,
    ctf_defend        = 0.30,
  },
}
