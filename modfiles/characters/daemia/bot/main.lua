-- Daemia — bounty hunter. Always in the air, balanced kit, rocket-jump specialist.
-- Spanish flair: dramatic, talkative, high mobility.
return {

  traits = {
    attack_skill     = {0.35, 0.90},
    view_factor      = {0.45, 0.90},
    view_maxchange   = {150.00, 260.00},
    reaction_time    = {2.20, 0.15},
    croucher         = 0.00,
    jumper           = 1.00,
    weaponjumping    = {0.50, 1.00},
    grapple_user     = 0.30,
    aggression       = {0.50, 0.80},
    selfpreservation = {0.55, 0.40},
    vengefulness     = {0.50, 0.75},
    camper           = 0.10,
    easy_fragger     = {0.50, 0.85},
    alertness        = {0.50, 0.85},
    firethrottle     = {0.00, 0.50},
    walker           = 0.00,
  },

  aim = {
    accuracy                  = {0.40, 0.85},
    accuracy_machinegun       = {0.20, 0.55},
    accuracy_shotgun          = {0.25, 0.65},
    accuracy_grenade_launcher = {0.30, 0.70},
    accuracy_rocket_launcher  = {0.40, 0.85},
    accuracy_lightning_gun    = {0.35, 0.75},
    accuracy_railgun          = {0.35, 0.80},
    accuracy_plasma_rifle     = {0.30, 0.65},
    skill                     = {0.40, 0.85},
    skill_machinegun          = {0.20, 0.50},
    skill_shotgun             = {0.25, 0.60},
    skill_grenade_launcher    = {0.30, 0.70},
    skill_rocket_launcher     = {0.40, 0.90},
    skill_lightning_gun       = {0.35, 0.75},
    skill_railgun             = {0.35, 0.80},
    skill_plasma_rifle        = {0.30, 0.65},
  },

  movement = {
    strafe_jump   = 0.10,
    rocket_jump   = 0.30,
    bunny_hop     = 0.10,
    dodge_on_fire = {0.30, 0.80},
    use_jumppads  = true,
    swim          = true,
  },

  attacks = {
    { "rl1", aim_height = 0  },  -- rocket: jump fuel and main damage tool, aim at feet
    { "rg1", aim_height = 36 },  -- rail: precision finisher
    { "lg1", aim_height = 28 },  -- lightning: close pursuit
    { "gl1", aim_height = 0  },  -- grenades: traps mid-air, aim at feet
    { "sg1", aim_height = 28 },  -- shotgun: landing shot
    { "pr1", aim_height = 28 },  -- plasma: suppression
    { "mg1", aim_height = 28 },
  },

  items = {
    "powerup_quad",
    "weapon_rl",
    "armor_red",
    "health_mega",
    "weapon_rg",
    "weapon_lg",
    "armor_yellow",
    "health_large",
    "powerup_haste",
    "ammo_rl",
    "weapon_gl",
    "weapon_sg",
    "weapon_pr",
    "ammo_rg",
    "health_medium",
    "ammo_lg",
    "ammo_gl",
    "ammo_pr",
    "ammo_sg",
    "ammo_mg",
    "health_small",
    "powerup_invis",
    "powerup_regen",
    "powerup_battlesuit",
  },

  chats = {
    file          = "characters/daemia/bot/chats",
    insult        = 0.65,
    misc          = 0.55,
    startendlevel = 0.60,
    enterexitgame = 0.55,
    kill          = 0.60,
    death         = 0.50,
    enemysuicide  = 0.55,
    hittalking    = 0.20,
    hitnodeath    = 0.50,
    hitnokill     = 0.55,
    random        = 0.55,
    reply         = {0.40, 0.10},
  },
}
