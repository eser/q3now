# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT required")
endif()

file(READ "${ROOT}/code/cgame/cg_weapons.c" WEAPONS)
file(READ "${ROOT}/code/cgame/cg_event.c" EVENTS)
file(READ "${ROOT}/code/cgame/cg_consolecmds.c" CONSOLE_COMMANDS)
file(READ "${ROOT}/code/cgame/wired/cg_wired_particles.c" PARTICLE_CLASSES)
file(READ "${ROOT}/code/qcommon/wired/render/primitives.h" PRIMITIVES)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/particle.vert" PARTICLE_VERTEX)
file(READ "${ROOT}/code/game/weapons/g_machinegun.c" MACHINEGUN)
file(READ "${ROOT}/code/game/bg_public.h" BG_PUBLIC)
file(READ "${ROOT}/modfiles/scripts/effects/machinegun-impact.lua" MG_PROFILE)
file(READ "${ROOT}/modfiles/scripts/effects/shotgun-impact.lua" SG_PROFILE)
file(READ "${ROOT}/modfiles/scripts/effects/machinegun-impact-reduced.lua" MG_REDUCED_PROFILE)
file(READ "${ROOT}/modfiles/scripts/effects/shotgun-impact-reduced.lua" SG_REDUCED_PROFILE)
file(READ "${ROOT}/modfiles/scripts/effects/machinegun-tracer.lua" MG_TRACER_PROFILE)

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
require_text(PRIMITIVES "PRIM_FLAG_PARTICLE_MOTION_TRAIL"
	"backend-neutral particle motion-trail contract")
require_text(PARTICLE_CLASSES
	"PRIM_FLAG_ADDITIVE | PRIM_FLAG_PARTICLE_MOTION_TRAIL"
	"Quake 4 impact spark motion-trail adoption")
require_text(PARTICLE_VERTEX "PARTICLE_MOTION_TRAIL_SECONDS = 0.10"
	"Quake 4 recent-motion history window")
require_text(PARTICLE_VERTEX "tail = head - p.vel * trailTime"
	"GPU-owned animated trail history")
require_text(PARTICLE_CLASSES "CG_RegisterParticleClass( \"hitscan_impact_flash\", &cls )"
	"Quake 4 short additive impact-flash layer")
require_text(PARTICLE_CLASSES "CG_RegisterParticleClass( \"shotgun_impact_flash\", &cls )"
	"Quake 4 eight-unit shotgun impact-flash layer")
require_text(PARTICLE_CLASSES "CG_RegisterParticleClass( \"hitscan_surface_chips\", &cls )"
	"bounded GPU surface-chip recipe")
require_text(PARTICLE_CLASSES "CG_RegisterParticleClass( \"hitscan_dust_puff\", &cls )"
	"bounded GPU dust recipe")

require_text(WEAPONS "CG_WiredFx_InitEvent( &event, profile, impact->origin, impact->normal );"
	"single semantic impact occurrence")
require_text(WEAPONS "CG_EmitHitscanImpact( &impact );"
	"test seam reusing the production semantic occurrence")
require_text(CONSOLE_COMMANDS "{ \"wiredFxTestHitscan\", CG_WiredFxTestHitscan_f }"
	"deterministic hitscan visual-test command")
require_text(CONSOLE_COMMANDS "{ \"wiredFxTestShotgun\", CG_WiredFxTestShotgun_f }"
	"deterministic shotgun visual-test command")
require_text(WEAPONS "WIRED_FX_PROFILE_SHOTGUN_IMPACT_REDUCED"
	"shotgun reduced-budget profile")
require_text(WEAPONS "WIRED_FX_PROFILE_MACHINEGUN_IMPACT_REDUCED"
	"machinegun reduced-budget profile")
forbid_text(WEAPONS "MAX_SHOTGUN_IMPACT_CLUSTERS" "shotgun impact clustering")
forbid_text(WEAPONS "CG_AddShotgunImpactCluster" "shotgun spatial accumulator")
forbid_text(WEAPONS "CG_FlushShotgunImpactBatch" "shotgun cluster flush")
require_text(WEAPONS "incomingDot < -0.02f && incomingDot > -0.62f"
	"grazing-angle cosmetic ricochet gate")
require_text(WEAPONS "WIRED_FX_CONDITION_DRY_SECONDARY"
	"legacy dry-only secondary impact layers")
require_text(MG_PROFILE "particle=\"hitscan_surface_chips\""
	"Quake 4 concrete chunk recipe")
require_text(MG_PROFILE "default-moving-sparks"
	"separate moving-spark layer")
require_text(MG_PROFILE "default-line-sparks"
	"separate short line-spark layer")
require_text(MG_PROFILE "default-side-streaks"
	"separate surface-streak layer")
require_text(MG_PROFILE "delay={.1,.1}"
	"delayed impact smoke layer")
require_text(MG_PROFILE "startCondition=17408"
	"authored ricochet response")
require_text(MG_PROFILE "startCondition=16385"
	"default-material dry secondary response")
require_text(SG_PROFILE "size=2.5"
	"per-pellet mark scale")
require_text(SG_PROFILE "default-moving-sparks"
	"separate shotgun moving-spark layer")
require_text(SG_PROFILE "particle=\"shotgun_impact_flash\""
	"shotgun-specific eight-unit flash silhouette")
require_text(SG_PROFILE "default-line-sparks"
	"separate shotgun line-spark layer")
require_text(SG_PROFILE "default-side-streaks"
	"separate shotgun surface-streak layer")
require_text(SG_PROFILE "delay={.1,.1}"
	"delayed shotgun smoke layer")
foreach(PROFILE MG_PROFILE SG_PROFILE MG_REDUCED_PROFILE SG_REDUCED_PROFILE MG_TRACER_PROFILE)
	forbid_text(${PROFILE} "lodFar=" "migration-only finite-distance effect cull")
	require_text(${PROFILE} "maxInstances=128" "global-runtime-sized occurrence admission")
endforeach()
forbid_text(WEAPONS "CG_HitscanImpactRecipe" "cgame recipe table")
forbid_text(WEAPONS "CG_EmitHitscanParticleClass" "direct particle choreography")
forbid_text(WEAPONS "CG_EmitHitscanStreaks" "direct streak choreography")
forbid_text(WEAPONS "CG_ImpactMark( cgs.media.bulletMarkShader" "direct bullet mark")

forbid_text(WEAPONS "CG_MissileHitWall( PROJ_SHOTGUN"
	"per-pellet legacy model explosion")
forbid_text(WEAPONS "CG_MissileHitWall( PROJ_MACHINEGUN"
	"legacy machinegun model explosion")
forbid_text(WEAPONS "case PROJ_SHOTGUN:"
	"legacy shotgun bulletFlashModel switch branch")
forbid_text(WEAPONS "case PROJ_MACHINEGUN:"
	"legacy machinegun bulletFlashModel switch branch")

message(STATUS "cgame material-aware WiredFX impact policy: PASS")
