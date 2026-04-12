-- Tao — suicidal berserker. Zero self-preservation. Pure destruction.
-- Gauntlet first, always closing distance, ignores items entirely.
return {

  traits = {
    attack_skill     = {0.50, 1.00},
    view_factor      = {0.25, 1.00},
    view_maxchange   = {180.00, 280.00},
    reaction_time    = {2.50, 0.00},
    croucher         = 0.00,
    jumper           = {0.50, 1.00},
    weaponjumping    = {0.50, 1.00},
    grapple_user     = {0.50, 0.00},
    aggression       = {0.50, 1.00},
    selfpreservation = {0.50, 0.00},
    vengefulness     = {0.50, 1.00},
    camper           = 0.00,
    easy_fragger     = {0.50, 1.00},
    alertness        = {0.50, 1.00},
    firethrottle     = {0.00, 1.00},
    walker           = 0.00,
  },

  aim = {
    accuracy                  = {0.35, 0.85},
    accuracy_machinegun       = {0.20, 0.55},
    accuracy_shotgun          = {0.35, 0.80},
    accuracy_grenade_launcher = {0.20, 0.70},
    accuracy_rocket_launcher  = {0.25, 0.85},
    accuracy_lightning_gun    = {0.40, 0.90},
    accuracy_railgun          = {0.15, 0.55},
    accuracy_plasma_rifle     = {0.25, 0.70},
    skill                     = {0.35, 0.85},
    skill_machinegun          = {0.20, 0.50},
    skill_shotgun             = {0.30, 0.75},
    skill_grenade_launcher    = {0.20, 0.70},
    skill_rocket_launcher     = {0.25, 0.85},
    skill_lightning_gun       = {0.40, 0.90},
    skill_railgun             = {0.15, 0.55},
    skill_plasma_rifle        = {0.25, 0.70},
  },

  movement = {
    strafe_jump   = 0.00,
    rocket_jump   = 0.15,
    bunny_hop     = 0.00,
    dodge_on_fire = {0.00, 0.10},
    use_jumppads  = true,
    swim          = true,
  },

  attacks = {
    { "g2",  aim_height = 28 },  -- gauntlet lunge: suicidal opener at high skill
    { "g1",  aim_height = 28 },  -- gauntlet primary: dramatic statement
    { "lg1", aim_height = 28 },  -- lightning: close-range DPS
    { "rl1", aim_height = 0  },  -- rocket: self-splash accepted, aim at feet
    { "sg1", aim_height = 28 },  -- shotgun: point blank
    { "sg2", aim_height = 28 },  -- double blast: close charge
    { "pr1", aim_height = 28 },  -- plasma: suppression
    { "gl1", aim_height = 0  },  -- grenades: trailing explosions, aim at feet
    { "mg1", aim_height = 28 },
    { "rg1", aim_height = 36 },  -- rail: last resort
  },

  items = {
    "powerup_quad",
    "powerup_haste",
    "weapon_lg",
    "weapon_rl",
    "armor_red",
    "weapon_sg",
    "health_mega",
    "armor_yellow",
    "health_large",
    "weapon_pr",
    "weapon_gl",
    "ammo_lg",
    "ammo_rl",
    "ammo_sg",
    "ammo_pr",
    "ammo_gl",
    "health_medium",
    "weapon_rg",
    "ammo_rg",
    "ammo_mg",
    "health_small",
    "powerup_invis",
    "powerup_regen",
    "powerup_battlesuit",
  },

  chats = {
    file          = "characters/tao/bot/chats",
    insult        = 0.375,
    misc          = 0.10,
    startendlevel = 0.15,
    enterexitgame = 0.15,
    kill          = 0.20,
    death         = 0.08,
    enemysuicide  = 0.12,
    hittalking    = 0.05,
    hitnodeath    = 0.10,
    hitnokill     = 0.10,
    random        = 0.08,
    reply         = {0.10, 0.05},
  },
}
