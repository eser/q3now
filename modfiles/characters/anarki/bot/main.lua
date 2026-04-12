-- Anarki — cyberpunk skirmisher. Chaotic, unpredictable, health injector.
-- Never stays still. Punishes at close range, ignores long-range discipline.
return {

  traits = {
    attack_skill     = {0.45, 0.90},
    view_factor      = {0.50, 0.95},
    view_maxchange   = {180.00, 320.00},
    reaction_time    = {1.80, 0.05},
    croucher         = 0.00,
    jumper           = 1.00,
    weaponjumping    = {0.60, 1.00},
    grapple_user     = {0.30, 0.60},
    aggression       = {0.70, 1.00},
    selfpreservation = {0.30, 0.15},
    vengefulness     = {0.65, 1.00},
    camper           = 0.00,
    easy_fragger     = {0.60, 0.95},
    alertness        = {0.60, 0.95},
    firethrottle     = {0.60, 1.00},
    walker           = 0.00,
  },

  aim = {
    accuracy                  = {0.20, 0.75},
    accuracy_machinegun       = {0.15, 0.55},
    accuracy_shotgun          = {0.30, 0.80},
    accuracy_grenade_launcher = {0.20, 0.65},
    accuracy_rocket_launcher  = {0.20, 0.70},
    accuracy_lightning_gun    = {0.35, 0.85},
    accuracy_railgun          = {0.10, 0.50},
    accuracy_plasma_rifle     = {0.25, 0.70},
    skill                     = {0.20, 0.75},
    skill_machinegun          = {0.15, 0.50},
    skill_shotgun             = {0.30, 0.80},
    skill_grenade_launcher    = {0.20, 0.65},
    skill_rocket_launcher     = {0.20, 0.70},
    skill_lightning_gun       = {0.35, 0.85},
    skill_railgun             = {0.10, 0.50},
    skill_plasma_rifle        = {0.25, 0.70},
  },

  movement = {
    strafe_jump   = 0.00,
    rocket_jump   = 0.20,
    bunny_hop     = 0.00,
    dodge_on_fire = {0.50, 0.90},
    use_jumppads  = true,
    swim          = true,
  },

  attacks = {
    { "lg1", aim_height = 28 },  -- lightning: primary damage tool
    { "rl1", aim_height = 0  },  -- rocket: chaotic splash, aim at feet
    { "sg1", aim_height = 28 },  -- shotgun: in-your-face opener
    { "sg2", aim_height = 28 },  -- double blast: opening burst
    { "pr1", aim_height = 28 },  -- plasma: spray suppression
    { "gl1", aim_height = 0  },  -- grenades: chaotic lobbing, aim at feet
    { "mg1", aim_height = 28 },
    { "rg1", aim_height = 36 },  -- rail: rarely used, unpredictable
  },

  items = {
    "powerup_quad",
    "health_mega",
    "health_large",
    "powerup_haste",
    "armor_red",
    "weapon_lg",
    "weapon_rl",
    "armor_yellow",
    "health_medium",
    "powerup_invis",
    "weapon_sg",
    "weapon_pr",
    "weapon_rg",
    "ammo_lg",
    "ammo_rl",
    "ammo_sg",
    "ammo_pr",
    "health_small",
    "ammo_rg",
    "weapon_gl",
    "ammo_gl",
    "ammo_mg",
    "powerup_regen",
    "powerup_battlesuit",
  },

  chats = {
    file          = "characters/anarki/bot/chats",
    insult        = 0.70,
    misc          = 0.65,
    startendlevel = 0.60,
    enterexitgame = 0.65,
    kill          = 0.70,
    death         = 0.60,
    enemysuicide  = 0.65,
    hittalking    = 0.25,
    hitnodeath    = 0.60,
    hitnokill     = 0.65,
    random        = 0.70,
    reply         = {0.50, 0.15},
  },
}
