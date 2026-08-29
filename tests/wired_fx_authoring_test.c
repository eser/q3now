// SPDX-License-Identifier: GPL-3.0-or-later

#include "../code/qcommon/wired/core/scripting/wired_fx_authoring.h"
#include "../code/render/frontend/render_effect_profile.h"

#include <assert.h>
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
		"return { schemaVersion=1, maxActiveActions=17, maxInstances=128, seed=225,"
		" duration=4, lodNear=0, lodFar=4096, boundsRadius=512, actions={"
		"{type='light',id='core',fire='smoke',material='hot',radius={32,48,64},intensity=3,flags={'noShadows'},startCondition={3,1}},"
		"{type='particle',id='smoke',parent='core',particle='smoke',maxInstances=64,maxParticles=64,spawnRate=20,velocityScale=1,flags={'gpuLifecycle'}},"
		"{type='decal',id='mark1',material='burn',size=32,depth=4},"
		"{type='decal2',id='mark2',material='scorch',size=48,depth=8},"
		"{type='model',id='debris',model='chunk',material='charred',origin='trackLocal',rotation='angles',angles={10,20,30}},"
		"{type='sound',id='boom',sound='rocket',channel=2},"
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
		"{type='godray',id='rays',material='rays',godrayColor={1,.5,.2,1},colorScale=2,size=64,sourceSize=16}"
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
	assert( profile.actions[0].startConditionMask == ( ( (uint64_t)1u << 32 ) | 3u ) );
	assert( profile.actions[1].parentAction == 0u );
	assert( profile.actions[1].payload.particle.maxParticles == 64u );
	assert( profile.actions[4].originType == WIRED_FX_ORIGIN_TRACK_LOCAL );
	assert( profile.actions[4].rotationType == WIRED_FX_ROTATION_EXPLICIT_ANGLES );
	assert( profile.actions[4].rotationDegrees[1] == 20.0f );
	assert( profile.actions[10].payload.environment.renderParmCount == 1u );
	assert( profile.actions[16].payload.godray.sourceSize == 16u );
	assert( calls == 15u );
	assert( WiredFx_ValidateProfile( &profile ) == WIRED_FX_VALID );
	lua_pop( L, 1 );

#ifdef WIRED_SOURCE_DIR
	assert( luaL_loadfile( L, WIRED_SOURCE_DIR "/modfiles/scripts/effects/rocket-explosion.lua" ) == 0 );
	assert( lua_pcall( L, 0, 1, 0 ) == 0 );
	assert( WiredFxAuthoring_ReadProfile( &profile, L, -1, &resolver, error, sizeof( error ) ) );
	assert( profile.actionCount == 10u );
	assert( profile.actions[0].type == WIRED_FX_ACTION_PARTICLE );
	assert( profile.actions[2].type == WIRED_FX_ACTION_PARTICLE );
	assert( profile.actions[2].payload.particle.maxParticles == 1u );
	assert( profile.actions[5].startConditionMask == WIRED_FX_CONDITION_MATERIAL_METAL );
	assert( profile.actions[6].type == WIRED_FX_ACTION_LIGHT );
	assert( profile.actions[9].type == WIRED_FX_ACTION_DECAL );
	assert( WiredFx_ValidateProfile( &profile ) == WIRED_FX_VALID );
	lua_pop( L, 1 );
	assert( luaL_loadfile( L, WIRED_SOURCE_DIR "/modfiles/scripts/effects/rocket-detonation.lua" ) == 0 );
	assert( lua_pcall( L, 0, 1, 0 ) == 0 );
	assert( WiredFxAuthoring_ReadProfile( &profile, L, -1, &resolver, error, sizeof( error ) ) );
	assert( profile.actionCount == 9u );
	assert( profile.actions[2].type == WIRED_FX_ACTION_PARTICLE );
	assert( profile.actions[2].payload.particle.maxParticles == 1u );
	assert( profile.actions[8].type == WIRED_FX_ACTION_RADIAL_BLUR );
	assert( WiredFx_ValidateProfile( &profile ) == WIRED_FX_VALID );
	lua_pop( L, 1 );
	assert( luaL_loadfile( L, WIRED_SOURCE_DIR "/modfiles/scripts/effects/rocket-underwater.lua" ) == 0 );
	assert( lua_pcall( L, 0, 1, 0 ) == 0 );
	assert( WiredFxAuthoring_ReadProfile( &profile, L, -1, &resolver, error, sizeof( error ) ) );
	assert( profile.actionCount == 7u );
	assert( profile.actions[2].type == WIRED_FX_ACTION_PARTICLE );
	assert( profile.actions[6].type == WIRED_FX_ACTION_SCREEN_SHAKE );
	assert( WiredFx_ValidateProfile( &profile ) == WIRED_FX_VALID );
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
