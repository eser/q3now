# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT is required")
endif()

function(require_text path needle)
  file(READ "${ROOT}/${path}" text)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${path}: missing temporal producer seam: ${needle}")
  endif()
endfunction()

function(require_absent path needle)
  file(READ "${ROOT}/${path}" text)
  string(FIND "${text}" "${needle}" found)
  if(NOT found EQUAL -1)
    message(FATAL_ERROR "${path}: forbidden temporal producer seam: ${needle}")
  endif()
endfunction()

function(require_count path needle expected)
  file(READ "${ROOT}/${path}" text)
  string(REPLACE "${needle}" "" stripped "${text}")
  string(LENGTH "${text}" before)
  string(LENGTH "${stripped}" after)
  string(LENGTH "${needle}" needle_length)
  math(EXPR count "(${before} - ${after}) / ${needle_length}")
  if(NOT count EQUAL expected)
    message(FATAL_ERROR
      "${path}: expected ${expected} exact '${needle}' seams, found ${count}")
  endif()
endfunction()

require_text("code/cgame/cg_snapshot.c" "CG_TemporalIdentityObserve(")
require_text("code/cgame/cg_snapshot.c" "CG_TemporalIdentityMarkAllDiscontinuous();")
require_text("code/cgame/cg_snapshot.c" "SNAPFLAG_SERVERCOUNT")
require_text("code/cgame/cg_servercmds.c" "CG_TemporalIdentityMarkAllDiscontinuous();")
require_absent("code/cgame/cg_snapshot.c" "if ( cg.mapRestart )")
require_text("code/cgame/cg_ents.c" "owner = &cg_entities[entityNumber]")
require_text("code/cgame/cg_ents.c" "ent->renderfx & RF_FORCE_ENT_ALPHA")
require_text("code/cgame/cg_ents.c" "REF_ENTITY_MOTION_ROLE_GENERAL")
require_text("code/cgame/cg_ents.c" "REF_ENTITY_MOTION_ROLE_ITEM_PRIMARY")
require_text("code/cgame/cg_ents.c" "REF_ENTITY_MOTION_ROLE_ITEM_BARREL")
require_text("code/cgame/cg_ents.c" "REF_ENTITY_MOTION_ROLE_MOVER_PRIMARY")
require_text("code/cgame/cg_ents.c" "REF_ENTITY_MOTION_ROLE_MOVER_SECONDARY")
require_text("code/cgame/cg_ents.c" "REF_ENTITY_MOTION_ROLE_GRAPPLE")
require_text("code/cgame/cg_players.c" "REF_ENTITY_MOTION_ROLE_PLAYER_BODY")
require_text("code/cgame/cg_players.c" "REF_ENTITY_MOTION_ROLE_PLAYER_LEGS")
require_text("code/cgame/cg_players.c" "REF_ENTITY_MOTION_ROLE_PLAYER_TORSO")
require_text("code/cgame/cg_players.c" "REF_ENTITY_MOTION_ROLE_PLAYER_HEAD")
require_text("code/cgame/cg_creature.c" "REF_ENTITY_MOTION_ROLE_CREATURE_BODY")

# Pin the live producer surface.  Seven occurrences in cg_ents are the helper
# definition plus GENERAL, ITEM primary/barrel, GRAPPLE and MOVER primary/
# secondary.  The sole player-side occurrence is the visible base draw inside
# CG_AddRefEntityWithPowerups; overlays and first-person weapons stay ordinary.
require_count("code/cgame/cg_ents.c" "CG_AddRefEntityTemporalBase(" 7)
require_count("code/cgame/cg_players.c" "CG_AddRefEntityTemporalBase(" 1)
require_count("code/cgame/cg_players.c" "trap_R_AddRefEntityToScene( ent );" 8)
require_absent("code/cgame/cg_weapons.c" "CG_AddRefEntityTemporalBase(")
require_absent("code/cgame/cg_localents.c" "CG_AddRefEntityTemporalBase(")
require_absent("code/cgame/cg_effects.c" "CG_AddRefEntityTemporalBase(")
require_absent("code/cgame/cg_players.c" "if ( cg.mapRestart )")
require_text("code/cgame/cg_players.c" "CG_AddRefEntityTemporalBase( cent, ent, baseRole );")

# The invisible replacement and all powerup shells must remain ordinary draws.
require_text("code/cgame/cg_players.c" "ent->customShader = cgs.media.invisShader;")
require_text("code/cgame/cg_players.c" "trap_R_AddRefEntityToScene( ent );")
require_text("code/cgame/cg_ents.c" "REF_ENTITY_MOTION_ROLE_NONE")

message(STATUS "cgame temporal identity producer/source policy: PASS")
