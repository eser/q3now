# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT required")
endif()

file(READ "${ROOT}/code/cgame/cg_weapons.c" WEAPONS)
file(READ "${ROOT}/code/cgame/cg_event.c" EVENTS)
file(READ "${ROOT}/code/cgame/wired/cg_wired_particles.c" PARTICLE_CLASSES)
file(READ "${ROOT}/code/game/weapons/g_machinegun.c" MACHINEGUN)
file(READ "${ROOT}/code/game/bg_public.h" BG_PUBLIC)

function(require_text source needle label)
	string(FIND "${${source}}" "${needle}" position)
	if(position EQUAL -1)
		message(FATAL_ERROR "Hitscan impact policy lost ${label}: ${needle}")
	endif()
endfunction()

function(forbid_text source needle label)
	string(FIND "${${source}}" "${needle}" position)
	if(NOT position EQUAL -1)
		message(FATAL_ERROR "Hitscan impact policy found retired ${label}: ${needle}")
	endif()
endfunction()

require_text(BG_PUBLIC "BG_HitscanImpactMaterialForSurfaceFlags" "shared surface classifier")
require_text(BG_PUBLIC "SURF_METALSTEPS" "metal response")
require_text(BG_PUBLIC "SURF_DUST" "dust response")
require_text(MACHINEGUN
	"tent->s.generic1 = BG_HitscanImpactMaterialForSurfaceFlags( tr.surfaceFlags );"
	"authoritative machinegun material event payload")
require_text(EVENTS "(hitscanImpactMaterial_t)es->generic1"
	"machinegun material event consumption")

require_text(PARTICLE_CLASSES "CG_RegisterParticleClass( \"hitscan_metal_sparks\", &cls )"
	"bounded GPU metal spark recipe")
require_text(PARTICLE_CLASSES "CG_RegisterParticleClass( \"hitscan_impact_flash\", &cls )"
	"Quake 4 short additive impact-flash layer")
require_text(PARTICLE_CLASSES "CG_RegisterParticleClass( \"hitscan_surface_chips\", &cls )"
	"bounded GPU surface-chip recipe")
require_text(PARTICLE_CLASSES "CG_RegisterParticleClass( \"hitscan_dust_puff\", &cls )"
	"bounded GPU dust recipe")

require_text(WEAPONS "cgHitscanImpactDesc_t" "shared hitscan impact descriptor")
require_text(WEAPONS "CG_EmitHitscanImpact( const cgHitscanImpactDesc_t *impact )"
	"single material-aware composition point")
require_text(WEAPONS "CG_HitscanImpactRecipe" "Quake 4-authored recipe selection")
require_text(WEAPONS "cg_hitscanImpactDetail.integer <= 0"
	"deterministic Quake 4 MP reduced-budget tier")
require_text(WEAPONS "impact.emitSecondary = qtrue;"
	"per-pellet authored secondary composition")
require_text(WEAPONS "CG_EmitHitscanStreaks( impact, &recipe );"
	"directional additive impact streak layer")
forbid_text(WEAPONS "MAX_SHOTGUN_IMPACT_CLUSTERS" "shotgun impact clustering")
forbid_text(WEAPONS "CG_AddShotgunImpactCluster" "shotgun spatial accumulator")
forbid_text(WEAPONS "CG_FlushShotgunImpactBatch" "shotgun cluster flush")
require_text(WEAPONS "incomingDot >= -0.02f || incomingDot <= -0.62f"
	"grazing-angle cosmetic ricochet gate")
require_text(WEAPONS "beam.duration       = 0.070f;"
	"short bounded ricochet streak")
require_text(WEAPONS "recipe.flashCount  = 1;"
	"Quake 4 MP flash budget")
require_text(WEAPONS "recipe.chipCount   = 9;"
	"Quake 4 machinegun concrete chunk midpoint")
require_text(WEAPONS "CG_PointContents( impact->origin, 0 ) & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA )"
	"liquid secondary-effect suppression")

forbid_text(WEAPONS "CG_MissileHitWall( PROJ_SHOTGUN"
	"per-pellet legacy model explosion")
forbid_text(WEAPONS "CG_MissileHitWall( PROJ_MACHINEGUN"
	"legacy machinegun model explosion")
forbid_text(WEAPONS "case PROJ_SHOTGUN:"
	"legacy shotgun bulletFlashModel switch branch")
forbid_text(WEAPONS "case PROJ_MACHINEGUN:"
	"legacy machinegun bulletFlashModel switch branch")

message(STATUS "cgame material-aware hitscan impact policy: PASS")
