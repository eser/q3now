// SPDX-License-Identifier: GPL-3.0-or-later

#include "../code/qcommon/wired/core/scripting/wired_fx_authoring.h"
#include "../code/render/frontend/render_effect_profile.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <lauxlib.h>
#include <lualib.h>

static uint32_t Resolve( wiredFxResourceType_t type, const char *name, void *userData ) {
	uint32_t *calls = (uint32_t *)userData;
	assert( name && *name );
	( *calls )++;
	return 100u + (uint32_t)type;
}

static int PushResult( lua_State *L, const char *source ) {
	if ( luaL_loadstring( L, source ) != 0 ) return 0;
	if ( lua_pcall( L, 0, 1, 0 ) != 0 ) return 0;
	return lua_istable( L, -1 );
}

int main( void ) {
	static const char *const source =
		"return { schemaVersion=1, maxActiveActions=19, maxInstances=128, seed=225,"
		" duration=4, lodNear=0, lodFar=4096, boundsRadius=512, actions={"
		"{type='light',id='core',fire='smoke',material='hot',radius={32,48,64},radiusEnd={96,112,128},intensity=3,lightLifetime=.1,flags={'noShadows'},startCondition={3,1}},"
		"{type='particle',id='smoke',parent='core',particle='smoke',maxInstances=64,maxParticles=64,spawnRate=20,velocityScale=1,flags={'gpuLifecycle'}},"
		"{type='decal',id='mark1',material='burn',size=32,depth=4},"
		"{type='decal2',id='mark2',material='scorch',size=48,depth=8},"
		"{type='model',id='debris',model='chunk',material='charred',origin='trackLocal',rotation='angles',angles={10,20,30}},"
		"{type='sound',id='boom',sound='rocket',channel=2,looping=1,sourceBound=1},"
		"{type='screenShake',id='quake',magnitude=4,controllerScale=.5,maxAngles={1,2,3},maxOffset={4,5,6}},"
		"{type='controllerShake',id='rumble',highMagnitude=1,lowMagnitude=.5,highDuration=.2,lowDuration=.4,amplitudeCurve='pulse'},"
		"{type='wind',id='blast',angle=45,multiplier=2,strength={1,3}},"
		"{type='renderParm',id='parm',parm='heat',value={1,2,3,4}},"
		"{type='envOverride',id='env1',environment='hell',renderParms={{parm='fog',value={.1,.2,.3,.4}}}},"
		"{type='envChange',id='env2',environment='clear'},"
		"{type='flare',id='flash',flare='flash',position={1,2,3},autosprite=1},"
		"{type='radialBlur',id='blur',maxScale=1.5},"
		"{type='ribbon',id='ribbon',ribbon='shockwave'},"
		"{type='fadeParent',id='fade'},"
		"{type='godray',id='rays',material='rays',godrayColor={1,.5,.2,1},colorScale=2,size=64,sourceSize=16},"
		"{type='sprite',id='sprite',material='flash',radius={8,16},velocity={0,0,4},spriteLifetime=.25,randomRotation=1},"
		"{type='beam',id='beam',ribbon='shockwave',width=.25,endWidth=.05,count=3,maxInstances=3,length={10,20},normalScale={.5,1},spread=.7,lifetime=.16,ribbonFadeOut=.112}"
		"}}";
	lua_State *L = luaL_newstate();
	wiredFxAuthoringResolver_t resolver;
	wiredFxProfile_t profile;
	char error[160];
	uint32_t calls = 0u;
	assert( L );
	resolver.resolveResource = Resolve;
	resolver.userData = &calls;
	assert( PushResult( L, source ) );
	assert( WiredFxAuthoring_ReadProfile( &profile, L, -1, &resolver, error, sizeof( error ) ) );
	assert( profile.actionCount == WIRED_FX_ACTION_COUNT );
	assert( profile.actions[0].fireAction == 1u );
	assert( profile.actions[0].payload.light.lifetime == .1f );
	assert( profile.actions[0].payload.light.radiusEnd[2] == 128.0f );
	assert( profile.actions[0].startConditionMask == ( ( (uint64_t)1u << 32 ) | 3u ) );
	assert( profile.actions[1].parentAction == 0u );
	assert( profile.actions[1].payload.particle.maxParticles == 64u );
	assert( profile.actions[4].originType == WIRED_FX_ORIGIN_TRACK_LOCAL );
	assert( profile.actions[4].rotationType == WIRED_FX_ROTATION_EXPLICIT_ANGLES );
	assert( profile.actions[4].rotationDegrees[1] == 20.0f );
	assert( profile.actions[5].payload.sound.looping == 1u );
	assert( profile.actions[5].payload.sound.sourceBound == 1u );
	assert( profile.actions[10].payload.environment.renderParmCount == 1u );
	assert( profile.actions[16].payload.godray.sourceSize == 16u );
	assert( profile.actions[17].payload.sprite.radius[1] == 16.0f );
	assert( profile.actions[17].payload.sprite.lifetime == .25f );
	assert( profile.actions[18].type == WIRED_FX_ACTION_BEAM );
	assert( profile.actions[18].payload.ribbon.count == 3u );
	assert( profile.actions[18].payload.ribbon.lifetime == .16f );
	assert( calls == 17u );
	assert( WiredFx_ValidateProfile( &profile ) == WIRED_FX_VALID );
	lua_pop( L, 1 );

#ifdef WIRED_SOURCE_DIR
	static const char *const partOneProfiles[] = {
		"rocket-trail.lua", "grenade-trail.lua", "grenade-explosion.lua",
		"machinegun-impact.lua",
		"machinegun-impact-reduced.lua", "shotgun-impact.lua",
		"shotgun-impact-reduced.lua", "machinegun-tracer.lua",
		"machinegun-fire.lua", "shotgun-fire.lua", "shotgun-fire-wide.lua",
		"grenade-fire.lua", "rocket-fire.lua", "weapon-water-trail.lua",
		"weapon-water-splash.lua", "grenade-bounce.lua", "rocket-flight.lua",
		"shotgun-smoke.lua", "shotgun-smoke-wide.lua",
		"rocket-layered-explosion.lua", "rocket-layered-detonation.lua",
		"rocket-layered-underwater.lua", "cinematic-lens-flare.lua"
	};
	uint32_t profileIndex;
	char profilePath[512];
	assert( luaL_loadfile( L, WIRED_SOURCE_DIR "/modfiles/scripts/effects/rocket-explosion.lua" ) == 0 );
	assert( lua_pcall( L, 0, 1, 0 ) == 0 );
	assert( WiredFxAuthoring_ReadProfile( &profile, L, -1, &resolver, error, sizeof( error ) ) );
	assert( profile.actionCount == 10u );
	assert( profile.actions[0].type == WIRED_FX_ACTION_PARTICLE );
	assert( profile.actions[1].type == WIRED_FX_ACTION_PARTICLE );
	assert( profile.actions[1].payload.particle.maxParticles == 1u );
	assert( profile.actions[2].payload.particle.maxParticles == 1u );
	assert( profile.actions[3].payload.particle.maxParticles == 24u );
	assert( profile.actions[6].type == WIRED_FX_ACTION_LIGHT );
	assert( profile.actions[6].payload.light.radiusEnd[0] == 300.0f );
	assert( profile.actions[6].payload.light.lifetime == 0.0f );
	assert( profile.actions[8].type == WIRED_FX_ACTION_SCREEN_SHAKE );
	assert( profile.actions[8].payload.screenShake.radius == 600.0f );
	assert( profile.actions[9].type == WIRED_FX_ACTION_DECAL );
	assert( profile.actions[9].payload.decal.size == 64.0f );
	assert( WiredFx_ValidateProfile( &profile ) == WIRED_FX_VALID );
	lua_pop( L, 1 );
	assert( luaL_loadfile( L, WIRED_SOURCE_DIR "/modfiles/scripts/effects/rocket-detonation.lua" ) == 0 );
	assert( lua_pcall( L, 0, 1, 0 ) == 0 );
	assert( WiredFxAuthoring_ReadProfile( &profile, L, -1, &resolver, error, sizeof( error ) ) );
	assert( profile.actionCount == 9u );
	assert( profile.actions[1].type == WIRED_FX_ACTION_PARTICLE );
	assert( profile.actions[1].payload.particle.maxParticles == 1u );
	assert( profile.actions[7].type == WIRED_FX_ACTION_SCREEN_SHAKE );
	assert( profile.actions[7].payload.screenShake.radius == 600.0f );
	assert( profile.actions[8].type == WIRED_FX_ACTION_RADIAL_BLUR );
	assert( WiredFx_ValidateProfile( &profile ) == WIRED_FX_VALID );
	lua_pop( L, 1 );
	assert( luaL_loadfile( L, WIRED_SOURCE_DIR "/modfiles/scripts/effects/rocket-underwater.lua" ) == 0 );
	assert( lua_pcall( L, 0, 1, 0 ) == 0 );
	assert( WiredFxAuthoring_ReadProfile( &profile, L, -1, &resolver, error, sizeof( error ) ) );
	assert( profile.actionCount == 7u );
	assert( profile.lodFar == 6144.0f );
	assert( profile.actions[2].type == WIRED_FX_ACTION_PARTICLE );
	assert( profile.actions[6].type == WIRED_FX_ACTION_SCREEN_SHAKE );
	assert( profile.actions[6].payload.screenShake.radius == 600.0f );
	assert( WiredFx_ValidateProfile( &profile ) == WIRED_FX_VALID );
	lua_pop( L, 1 );
	assert( luaL_loadfile( L, WIRED_SOURCE_DIR "/modfiles/scripts/effects/world-earthquake.lua" ) == 0 );
	assert( lua_pcall( L, 0, 1, 0 ) == 0 );
	assert( WiredFxAuthoring_ReadProfile( &profile, L, -1, &resolver, error, sizeof( error ) ) );
	assert( profile.actionCount == 1u );
	assert( profile.maxInstances == 64u );
	assert( profile.actions[0].type == WIRED_FX_ACTION_SCREEN_SHAKE );
	assert( profile.actions[0].payload.screenShake.maxAngles[PITCH] == 0.2f );
	assert( profile.actions[0].payload.screenShake.maxOffset[0] == 0.2f );
	assert( WiredFx_ValidateProfile( &profile ) == WIRED_FX_VALID );
	lua_pop( L, 1 );
	for ( profileIndex = 0u; profileIndex < ARRAY_LEN( partOneProfiles ); ++profileIndex ) {
		snprintf( profilePath, sizeof( profilePath ), "%s/modfiles/scripts/effects/%s",
			WIRED_SOURCE_DIR, partOneProfiles[profileIndex] );
		assert( luaL_loadfile( L, profilePath ) == 0 );
		assert( lua_pcall( L, 0, 1, 0 ) == 0 );
		assert( WiredFxAuthoring_ReadProfile( &profile, L, -1, &resolver, error, sizeof( error ) ) );
		assert( WiredFx_ValidateProfile( &profile ) == WIRED_FX_VALID );
		if ( !strcmp( partOneProfiles[profileIndex], "grenade-bounce.lua" ) ) {
			assert( profile.actionCount == 2u );
			assert( profile.actions[0].type == WIRED_FX_ACTION_SOUND );
			assert( profile.actions[0].startConditionMask == WIRED_FX_CONDITION_VARIANT_0 );
			assert( profile.actions[0].payload.sound.sourceBound == 1u );
			assert( profile.actions[1].type == WIRED_FX_ACTION_SOUND );
			assert( profile.actions[1].startConditionMask == WIRED_FX_CONDITION_VARIANT_1 );
			assert( profile.actions[1].payload.sound.sourceBound == 1u );
		}
		if ( !strcmp( partOneProfiles[profileIndex], "rocket-flight.lua" ) ) {
			assert( profile.actionCount == 2u );
			assert( profile.maxInstances == 128u );
			assert( profile.actions[1].type == WIRED_FX_ACTION_SOUND );
			assert( profile.actions[1].payload.sound.looping == 1u );
		}
		if ( !strcmp( partOneProfiles[profileIndex], "grenade-explosion.lua" ) ) {
			assert( profile.actionCount == 6u );
			assert( profile.actions[0].type == WIRED_FX_ACTION_SPRITE );
			assert( profile.actions[0].payload.sprite.startTimeJitter == 0.063f );
			assert( profile.actions[0].payload.sprite.startTimeJitterSteps == 64u );
			assert( profile.actions[1].type == WIRED_FX_ACTION_PARTICLE );
			assert( profile.actions[1].payload.particle.maxParticles == 12u );
			assert( profile.actions[1].offset[2] == 2.0f );
			assert( profile.actions[2].type == WIRED_FX_ACTION_LIGHT );
			assert( profile.actions[2].payload.light.radius[0] == 300.0f );
			assert( profile.actions[2].payload.light.startTimeJitter == 0.063f );
			assert( profile.actions[2].payload.light.startTimeJitterSteps == 64u );
			assert( profile.actions[2].offset[2] == 16.0f );
			assert( profile.actions[4].type == WIRED_FX_ACTION_SCREEN_SHAKE );
			assert( profile.actions[4].payload.screenShake.radius == 600.0f );
			assert( profile.actions[5].type == WIRED_FX_ACTION_DECAL );
			assert( profile.actions[5].payload.decal.size == 64.0f );
			assert( profile.actions[5].duration == 10.0f );
		}
		if ( !strcmp( partOneProfiles[profileIndex], "rocket-layered-explosion.lua" ) ) {
			assert( profile.actionCount == 11u );
			assert( profile.actions[0].type == WIRED_FX_ACTION_SPRITE );
			assert( profile.actions[0].payload.sprite.lifetime == 0.72f );
			assert( profile.actions[1].type == WIRED_FX_ACTION_PARTICLE );
			assert( profile.actions[1].payload.particle.maxParticles == 40u );
			assert( profile.actions[2].payload.particle.maxParticles == 4u );
			assert( profile.actions[4].type == WIRED_FX_ACTION_BEAM );
			assert( profile.actions[5].payload.particle.maxParticles == 15u );
			assert( profile.actions[7].type == WIRED_FX_ACTION_LIGHT );
			assert( profile.actions[7].payload.light.radius[0] == 96.0f );
			assert( profile.actions[7].payload.light.radiusEnd[0] == 360.0f );
			assert( profile.actions[7].payload.light.intensity == 1.25f );
			assert( profile.actions[7].payload.light.lifetime == 1.0f );
			assert( profile.actions[7].offset[2] == 12.0f );
			assert( profile.actions[9].type == WIRED_FX_ACTION_SCREEN_SHAKE );
			assert( profile.actions[10].type == WIRED_FX_ACTION_DECAL );
		}
		if ( !strcmp( partOneProfiles[profileIndex], "rocket-layered-detonation.lua" ) ) {
			assert( profile.actionCount == 9u );
			assert( profile.actions[1].payload.particle.maxParticles == 40u );
			assert( profile.actions[2].payload.particle.maxParticles == 4u );
			assert( profile.actions[3].payload.particle.maxParticles == 15u );
			assert( profile.actions[5].type == WIRED_FX_ACTION_LIGHT );
			assert( profile.actions[5].payload.light.radius[0] == 44.0f );
			assert( profile.actions[5].payload.light.radiusEnd[0] == 380.0f );
			assert( profile.actions[5].payload.light.lifetime == 1.0f );
			assert( profile.actions[8].type == WIRED_FX_ACTION_RADIAL_BLUR );
		}
		if ( !strcmp( partOneProfiles[profileIndex], "rocket-layered-underwater.lua" ) ) {
			assert( profile.actionCount == 7u );
			assert( profile.actions[0].type == WIRED_FX_ACTION_SPRITE );
			assert( profile.actions[1].payload.particle.maxParticles == 40u );
			assert( profile.actions[6].type == WIRED_FX_ACTION_RADIAL_BLUR );
		}
		if ( !strcmp( partOneProfiles[profileIndex], "machinegun-tracer.lua" ) ) {
			assert( profile.actionCount == 2u );
			assert( profile.actions[0].type == WIRED_FX_ACTION_BEAM );
			assert( profile.actions[0].payload.ribbon.width == 1.0f );
			assert( profile.actions[0].payload.ribbon.endWidth == 1.0f );
			assert( profile.actions[0].payload.ribbon.lifetime == 0.0f );
			assert( profile.actions[1].type == WIRED_FX_ACTION_SOUND );
			assert( profile.actions[1].originType == WIRED_FX_ORIGIN_MIDPOINT );
		}
		if ( !strcmp( partOneProfiles[profileIndex], "machinegun-fire.lua" ) ||
			 !strcmp( partOneProfiles[profileIndex], "shotgun-fire.lua" ) ||
			 !strcmp( partOneProfiles[profileIndex], "shotgun-fire-wide.lua" ) ||
			 !strcmp( partOneProfiles[profileIndex], "grenade-fire.lua" ) ||
			 !strcmp( partOneProfiles[profileIndex], "rocket-fire.lua" ) ) {
			uint32_t actionIndex;
			for ( actionIndex = 0u; actionIndex < profile.actionCount; ++actionIndex ) {
				if ( profile.actions[actionIndex].type == WIRED_FX_ACTION_SOUND ) {
					assert( profile.actions[actionIndex].payload.sound.sourceBound == 1u );
					assert( ( profile.actions[actionIndex].startConditionMask &
						WIRED_FX_CONDITION_FIRE_ONESHOT ) != 0u );
				}
				if ( profile.actions[actionIndex].type != WIRED_FX_ACTION_LIGHT )
					continue;
				assert( profile.actions[actionIndex].payload.light.radius[0] == 300.0f );
				assert( profile.actions[actionIndex].duration == 0.001f );
				assert( profile.actions[actionIndex].payload.light.radiusJitter == 31.0f );
				assert( profile.actions[actionIndex].payload.light.radiusJitterSteps == 32u );
				assert( profile.actions[actionIndex].payload.light.lifetime == 0.0f );
				assert( profile.actions[actionIndex].fadeOutTime == 0.0f );
				assert( ( profile.actions[actionIndex].flags & WIRED_FX_ACTION_LOOP ) != 0u );
				assert( profile.actions[actionIndex].startConditionMask ==
					WIRED_FX_CONDITION_MUZZLE_PRESENT );
			}
		}
		if ( !strcmp( partOneProfiles[profileIndex], "shotgun-fire-wide.lua" ) ) {
			assert( profile.actionCount == 4u );
			assert( profile.actions[0].type == WIRED_FX_ACTION_SOUND );
			assert( profile.actions[1].type == WIRED_FX_ACTION_SOUND );
			assert( profile.actions[0].payload.sound.sound ==
				profile.actions[1].payload.sound.sound );
			assert( profile.actions[2].type == WIRED_FX_ACTION_SCREEN_SHAKE );
			assert( profile.actions[2].payload.screenShake.mode == 1u );
			assert( profile.actions[2].duration == 0.2f );
			assert( profile.actions[2].payload.screenShake.decayExponent == 2.0f );
			assert( profile.actions[2].payload.screenShake.maxAngles[PITCH] == 3.0f );
			assert( profile.actions[2].payload.screenShake.maxAngles[ROLL] == 1.5f );
		}
		if ( !strcmp( partOneProfiles[profileIndex], "machinegun-impact.lua" ) ) {
			assert( profile.actionCount == 21u );
			assert( profile.actions[0].payload.particle.maxParticles == 1u );
			assert( profile.actions[1].payload.particle.maxParticles == 3u );
			assert( profile.actions[2].type == WIRED_FX_ACTION_BEAM );
			assert( profile.actions[2].payload.ribbon.count == 3u );
			assert( profile.actions[2].payload.ribbon.width == 0.15f );
			assert( profile.actions[2].payload.ribbon.endWidth == 0.025f );
			assert( profile.actions[2].offset[2] == 1.5f );
			assert( profile.actions[3].payload.ribbon.count == 6u );
			assert( profile.actions[4].payload.particle.maxParticles == 1u );
			assert( profile.actions[4].delay[0] == 0.1f );
			assert( profile.actions[6].payload.particle.maxParticles == 4u );
			assert( profile.actions[7].payload.ribbon.count == 3u );
			assert( profile.actions[8].payload.ribbon.count == 6u );
			assert( profile.actions[11].payload.particle.maxParticles == 2u );
			assert( profile.actions[12].payload.ribbon.count == 1u );
			assert( profile.actions[13].payload.ribbon.count == 4u );
			assert( profile.actions[14].payload.particle.maxParticles == 2u );
			assert( profile.actions[15].payload.particle.maxParticles == 4u );
			assert( profile.actions[16].type == WIRED_FX_ACTION_DECAL );
			assert( profile.actions[16].payload.decal.size == 5.0f );
			assert( profile.actions[16].duration == 10.0f );
			assert( profile.actions[17].type == WIRED_FX_ACTION_BEAM );
			assert( profile.actions[17].payload.ribbon.width == 0.38f );
			assert( profile.actions[17].payload.ribbon.endWidth == 0.06f );
			assert( profile.actions[17].offset[2] == 1.75f );
			assert( profile.actions[18].type == WIRED_FX_ACTION_SOUND );
			assert( profile.actions[19].type == WIRED_FX_ACTION_SOUND );
			assert( profile.actions[20].type == WIRED_FX_ACTION_SOUND );
		}
		if ( !strcmp( partOneProfiles[profileIndex], "shotgun-impact.lua" ) ) {
			assert( profile.actionCount == 21u );
			assert( profile.actions[0].payload.particle.maxParticles == 1u );
			assert( profile.actions[1].payload.particle.maxParticles == 2u );
			assert( profile.actions[2].payload.ribbon.count == 2u );
			assert( profile.actions[2].payload.ribbon.length[1] == 6.0f );
			assert( profile.actions[2].payload.ribbon.lifetime == 0.38f );
			assert( profile.actions[3].payload.ribbon.count == 3u );
			assert( profile.actions[4].payload.particle.maxParticles == 1u );
			assert( profile.actions[4].delay[0] == 0.1f );
			assert( profile.actions[6].payload.particle.maxParticles == 3u );
			assert( profile.actions[8].payload.ribbon.count == 4u );
			assert( profile.actions[11].payload.particle.maxParticles == 1u );
			assert( profile.actions[13].payload.ribbon.count == 2u );
			assert( profile.actions[14].payload.particle.maxParticles == 2u );
			assert( profile.actions[15].payload.particle.maxParticles == 3u );
			assert( profile.actions[16].payload.decal.size == 2.5f );
		}
		if ( !strcmp( partOneProfiles[profileIndex], "machinegun-impact-reduced.lua" ) ) {
			assert( profile.actionCount == 10u );
			assert( profile.actions[0].payload.particle.maxParticles == 1u );
			assert( profile.actions[1].payload.particle.maxParticles == 2u );
			assert( profile.actions[2].payload.ribbon.count == 2u );
			assert( profile.actions[3].payload.ribbon.count == 4u );
			assert( profile.actions[4].payload.particle.maxParticles == 1u );
			assert( profile.actions[4].delay[0] == 0.1f );
		}
		if ( !strcmp( partOneProfiles[profileIndex], "shotgun-impact-reduced.lua" ) ) {
			assert( profile.actionCount == 10u );
			assert( profile.actions[0].payload.particle.maxParticles == 1u );
			assert( profile.actions[1].payload.particle.maxParticles == 1u );
			assert( profile.actions[2].payload.ribbon.count == 1u );
			assert( profile.actions[3].payload.ribbon.count == 2u );
			assert( profile.actions[4].payload.particle.maxParticles == 1u );
			assert( profile.actions[4].delay[0] == 0.1f );
		}
		if ( strstr( partOneProfiles[profileIndex], "impact" ) != NULL ) {
			uint32_t actionIndex;
			for ( actionIndex = 0u; actionIndex < profile.actionCount; ++actionIndex ) {
				if ( profile.actions[actionIndex].type == WIRED_FX_ACTION_DECAL ) {
					assert( ( profile.actions[actionIndex].startConditionMask &
						WIRED_FX_CONDITION_DRY_SECONDARY ) == 0u );
				} else {
					assert( ( profile.actions[actionIndex].startConditionMask &
						WIRED_FX_CONDITION_DRY_SECONDARY ) != 0u );
				}
			}
		}
		if ( strstr( partOneProfiles[profileIndex], "shotgun-smoke" ) != NULL ) {
			assert( profile.actionCount == 1u );
			assert( profile.actions[0].type == WIRED_FX_ACTION_SPRITE );
			assert( ( profile.actions[0].flags & WIRED_FX_ACTION_VIEW_DEPTH_HACK ) != 0u );
		}
		if ( !strcmp( partOneProfiles[profileIndex], "shotgun-smoke.lua" ) ) {
			assert( profile.actions[0].payload.sprite.radius[0] == 16.0f );
			assert( profile.actions[0].payload.sprite.radius[1] == 16.0f );
			assert( profile.actions[0].payload.sprite.velocity[2] == 8.0f );
			assert( profile.actions[0].color[3] == 0.2f );
			assert( profile.actions[0].fadeOutTime == 0.8f );
		}
		if ( !strcmp( partOneProfiles[profileIndex], "rocket-trail.lua" ) ||
			 !strcmp( partOneProfiles[profileIndex], "grenade-trail.lua" ) ) {
			uint32_t actionIndex;
			for ( actionIndex = 0u; actionIndex < profile.actionCount; ++actionIndex ) {
				if ( profile.actions[actionIndex].type == WIRED_FX_ACTION_PARTICLE )
					assert( profile.actions[actionIndex].payload.particle.pathSampling ==
						WIRED_FX_PATH_SAMPLING_ENDPOINT );
			}
		}
		if ( !strcmp( partOneProfiles[profileIndex], "rocket-trail.lua" ) ) {
			assert( profile.actionCount == 3u );
			assert( profile.actions[0].payload.particle.trailSpacing == 8.0f );
			assert( profile.actions[0].payload.particle.maxParticles == 8u );
			assert( profile.actions[1].payload.particle.spawnRate == 50.0f );
			assert( profile.actions[1].payload.particle.maxParticles == 6u );
			assert( profile.actions[2].payload.particle.spawnRate == 25.0f );
			assert( profile.actions[2].payload.particle.maxParticles == 3u );
		}
		if ( !strcmp( partOneProfiles[profileIndex], "grenade-trail.lua" ) ) {
			assert( profile.actionCount == 1u );
			assert( profile.actions[0].payload.particle.trailSpacing == 4.0f );
			assert( profile.actions[0].payload.particle.spawnRate == 20.0f );
		}
		if ( !strcmp( partOneProfiles[profileIndex], "weapon-water-trail.lua" ) ) {
			assert( profile.actions[0].payload.particle.pathSampling ==
				WIRED_FX_PATH_SAMPLING_RANDOM_SPACING );
		}
		lua_pop( L, 1 );
	}
	assert( luaL_loadfile( L, WIRED_SOURCE_DIR "/modfiles/scripts/effects/manifest.lua" ) == 0 );
	assert( lua_pcall( L, 0, 1, 0 ) == 0 );
	assert( lua_istable( L, -1 ) );
	assert( lua_objlen( L, -1 ) == 29u );
	lua_rawgeti( L, -1, 29 );
	assert( lua_istable( L, -1 ) );
	lua_getfield( L, -1, "handle" );
	assert( (uint32_t)lua_tointeger( L, -1 ) == WIRED_FX_PROFILE_CINEMATIC_LENS_FLARE );
	lua_pop( L, 1 );
	lua_getfield( L, -1, "path" );
	assert( !strcmp( lua_tostring( L, -1 ), "scripts/effects/cinematic-lens-flare.lua" ) );
	lua_pop( L, 2 );
	lua_pop( L, 1 );
#endif

	assert( PushResult( L, "return {maxActiveActions=1,maxInstances=1,actions={{type='particle',id='bad',fire='missing',particle=1,maxParticles=1}}}" ) );
	assert( !WiredFxAuthoring_ReadProfile( &profile, L, -1, NULL, error, sizeof( error ) ) );
	assert( strstr( error, "unresolved" ) != NULL );
	assert( profile.schemaVersion == 0u );
	lua_pop( L, 1 );

	assert( PushResult( L, "return {maxActiveActions=1,maxInstances=1,actions={{type='earthquake',id='bad'}}}" ) );
	assert( !WiredFxAuthoring_ReadProfile( &profile, L, -1, NULL, error, sizeof( error ) ) );
	assert( strstr( error, "unknown" ) != NULL );
	lua_close( L );
	return 0;
}
