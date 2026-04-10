-- Keel — cold cyborg heavy. Methodical destruction, heavy firepower.
-- High firethrottle, prefers splash and sustained fire over precise shots.
return {

  traits = {
    attack_skill     = {0.45, 0.95},
    view_factor      = {0.40, 0.85},
    view_maxchange   = {80.00, 200.00},
    reaction_time    = {1.50, 0.10},
    croucher         = 0.20,
    jumper           = {0.20, 0.50},
    weaponjumping    = {0.25, 0.65},
    grapple_user     = {0.15, 0.40},
    aggression       = {0.65, 0.90},
    selfpreservation = {0.40, 0.25},
    vengefulness     = 0.65,
    camper           = 0.15,
    easy_fragger     = {0.55, 0.90},
    alertness        = {0.70, 0.95},
    firethrottle     = {0.70, 1.00},
    walker           = 0.20,
  },

  aim = {
    accuracy                  = {0.30, 0.85},
    accuracy_machinegun       = {0.35, 0.80},
    accuracy_shotgun          = {0.30, 0.70},
    accuracy_grenade_launcher = {0.40, 0.90},
    accuracy_rocket_launcher  = {0.40, 0.90},
    accuracy_lightning_gun    = {0.35, 0.85},
    accuracy_railgun          = {0.30, 0.80},
    accuracy_plasma_rifle     = {0.30, 0.75},
    skill                     = {0.30, 0.85},
    skill_machinegun          = {0.30, 0.75},
    skill_shotgun             = {0.25, 0.65},
    skill_grenade_launcher    = {0.40, 0.90},
    skill_rocket_launcher     = {0.40, 0.90},
    skill_lightning_gun       = {0.35, 0.85},
    skill_railgun             = {0.30, 0.80},
    skill_plasma_rifle        = {0.30, 0.75},
  },

  movement = {
    strafe_jump   = 0.40,
    rocket_jump   = 0.55,
    bunny_hop     = 0.45,
    dodge_on_fire = {0.05, 0.30},
    use_jumppads  = true,
    swim          = true,
  },

  attacks = {
    "rl1",   -- rocket: primary destruction tool
    "lg1",   -- lightning: sustained heavy DPS
    "gl1",   -- grenades: area saturation
    "rl2",   -- mortar: long-range harassment
    "rg1",   -- rail: methodical long shots
    "sg1",   -- shotgun: close-range punish
    "pr1",   -- plasma: suppression spray
    "mg1",
  },

  items = {
    "powerup_quad",
    "weapon_rl",
    "weapon_lg",
    "armor_red",
    "health_mega",
    "weapon_gl",
    "armor_yellow",
    "health_large",
    "powerup_haste",
    "weapon_rg",
    "weapon_sg",
    "weapon_pr",
    "ammo_rl",
    "ammo_lg",
    "ammo_gl",
    "health_medium",
    "ammo_rg",
    "ammo_pr",
    "ammo_sg",
    "ammo_mg",
    "health_small",
    "powerup_regen",
    "powerup_battlesuit",
    "powerup_invis",
  },

  chats = {
    file          = "characters/keel/bot/chats",
    insult        = 0.15,
    misc          = 0.10,
    startendlevel = 0.15,
    enterexitgame = 0.15,
    kill          = 0.20,
    death         = 0.08,
    enemysuicide  = 0.12,
    hittalking    = 0.05,
    hitnodeath    = 0.10,
    hitnokill     = 0.12,
    random        = 0.10,
    reply         = 0.12,
  },
}
