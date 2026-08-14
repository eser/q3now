// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_temporal_motion_targets.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL temporal motion targets line %d: %s\n", __LINE__, #x ); return 1; } } while ( 0 )

struct ralBackend_s { int id; };
struct ralTexture_s { int id; };
struct ralTextureView_s { int id; };

static ralCaps_t caps;
static struct ralTexture_s textures[32];
static struct ralTextureView_s views[32];
static ralTextureCreateInfo_t textureInfos[32];
static ralTextureViewCreateInfo_t viewInfos[32];
static int textureCalls, viewCalls, textureDestroys, viewDestroys;
static int capsCalls, supportCalls, destroyedProtected;
static int failTextureAt, failViewAt;
static int aliasTextureAt, aliasViewAt;
static ralTexture_t *aliasTextureValue;
static ralTextureView_t *aliasViewValue;
static ralTexture_t *protectedTextureA, *protectedTextureB;
static ralTextureView_t *protectedViewA, *protectedViewB;
static int supportVelocity = 1, supportValidity = 1, badUsageQuery;
static char destroyKinds[64];
static int destroyIds[64], destroyCount;

const ralCaps_t *Ral_GetCaps( ralBackend_t *backend ) {
	capsCalls++;
	return backend ? &caps : NULL;
}

qboolean Ral_TextureFormatSupports( ralBackend_t *backend, ralFormat_t format,
		ralTextureUsage_t usage ) {
	const ralTextureUsage_t exact = (ralTextureUsage_t)(
		RAL_TEXTURE_USAGE_COLOR_ATTACHMENT | RAL_TEXTURE_USAGE_SAMPLED
		| RAL_TEXTURE_USAGE_TRANSFER_SRC );
	supportCalls++;
	if ( !backend || usage != exact ) badUsageQuery = 1;
	if ( format == RAL_FORMAT_R16G16_SFLOAT ) return supportVelocity;
	if ( format == RAL_FORMAT_R8_UNORM ) return supportValidity;
	return qfalse;
}

ralTexture_t *Ral_CreateTexture( ralBackend_t *backend,
		const ralTextureCreateInfo_t *ci ) {
	int call = ++textureCalls;
	(void)backend;
	textureInfos[call] = *ci;
	if ( call == failTextureAt ) return NULL;
	if ( call == aliasTextureAt ) return aliasTextureValue
		? aliasTextureValue : &textures[call - 1];
	textures[call].id = call;
	return &textures[call];
}

ralTextureView_t *Ral_CreateTextureView( ralBackend_t *backend,
		const ralTextureViewCreateInfo_t *ci ) {
	int call = ++viewCalls;
	(void)backend;
	viewInfos[call] = *ci;
	if ( call == failViewAt ) return NULL;
	if ( call == aliasViewAt ) return aliasViewValue
		? aliasViewValue : &views[call - 1];
	views[call].id = call;
	return &views[call];
}

void Ral_DestroyTexture( ralTexture_t *texture ) {
	struct ralTexture_s *fake = (struct ralTexture_s *)texture;
	if ( texture == protectedTextureA || texture == protectedTextureB )
		destroyedProtected++;
	textureDestroys++;
	destroyKinds[destroyCount] = 'T';
	destroyIds[destroyCount++] = fake->id;
}

void Ral_DestroyTextureView( ralTextureView_t *view ) {
	struct ralTextureView_s *fake = (struct ralTextureView_s *)view;
	if ( view == protectedViewA || view == protectedViewB )
		destroyedProtected++;
	viewDestroys++;
	destroyKinds[destroyCount] = 'V';
	destroyIds[destroyCount++] = fake->id;
}

int main( void ) {
	struct ralBackend_s backendA = { 1 }, backendB = { 2 };
	temporalMotionTargets_t targets, before;
	const ralTextureUsage_t exactUsage = (ralTextureUsage_t)(
		RAL_TEXTURE_USAGE_COLOR_ATTACHMENT | RAL_TEXTURE_USAGE_SAMPLED
		| RAL_TEXTURE_USAGE_TRANSFER_SRC );
	int td, vd, eventBase, capBase, supportBase;

	memset( &caps, 0, sizeof( caps ) );
	caps.independentBlend = qtrue;
	caps.maxColorAttachments = 8;
	caps.maxTextureDimension2D = 4096;
	R_TemporalMotionTargetsInit( &targets );
	CHECK( !targets.ready && targets.allocationGeneration == 0 );
	before = targets;
	CHECK( !R_TemporalMotionTargetsEnsure( NULL, &backendA, 1280, 720, 1 ) );
	CHECK( !R_TemporalMotionTargetsEnsure( &targets, NULL, 1280, 720, 1 ) );
	CHECK( !R_TemporalMotionTargetsEnsure( &targets, &backendA, 0, 720, 1 ) );
	CHECK( memcmp( &targets, &before, sizeof( targets ) ) == 0 );

	caps.independentBlend = qfalse;
	CHECK( !R_TemporalMotionTargetsEnsure( &targets, &backendA, 1280, 720, 1 ) );
	CHECK( textureCalls == 0 && viewCalls == 0 );
	caps.independentBlend = qtrue;
	caps.maxColorAttachments = 2;
	CHECK( !R_TemporalMotionTargetsEnsure( &targets, &backendA, 1280, 720, 1 ) );
	CHECK( textureCalls == 0 && viewCalls == 0 );
	caps.maxColorAttachments = 8;
	caps.maxTextureDimension2D = 1024;
	CHECK( !R_TemporalMotionTargetsEnsure( &targets, &backendA, 1280, 720, 1 ) );
	CHECK( !R_TemporalMotionTargetsEnsure( &targets, &backendA, 720, 1280, 1 ) );
	CHECK( textureCalls == 0 && viewCalls == 0 );
	caps.maxTextureDimension2D = 4096;
	supportVelocity = 0;
	CHECK( !R_TemporalMotionTargetsEnsure( &targets, &backendA, 1280, 720, 1 ) );
	supportVelocity = 1;
	supportValidity = 0;
	CHECK( !R_TemporalMotionTargetsEnsure( &targets, &backendA, 1280, 720, 1 ) );
	CHECK( textureCalls == 0 && viewCalls == 0 );
	supportValidity = 1;

	CHECK( R_TemporalMotionTargetsEnsure( &targets, &backendA, 1280, 720, 1 ) );
	CHECK( targets.ready && targets.backend == &backendA );
	CHECK( targets.width == 1280 && targets.height == 720
		&& targets.topologyEpoch == 1 && targets.allocationGeneration == 1 );
	CHECK( textureCalls == 2 && viewCalls == 2 && !badUsageQuery );
	CHECK( textureInfos[1].format == RAL_FORMAT_R16G16_SFLOAT );
	CHECK( textureInfos[2].format == RAL_FORMAT_R8_UNORM );
	CHECK( textureInfos[1].usage == exactUsage && textureInfos[2].usage == exactUsage );
	CHECK( textureInfos[1].width == 1280 && textureInfos[1].height == 720 );
	CHECK( textureInfos[1].type == RAL_TEXTURE_2D
		&& textureInfos[1].depthOrArrayLayers == 1
		&& textureInfos[1].mipLevels == 1 && textureInfos[1].sampleCount == 1
		&& textureInfos[1].memory == RAL_MEMORY_DEVICE_LOCAL );
	CHECK( viewInfos[1].texture == targets.velocity
		&& viewInfos[2].texture == targets.validity );
	CHECK( viewInfos[1].viewType == RAL_TEXTURE_2D
		&& viewInfos[1].format == RAL_FORMAT_UNDEFINED );
	before = targets;
	capBase = capsCalls; supportBase = supportCalls;
	CHECK( R_TemporalMotionTargetsEnsure( &targets, &backendA, 1280, 720, 1 ) );
	CHECK( memcmp( &targets, &before, sizeof( targets ) ) == 0 );
	CHECK( textureCalls == 2 && viewCalls == 2
		&& capsCalls == capBase && supportCalls == supportBase );

	eventBase = destroyCount;
	CHECK( R_TemporalMotionTargetsEnsure( &targets, &backendA, 1600, 900, 1 ) );
	CHECK( targets.allocationGeneration == 2 && targets.width == 1600 );
	CHECK( destroyCount == eventBase + 4 );
	CHECK( destroyKinds[eventBase + 0] == 'V' && destroyIds[eventBase + 0] == 2 );
	CHECK( destroyKinds[eventBase + 1] == 'V' && destroyIds[eventBase + 1] == 1 );
	CHECK( destroyKinds[eventBase + 2] == 'T' && destroyIds[eventBase + 2] == 2 );
	CHECK( destroyKinds[eventBase + 3] == 'T' && destroyIds[eventBase + 3] == 1 );

	before = targets;
	td = textureDestroys; vd = viewDestroys;
	failTextureAt = textureCalls + 2;
	CHECK( !R_TemporalMotionTargetsEnsure( &targets, &backendA, 1600, 900, 2 ) );
	CHECK( memcmp( &targets, &before, sizeof( targets ) ) == 0 );
	CHECK( textureDestroys == td + 1 && viewDestroys == vd + 1 );
	failTextureAt = 0;

	before = targets;
	td = textureDestroys; vd = viewDestroys;
	failViewAt = viewCalls + 2;
	CHECK( !R_TemporalMotionTargetsEnsure( &targets, &backendA, 1600, 900, 2 ) );
	CHECK( memcmp( &targets, &before, sizeof( targets ) ) == 0 );
	CHECK( textureDestroys == td + 2 && viewDestroys == vd + 1 );
	failViewAt = 0;

	CHECK( R_TemporalMotionTargetsEnsure( &targets, &backendB, 1600, 900, 1 ) );
	CHECK( targets.backend == &backendB && targets.allocationGeneration == 3 );
	targets.allocationGeneration = UINT32_MAX;
	before = targets;
	CHECK( !R_TemporalMotionTargetsEnsure( &targets, &backendB, 1601, 900, 1 ) );
	CHECK( memcmp( &targets, &before, sizeof( targets ) ) == 0 );

	eventBase = destroyCount;
	R_TemporalMotionTargetsRelease( &targets );
	CHECK( !targets.ready && targets.backend == NULL
		&& targets.allocationGeneration == UINT32_MAX );
	CHECK( destroyCount == eventBase + 4 );
	R_TemporalMotionTargetsRelease( &targets );
	CHECK( destroyCount == eventBase + 4 );

	// Texture/view wrappers must be distinct. Each adversarial failure is
	// cleanup-safe and leaves an existing live owner byte-identical.
	{
		temporalMotionTargets_t aliased;
		int textureDestroyBase, viewDestroyBase;
		R_TemporalMotionTargetsInit( &aliased );
		CHECK( R_TemporalMotionTargetsEnsure( &aliased, &backendA, 64, 64, 1 ) );
		before = aliased;
		protectedTextureA = aliased.velocity;
		protectedTextureB = aliased.validity;
		protectedViewA = aliased.velocityView;
		protectedViewB = aliased.validityView;
		destroyedProtected = 0;
		aliasTextureValue = aliased.velocity;
		aliasTextureAt = textureCalls + 1;
		CHECK( !R_TemporalMotionTargetsEnsure( &aliased, &backendA, 65, 64, 1 ) );
		CHECK( memcmp( &aliased, &before, sizeof( aliased ) ) == 0
			&& destroyedProtected == 0 );
		aliasTextureValue = aliased.validity;
		aliasTextureAt = textureCalls + 2;
		CHECK( !R_TemporalMotionTargetsEnsure( &aliased, &backendA, 65, 64, 1 ) );
		CHECK( memcmp( &aliased, &before, sizeof( aliased ) ) == 0
			&& destroyedProtected == 0 );
		aliasTextureAt = 0; aliasTextureValue = NULL;
		aliasViewValue = aliased.velocityView;
		aliasViewAt = viewCalls + 1;
		CHECK( !R_TemporalMotionTargetsEnsure( &aliased, &backendA, 65, 64, 1 ) );
		CHECK( memcmp( &aliased, &before, sizeof( aliased ) ) == 0
			&& destroyedProtected == 0 );
		aliasViewValue = aliased.validityView;
		aliasViewAt = viewCalls + 2;
		CHECK( !R_TemporalMotionTargetsEnsure( &aliased, &backendA, 65, 64, 1 ) );
		CHECK( memcmp( &aliased, &before, sizeof( aliased ) ) == 0
			&& destroyedProtected == 0 );
		aliasViewAt = 0; aliasViewValue = NULL;
		textureDestroyBase = textureDestroys;
		viewDestroyBase = viewDestroys;
		aliasTextureAt = textureCalls + 2;
		CHECK( !R_TemporalMotionTargetsEnsure( &aliased, &backendA, 65, 64, 1 ) );
		CHECK( memcmp( &aliased, &before, sizeof( aliased ) ) == 0
			&& textureDestroys == textureDestroyBase + 1
			&& viewDestroys == viewDestroyBase + 1 );
		aliasTextureAt = 0;
		textureDestroyBase = textureDestroys;
		viewDestroyBase = viewDestroys;
		aliasViewAt = viewCalls + 2;
		CHECK( !R_TemporalMotionTargetsEnsure( &aliased, &backendA, 65, 64, 1 ) );
		CHECK( memcmp( &aliased, &before, sizeof( aliased ) ) == 0
			&& textureDestroys == textureDestroyBase + 2
			&& viewDestroys == viewDestroyBase + 1 );
		aliasViewAt = 0;
		protectedTextureA = protectedTextureB = NULL;
		protectedViewA = protectedViewB = NULL;
		R_TemporalMotionTargetsRelease( &aliased );
	}

	puts( "PASS temporal motion target owner contract" );
	return 0;
}
