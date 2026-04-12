-- Taa — silent berserker. Maximum aggression, no conversation.
-- Always in melee range, never retreats, never speaks.
return {

  traits = {
    attack_skill     = {0.50, 1.00},
    view_factor      = {0.25, 1.00},
    view_maxchange   = {180.00, 280.00},
    reaction_time    = {2.50, 0.00},
    croucher         = 0.00,
    jumper           = {0.25, 0.00},
    weaponjumping    = {0.25, 0.00},
    grapple_user     = {0.50, 0.00},
    aggression       = {0.50, 1.00},
    selfpreservation = 0.50,
    vengefulness     = {0.50, 1.00},
    camper           = 0.00,
    easy_fragger     = {0.50, 1.00},
    alertness        = {0.50, 1.00},
    firethrottle     = {0.00, 1.00},
    walker           = 0.00,
  },

  aim = {
    accuracy                  = {0.40, 0.90},
    accuracy_machinegun       = {0.20, 0.55},
    accuracy_shotgun          = {0.35, 0.80},
    accuracy_grenade_launcher = {0.20, 0.65},
    accuracy_rocket_launcher  = {0.25, 0.80},
    accuracy_lightning_gun    = {0.40, 0.90},
    accuracy_railgun          = {0.20, 0.65},
    accuracy_plasma_rifle     = {0.30, 0.75},
    skill                     = {0.40, 0.90},
    skill_machinegun          = {0.20, 0.50},
    skill_shotgun             = {0.30, 0.75},
    skill_grenade_launcher    = {0.20, 0.65},
    skill_rocket_launcher     = {0.25, 0.80},
    skill_lightning_gun       = {0.40, 0.90},
    skill_railgun             = {0.20, 0.65},
    skill_plasma_rifle        = {0.30, 0.75},
  },

  movement = {
    strafe_jump   = 0.00,
    rocket_jump   = 0.45,
    bunny_hop     = 0.00,
    dodge_on_fire = {0.05, 0.30},
    use_jumppads  = true,
    swim          = true,
  },

  attacks = {
    { "lg1", aim_height = 28 },  -- lightning: primary close-range weapon
    { "rl1", aim_height = 0  },  -- rocket: charge in and fire, aim at feet
    { "sg1", aim_height = 28 },  -- shotgun: point-blank destruction
    { "sg2", aim_height = 28 },  -- double blast: opening charge burst
    { "pr1", aim_height = 28 },  -- plasma: suppress then close
    { "gl1", aim_height = 0  },  -- grenades: thrown while rushing, aim at feet
    { "mg1", aim_height = 28 },
    { "rg1", aim_height = 36 },  -- rail: rarely used
  },

  items = {
    "powerup_quad",
    "powerup_haste",
    "weapon_lg",
    "weapon_rl",
    "armor_red",
    "health_mega",
    "weapon_sg",
    "armor_yellow",
    "health_large",
    "powerup_invis",
    "weapon_pr",
    "weapon_gl",
    "ammo_lg",
    "ammo_rl",
    "ammo_sg",
    "health_medium",
    "ammo_pr",
    "ammo_gl",
    "weapon_rg",
    "ammo_rg",
    "ammo_mg",
    "health_small",
    "powerup_regen",
    "powerup_battlesuit",
  },

  chats = {
    file          = "characters/taa/bot/chats",
    insult        = 0.35,
    misc          = 0.10,
    startendlevel = 0.15,
    enterexitgame = 0.10,
    kill          = 0.20,
    death         = 0.05,
    enemysuicide  = 0.10,
    hittalking    = 0.05,
    hitnodeath    = 0.10,
    hitnokill     = 0.10,
    random        = 0.05,
    reply         = {0.10, 0.05},
  },
}
