-- Sarge — veteran soldier. Steady aggression, railgun/rocket specialist.
-- High ground control, punishes exposed enemies.
return {

  traits = {
    attack_skill     = {0.45, 0.90},
    view_factor      = {0.50, 0.90},
    view_maxchange   = {120.00, 240.00},
    reaction_time    = {2.00, 0.30},
    croucher         = 0.10,
    jumper           = {0.30, 0.65},
    weaponjumping    = {0.30, 0.65},
    grapple_user     = {0.20, 0.40},
    aggression       = {0.55, 0.80},
    selfpreservation = {0.55, 0.35},
    vengefulness     = 0.55,
    camper           = 0.20,
    easy_fragger     = {0.50, 0.85},
    alertness        = {0.55, 0.90},
    firethrottle     = {0.00, 0.55},
    walker           = 0.05,
  },

  aim = {
    accuracy                  = {0.35, 0.80},
    accuracy_machinegun       = {0.25, 0.60},
    accuracy_shotgun          = {0.30, 0.65},
    accuracy_grenade_launcher = {0.25, 0.65},
    accuracy_rocket_launcher  = {0.35, 0.80},
    accuracy_lightning_gun    = {0.30, 0.70},
    accuracy_railgun          = {0.40, 0.90},
    accuracy_plasma_rifle     = {0.25, 0.60},
    skill                     = {0.35, 0.80},
    skill_machinegun          = {0.25, 0.55},
    skill_shotgun             = {0.25, 0.60},
    skill_grenade_launcher    = {0.30, 0.70},
    skill_rocket_launcher     = {0.35, 0.85},
    skill_lightning_gun       = {0.30, 0.70},
    skill_railgun             = {0.40, 0.90},
    skill_plasma_rifle        = {0.25, 0.60},
  },

  movement = {
    strafe_jump   = 0.30,
    rocket_jump   = 0.45,
    bunny_hop     = 0.40,
    dodge_on_fire = {0.05, 0.20},
    use_jumppads  = true,
    swim          = true,
  },

  attacks = {
    "rg1",   -- rail: veteran's bread and butter
    "rl1",   -- rocket: preferred for room control
    "lg1",   -- lightning: close backup
    "gl1",   -- grenades: area denial
    "sg1",   -- shotgun: last resort close range
    "pr1",   -- plasma: suppression
    "mg1",
  },

  items = {
    "powerup_quad",
    "armor_red",
    "health_mega",
    "weapon_rl",
    "weapon_rg",
    "weapon_lg",
    "armor_yellow",
    "health_large",
    "weapon_gl",
    "powerup_haste",
    "weapon_sg",
    "weapon_pr",
    "health_medium",
    "ammo_rl",
    "ammo_rg",
    "ammo_lg",
    "ammo_gl",
    "ammo_pr",
    "ammo_sg",
    "ammo_mg",
    "health_small",
    "powerup_regen",
    "powerup_battlesuit",
  },

  chats = {
    file          = "characters/sarge/bot/chats",
    insult        = 0.55,
    misc          = 0.40,
    startendlevel = 0.50,
    enterexitgame = 0.50,
    kill          = 0.55,
    death         = 0.35,
    enemysuicide  = 0.40,
    hittalking    = 0.15,
    hitnodeath    = 0.40,
    hitnokill     = 0.45,
    random        = 0.40,
    reply         = {0.35, 0.10},
  },
}
