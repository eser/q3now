-- Crash — combat veteran. Precision, patience, survival above all.
-- Waits for the right shot, retreats to heal, never overcommits.
return {

  traits = {
    attack_skill     = {0.40, 0.90},
    view_factor      = {0.55, 0.90},
    view_maxchange   = {100.00, 200.00},
    reaction_time    = {2.50, 0.20},
    croucher         = 0.25,
    jumper           = {0.25, 0.60},
    weaponjumping    = {0.20, 0.55},
    grapple_user     = {0.20, 0.45},
    aggression       = {0.35, 0.65},
    selfpreservation = {0.80, 0.65},
    vengefulness     = {0.40, 0.60},
    camper           = {0.40, 0.65},
    easy_fragger     = {0.45, 0.80},
    alertness        = {0.60, 0.95},
    firethrottle     = {0.00, 0.50},
    walker           = 0.10,
  },

  aim = {
    accuracy                  = {0.45, 0.90},
    accuracy_machinegun       = {0.35, 0.75},
    accuracy_shotgun          = {0.35, 0.75},
    accuracy_grenade_launcher = {0.30, 0.70},
    accuracy_rocket_launcher  = {0.35, 0.80},
    accuracy_lightning_gun    = {0.35, 0.80},
    accuracy_railgun          = {0.50, 0.95},
    accuracy_plasma_rifle     = {0.30, 0.70},
    skill                     = {0.45, 0.90},
    skill_machinegun          = {0.30, 0.70},
    skill_shotgun             = {0.30, 0.70},
    skill_grenade_launcher    = {0.30, 0.70},
    skill_rocket_launcher     = {0.35, 0.80},
    skill_lightning_gun       = {0.35, 0.80},
    skill_railgun             = {0.50, 0.95},
    skill_plasma_rifle        = {0.30, 0.70},
  },

  movement = {
    strafe_jump   = 0.40,
    rocket_jump   = 0.60,
    bunny_hop     = 0.45,
    dodge_on_fire = {0.20, 0.55},
    use_jumppads  = true,
    swim          = true,
  },

  attacks = {
    "rg1",   -- rail: the precision veteran's weapon
    "lg1",   -- lightning: controlled close DPS
    "rl1",   -- rocket: deliberate placement
    "sg1",   -- shotgun: close-quarters punish
    "gl1",   -- grenades: patient area control
    "pr1",   -- plasma: defensive suppression
    "mg1",
  },

  items = {
    "powerup_quad",
    "armor_red",
    "health_mega",
    "weapon_rg",
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
    "ammo_pr",
    "ammo_sg",
    "ammo_mg",
    "health_small",
    "powerup_haste",
    "powerup_invis",
  },

  chats = {
    file          = "characters/crash/bot/chats",
    insult        = 0.30,
    misc          = 0.40,
    startendlevel = 0.45,
    enterexitgame = 0.40,
    kill          = 0.45,
    death         = 0.30,
    enemysuicide  = 0.35,
    hittalking    = 0.10,
    hitnodeath    = 0.35,
    hitnokill     = 0.35,
    random        = 0.35,
    reply         = {0.35, 0.10},
  },
}
