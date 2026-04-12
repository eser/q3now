-- Grunt — camper and defender. High self-preservation, steady aggression.
-- Holds positions, punishes with railgun, ignores reckless play.
return {

  traits = {
    attack_skill     = {0.40, 0.90},
    view_factor      = {0.60, 0.90},
    view_maxchange   = {80.00, 200.00},
    reaction_time    = {3.00, 0.40},
    croucher         = 0.20,
    jumper           = {0.30, 0.60},
    weaponjumping    = {0.25, 0.55},
    grapple_user     = {0.40, 0.65},
    aggression       = {0.40, 0.65},
    selfpreservation = {0.80, 0.65},
    vengefulness     = {0.45, 0.70},
    camper           = 0.80,
    easy_fragger     = {0.45, 0.80},
    alertness        = {0.50, 0.85},
    firethrottle     = {0.00, 0.45},
    walker           = 0.10,
  },

  aim = {
    accuracy                  = {0.40, 0.85},
    accuracy_machinegun       = {0.30, 0.70},
    accuracy_shotgun          = {0.30, 0.70},
    accuracy_grenade_launcher = {0.25, 0.65},
    accuracy_rocket_launcher  = {0.30, 0.70},
    accuracy_lightning_gun    = {0.25, 0.65},
    accuracy_railgun          = {0.45, 0.90},
    accuracy_plasma_rifle     = {0.25, 0.60},
    skill                     = {0.35, 0.80},
    skill_machinegun          = {0.30, 0.65},
    skill_shotgun             = {0.30, 0.65},
    skill_grenade_launcher    = {0.25, 0.60},
    skill_rocket_launcher     = {0.30, 0.70},
    skill_lightning_gun       = {0.25, 0.60},
    skill_railgun             = {0.45, 0.90},
    skill_plasma_rifle        = {0.25, 0.60},
  },

  movement = {
    strafe_jump   = 0.55,
    rocket_jump   = 0.70,
    bunny_hop     = 0.60,
    dodge_on_fire = {0.05, 0.25},
    use_jumppads  = true,
    swim          = true,
  },

  attacks = {
    { "rg1", aim_height = 36 },  -- rail: defender's sniper shot
    { "rl1", aim_height = 0  },  -- rocket: hold the area, aim at feet
    { "lg1", aim_height = 28 },  -- lightning: punish rushers
    { "sg1", aim_height = 28 },  -- shotgun: doorway defense
    { "gl1", aim_height = 0  },  -- grenades: corridor traps, aim at feet
    { "pr1", aim_height = 28 },  -- plasma: suppression
    { "mg1", aim_height = 28 },
  },

  items = {
    "powerup_quad",
    "armor_red",
    "weapon_rg",
    "health_mega",
    "weapon_rl",
    "weapon_lg",
    "armor_yellow",
    "health_large",
    "powerup_regen",
    "powerup_battlesuit",
    "weapon_gl",
    "weapon_sg",
    "health_medium",
    "ammo_rg",
    "ammo_rl",
    "ammo_lg",
    "ammo_gl",
    "ammo_sg",
    "ammo_pr",
    "ammo_mg",
    "health_small",
    "powerup_haste",
    "powerup_invis",
  },

  chats = {
    file          = "characters/grunt/bot/chats",
    insult        = 0.55,
    misc          = 0.40,
    startendlevel = 0.45,
    enterexitgame = 0.45,
    kill          = 0.50,
    death         = 0.30,
    enemysuicide  = 0.40,
    hittalking    = 0.10,
    hitnodeath    = 0.40,
    hitnokill     = 0.45,
    random        = 0.40,
    reply         = {0.40, 0.10},
  },
}
