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
file(READ "${ROOT}/code/cgame/cg_event.c" EVENTS)
file(READ "${ROOT}/code/cgame/cg_local.h" CG_LOCAL)
file(READ "${ROOT}/code/cgame/cg_q1_particles.c" Q1_TRAILS)
file(READ "${ROOT}/code/cgame/wired/cg_wired_particles.c" PARTICLE_CLASSES)
file(READ "${ROOT}/code/client/cl_wired_fx.c" FX_CLIENT)
file(READ "${ROOT}/code/game/g_weapon.c" GAME_WEAPONS)
file(READ "${ROOT}/modfiles/scripts/q3now.shader" Q3NOW_SHADERS)
file(READ "${ROOT}/modfiles/scripts/effects/rocket-trail.lua" ROCKET_PROFILE)
file(READ "${ROOT}/modfiles/scripts/effects/grenade-trail.lua" GRENADE_PROFILE)
file(READ "${ROOT}/modfiles/scripts/effects/grenade-explosion.lua" GRENADE_EXPLOSION_PROFILE)
file(READ "${ROOT}/modfiles/scripts/effects/weapon-water-trail.lua" WATER_PROFILE)
file(READ "${ROOT}/modfiles/scripts/effects/rocket-flight.lua" ROCKET_FLIGHT_PROFILE)
file(READ "${ROOT}/modfiles/scripts/effects/grenade-bounce.lua" GRENADE_BOUNCE_PROFILE)
file(READ "${ROOT}/modfiles/scripts/effects/machinegun-fire.lua" MACHINEGUN_FIRE_PROFILE)
file(READ "${ROOT}/modfiles/scripts/effects/shotgun-fire.lua" SHOTGUN_FIRE_PROFILE)
file(READ "${ROOT}/modfiles/scripts/effects/grenade-fire.lua" GRENADE_FIRE_PROFILE)
file(READ "${ROOT}/modfiles/scripts/effects/rocket-fire.lua" ROCKET_FIRE_PROFILE)
file(READ "${ROOT}/modfiles/scripts/effects/manifest.lua" FX_MANIFEST)

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

# Gameplay owns trajectory and liquid classification; WiredFX owns trail
# composition, rates, budgets and endpoint-inclusive GPU path sampling.
require_text(GAME_WEAPONS
	"VectorCopy( origin, muzzlePoint );"
	"caller-authoritative lag-compensated projectile muzzle origin")
require_text(UTILS
	"VectorSubtract( cent->lerpOrigin, rawNow, visualOffset );"
	"shared rendered-projectile visual offset")
require_text(WEAPONS
	"CG_EvaluateVisualTrajectory( ent, startTime, lastPos );"
	"liquid and crossing segment alignment")
require_text(WEAPONS
	"weaponInfo->missileTrailAnchor = missileMins[0];"
	"model-derived rocket nozzle attachment")
require_text(WEAPONS
	"VectorMA( trailOrigin, wi->missileTrailAnchor, trailDirection, trailOrigin );"
	"projectile trail attachment transform")
require_text(WEAPONS
	"WIRED_FX_PROFILE_GRENADE_TRAIL : WIRED_FX_PROFILE_ROCKET_TRAIL"
	"semantic dry-trail dispatch")
require_text(FX_CLIENT
	"event->pathSpacing > 0.0f ? event->pathSpacing"
	"central authored path spacing")
require_text(FX_CLIENT
	"action->payload.particle.spawnRate * event->timeSpanSeconds"
	"FPS-independent authored rate")
require_text(FX_CLIENT
	"VectorScale( path, 0.5f / (float)count, shift );"
	"central endpoint-inclusive sampling")
require_text(ROCKET_PROFILE "particle=\"rocket_exhaust_core\"" "rocket exhaust recipe")
require_text(ROCKET_PROFILE "spawnRate=50" "rocket smoke cadence")
require_text(ROCKET_PROFILE "spawnRate=25" "rocket ember cadence")
require_text(GRENADE_PROFILE "particle=\"grenade_classic_trail\"" "classic grenade recipe")
require_text(WEAPONS
	"CG_WiredFx_InitEvent( &event, WIRED_FX_PROFILE_GRENADE_EXPLOSION,"
	"medium-independent pre-migration grenade detonation recipe")
forbid_text(WEAPONS
	"underwater ? WIRED_FX_PROFILE_GRENADE_UNDERWATER"
	"medium-specific grenade presentation substitution")
require_text(GRENADE_EXPLOSION_PROFILE
	"offset={0,0,16}"
	"grenade sprite-attached light origin")
require_text(GRENADE_EXPLOSION_PROFILE
	"startTimeJitter=.063,startTimeJitterSteps=64"
	"grenade sprite-attached light shared legacy start-time skew")
require_text(GRENADE_EXPLOSION_PROFILE
	"duration=.001,fadeOut=.3"
	"grenade sprite-attached light lifetime")
forbid_text(GRENADE_EXPLOSION_PROFILE "lodFar="
	"migration-only finite-distance grenade detonation cull")
require_text(GRENADE_EXPLOSION_PROFILE "maxInstances=128"
	"global-runtime-sized grenade detonation admission")
require_text(GRENADE_EXPLOSION_PROFILE
	"particle=\"explosion_hot_shrapnel\",maxInstances=12,maxParticles=12,offset={0,0,2}"
	"pre-migration grenade detonation shrapnel")
require_text(ROCKET_PROFILE "maxInstances=128"
	"global-runtime-sized rocket trail admission")
require_text(GRENADE_PROFILE "maxInstances=1024,maxParticles=1024"
	"unclipped pre-migration grenade trail population")
require_text(WATER_PROFILE "particle=\"weapon_water_bubble_trail\"" "shared liquid recipe")
require_text(WATER_PROFILE "maxInstances=1024,maxParticles=1024"
	"unclipped pre-migration water trail population")
require_text(ROCKET_FLIGHT_PROFILE "type=\"light\"" "authored rocket flight light")
require_text(ROCKET_FLIGHT_PROFILE "maxInstances=128"
	"global-runtime-sized rocket flight admission")
require_text(ROCKET_FLIGHT_PROFILE "looping=1" "entity-bound rocket flight loop")
require_text(FX_MANIFEST
	"{ handle = 20, path = \"scripts/effects/rocket-flight.lua\" }"
	"stable rocket flight profile handle")
require_text(FX_MANIFEST
	"{ handle = 21, path = \"scripts/effects/grenade-bounce.lua\" }"
	"stable grenade bounce profile handle")
require_text(ENTS "WIRED_FX_PROFILE_ROCKET_FLIGHT" "semantic rocket flight occurrence")
require_text(ENTS
	"WIRED_FX_EVENT_HAS_SOURCE_ENTITY | WIRED_FX_EVENT_HAS_VELOCITY"
	"rocket flight sound identity and Doppler payload")
require_text(ENTS
	"if ( !wiredFxOwnsFlight && weapon->missileDlight )"
	"non-WiredFX missile light fallback boundary")
require_text(ENTS
	"if ( !wiredFxOwnsFlight && weapon->missileSound )"
	"non-WiredFX missile sound fallback boundary")
require_text(FX_CLIENT "S_AddLoopingSound( event->sourceEntityNum"
	"looping sound lowering")
require_text(WEAPONS "CG_WiredFx_WeaponMuzzlePresent( cent, weaponNum, flash.origin,"
	"exact oriented tag_flash WiredFX muzzle presentation")
require_text(WEAPONS "cent->wiredFxMuzzleTime == cent->muzzleFlashTime"
	"idempotent one-shot muzzle occurrence across view/world passes")
require_text(WEAPONS "if ( localView ) conditionMask |= WIRED_FX_CONDITION_LOCAL_VIEW;"
	"local view muzzle presentation classification")
require_text(WEAPONS "WIRED_FX_PROFILE_SHOTGUN_SMOKE_WIDE"
	"shotgun smoke attached to exact tag_flash presentation")
require_text(WEAPONS "cent->wiredFxMuzzleSecondary = secondary;"
	"secondary fire presentation ownership")
require_text(FX_CLIENT "WIRED_FX_ACTION_VIEW_DEPTH_HACK"
	"view-attached muzzle sprite weapon-depth lowering")
forbid_text(WEAPONS "CG_WiredFx_ShotgunSmoke("
	"server-event shotgun smoke attachment")
forbid_text(WEAPONS "VectorMA( es->pos.trBase, 32.0f, direction, origin );"
	"arbitrary shotgun smoke muzzle push")
require_text(FX_CLIENT
	"S_StartSound( NULL, event->sourceEntityNum,"
	"declarative entity-bound one-shot sound lowering")
require_text(FX_CLIENT
	"S_StartSound( origin, ENTITYNUM_WORLD,"
	"declarative world-positioned one-shot sound lowering")
require_text(MACHINEGUN_FIRE_PROFILE "id=\"muzzle-light\"" "machinegun authored muzzle light")
require_text(SHOTGUN_FIRE_PROFILE "id=\"muzzle-light\"" "shotgun authored muzzle light")
require_text(GRENADE_FIRE_PROFILE "id=\"muzzle-light\"" "grenade launcher authored muzzle light")
require_text(ROCKET_FIRE_PROFILE "id=\"muzzle-light\"" "rocket launcher authored muzzle light")
require_text(EVENTS "WIRED_FX_PROFILE_GRENADE_BOUNCE"
	"semantic grenade bounce occurrence")
require_text(EVENTS "WIRED_FX_CONDITION_VARIANT_0 << variant"
	"deterministic grenade bounce sound variant")
require_text(GRENADE_BOUNCE_PROFILE "sound=\"sound/weapons/grenade/hgrenb1a.opus\""
	"authored grenade bounce sound variant zero")
require_text(GRENADE_BOUNCE_PROFILE "sound=\"sound/weapons/grenade/hgrenb2a.opus\""
	"authored grenade bounce sound variant one")
require_text(GRENADE_BOUNCE_PROFILE "sourceBound=1"
	"entity-bound grenade bounce spatialization")
require_text(GRENADE_BOUNCE_PROFILE "maxInstances=128"
	"global-runtime-sized grenade bounce admission")
forbid_text(EVENTS "hgrenb1aSound" "direct grenade bounce sound choreography")
forbid_text(EVENTS "hgrenb2aSound" "direct grenade bounce sound choreography")
forbid_text(CG_MAIN "hgrenb1aSound" "obsolete grenade bounce sound registration")
forbid_text(CG_MAIN "hgrenb2aSound" "obsolete grenade bounce sound registration")
forbid_text(CG_LOCAL "hgrenb1aSound" "obsolete grenade bounce media handle")
forbid_text(CG_LOCAL "hgrenb2aSound" "obsolete grenade bounce media handle")
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
	"cls.scatterMagnitude   = 3.0f;"
	"preserved classic grenade scatter")
require_text(PARTICLE_CLASSES
	"cls.lifetimeMean       = 0.90f;"
	"competitive grenade trail persistence")
require_text(PARTICLE_CLASSES
	"const float grey = 0.18f + 0.03f * (float)i;"
	"competitive grenade trail contrast")
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
forbid_text(WEAPONS
	"CG_ExplosionParticles"
	"CPU sprite-and-per-particle-light impact burst")
forbid_text(WEAPONS "CG_EmitProjectileTrailLayer" "cgame particle choreography")
forbid_text(WEAPONS "cgs.media.rocketExhaustClass, coreCount" "direct rocket emission")
forbid_text(WEAPONS "cgs.media.grenadeTrailClass, count" "direct grenade emission")
require_text(Q1_TRAILS "case PROJ_SPIKE:" "Q1 spike missile dispatch")
require_text(Q1_TRAILS "case PROJ_LASER:" "Q1 laser missile dispatch")
require_text(Q1_TRAILS "case PROJ_LAVABALL:" "Q1 lavaball missile dispatch")

message(STATUS "cgame projectile trail WiredFX policy: PASS")
