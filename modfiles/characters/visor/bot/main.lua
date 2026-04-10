-- Visor — cybernetic duelist. Enhanced optics: railgun specialist.
-- Clinical precision, reads enemy movement, rewards patience.
return {

  traits = {
    attack_skill     = {0.40, 0.90},
    view_factor      = {0.55, 0.95},
    view_maxchange   = {140.00, 280.00},
    reaction_time    = {1.80, 0.10},
    croucher         = 0.10,
    jumper           = {0.35, 0.70},
    weaponjumping    = {0.30, 0.65},
    grapple_user     = {0.20, 0.45},
    aggression       = {0.45, 0.75},
    selfpreservation = {0.60, 0.45},
    vengefulness     = {0.50, 0.75},
    camper           = 0.20,
    easy_fragger     = {0.50, 0.85},
    alertness        = {0.65, 1.00},
    firethrottle     = {0.00, 0.50},
    walker           = 0.05,
  },

  aim = {
    accuracy                  = {0.40, 0.85},
    accuracy_machinegun       = {0.30, 0.70},
    accuracy_shotgun          = {0.30, 0.70},
    accuracy_grenade_launcher = {0.25, 0.65},
    accuracy_rocket_launcher  = {0.35, 0.80},
    accuracy_lightning_gun    = {0.40, 0.85},
    accuracy_railgun          = {0.55, 1.00},
    accuracy_plasma_rifle     = {0.30, 0.70},
    skill                     = {0.40, 0.85},
    skill_machinegun          = {0.25, 0.65},
    skill_shotgun             = {0.25, 0.65},
    skill_grenade_launcher    = {0.25, 0.65},
    skill_rocket_launcher     = {0.35, 0.80},
    skill_lightning_gun       = {0.40, 0.85},
    skill_railgun             = {0.55, 1.00},
    skill_plasma_rifle        = {0.30, 0.70},
  },

  movement = {
    strafe_jump   = 0.30,
    rocket_jump   = 0.55,
    bunny_hop     = 0.35,
    dodge_on_fire = {0.20, 0.60},
    use_jumppads  = true,
    swim          = true,
  },

  attacks = {
    "rg1",   -- rail: the duelist's signature
    "lg1",   -- lightning: dominant close range
    "lg2",   -- chain arc: punish groups
    "rl1",   -- rocket: medium support
    "sg1",   -- shotgun: close desperation
    "pr1",   -- plasma: suppression
    "gl1",   -- grenades: area denial
    "mg1",
  },

  items = {
    "powerup_quad",
    "armor_red",
    "weapon_rg",
    "health_mega",
    "weapon_lg",
    "weapon_rl",
    "armor_yellow",
    "health_large",
    "powerup_haste",
    "powerup_invis",
    "weapon_gl",
    "weapon_sg",
    "weapon_pr",
    "ammo_rg",
    "ammo_lg",
    "ammo_rl",
    "health_medium",
    "ammo_gl",
    "ammo_pr",
    "ammo_sg",
    "ammo_mg",
    "health_small",
    "powerup_regen",
    "powerup_battlesuit",
  },

  chats = {
    file          = "characters/visor/bot/chats",
    insult        = 0.35,
    misc          = 0.30,
    startendlevel = 0.40,
    enterexitgame = 0.35,
    kill          = 0.40,
    death         = 0.20,
    enemysuicide  = 0.30,
    hittalking    = 0.10,
    hitnodeath    = 0.30,
    hitnokill     = 0.35,
    random        = 0.25,
    reply         = {0.30, 0.08},
  },
}
