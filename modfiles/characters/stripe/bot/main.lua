-- Stripe — cautious southern fighter. Low aggression, high self-preservation.
-- Great rocket jumper; avoids direct confrontation, repositions constantly.
return {

  traits = {
    attack_skill     = {0.25, 0.65},
    view_factor      = {0.25, 0.55},
    view_maxchange   = {80.00, 160.00},
    reaction_time    = {3.50, 2.00},
    croucher         = 0.05,
    jumper           = 1.00,
    weaponjumping    = 1.00,
    grapple_user     = 0.40,
    aggression       = {0.15, 0.40},
    selfpreservation = {0.70, 0.55},
    vengefulness     = {0.30, 0.50},
    camper           = 0.45,
    easy_fragger     = {0.40, 0.65},
    alertness        = {0.40, 0.75},
    firethrottle     = {0.00, 0.35},
    walker           = 0.00,
  },

  aim = {
    accuracy                  = {0.25, 0.60},
    accuracy_machinegun       = {0.20, 0.50},
    accuracy_shotgun          = {0.20, 0.50},
    accuracy_grenade_launcher = {0.20, 0.55},
    accuracy_rocket_launcher  = {0.30, 0.65},
    accuracy_lightning_gun    = {0.20, 0.50},
    accuracy_railgun          = {0.25, 0.60},
    accuracy_plasma_rifle     = {0.20, 0.50},
    skill                     = {0.25, 0.60},
    skill_machinegun          = {0.20, 0.50},
    skill_shotgun             = {0.20, 0.50},
    skill_grenade_launcher    = {0.20, 0.55},
    skill_rocket_launcher     = {0.30, 0.70},
    skill_lightning_gun       = {0.20, 0.50},
    skill_railgun             = {0.25, 0.60},
    skill_plasma_rifle        = {0.20, 0.50},
  },

  movement = {
    strafe_jump   = 0.05,
    rocket_jump   = 0.10,
    bunny_hop     = 0.05,
    dodge_on_fire = {0.40, 0.80},
    use_jumppads  = true,
    swim          = true,
  },

  attacks = {
    { "rl1", aim_height = 0  },  -- rocket: jump tool + main damage, aim at feet
    { "rl2", aim_height = 0  },  -- mortar: long-range harassment, aim at feet
    { "rg1", aim_height = 36 },  -- rail: careful potshot
    { "lg1", aim_height = 28 },  -- lightning: reluctant close range
    { "gl1", aim_height = 0  },  -- grenades: parting gifts while retreating, aim at feet
    { "sg1", aim_height = 28 },  -- shotgun: close panic shot
    { "pr1", aim_height = 28 },  -- plasma: suppression while fleeing
    { "mg1", aim_height = 28 },
  },

  items = {
    "powerup_quad",
    "weapon_rl",
    "armor_red",
    "health_mega",
    "armor_yellow",
    "weapon_rg",
    "weapon_lg",
    "health_large",
    "ammo_rl",
    "powerup_haste",
    "weapon_gl",
    "weapon_sg",
    "weapon_pr",
    "health_medium",
    "ammo_rg",
    "ammo_lg",
    "ammo_gl",
    "ammo_pr",
    "ammo_sg",
    "ammo_mg",
    "health_small",
    "powerup_regen",
    "powerup_battlesuit",
    "powerup_invis",
  },

  chats = {
    file          = "characters/stripe/bot/chats",
    insult        = 0.25,
    misc          = 0.45,
    startendlevel = 0.45,
    enterexitgame = 0.40,
    kill          = 0.45,
    death         = 0.50,
    enemysuicide  = 0.35,
    hittalking    = 0.10,
    hitnodeath    = 0.35,
    hitnokill     = 0.35,
    random        = 0.45,
    reply         = {0.45, 0.10},
  },
}
