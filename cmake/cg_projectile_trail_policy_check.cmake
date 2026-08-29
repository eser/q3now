# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT required")
endif()

file(READ "${ROOT}/code/cgame/cg_weapons.c" WEAPONS)
file(READ "${ROOT}/code/cgame/cg_main.c" CG_MAIN)
file(READ "${ROOT}/code/cgame/cg_utils.c" UTILS)
file(READ "${ROOT}/code/cgame/cg_ents.c" ENTS)
file(READ "${ROOT}/code/cgame/cg_q1_particles.c" Q1_TRAILS)
file(READ "${ROOT}/code/cgame/wired/cg_wired_particles.c" PARTICLE_CLASSES)
file(READ "${ROOT}/code/game/g_weapon.c" GAME_WEAPONS)
file(READ "${ROOT}/modfiles/scripts/q3now.shader" Q3NOW_SHADERS)

function(require_text source needle label)
	string(FIND "${${source}}" "${needle}" position)
	if(position EQUAL -1)
		message(FATAL_ERROR "Projectile trail policy lost ${label}: ${needle}")
	endif()
endfunction()

function(forbid_text source needle label)
	string(FIND "${${source}}" "${needle}" position)
	if(NOT position EQUAL -1)
		message(FATAL_ERROR "Projectile trail policy found retired ${label}: ${needle}")
	endif()
endfunction()

# Projectile prediction/nudging changes cent->lerpOrigin after evaluating the
# server trajectory. The whole historical trail segment must receive that same
# offset so rocket/grenade smoke, bubbles and water crossings stay attached.
require_text(GAME_WEAPONS
	"VectorCopy( origin, muzzlePoint );"
	"caller-authoritative lag-compensated projectile muzzle origin")
require_text(UTILS
	"VectorSubtract( cent->lerpOrigin, rawNow, visualOffset );"
	"shared rendered-projectile visual offset")
require_text(UTILS
	"CG_ProjectileTrailStepForSpacing"
	"speed-bounded projectile trail spacing")
require_text(WEAPONS
	"CG_EvaluateVisualTrajectory( ent, startTime, lastPos );"
	"liquid and crossing segment alignment")
require_text(WEAPONS
	"VectorScale( axis, 0.5f / (float)count, pathShift );"
	"shared endpoint-inclusive projectile path sampling")
require_text(WEAPONS
	"weaponInfo->missileTrailAnchor = missileMins[0];"
	"model-derived rocket nozzle attachment")
require_text(WEAPONS
	"VectorMA( trailOrigin, wi->missileTrailAnchor, trailDirection, trailOrigin );"
	"projectile trail attachment transform")
require_text(WEAPONS
	"VectorAdd( end, pathShift, emitter.end );"
	"rendered-projectile endpoint attachment")
require_text(WEAPONS
	"int count = (int)ceilf( distance / 4.0f );"
	"grenade-only classic point spacing")
require_text(WEAPONS
	"CG_EmitProjectileTrailLayer( cgs.media.grenadeTrailClass, count,"
	"dedicated classic grenade layer")
require_text(WEAPONS
	"int coreCount  = (int)ceilf( distance / 8.0f );"
	"bounded rocket exhaust subdivision")
require_text(WEAPONS
	"int smokeCount = cg.time / 20 - startTime / 20; // 50 Hz"
	"FPS-independent rocket smoke cadence")
require_text(WEAPONS
	"int emberCount = cg.time / 40 - startTime / 40; // 25 Hz"
	"FPS-independent sparse rocket ember cadence")
require_text(WEAPONS
	"if ( coreCount > 8 ) coreCount = 8;"
	"rocket exhaust hitch bound")
require_text(WEAPONS
	"if ( smokeCount > 6 ) smokeCount = 6;"
	"rocket smoke hitch bound")
require_text(WEAPONS
	"if ( emberCount > 3 ) emberCount = 3;"
	"rocket ember hitch bound")
require_text(WEAPONS
	"CG_EmitProjectileTrailLayer( cgs.media.rocketExhaustClass, coreCount,"
	"Q4-style rocket exhaust layer")
require_text(WEAPONS
	"CG_EmitProjectileTrailLayer( cgs.media.rocketSmokeClass, smokeCount,"
	"Q4-style rocket smoke layer")
require_text(WEAPONS
	"CG_EmitProjectileTrailLayer( cgs.media.rocketEmberClass, emberCount,"
	"Q4-style rocket ember layer")
require_text(WEAPONS
	"ent->trailTime = cg.time;"
	"per-visual-frame projectile endpoint cursor")
require_text(WEAPONS
	"VectorCopy( ent->lerpOrigin, origin );"
	"grapple beam rendered endpoint")
require_text(ENTS
	"!CG_Q1_MaybeEmitMissileTrail( cent ) && weapon->missileTrailFunc"
	"pType-owned missile trail dispatch")
require_text(Q1_TRAILS
	"CG_EvaluateVisualTrajectory( cent, t, origin );"
	"Q1 historical visual trajectory samples")
require_text(Q1_TRAILS
	"cent->trailTime = lastBoundary;"
	"Q1 fixed-grid remainder preservation")
require_text(PARTICLE_CLASSES
	"CG_RegisterParticleClass( \"grenade_classic_trail\", &cls )"
	"dedicated classic grenade trail visual recipe")
require_text(PARTICLE_CLASSES
	"cls.shader             = cgs.media.whiteShader;"
	"classic grenade point-particle shader")
require_text(PARTICLE_CLASSES
	"cls.sizeStart        = 0.65f;"
	"bounded classic grenade particle scale")
require_text(PARTICLE_CLASSES
	"cls.gravityScale     = -0.03f;"
	"classic grenade particle up-drift")
require_text(PARTICLE_CLASSES
	"CG_RegisterParticleClass( \"rocket_exhaust_core\", &cls )"
	"dedicated additive rocket exhaust class")
require_text(PARTICLE_CLASSES
	"CG_RegisterParticleClass( \"rocket_smoke_wake\", &cls )"
	"dedicated expanding rocket smoke class")
require_text(PARTICLE_CLASSES
	"CG_RegisterParticleClass( \"rocket_hot_embers\", &cls )"
	"dedicated sparse rocket ember class")
require_text(PARTICLE_CLASSES
	"CG_RegisterParticleClass( \"explosion_hot_shrapnel\", &cls )"
	"shared warm GPU impact-shrapnel class")
require_text(PARTICLE_CLASSES
	"cls.velocityShape      = VEL_CONE;"
	"impact-normal directional shrapnel cone")
require_text(PARTICLE_CLASSES
	"void CG_ResetParticleClassRegistry( void )"
	"restart-safe local particle class registry reset")
require_text(CG_MAIN
	"CG_ResetParticleClassRegistry();"
	"particle class registry reset before media re-registration")
require_text(PARTICLE_CLASSES
	"cls.axialSpeed         = -24.0f;"
	"rocket exhaust rearward drift")
require_text(PARTICLE_CLASSES
	"cls.sizeEnd            = 24.0f;"
	"bounded expanding rocket smoke scale")
require_text(PARTICLE_CLASSES
	"cls.lifetimeMean       = 0.42f;"
	"short-lived rocket micro-embers")
require_text(Q3NOW_SHADERS
	"rocketExhaustGlow"
	"rocket-specific additive soft-core material")
require_text(WEAPONS
	"CG_ExplosionShrapnel( pType, origin, dir );"
	"rocket and grenade impact shrapnel dispatch")
require_text(WEAPONS
	"count = 18;"
	"bounded rocket impact shard count")
require_text(WEAPONS
	"count = 12;"
	"bounded grenade impact shard count")
forbid_text(WEAPONS
	"CG_ExplosionParticles"
	"CPU sprite-and-per-particle-light impact burst")
require_text(Q1_TRAILS "case PROJ_SPIKE:" "Q1 spike missile dispatch")
require_text(Q1_TRAILS "case PROJ_LASER:" "Q1 laser missile dispatch")
require_text(Q1_TRAILS "case PROJ_LAVABALL:" "Q1 lavaball missile dispatch")

message(STATUS "cgame projectile trail visual-origin policy: PASS")
