-- Major — female all-rounder. Methodical, patient, camps effectively at high skill.
-- Transitions from cautious to oppressive aggressor as skill rises.
return {

  traits = {
    attack_skill     = {0.40, 0.95},
    view_factor      = {0.45, 0.90},
    view_maxchange   = {100.00, 220.00},
    reaction_time    = {2.80, 0.25},
    croucher         = 0.15,
    jumper           = {0.35, 0.70},
    weaponjumping    = {0.30, 0.70},
    grapple_user     = {0.25, 0.50},
    aggression       = {0.35, 0.80},
    selfpreservation = {0.70, 0.45},
    vengefulness     = {0.40, 0.65},
    camper           = {0.30, 0.75},
    easy_fragger     = {0.45, 0.85},
    alertness        = {0.50, 0.90},
    firethrottle     = {0.00, 0.60},
    walker           = 0.05,
  },

  aim = {
    accuracy                  = {0.35, 0.90},
    accuracy_machinegun       = {0.30, 0.70},
    accuracy_shotgun          = {0.30, 0.70},
    accuracy_grenade_launcher = {0.30, 0.75},
    accuracy_rocket_launcher  = {0.35, 0.85},
    accuracy_lightning_gun    = {0.30, 0.75},
    accuracy_railgun          = {0.40, 0.90},
    accuracy_plasma_rifle     = {0.30, 0.70},
    skill                     = {0.35, 0.90},
    skill_machinegun          = {0.25, 0.65},
    skill_shotgun             = {0.25, 0.65},
    skill_grenade_launcher    = {0.30, 0.75},
    skill_rocket_launcher     = {0.35, 0.85},
    skill_lightning_gun       = {0.30, 0.75},
    skill_railgun             = {0.40, 0.90},
    skill_plasma_rifle        = {0.30, 0.70},
  },

  movement = {
    strafe_jump   = 0.35,
    rocket_jump   = 0.55,
    bunny_hop     = 0.40,
    dodge_on_fire = {0.10, 0.40},
    use_jumppads  = true,
    swim          = true,
  },

  attacks = {
    "rg1",   -- rail: methodical precision
    "rl1",   -- rocket: area control
    "lg1",   -- lightning: close follow-through
    "gl1",   -- grenades: patient area denial
    "sg1",   -- shotgun: close punish
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
    "powerup_haste",
    "weapon_gl",
    "weapon_sg",
    "weapon_pr",
    "powerup_regen",
    "health_medium",
    "ammo_rl",
    "ammo_rg",
    "ammo_lg",
    "ammo_gl",
    "ammo_pr",
    "ammo_sg",
    "ammo_mg",
    "health_small",
    "powerup_battlesuit",
    "powerup_invis",
  },

  chats = {
    file          = "characters/major/bot/chats",
    insult        = 0.40,
    misc          = 0.50,
    startendlevel = 0.55,
    enterexitgame = 0.50,
    kill          = 0.50,
    death         = 0.40,
    enemysuicide  = 0.45,
    hittalking    = 0.15,
    hitnodeath    = 0.45,
    hitnokill     = 0.45,
    random        = 0.50,
    reply         = {0.45, 0.10},
  },
}
