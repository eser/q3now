// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_opengl_frontend.h"
#include "ral_opengl_lighting.h"
#include "ral_opengl_world.h"
#include "ral_opengl_product.h"
#include "ral_lighting_product.h"
#include "ral_atmosphere_conformance.h"
#include "maps/map_format_registry.h"
#include "../code/render/frontend/render_material_stage_gpu.h"
#include "../code/render/frontend/render_atmosphere_gpu.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK( condition ) do { if ( !( condition ) ) { \
	fprintf( stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, \
		#condition ); return 1; } } while ( 0 )

static ralIrradianceEntitySampleReceipt_t LocalIrradiance( uint64_t generation,
		uint64_t entityId ) {
	ralIrradianceEntitySampleReceipt_t r;
	memset( &r, 0, sizeof( r ) );
	r.schemaVersion = RAL_IRRADIANCE_ENTITY_RECEIPT_SCHEMA_VERSION;
	r.queryGeneration = generation; r.entityId = entityId;
	r.volumeId = 7u; r.layoutHash = 8u;
	r.probes.schemaVersion = RAL_IRRADIANCE_RECEIPT_SCHEMA_VERSION;
	r.probes.queryGeneration = generation; r.probes.productGeneration = 9u;
	r.probes.fallback = RAL_IRRADIANCE_FALLBACK_LIGHTGRID;
	r.probes.usedFallback = qtrue; r.probes.ready = qtrue;
	for ( uint32_t channel = 0u; channel < 3u; ++channel ) {
		r.blendedCoefficientsQ16[0][channel] = RAL_LIGHT_Q16_ONE;
		r.diffuseIrradianceQ16[channel] = RAL_LIGHT_Q16_ONE;
	}
	r.contributorHash = 10u;
	r.coefficientHash = Ral_IrradianceCoefficientHash( r.blendedCoefficientsQ16 );
	r.usedFallback = qtrue; r.ready = qtrue;
	return r;
}

typedef unsigned int fakeName_t;
typedef unsigned int fakeEnum_t;
typedef int fakeInt_t;
typedef int fakeSize_t;
typedef long fakeSizePtr_t;
typedef long fakeIntPtr_t;
typedef unsigned char fakeBool_t;
typedef unsigned long long fakeUInt64_t;

/* Observe actual GL uploads/bindings; do not emulate the compute shaders. */
enum { ATM_TILE, ATM_INJECT, ATM_LIGHT, ATM_CLOUD, ATM_INTEGRATE, ATM_PASSES };
static unsigned s_atmShader[8192], s_atmProgram[8192];
static struct { fakeSizePtr_t size; unsigned flags; unsigned char bytes[192]; } s_buffers[8192];
static qboolean s_bufferLive[8192];
static unsigned s_liveBuffers, s_bufferLifetimeErrors, s_bufferCreateCalls[12];
static struct {
	unsigned count, attempts, storageCalls, failures;
	int createIndex, storageIndex;
	fakeName_t names[11];
} s_bufferFault;
static fakeName_t s_ubo[4], s_ssbo[4];
static struct {
	unsigned count, order[ATM_PASSES], dims[ATM_PASSES][3];
	unsigned char params[ATM_PASSES][192];
	fakeName_t bindings[ATM_PASSES][4];
	renderAtmosphereVolumeGpu_t volumes[RAL_ATMOSPHERE_MAX_VOLUMES];
	renderAtmosphereLocalLightGpu_t lights[RAL_ATMOSPHERE_MAX_LIGHTS];
	fakeName_t volumeBuffer, lightBuffer;
} s_atm;

static int CheckAtmospherePacket( const renderSubmissionState_t *frontend,
		const ralOpenGlProductFrameReceipt_t *receipt, qboolean historyValid ) {
	renderAtmosphereGpuFrame_t expected;
	const ralAtmospherePlanReceipt_t *p = &receipt->atmosphere;
	if ( !p->computeDispatchCount ) { CHECK( !s_atm.count ); return 0; }
	CHECK( RenderAtmosphereGpu_Build( frontend, p, 80u*45u*64u, historyValid, &expected ) );
	CHECK( s_atm.count == p->computeDispatchCount+1u );
	CHECK( receipt->atmosphereNativeDispatchCount == s_atm.count );
	const void *params[] = { &expected.tile, &expected.inject, &expected.light,
		&expected.cloud, &expected.integrate };
	const size_t sizes[] = { sizeof(expected.tile), sizeof(expected.inject),
		sizeof(expected.light), sizeof(expected.cloud), sizeof(expected.integrate) };
	unsigned ordinal = 0;
	for ( unsigned pass = 0; pass < ATM_PASSES; ++pass ) {
		if ( pass == ATM_CLOUD && !p->cloudsActive ) continue;
		CHECK( s_atm.order[ordinal++] == pass );
		CHECK( !memcmp( s_atm.params[pass], params[pass], sizes[pass] ) );
		const unsigned group = pass == ATM_TILE || pass == ATM_INTEGRATE ? 8u : 4u;
		CHECK( s_atm.dims[pass][0] == (p->froxelWidth+group-1u)/group );
		CHECK( s_atm.dims[pass][1] == (p->froxelHeight+group-1u)/group );
		CHECK( s_atm.dims[pass][2] == (group == 8u ? 1u : (p->froxelDepth+3u)/4u) );
	}
	CHECK( !memcmp( s_atm.volumes, expected.volumes, sizeof(expected.volumes) ) );
	CHECK( !memcmp( s_atm.lights, expected.lights, sizeof(expected.lights) ) );
	CHECK( s_atm.bindings[ATM_TILE][0] == s_atm.lightBuffer );
	CHECK( s_buffers[s_atm.bindings[ATM_TILE][1]].size == sizeof(float) );
	CHECK( s_atm.bindings[ATM_INJECT][0] == s_atm.volumeBuffer );
	CHECK( s_atm.bindings[ATM_LIGHT][0] == s_atm.bindings[ATM_INJECT][1] );
	CHECK( s_atm.bindings[ATM_LIGHT][1] == s_atm.bindings[ATM_TILE][2] );
	CHECK( s_atm.bindings[ATM_LIGHT][2] == s_atm.lightBuffer );
	CHECK( s_atm.bindings[ATM_INTEGRATE][0] == (p->cloudsActive
		? s_atm.bindings[ATM_CLOUD][1] : s_atm.bindings[ATM_LIGHT][3]) );
	if ( p->cloudsActive )
		CHECK( s_atm.bindings[ATM_CLOUD][0] == s_atm.bindings[ATM_LIGHT][3] );
	/* No input/output aliasing in a media or history dispatch. */
	CHECK( s_atm.bindings[ATM_INJECT][1] != s_atm.volumeBuffer );
	for ( unsigned a = 0; a < 4; ++a ) for ( unsigned b = a+1; b < 4; ++b )
		CHECK( s_atm.bindings[ATM_INTEGRATE][a] != s_atm.bindings[ATM_INTEGRATE][b] );
	return 0;
}

static renderWorldVertex_t s_beamUpload[4];
static qboolean s_beamUploadSeen;
static float s_beamMaterial[20];
static qboolean s_beamMaterialActive;
static fakeName_t s_currentTexture, s_beamTextures[RENDER_MATERIAL_MAX_STAGES];
static uint32_t s_beamStageDraws;
static uint32_t s_deletedTextures;
static float s_beamStageUv[RENDER_MATERIAL_MAX_STAGES][4];
static fakeEnum_t s_blendSource, s_blendDestination;
static fakeEnum_t s_beamBlendSource, s_beamBlendDestination;
static struct {
	fakeEnum_t source, destination;
	uintptr_t offset;
	fakeSize_t count;
} s_firstDraws[11];
static uint32_t s_firstDrawCount;
static renderSkyMaterialGpuParams_t s_skyParams;
static qboolean s_skyPending;
static fakeName_t s_skyTextures[10], s_skyProgram, s_activeProgram;
static uint32_t s_skyDraws;
static fakeBool_t s_skyDepthWrite;
static fakeName_t s_captureFramebuffer, s_captureVao;
static int s_captureFace = -1, s_viewport[4] = {0, 0, 1280, 720};
static int s_rejectCaptureFace = -1;
static uint32_t s_captureDraws, s_captureMask;
static float s_captureTimes[6];
static float s_capturePhysical[6], s_lastPhysicalBlend;
static fakeName_t s_captureTextures[6][10];
static fakeBool_t s_depthEnabled, s_blendEnabled, s_cullEnabled, s_scissorEnabled;
static qboolean s_probeTierBindings;
static fakeName_t s_firstTierBindings[4];
static unsigned s_tierBindingsSeen;

static struct {
	fakeName_t nextName;
	fakeEnum_t error;
	uint32_t drawCount;
	uint32_t textureUploadCount;
	uint32_t textureMipmapCount;
	uint32_t dispatchCount;
	uint32_t barrierCount;
	uint32_t imageBindCount;
	uint32_t fenceCount;
	uint32_t completedFenceCount;
	uint32_t queryBeginCount;
	uint32_t queryResultCount;
	fakeName_t activeTimeElapsedQuery;
	uint32_t weatherDrawCount;
	fakeBool_t depthWriteEnabled;
	uint32_t depthClearCount;
	uint32_t maskedDepthClearCount;
	fakeName_t boundTextureByUnit[10];
} s_gl;

/* Command/lifetime test double only: visibility values are supplied by the
 * test, not a CPU replacement for the independently GPU-tested shader. */
static struct {
	fakeName_t name;
	float records[LENS_MAX_CGSOURCES][8];
} s_lensBuffers[3];
static struct { qboolean lens, signaled, deleted; } s_fences[256];
static uint32_t s_fenceIndex, s_lastLensFence, s_lensReads, s_lensCopies;
static fakeName_t s_lensBinding;
static qboolean s_lensCopyPending, s_lensFencePending, s_holdLens, s_failLensRead;
static float s_lensResult = 0.75f;
static fakeName_t s_drawFramebuffer, s_readFramebuffer, s_lastHdrFramebuffer;
static fakeName_t s_lastResetAccumulator, s_lastCopySource, s_lastCopyDestination;
static unsigned s_compositeCount, s_histogramClears, s_accumulatorResets, s_accumulatorCopies;
static ralExposureBlock_t s_lastExposureBlock;
static qboolean s_rejectFramebuffer, s_rejectComposite;
static float s_lastDrawUniforms[16];
static float s_lastFullAtmosphereFar;
static unsigned s_pbrMaterialUploads;
static unsigned s_hdrSceneDraws, s_uiOutputDraws;
static fakeBool_t s_srgbEnabled;
static qboolean s_linearOutput, s_rejectEncodingQuery;
static unsigned s_defaultEncodingQueries, s_customEncodingQueries;
/* Inspect actual native uploads/bindings, not a CPU simulation of weather. */
static float s_weatherFrame[288], s_heightgridSamples[4];
static fakeName_t s_heightgridTexture, s_weatherHeightgridBinding;
static unsigned s_heightgridUploads, s_heightgridDeletes;
static fakeName_t s_heightgridNames[16];
static qboolean s_weatherPending, s_rejectHeightgrid;

static fakeBool_t FakeIsEnabled( fakeEnum_t state ) {
	switch (state) {
	case 0x8db9u: return s_srgbEnabled;
	case 0x0b71u: return s_depthEnabled;
	case 0x0be2u: return s_blendEnabled;
	case 0x0b44u: return s_cullEnabled;
	case 0x0c11u: return s_scissorEnabled;
	default: s_gl.error = 0x0500; return 0;
	}
}
static void FakeGetFramebufferAttachmentParameteriv( fakeEnum_t target,
		fakeEnum_t attachment, fakeEnum_t name, fakeInt_t *out ) {
	if ( target != 0x8ca9u || name != 0x8210u || s_rejectEncodingQuery
			|| attachment != (s_drawFramebuffer ? 0x8ce0u : 0x0402u) )
		s_gl.error = 0x0502;
	if ( s_drawFramebuffer ) s_customEncodingQueries++;
	else s_defaultEncodingQueries++;
	*out = s_linearOutput ? 0x2601 : 0x8c40;
}

static void FakeGetIntegerv( fakeEnum_t name, fakeInt_t *out ) {
	switch ( name ) {
	case 0x8ca6: *out = (fakeInt_t)s_drawFramebuffer; break;
	case 0x8caa: *out = (fakeInt_t)s_readFramebuffer; break;
	case 0x0ba2: memcpy(out, s_viewport, sizeof(s_viewport)); break;
	case 0x8b8d: *out = (fakeInt_t)s_activeProgram; break;
	case 0x85b5: *out = (fakeInt_t)s_captureVao; break;
	case 0x821b: *out = 4; break;
	case 0x821c: *out = 6; break;
	case 0x9126: *out = 1; break;
	case 0x8cdf: *out = 8; break;
	case 0x0d33: *out = 16384; break;
	case 0x8073: *out = 2048; break;
	case 0x88ff: *out = 2048; break;
	case 0x90eb: *out = 1024; break;
	case 0x8a34: *out = 256; break;
	case 0x90df: *out = 16; break;
	case 0x8b4d: *out = 192; break;
	default: *out = 0; s_gl.error = 0x0500; break;
	}
}
static const unsigned char *FakeGetString( fakeEnum_t name ) {
	(void)name; return (const unsigned char *)"Wired OpenGL 4.6 test";
}
static fakeEnum_t FakeGetError( void ) {
	fakeEnum_t error = s_gl.error; s_gl.error = 0u; return error;
}
static void FakeCreateNames( fakeSize_t count, fakeName_t *names ) {
	for ( int i = 0; i < count; ++i ) names[i] = ++s_gl.nextName;
}
static void FakeCreateBuffers( fakeSize_t count, fakeName_t *names ) {
	const qboolean inject = count == (fakeSize_t)s_bufferFault.count && !s_bufferFault.attempts;
	if ( count > 0 && count < 12 ) ++s_bufferCreateCalls[count];
	if ( inject ) ++s_bufferFault.attempts;
	for ( int i = 0; i < count; ++i ) {
		if ( inject && i == s_bufferFault.createIndex ) {
			names[i] = 0;
			s_gl.error = 0x0505; /* GL_OUT_OF_MEMORY */
			++s_bufferFault.failures;
		} else {
			names[i] = ++s_gl.nextName;
			if ( names[i] >= 8192u || s_bufferLive[names[i]] ) {
				++s_bufferLifetimeErrors; s_gl.error = 0x0505; return;
			}
			s_bufferLive[names[i]] = qtrue; ++s_liveBuffers;
		}
		if ( inject ) s_bufferFault.names[i] = names[i];
	}
}
static void FakeCreateTextures( fakeEnum_t target, fakeSize_t count,
		fakeName_t *names ) { (void)target; FakeCreateNames( count, names ); }
static void FakeNamedBufferStorage( fakeName_t name, fakeSizePtr_t size,
		const void *data, unsigned int flags ) {
	for ( unsigned i = 0; i < s_bufferFault.count; ++i )
		if ( name && name == s_bufferFault.names[i] &&
				(int)s_bufferFault.storageCalls++ == s_bufferFault.storageIndex ) {
			s_gl.error = 0x0505; ++s_bufferFault.failures; return;
		}
	(void)data; (void)flags; if ( !name || size <= 0 ) s_gl.error = 0x0501;
	if ( name >= 8192u ) { s_gl.error = 0x0501; return; }
	if ( !s_bufferLive[name] ) { ++s_bufferLifetimeErrors; s_gl.error = 0x0502; return; }
	s_buffers[name].size = size; s_buffers[name].flags = flags;
	if ( data && size == sizeof( s_beamUpload ) ) {
		memcpy( s_beamUpload, data, sizeof( s_beamUpload ) );
		s_beamUploadSeen = qtrue;
	}
	if ( size == sizeof( s_lensBuffers[0].records ) ) {
		for ( unsigned i = 0; i < 3; ++i ) if ( !s_lensBuffers[i].name ) {
			s_lensBuffers[i].name = name; return;
		}
		s_gl.error = 0x0505;
	}
}
static void FakeNamedBufferSubData( fakeName_t name, fakeIntPtr_t offset,
		fakeSizePtr_t size, const void *data ) {
	if ( !name || offset != 0 || size <= 0 || !data ) s_gl.error = 0x0501;
	if ( name >= 8192u || size > s_buffers[name].size || !(s_buffers[name].flags & 0x100u) ) {
		s_gl.error = 0x0502; return;
	}
	if ( data && size <= 192 ) memcpy( s_buffers[name].bytes, data, size );
	if ( data && size == sizeof(s_atm.volumes) ) {
		memcpy(s_atm.volumes, data, size); s_atm.volumeBuffer = name;
	}
	if ( data && size == sizeof(s_atm.lights) ) {
		memcpy(s_atm.lights, data, size); s_atm.lightBuffer = name;
	}
	if ( size == sizeof(s_weatherFrame) && data ) {
		memcpy(s_weatherFrame, data, sizeof(s_weatherFrame));
		s_weatherPending = qtrue;
	}
	if ( size == sizeof(s_skyParams) && data ) {
		memcpy(&s_skyParams, data, sizeof(s_skyParams));
		s_skyPending = qtrue;
	}
	if ( size == 256 && data ) memcpy( s_lastDrawUniforms, data, sizeof(s_lastDrawUniforms) );
	if ( size == sizeof(renderProductDrawGpuParams_t) && data ) {
		const renderProductDrawGpuParams_t *draw = data;
		s_lastPhysicalBlend = draw->atmosphereHeightSky[2];
		if ( draw->atmosphereFroxelGrid[2] )
			s_lastFullAtmosphereFar = draw->atmosphereColorVisibility[3];
	}
	if (size == 96 && data && s_buffers[name].size == sizeof(renderSkyMaterialGpuParams_t)) {
		if (((const float *)data)[4] >= 0.0f) ++s_pbrMaterialUploads;
		s_beamMaterialActive = ((const uint32_t *)data)[20] == 1u;
		if (s_beamMaterialActive) memcpy(s_beamMaterial, data, sizeof(s_beamMaterial));
	}
	if ( size == sizeof(s_lastExposureBlock) && data )
		memcpy( &s_lastExposureBlock, data, sizeof(s_lastExposureBlock) );
	if ( size == sizeof(float) ) {
		if ( *(const float *)data != 1.0f ) s_gl.error = 0x0501;
		s_lastResetAccumulator = name; s_accumulatorResets++;
	}
	for ( unsigned i = 0; i < 3; ++i ) if ( s_lensBuffers[i].name == name ) {
		if ( size > (fakeSizePtr_t)sizeof( s_lensBuffers[i].records ) ) s_gl.error = 0x0501;
		else memcpy( s_lensBuffers[i].records, data, size );
	}
}
static void FakeGetNamedBufferSubData( fakeName_t name, fakeIntPtr_t offset,
		fakeSizePtr_t size, void *data ) {
	if ( s_failLensRead ) { s_gl.error = 0x0502; return; }
	for ( unsigned i = 0; i < 3; ++i ) if ( s_lensBuffers[i].name == name
			&& offset == 0 && size > 0 && size <= (fakeSizePtr_t)sizeof( s_lensBuffers[i].records ) && data ) {
		memcpy( data, s_lensBuffers[i].records, size ); s_lensReads++; return;
	}
	s_gl.error = 0x0501;
}
static void FakeCopyTextureSubImage2D( fakeName_t name, fakeInt_t level,
		fakeInt_t xo, fakeInt_t yo, fakeInt_t x, fakeInt_t y,
		fakeSize_t width, fakeSize_t height ) {
	if ( !name || level || xo || yo || x || y || width != 1280 || height != 720 )
		s_gl.error = 0x0501;
	s_lensCopyPending = qtrue; s_lensCopies++;
}
static void FakeBindBufferBase( fakeEnum_t target, unsigned int index,
		fakeName_t name ) {
	if ( index < 4u ) {
		if ( target == 0x8a11u ) s_ubo[index] = name;
		if ( target == 0x90d2u ) s_ssbo[index] = name;
	}
	if ( ( target != 0x8a11u && target != 0x90d2u )
			|| ( index > 3u && index != 8u ) || !name ) s_gl.error = 0x0501;
	if ( target == 0x90d2u && index == 0 ) s_lensBinding = name;
}
static void *FakeMapNamedBufferRange( fakeName_t name, fakeIntPtr_t offset,
		fakeSizePtr_t size, unsigned int flags ) {
	static unsigned char storage[4096];
	(void)offset; (void)flags;
	if ( !name || size <= 0 || size > (fakeSizePtr_t)sizeof( storage ) ) return NULL;
	return storage;
}
static fakeBool_t FakeUnmapNamedBuffer( fakeName_t name ) { return name ? 1u : 0u; }
static void FakeCopyNamedBufferSubData( fakeName_t source, fakeName_t destination,
		fakeIntPtr_t sourceOffset, fakeIntPtr_t destinationOffset, fakeSizePtr_t size ) {
	(void)sourceOffset; (void)destinationOffset;
	if ( !source || !destination || size <= 0 ) s_gl.error = 0x0501;
	if ( size == sizeof(float) ) {
		s_lastCopySource = source; s_lastCopyDestination = destination;
		s_accumulatorCopies++;
	}
}
static void FakeBindFramebuffer( fakeEnum_t target, fakeName_t name ) {
	if ( target == 0x8d40u || target == 0x8ca9u ) s_drawFramebuffer = name;
	if ( target == 0x8d40u || target == 0x8ca8u ) s_readFramebuffer = name;
}
static void FakeNamedFramebufferTexture( fakeName_t framebuffer,
		fakeEnum_t attachment, fakeName_t texture, fakeInt_t level ) {
	if ( !framebuffer || level
			|| (attachment != 0x8ce0u && attachment != 0x8d00u) ) s_gl.error = 0x0501;
	if (!texture && framebuffer == s_captureFramebuffer) s_captureFace = -1;
	else s_lastHdrFramebuffer = framebuffer;
}
static void FakeNamedFramebufferTextureLayer(fakeName_t framebuffer,
		fakeEnum_t attachment, fakeName_t texture, fakeInt_t level, fakeInt_t layer) {
	if (!framebuffer || !texture || attachment != 0x8ce0u || level || layer < 0 || layer >= 6)
		s_gl.error = 0x0501;
	s_captureFramebuffer = framebuffer; s_captureFace = layer;
}
static fakeEnum_t FakeCheckNamedFramebufferStatus( fakeName_t framebuffer, fakeEnum_t target ) {
	if ( !framebuffer || target != 0x8d40u ) s_gl.error = 0x0501;
	if (framebuffer == s_captureFramebuffer && s_captureFace >= 0
			&& s_captureFace == s_rejectCaptureFace) return 0x8cd6u;
	return s_rejectFramebuffer ? 0x8cd6u : 0x8cd5u;
}
static void FakeClearNamedBufferData( fakeName_t buffer, fakeEnum_t format,
		fakeEnum_t channels, fakeEnum_t type, const void *value ) {
	if ( !buffer || format != 0x8236u || channels != 0x8d94u || type != 0x1405u
			|| !value || *(const uint32_t *)value ) s_gl.error = 0x0501;
	s_histogramClears++;
}
static void FakeDrawArrays( fakeEnum_t mode, fakeInt_t first, fakeSize_t count ) {
	if (s_drawFramebuffer == s_captureFramebuffer && s_captureFramebuffer) {
		if (mode != 5u || count != 4 || s_captureFace < 0 || first != s_captureFace * 4
				|| s_srgbEnabled || s_depthEnabled || s_blendEnabled || s_cullEnabled || s_scissorEnabled
				|| s_viewport[0] || s_viewport[1] || s_viewport[2] != 128 || s_viewport[3] != 128)
			s_gl.error = 0x0502;
		if (s_captureFace >= 0 && s_captureFace < 6) {
			s_captureMask |= 1u << s_captureFace;
			s_captureTimes[s_captureFace] = s_skyParams.timeHeight[0];
			s_capturePhysical[s_captureFace] = s_lastPhysicalBlend;
			memcpy(s_captureTextures[s_captureFace], s_gl.boundTextureByUnit, sizeof(s_captureTextures[0]));
		}
		s_captureDraws++; s_skyPending = qfalse; return;
	}
	if ( mode != 5u || first || count != 4 || s_drawFramebuffer == s_lastHdrFramebuffer
			|| !s_srgbEnabled || s_gl.depthWriteEnabled || s_rejectComposite ) s_gl.error = 0x0502;
	s_compositeCount++;
}
static void FakeDeleteNames( fakeSize_t count, const fakeName_t *names ) {
	(void)count; (void)names;
	for ( int n = 0; n < count; ++n )
		for ( unsigned i = 0; i < 3; ++i )
			if ( s_lensBuffers[i].name == names[n] ) s_lensBuffers[i].name = 0;
}
static void FakeDeleteBuffers( fakeSize_t count, const fakeName_t *names ) {
	for ( int i = 0; i < count; ++i ) {
		if ( !names[i] ) continue;
		if ( names[i] >= 8192u || !s_bufferLive[names[i]] ) {
			++s_bufferLifetimeErrors; continue;
		}
		s_bufferLive[names[i]] = qfalse; --s_liveBuffers;
		memset( &s_buffers[names[i]], 0, sizeof(s_buffers[0]) );
	}
	FakeDeleteNames( count, names );
}
static void FakeDeleteTextures( fakeSize_t count, const fakeName_t *names ) {
	for ( int i = 0; i < count; ++i )
		for (unsigned j = 0; j < 16; ++j) if (names[i] && names[i] == s_heightgridNames[j]) {
			s_heightgridDeletes++; s_heightgridNames[j] = 0;
			if (names[i] == s_heightgridTexture) s_heightgridTexture = 0;
		}
	for ( int i = 0; i < count; ++i ) if ( names[i] ) ++s_deletedTextures;
	FakeDeleteNames( count, names );
}
static void FakeTextureStorage2D( fakeName_t name, fakeSize_t levels,
		fakeEnum_t format, fakeSize_t width, fakeSize_t height ) {
	(void)format; if ( !name || levels <= 0 || width <= 0 || height <= 0 )
		s_gl.error = 0x0501;
}
static void FakeGenerateTextureMipmap( fakeName_t name ) {
	if ( !name ) s_gl.error = 0x0501; else s_gl.textureMipmapCount++;
}
static void FakeTextureStorage3D( fakeName_t name, fakeSize_t levels,
		fakeEnum_t format, fakeSize_t width, fakeSize_t height, fakeSize_t depth ) {
	(void)format; if ( !name || levels <= 0 || width <= 0 || height <= 0 || depth <= 0 )
		s_gl.error = 0x0501;
}
static void FakeTextureSubImage2D( fakeName_t name, fakeInt_t level,
		fakeInt_t x, fakeInt_t y, fakeSize_t width, fakeSize_t height,
		fakeEnum_t format, fakeEnum_t type, const void *pixels ) {
	(void)level; (void)x; (void)y; (void)format; (void)type;
	if ( format == 0x1903u ) {
		if ( type != 0x1406u || level || x || y || width != 2 || height != 2 || !pixels ) {
			s_gl.error = 0x0501; return;
		}
		if ( s_rejectHeightgrid ) { s_gl.error = 0x0505; return; }
		memcpy(s_heightgridSamples, pixels, sizeof(s_heightgridSamples));
		if (s_heightgridUploads >= 16) { s_gl.error = 0x0505; return; }
		s_heightgridNames[s_heightgridUploads] = name;
		s_heightgridTexture = name; s_heightgridUploads++;
	}
	if ( !name || width <= 0 || height <= 0 || !pixels ) s_gl.error = 0x0501;
	else s_gl.textureUploadCount++;
}
static void FakeTextureSubImage3D( fakeName_t name, fakeInt_t level,
		fakeInt_t x, fakeInt_t y, fakeInt_t z, fakeSize_t width, fakeSize_t height,
		fakeSize_t depth, fakeEnum_t format, fakeEnum_t type, const void *pixels ) {
	(void)level; (void)x; (void)y; (void)z; (void)format; (void)type;
	if ( !name || width <= 0 || height <= 0 || depth <= 0 || !pixels ) s_gl.error = 0x0501;
	else s_gl.textureUploadCount++;
}
static void FakeTextureParameteri( fakeName_t name, fakeEnum_t key,
		fakeInt_t value ) { (void)key; (void)value; if ( !name ) s_gl.error = 0x0501; }
static void FakeSamplerParameteri( fakeName_t name, fakeEnum_t key,
		fakeInt_t value ) { (void)key; (void)value; if ( !name ) s_gl.error = 0x0501; }
static void FakeVertexArrayVertexBuffer( fakeName_t vao, unsigned int binding,
		fakeName_t buffer, fakeIntPtr_t offset, fakeSize_t stride ) {
	(void)binding; (void)offset; if ( !vao || !buffer || stride <= 0 ) s_gl.error = 0x0501;
}
static void FakeVertexArrayElementBuffer( fakeName_t vao, fakeName_t buffer ) {
	if ( !vao || !buffer ) s_gl.error = 0x0501;
}
static void FakeEnableVertexArrayAttrib( fakeName_t vao, unsigned int location ) {
	(void)location; if ( !vao ) s_gl.error = 0x0501;
}
static void FakeVertexArrayAttribFormat( fakeName_t vao, unsigned int location,
		fakeInt_t size, fakeEnum_t type, fakeBool_t normalized,
		unsigned int offset ) {
	(void)location; (void)type; (void)normalized; (void)offset;
	if ( !vao || size <= 0 ) s_gl.error = 0x0501;
}
static void FakeVertexArrayAttribBinding( fakeName_t vao, unsigned int location,
		unsigned int binding ) { (void)location; (void)binding; if ( !vao ) s_gl.error = 0x0501; }
static void FakeBindTextureUnit( unsigned int unit, fakeName_t texture ) {
	if (s_probeTierBindings && unit >= 6u && unit <= 9u
		&& !(s_tierBindingsSeen & (1u << (unit - 6u)))) {
		s_firstTierBindings[unit - 6u] = texture;
		s_tierBindingsSeen |= 1u << (unit - 6u);
	}
	if (unit == 0u) s_currentTexture = texture;
	if ( unit < 10u ) s_gl.boundTextureByUnit[unit] = texture;
}
static void FakeBindImageTexture( unsigned int unit, fakeName_t texture,
	fakeInt_t level, fakeBool_t layered, fakeInt_t layer, fakeEnum_t access,
	fakeEnum_t format ) {
	if ( unit != 0u || !texture || level < 0 || level > 7 || layered > 1u || layer != 0 ||
		access != 0x88b9u || format != 0x881au ) s_gl.error = 0x0501;
	else s_gl.imageBindCount++;
}
static void FakeBindVertexArray( fakeName_t vao ) { s_captureVao = vao; }
static void FakeUseProgram( fakeName_t program ) {
	s_activeProgram = program;
}
static void FakeDrawElements( fakeEnum_t mode, fakeSize_t count,
		fakeEnum_t type, const void *offset ) {
	if ( s_firstDrawCount < 11u ) {
		s_firstDraws[s_firstDrawCount].source = s_blendSource;
		s_firstDraws[s_firstDrawCount].destination = s_blendDestination;
		s_firstDraws[s_firstDrawCount].offset = (uintptr_t)offset;
		s_firstDraws[s_firstDrawCount].count = count;
		s_firstDrawCount++;
	}
	if (s_skyPending) {
		memcpy(s_skyTextures, s_gl.boundTextureByUnit, sizeof(s_skyTextures));
		s_skyProgram = s_activeProgram;
		s_skyDepthWrite = s_gl.depthWriteEnabled;
		s_skyDraws++; s_skyPending = qfalse;
	}
	if (s_beamMaterialActive) {
		if (s_beamStageDraws < RENDER_MATERIAL_MAX_STAGES) {
			s_beamTextures[s_beamStageDraws] = s_currentTexture;
			memcpy(s_beamStageUv[s_beamStageDraws], s_beamMaterial + 16u, 4u * sizeof(float));
		}
		s_beamStageDraws++;
	}
	if ( s_beamUploadSeen && count == 6 ) {
		s_beamBlendSource = s_blendSource;
		s_beamBlendDestination = s_blendDestination;
	}
	(void)offset; if ( mode != 4u || count <= 0 || type != 0x1405 ) s_gl.error = 0x0501;
	else s_gl.drawCount++;
	if ( s_lastHdrFramebuffer && s_drawFramebuffer == s_lastHdrFramebuffer ) {
		if ( s_srgbEnabled || s_lastDrawUniforms[3] != 0.0f || s_lastDrawUniforms[7] != 1.0f
				|| s_lastDrawUniforms[11] != 1.0f ) s_gl.error = 0x0502;
		s_hdrSceneDraws++;
	} else if ( s_lastHdrFramebuffer && s_lastDrawUniforms[3] == 1.0f ) {
		if ( !s_compositeCount || !s_srgbEnabled ) s_gl.error = 0x0502;
		s_uiOutputDraws++;
	}
}
static void FakeDrawArraysInstanced( fakeEnum_t mode, fakeInt_t first,
		fakeSize_t count, fakeSize_t instances ) {
	if ( mode != 4u || first != 0 || count != 6 || instances <= 0 )
		s_gl.error = 0x0501;
	else s_gl.weatherDrawCount++;
}
static void FakeState( fakeEnum_t state ) { (void)state; }
static void FakeSetEnabled(fakeEnum_t state, fakeBool_t enabled) {
	switch (state) {
	case 0x8db9u: s_srgbEnabled = enabled; break;
	case 0x0b71u: s_depthEnabled = enabled; break;
	case 0x0be2u: s_blendEnabled = enabled; break;
	case 0x0b44u: s_cullEnabled = enabled; break;
	case 0x0c11u: s_scissorEnabled = enabled; break;
	default: break;
	}
}
static void FakeEnable( fakeEnum_t state ) { FakeSetEnabled(state, 1); }
static void FakeDisable( fakeEnum_t state ) { FakeSetEnabled(state, 0); }
static void FakeBlendFunc( fakeEnum_t source, fakeEnum_t destination ) {
	s_blendSource = source; s_blendDestination = destination;
}
/* TAA coverage plumbing in the world pass (attachment 1); these contract frames run without anti-aliasing, so it is never used. */
static void FakeClearBufferfv( unsigned int buffer, fakeInt_t drawBuffer, const float *value ) {
	(void)buffer; (void)drawBuffer; (void)value;
}
static unsigned int s_blendEquation1 = 0x8006u;   /* attachment 1 (TAA trust) blend equation */
static void FakeBlendEquationi( unsigned int buffer, unsigned int mode ) { if ( buffer == 1u ) s_blendEquation1 = mode; }
static void FakeColorMaski( unsigned int index, fakeBool_t r, fakeBool_t g, fakeBool_t b, fakeBool_t a ) {
	(void)index; (void)r; (void)g; (void)b; (void)a;
}
static void FakeDepthMask( fakeBool_t enabled ) {
	s_gl.depthWriteEnabled = enabled;
}
static void FakeViewport( fakeInt_t x, fakeInt_t y, fakeSize_t width,
		fakeSize_t height ) {
	if ( width < 0 || height < 0 ) s_gl.error = 0x0501;
	s_viewport[0] = x; s_viewport[1] = y; s_viewport[2] = width; s_viewport[3] = height;
}
static void FakeClearColor( float red, float green, float blue, float alpha ) {
	(void)red; (void)green; (void)blue; (void)alpha;
}
static void FakeClear( unsigned int mask ) {
	if ( mask != ( 0x00004000u | 0x00000100u )
			&& mask != 0x00000100u ) s_gl.error = 0x0501;
	/* glClear obeys the write mask; a masked clear is legal but does no work. */
	if ( mask & 0x00000100u ) {
		if ( s_gl.depthWriteEnabled ) s_gl.depthClearCount++;
		else s_gl.maskedDepthClearCount++;
	}
}
static void FakePixelStorei( fakeEnum_t name, fakeInt_t value ) {
	if ( ( name != 0x0d05u && name != 0x0cf5u ) || value != 1 ) s_gl.error = 0x0501;
}
static void FakeReadPixels( fakeInt_t x, fakeInt_t y, fakeSize_t width,
		fakeSize_t height, fakeEnum_t format, fakeEnum_t type, void *pixels ) {
	(void)x; (void)y;
	if ( width <= 0 || height <= 0 || format != 0x1907u
			|| type != 0x1401u || !pixels ) s_gl.error = 0x0501;
	else memset( pixels, 0x7f, (size_t)width * (size_t)height * 3u );
}
static void FakeFinish( void ) {}
static fakeName_t FakeCreateShader( fakeEnum_t stage ) {
	return ( stage == 0x8b31 || stage == 0x8b30 || stage == 0x91b9 )
		? ++s_gl.nextName : 0u;
}
static void FakeDispatchCompute( unsigned int x, unsigned int y,
		unsigned int z ) {
	if ( s_activeProgram < 8192u && s_atmProgram[s_activeProgram] ) {
		const unsigned pass = s_atmProgram[s_activeProgram]-1u;
		const fakeName_t params = s_ubo[pass == ATM_TILE ? 3u : 0u];
		if ( !params || params >= 8192u ) { s_gl.error = 0x0502; return; }
		memcpy( s_atm.params[pass], s_buffers[params].bytes, 192 );
		memcpy( s_atm.bindings[pass], s_ssbo, sizeof(s_ssbo) );
		s_atm.dims[pass][0] = x; s_atm.dims[pass][1] = y; s_atm.dims[pass][2] = z;
		if ( s_atm.count < ATM_PASSES ) s_atm.order[s_atm.count] = pass;
		s_atm.count++;
	}
	if ( s_weatherPending ) {
		s_weatherHeightgridBinding = s_gl.boundTextureByUnit[0];
		s_weatherPending = qfalse;
	}
	if ( !x || !y || !z ) s_gl.error = 0x0501;
	else s_gl.dispatchCount++;
	if ( s_lensCopyPending ) {
		for ( unsigned i = 0; i < 3; ++i )
			if ( s_lensBuffers[i].name == s_lensBinding )
				for ( unsigned j = 0; j < LENS_MAX_CGSOURCES; ++j )
					s_lensBuffers[i].records[j][7] = s_lensResult;
		s_lensCopyPending = qfalse; s_lensFencePending = qtrue;
	}
}
static void FakeMemoryBarrier( unsigned int barriers ) {
	if ( !( barriers & ( 0x2000u | 0x20u | 0x200u ) ) ) s_gl.error = 0x0501;
	else s_gl.barrierCount++;
}
static void *FakeFenceSync( fakeEnum_t condition, unsigned int flags ) {
	if ( condition != 0x9117u || flags ) { s_gl.error = 0x0501; return NULL; }
	if ( s_fenceIndex >= 255u ) { s_gl.error = 0x0505; return NULL; }
	unsigned i = ++s_fenceIndex;
	s_fences[i].lens = s_lensFencePending;
	s_fences[i].signaled = !s_lensFencePending || !s_holdLens;
	if ( s_lensFencePending ) s_lastLensFence = i;
	s_lensFencePending = qfalse;
	s_gl.fenceCount++;
	return &s_fences[i];
}
static fakeEnum_t FakeClientWaitSync( void *sync, unsigned int flags,
	unsigned long long timeout ) {
	(void)timeout;
	if ( !sync || ( flags != 0u && flags != 1u ) ) {
		s_gl.error = 0x0501; return 0x911du;
	}
	for ( unsigned i = 1; i <= s_fenceIndex; ++i ) if ( sync == &s_fences[i] ) {
		if ( s_fences[i].deleted || ( s_fences[i].lens && ( flags || timeout ) ) ) {
			s_gl.error = 0x0502; return 0x911du;
		}
		if ( !s_fences[i].signaled ) return 0x911bu;
	}
	s_gl.completedFenceCount++;
	return 0x911cu;
}
static void FakeDeleteSync( void *sync ) {
	if ( !sync ) s_gl.error = 0x0501;
	for ( unsigned i = 1; i <= s_fenceIndex; ++i )
		if ( sync == &s_fences[i] ) s_fences[i].deleted = qtrue;
}
static void FakeBeginQuery( fakeEnum_t target, fakeName_t query ) {
	if ( target != 0x88bfu || !query || s_gl.activeTimeElapsedQuery )
		s_gl.error = 0x0501;
	else {
		s_gl.activeTimeElapsedQuery = query;
		s_gl.queryBeginCount++;
	}
}
static void FakeEndQuery( fakeEnum_t target ) {
	if ( target != 0x88bfu || !s_gl.activeTimeElapsedQuery )
		s_gl.error = 0x0501;
	s_gl.activeTimeElapsedQuery = 0u;
}
static void FakeGetQueryObjectui64v( fakeName_t query, fakeEnum_t name,
		fakeUInt64_t *value ) {
	if ( !query || name != 0x8866u || !value ) s_gl.error = 0x0501;
	else {
		*value = 250000u;
		s_gl.queryResultCount++;
	}
}
static unsigned s_toneMode;
static float s_toneExposure;
static void FakeShaderSource( fakeName_t shader, fakeSize_t count,
		const char *const *source, const fakeInt_t *length ) {
	if ( shader < 8192u && source && source[0] ) {
		const char *tokens[] = { "readonly buffer TileDepth", "uvec4 gridVolumeCount;",
			"uvec4 gridLightCount;", "vec4 layerErosion;", "uvec4 gridHistory;" };
		for ( unsigned p = 0; p < ATM_PASSES; ++p )
			if ( strstr(source[0], tokens[p]) ) s_atmShader[shader] = p+1u;
	}
	if ( !shader || (count != 1 && count != 3) || !source || !source[0]
			|| !length || length[0] <= 0 ) s_gl.error = 0x0501;
	if ( count == 3 && (!source[1] || length[1] <= 0
			|| !source[2] || length[2] <= 0) ) s_gl.error = 0x0501;
	if ( count == 3 && source[1] && strstr( source[1], "WIRED_SPEC_17" )
			&& sscanf( source[1], "#define WIRED_SPEC_17 %u\n#define WIRED_SPEC_18 %f",
				&s_toneMode, &s_toneExposure ) != 2 ) s_gl.error = 0x0501;
}
static void FakeCompileShader( fakeName_t shader ) { if ( !shader ) s_gl.error = 0x0501; }
static void FakeGetCompileStatus( fakeName_t name, fakeEnum_t key, fakeInt_t *status ) {
	if ( !name || ( key != 0x8b81 && key != 0x8b82 ) ) { *status = 0; s_gl.error = 0x0501; }
	else *status = 1;
}
static void FakeDeleteShader( fakeName_t shader ) { (void)shader; }
static fakeName_t FakeCreateProgram( void ) { return ++s_gl.nextName; }
static void FakeAttachShader( fakeName_t program, fakeName_t shader ) {
	if ( !program || !shader ) s_gl.error = 0x0501;
	if ( program < 8192u && shader < 8192u && s_atmShader[shader] )
		s_atmProgram[program] = s_atmShader[shader];
}
static void FakeLinkProgram( fakeName_t program ) { if ( !program ) s_gl.error = 0x0501; }
static void FakeDeleteProgram( fakeName_t program ) { (void)program; }

#define RETURN_PROC( functionName ) return (ralOpenGlProc_t)( functionName )
static ralOpenGlProc_t FakeResolve( const char *name, void *userData ) {
	(void)userData;
	if ( !strcmp( name, "glGetIntegerv" ) ) RETURN_PROC( FakeGetIntegerv );
	if ( !strcmp( name, "glCreateFramebuffers" ) ) RETURN_PROC( FakeCreateNames );
	if ( !strcmp( name, "glDeleteFramebuffers" ) ) RETURN_PROC( FakeDeleteNames );
	if ( !strcmp( name, "glBindFramebuffer" ) ) RETURN_PROC( FakeBindFramebuffer );
	if ( !strcmp( name, "glNamedFramebufferTexture" ) ) RETURN_PROC( FakeNamedFramebufferTexture );
	if ( !strcmp( name, "glNamedFramebufferTextureLayer" ) ) RETURN_PROC( FakeNamedFramebufferTextureLayer );
	if ( !strcmp( name, "glCheckNamedFramebufferStatus" ) ) RETURN_PROC( FakeCheckNamedFramebufferStatus );
	if ( !strcmp( name, "glClearNamedBufferData" ) ) RETURN_PROC( FakeClearNamedBufferData );
	if ( !strcmp( name, "glDrawArrays" ) ) RETURN_PROC( FakeDrawArrays );
	if ( !strcmp( name, "glGetString" ) ) RETURN_PROC( FakeGetString );
	if ( !strcmp( name, "glGetError" ) ) RETURN_PROC( FakeGetError );
	if ( !strcmp( name, "glClearBufferfv" ) ) RETURN_PROC( FakeClearBufferfv );
	if ( !strcmp( name, "glColorMaski" ) ) RETURN_PROC( FakeColorMaski );
	if ( !strcmp( name, "glBlendEquationi" ) ) RETURN_PROC( FakeBlendEquationi );
	if ( !strcmp( name, "glCreateBuffers" ) ) RETURN_PROC( FakeCreateBuffers );
	if ( !strcmp( name, "glNamedBufferStorage" ) ) RETURN_PROC( FakeNamedBufferStorage );
	if ( !strcmp( name, "glNamedBufferSubData" ) ) RETURN_PROC( FakeNamedBufferSubData );
	if ( !strcmp( name, "glGetNamedBufferSubData" ) ) RETURN_PROC( FakeGetNamedBufferSubData );
	if ( !strcmp( name, "glCopyTextureSubImage2D" ) ) RETURN_PROC( FakeCopyTextureSubImage2D );
	if ( !strcmp( name, "glBindBufferBase" ) ) RETURN_PROC( FakeBindBufferBase );
	if ( !strcmp( name, "glMapNamedBufferRange" ) ) RETURN_PROC( FakeMapNamedBufferRange );
	if ( !strcmp( name, "glUnmapNamedBuffer" ) ) RETURN_PROC( FakeUnmapNamedBuffer );
	if ( !strcmp( name, "glCopyNamedBufferSubData" ) ) RETURN_PROC( FakeCopyNamedBufferSubData );
	if ( !strcmp( name, "glDeleteBuffers" ) ) RETURN_PROC( FakeDeleteBuffers );
	if ( !strcmp( name, "glCreateTextures" ) ) RETURN_PROC( FakeCreateTextures );
	if ( !strcmp( name, "glTextureStorage2D" ) ) RETURN_PROC( FakeTextureStorage2D );
	if ( !strcmp( name, "glTextureStorage3D" ) ) RETURN_PROC( FakeTextureStorage3D );
	if ( !strcmp( name, "glTextureSubImage2D" ) ) RETURN_PROC( FakeTextureSubImage2D );
	if ( !strcmp( name, "glGenerateTextureMipmap" ) ) RETURN_PROC( FakeGenerateTextureMipmap );
	if ( !strcmp( name, "glTextureSubImage3D" ) ) RETURN_PROC( FakeTextureSubImage3D );
	if ( !strcmp( name, "glTextureParameteri" ) ) RETURN_PROC( FakeTextureParameteri );
	if ( !strcmp( name, "glDeleteTextures" ) ) RETURN_PROC( FakeDeleteTextures );
	if ( !strcmp( name, "glCreateSamplers" ) ) RETURN_PROC( FakeCreateNames );
	if ( !strcmp( name, "glSamplerParameteri" ) ) RETURN_PROC( FakeSamplerParameteri );
	if ( !strcmp( name, "glDeleteSamplers" ) ) RETURN_PROC( FakeDeleteNames );
	if ( !strcmp( name, "glFinish" ) ) RETURN_PROC( FakeFinish );
	if ( !strcmp( name, "glCreateVertexArrays" ) ) RETURN_PROC( FakeCreateNames );
	if ( !strcmp( name, "glDeleteVertexArrays" ) ) RETURN_PROC( FakeDeleteNames );
	if ( !strcmp( name, "glVertexArrayVertexBuffer" ) ) RETURN_PROC( FakeVertexArrayVertexBuffer );
	if ( !strcmp( name, "glVertexArrayElementBuffer" ) ) RETURN_PROC( FakeVertexArrayElementBuffer );
	if ( !strcmp( name, "glEnableVertexArrayAttrib" ) ) RETURN_PROC( FakeEnableVertexArrayAttrib );
	if ( !strcmp( name, "glVertexArrayAttribFormat" ) ) RETURN_PROC( FakeVertexArrayAttribFormat );
	if ( !strcmp( name, "glVertexArrayAttribBinding" ) ) RETURN_PROC( FakeVertexArrayAttribBinding );
	if ( !strcmp( name, "glBindTextureUnit" ) ) RETURN_PROC( FakeBindTextureUnit );
	if ( !strcmp( name, "glBindImageTexture" ) ) RETURN_PROC( FakeBindImageTexture );
	if ( !strcmp( name, "glBindVertexArray" ) ) RETURN_PROC( FakeBindVertexArray );
	if ( !strcmp( name, "glUseProgram" ) ) RETURN_PROC( FakeUseProgram );
	if ( !strcmp( name, "glDrawElements" ) ) RETURN_PROC( FakeDrawElements );
	if ( !strcmp( name, "glDrawArraysInstanced" ) )
		RETURN_PROC( FakeDrawArraysInstanced );
	if ( !strcmp( name, "glIsEnabled" ) ) RETURN_PROC( FakeIsEnabled );
	if ( !strcmp( name, "glGetFramebufferAttachmentParameteriv" ) ) RETURN_PROC( FakeGetFramebufferAttachmentParameteriv );
	if ( !strcmp( name, "glEnable" ) ) RETURN_PROC( FakeEnable );
	if ( !strcmp( name, "glDisable" ) ) RETURN_PROC( FakeDisable );
	if ( !strcmp( name, "glBlendFunc" ) ) RETURN_PROC( FakeBlendFunc );
	if ( !strcmp( name, "glDepthMask" ) ) RETURN_PROC( FakeDepthMask );
	if ( !strcmp( name, "glDepthFunc" ) ) RETURN_PROC( FakeState );
	if ( !strcmp( name, "glViewport" ) ) RETURN_PROC( FakeViewport );
	if ( !strcmp( name, "glClearColor" ) ) RETURN_PROC( FakeClearColor );
	if ( !strcmp( name, "glClear" ) ) RETURN_PROC( FakeClear );
	if ( !strcmp( name, "glPixelStorei" ) ) RETURN_PROC( FakePixelStorei );
	if ( !strcmp( name, "glReadPixels" ) ) RETURN_PROC( FakeReadPixels );
	if ( !strcmp( name, "glCreateShader" ) ) RETURN_PROC( FakeCreateShader );
	if ( !strcmp( name, "glShaderSource" ) ) RETURN_PROC( FakeShaderSource );
	if ( !strcmp( name, "glCompileShader" ) ) RETURN_PROC( FakeCompileShader );
	if ( !strcmp( name, "glGetShaderiv" ) ) RETURN_PROC( FakeGetCompileStatus );
	if ( !strcmp( name, "glDeleteShader" ) ) RETURN_PROC( FakeDeleteShader );
	if ( !strcmp( name, "glCreateProgram" ) ) RETURN_PROC( FakeCreateProgram );
	if ( !strcmp( name, "glAttachShader" ) ) RETURN_PROC( FakeAttachShader );
	if ( !strcmp( name, "glLinkProgram" ) ) RETURN_PROC( FakeLinkProgram );
	if ( !strcmp( name, "glGetProgramiv" ) ) RETURN_PROC( FakeGetCompileStatus );
	if ( !strcmp( name, "glDeleteProgram" ) ) RETURN_PROC( FakeDeleteProgram );
	if ( !strcmp( name, "glDispatchCompute" ) ) RETURN_PROC( FakeDispatchCompute );
	if ( !strcmp( name, "glMemoryBarrier" ) ) RETURN_PROC( FakeMemoryBarrier );
	if ( !strcmp( name, "glFenceSync" ) ) RETURN_PROC( FakeFenceSync );
	if ( !strcmp( name, "glClientWaitSync" ) ) RETURN_PROC( FakeClientWaitSync );
	if ( !strcmp( name, "glDeleteSync" ) ) RETURN_PROC( FakeDeleteSync );
	if ( !strcmp( name, "glGenQueries" ) ) RETURN_PROC( FakeCreateNames );
	if ( !strcmp( name, "glDeleteQueries" ) ) RETURN_PROC( FakeDeleteNames );
	if ( !strcmp( name, "glBeginQuery" ) ) RETURN_PROC( FakeBeginQuery );
	if ( !strcmp( name, "glEndQuery" ) ) RETURN_PROC( FakeEndQuery );
	if ( !strcmp( name, "glGetQueryObjectui64v" ) )
		RETURN_PROC( FakeGetQueryObjectui64v );
	return NULL;
}
#undef RETURN_PROC

static int LensFrame( renderSubmissionState_t *frontend, ralOpenGlProduct_t *product,
		refdef_t *view, uint64_t frame, int id, qboolean success,
		ralOpenGlProductFrameReceipt_t *receipt ) {
	renderSubmissionReceipt_t submission;
	lensSourceDesc_t source = { 0 };
	source.id = id; source.origin[0] = 8.0f; source.radius = 1.0f;
	CHECK( RenderSubmission_BeginFrame( frontend, frame ) );
	if ( id >= 0 ) CHECK( RenderSubmission_AddLensSource( frontend, &source ) );
	CHECK( RenderSubmission_RenderScene( frontend, view, 0 ) );
	CHECK( RenderSubmission_EndFrame( frontend, frame, &submission ) );
	const qboolean rendered = RalOpenGl_ProductRender( product, frontend, &submission, frame, receipt );
	if ( rendered != success ) fprintf( stderr, "frame=%llu expected=%d error=%x live=%u lifetime-errors=%u\n",
		(unsigned long long)frame, success, s_gl.error, s_liveBuffers, s_bufferLifetimeErrors );
	CHECK( rendered == success );
	return 0;
}

static int CheckAtmosphereAllocationLifecycle( ralOpenGlCore_t *core,
		const ralOpenGlCoreReceipt_t *coreReceipt, const mapFile_t *map,
		qboolean weather, qboolean recover ) {
	static renderSubmissionState_t frontend;
	ralOpenGlProduct_t *product = NULL;
	ralOpenGlProductFrameReceipt_t receipt, untouched;
	ralAtmosphereFixtureState_t fixture;
	refdef_t view = {0};
	const unsigned baseline = s_liveBuffers;
	const unsigned count = weather ? 3u : 11u;
	uint64_t frame = 1000u;
	CHECK( RalOpenGl_ProductCreate( core, coreReceipt, frame, &product ) );
	CHECK( RalOpenGl_ProductSetOutputExtent( product, 1280, 720 ) );
	CHECK( RenderSubmission_Init( &frontend, frame ) );
	const byte pixel[4] = {0, 255, 0, 255};
	CHECK( RenderSubmission_RegisterMaterialImage( &frontend, RENDER_ASSET_MATERIAL,
		"textures/green", qtrue, pixel, 1, 1 ) > 0 );
	CHECK( RenderSubmission_LoadWorld( &frontend, map, 0 ) );
	CHECK( Ral_AtmosphereConformanceFixture( RAL_ATMOSPHERE_FIXTURE_FOG, &fixture ) );
	view.width = 1280; view.height = 720; view.fov_x = 90; view.fov_y = 70;
	view.rdflags = RDF_NOWORLDMODEL;
	view.viewaxis[0][0] = view.viewaxis[1][1] = view.viewaxis[2][2] = 1;
	/* Warm unrelated render resources. Atmosphere remains unallocated in the
	 * analytic case; weather remains unallocated without precipitation. */
	fixture.state.qualityTier = weather ? ATMOSPHERE_QUALITY_FULL : ATMOSPHERE_QUALITY_ANALYTIC;
	CHECK( RenderSubmission_SetAtmosphere( &frontend, &fixture.state ) );
	CHECK( !LensFrame( &frontend, product, &view, ++frame, -1, qtrue, &receipt ) );
	fixture.state.qualityTier = ATMOSPHERE_QUALITY_FULL;
	if ( weather ) fixture.state.precipitation[0] = 0.5f;
	CHECK( RenderSubmission_SetAtmosphere( &frontend, &fixture.state ) );
	qboolean existing[8192];
	memcpy( existing, s_bufferLive, sizeof(existing) );
	const unsigned warmLive = s_liveBuffers;
	/* Fail every returned-name slot and every immutable-storage allocation.
	 * Consecutive rejected frames must not accumulate orphaned GL names. */
	for ( unsigned kind = 0; kind < 2; ++kind ) for ( unsigned slot = 0; slot < count; ++slot ) {
		memset( &s_bufferFault, 0, sizeof(s_bufferFault) );
		s_bufferFault.count = count;
		s_bufferFault.createIndex = kind == 0 ? (int)slot : -1;
		s_bufferFault.storageIndex = kind == 1 ? (int)slot : -1;
		memset( &receipt, 0xa5, sizeof(receipt) ); untouched = receipt;
		CHECK( !LensFrame( &frontend, product, &view, ++frame, -1, qfalse, &receipt ) );
		CHECK( s_bufferFault.attempts == 1 && s_bufferFault.failures == 1 );
		CHECK( !memcmp( &receipt, &untouched, sizeof(receipt) ) );
		CHECK( s_liveBuffers == warmLive );
		CHECK( !memcmp( existing, s_bufferLive, sizeof(existing) ) );
		CHECK( !s_gl.error && !s_bufferLifetimeErrors );
		memset( &s_bufferFault, 0, sizeof(s_bufferFault) );
	}
	if ( recover ) {
		CHECK( !LensFrame( &frontend, product, &view, ++frame, -1, qtrue, &receipt ) );
		CHECK( s_liveBuffers == warmLive + count );
		CHECK( receipt.atmosphereNativeDispatchCount == 4u );
		CHECK( receipt.weatherDispatchCount == (weather ? 1u : 0u) );
		const unsigned createCalls = s_bufferCreateCalls[count];
		memcpy( existing, s_bufferLive, sizeof(existing) );
		CHECK( !LensFrame( &frontend, product, &view, ++frame, -1, qtrue, &receipt ) );
		CHECK( s_bufferCreateCalls[count] == createCalls && s_liveBuffers == warmLive + count );
		CHECK( !memcmp( existing, s_bufferLive, sizeof(existing) ) );
	}
	/* Also exercise shutdown directly after failed initialization, not just
	 * recovery; fallback and other pre-existing resources retain one owner. */
	RenderSubmission_Reset( &frontend );
	RalOpenGl_ProductDestroy( product );
	CHECK( s_liveBuffers == baseline && !s_bufferLifetimeErrors && !s_gl.error );
	return 0;
}

int main( void ) {
	/* The bounded multi-world fixture exceeds the macOS main-thread stack. */
	static renderSubmissionState_t frontend;
	atmosphereFrameState_t atmosphere;
	renderSubmissionReceipt_t submission, staleSubmission;
	ralOpenGlCore_t *coreOwner = NULL;
	ralOpenGlCoreReceipt_t core;
	ralOpenGlCoreCreateInfo_t coreInfo;
	ralOpenGlFrontendPlanReceipt_t plan, before, exact;
	ralOpenGlWorldLowerInfo_t lowerInfo;
	ralOpenGlWorldReceipt_t worldReceipt, worldExact;
	ralOpenGlLighting_t *lighting = NULL;
	ralOpenGlLightingReceipt_t lightingReceipt, lightingExact;
	ralOpenGlProduct_t *product = NULL;
	ralOpenGlProductFrameReceipt_t productReceipt, productExact;
	ralAtmospherePlanRequest_t atmosphereRequest;
	ralSkyEnvironmentPlanRequest_t distantSkyRequest;
	ralSkyEnvironmentPlanReceipt_t distantSkyReceipt;
	renderDistantWorldDrawMetric_t distantMetrics[1];
	ralSkyDistantDrawCandidate_t distantCandidates[1];
	renderDistantWorldSubmission_t distantSubmission;
	ralSkyDistantDrawPlanReceipt_t distantDrawPlan;
	uint32_t distantMetricCount = 0u;
	byte readback[12];
	mapFile_t map, distantMap;
	dsurface_t surface;
	drawVert_t vertices[3];
	int indices[3] = { 0, 1, 2 };
	dshader_t shader;
	refdef_t view;
	refEntity_t modelEntity, spriteEntity;
	spriteDesc_t effectSprite;
	decalDesc_t effectDecal;
	ribbonDesc_t effectRibbon;
	ribbonPoint_t effectRibbonPoints[2];
	beamDesc_t effectBeam;
	renderEffectBeamPool_t beamPool, beamPoolCandidate;
	renderEffectBeamPoolReceipt_t beamPoolReceipt;
	renderEffectDecalPool_t decalPool, decalPoolCandidate;
	renderEffectDecalPoolReceipt_t decalPoolReceipt;
	refEntityMotion_t modelMotion;
	particleClass_t breathClass;
	atmosphereEffectProfile_t breathProfile;
	atmosphereEmitter_t breathEmitter;
	atmosphereMediaVolume_t media;
	qhandle_t material, msdfMaterial, model;
	uint64_t distantIdentity = 0u;
	uint32_t residentMaterialTextures[2] = { 7001u, 7002u };
	const byte green[16] = {
		0u, 255u, 0u, 255u, 0u, 255u, 0u, 255u,
		0u, 255u, 0u, 255u, 0u, 255u, 0u, 255u
	};
	memset( &s_gl, 0, sizeof( s_gl ) ); s_gl.nextName = 10u;
	memset( &coreInfo, 0, sizeof( coreInfo ) );
	coreInfo.generation = 71u; coreInfo.contextIdentity = 0x7102u;
	coreInfo.resolveProc = FakeResolve;
	CHECK( RalOpenGl_CoreCreate( &coreInfo, &coreOwner, &core ) );
	{
		ralLightingProductRequest_t request;
		ralLightingArtifactReceipt_t artifact;
		ralLightingRuntimePlan_t runtimePlan;
		ralStaticLightingCapabilities_t capabilities = { qtrue, qtrue, qtrue, qtrue };
		const ralLightVec3Q16_t radiance[2] = {
			{ 4 * RAL_LIGHT_Q16_ONE, RAL_LIGHT_Q16_ONE, 0 },
			{ RAL_LIGHT_Q16_ONE, 2 * RAL_LIGHT_Q16_ONE, 3 * RAL_LIGHT_Q16_ONE }
		};
		const ralLightVec3Q16_t direction[2] = {
			{ 0, 0, RAL_LIGHT_Q16_ONE }, { RAL_LIGHT_Q16_ONE, 0, RAL_LIGHT_Q16_ONE }
		};
		const uint8_t visibility[2] = { 255u, 128u };
		uint8_t radianceBytes[16], directionBytes[8], artifactBytes[512];
		memset( &request, 0, sizeof( request ) );
		request.schemaVersion = RAL_LIGHTING_PRODUCT_SCHEMA_VERSION;
		request.artifactGeneration = 7u;
		request.bake.schemaVersion = RAL_LIGHTING_BAKE_RECEIPT_SCHEMA_VERSION;
		request.bake.bakeGeneration = 1u;
		request.bake.staticIndirectKey = 2u;
		request.bake.producerVersion = 3u;
		request.bake.radianceHash = 4u;
		request.bake.directionHash = 5u;
		request.bake.patchCount = 2u;
		request.bake.linkCount = 1u;
		request.bake.completedBounces = 4u;
		request.bake.ready = qtrue;
		request.encoding = RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;
		request.pageWidth = request.pageHeight = 1u;
		request.pageCount = request.texelCount = 2u;
		request.indirectRadiance = radiance;
		request.dominantDirection = direction;
		request.stationaryVisibility = visibility;
		request.stationaryVisibilityCount = 2u;
		CHECK( Ral_LightingProductWrite( &request, radianceBytes, sizeof( radianceBytes ), directionBytes,
										 sizeof( directionBytes ), artifactBytes, sizeof( artifactBytes ), &artifact ) );
		CHECK( Ral_LightingRuntimePlanBuild( RAL_BACKEND_OPENGL, 8u, artifactBytes, artifact.byteLength, &artifact,
											&capabilities, &runtimePlan ) );
		CHECK( RalOpenGl_LightingUpload( coreOwner, &core, artifactBytes, artifact.byteLength, &runtimePlan, &lighting,
										&lightingReceipt ) && lightingReceipt.plan.planeCount == 3u &&
			   s_gl.textureUploadCount == 3u );
		lightingExact = lightingReceipt;
		CHECK( RalOpenGl_LightingReceiptExact( &lightingReceipt, &lightingExact ) );
		s_gl.textureUploadCount = 0u;
	}
	CHECK( RalOpenGl_ProductCreate( coreOwner, &core, 72u, &product ) );
	CHECK( RalOpenGl_ProductSetDirectionalLighting( product, &lightingReceipt ) );
	CHECK( RalOpenGl_ProductSetOutputExtent( product, 2u, 2u )
		&& RalOpenGl_ProductReadbackRgb( product, readback,
			(uint32_t)sizeof( readback ) )
		&& readback[0] == 0x7f
		&& RalOpenGl_ProductSetOutputExtent( product, 1280u, 720u ) );
	CHECK( RalOpenGl_CoreReceiptExact( &core, &core ) );
	CHECK( RenderSubmission_Init( &frontend, 71u ) );
	material = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_MATERIAL, "textures/green", qtrue, green, 2u, 2u );
	CHECK( material > 0 );
	memset( &breathClass, 0, sizeof( breathClass ) );
	breathClass.shader = material;
	breathClass.emitMode = EMIT_POINT;
	breathClass.scatterShape = SCATTER_SPHERE;
	breathClass.velocityShape = VEL_AXIAL_PLUS_CUBE;
	breathClass.scatterMagnitude = 1.5f;
	breathClass.axialSpeed = 8.0f;
	breathClass.cubeJitter = 1.0f;
	breathClass.lifetimeMean = 1.0f;
	breathClass.paletteCount = 1;
	breathClass.colorPalette[0][0] = 0.82f;
	breathClass.colorPalette[0][1] = 0.90f;
	breathClass.colorPalette[0][2] = 1.0f;
	breathClass.colorPalette[0][3] = 0.38f;
	breathClass.sizeStart = 1.0f;
	breathClass.sizeEnd = 8.0f;
	breathClass.drag = 0.8f;
	CHECK( RenderSubmission_RegisterParticleClass( &frontend, 1, &breathClass ) );
	memset( &breathProfile, 0, sizeof( breathProfile ) );
	breathProfile.schemaVersion = WIRED_ATMOSPHERE_EFFECT_PROFILE_SCHEMA_VERSION;
	breathProfile.stageCount = 1u;
	breathProfile.maxParticles = 24u;
	breathProfile.seed = 216u;
	breathProfile.duration = 1.0f;
	breathProfile.lodFar = 512.0f;
	breathProfile.boundsRadius = 32.0f;
	breathProfile.stages[0].trigger = ATMOSPHERE_STAGE_CONTINUOUS;
	breathProfile.stages[0].particleClass = 1u;
	breathProfile.stages[0].parentStage = UINT32_MAX;
	breathProfile.stages[0].maxParticles = 24u;
	breathProfile.stages[0].spawnRate = 24.0f;
	breathProfile.stages[0].duration = 1.0f;
	breathProfile.stages[0].lodFar = 512.0f;
	breathProfile.stages[0].boundsRadius = 32.0f;
	breathProfile.stages[0].intensityScale = 1.0f;
	CHECK( RenderSubmission_RegisterAtmosphereEffectProfile( &frontend, 1u,
		&breathProfile ) );
	memset( &map, 0, sizeof( map ) );
	memset( &surface, 0, sizeof( surface ) );
	memset( vertices, 0, sizeof( vertices ) );
	memset( &shader, 0, sizeof( shader ) );
	strcpy( map.name, "maps/opengl-plan.bsp" );
	map.checksum = 0x12345678;
	map.numSurfaces = 1; map.surfaces = &surface;
	map.numDrawVerts = 3; map.drawVerts = vertices;
	map.numDrawIndexes = 3; map.drawIndexes = indices;
	map.numShaders = 1; map.shaders = &shader;
	strcpy( shader.shader, "textures/green" );
	surface.surfaceType = MST_PLANAR; surface.numVerts = 3;
	surface.numIndexes = 3; surface.lightmapNum = 0;
	vertices[1].xyz[0] = 1.0f; vertices[2].xyz[1] = 1.0f;
	CHECK( RenderSubmission_LoadWorld( &frontend, &map, 0 ) );
	distantMap = map;
	strcpy( distantMap.name, "maps/opengl-distant-plan.bsp" );
	distantMap.checksum = 0x87654321;
	CHECK( RenderSubmission_LoadWorld( &frontend, &distantMap,
		RENDER_DISTANT_WORLD_BASE )
		&& RenderSubmission_WorldContentIdentity( &frontend,
			RENDER_DISTANT_WORLD_BASE, &distantIdentity )
		&& distantIdentity != 0u
		&& RenderSubmission_SelectWorld( &frontend, 0 ) );
	memset( &atmosphere, 0, sizeof( atmosphere ) );
	atmosphere.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	atmosphere.flags = ATMOSPHERE_FLAG_ENABLED
		| ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA | ATMOSPHERE_FLAG_SKY_LIGHTING;
	atmosphere.qualityTier = ATMOSPHERE_QUALITY_FULL;
	atmosphere.seed = 216u;
	atmosphere.bounds[0] = atmosphere.bounds[1] = atmosphere.bounds[2] = -64.0f;
	atmosphere.bounds[3] = atmosphere.bounds[4] = atmosphere.bounds[5] = 64.0f;
	atmosphere.temperatureC = -8.0f;
	atmosphere.humidity = 0.9f;
	atmosphere.indoorExposure = 1.0f;
	atmosphere.ambientColor[0] = 0.12f;
	atmosphere.ambientColor[1] = 0.16f;
	atmosphere.ambientColor[2] = 0.22f;
	atmosphere.mediaDensity = 0.1f;
	atmosphere.mediaHeightFalloff = 0.01f;
	atmosphere.cloudCover = 0.65f;
	atmosphere.cloudShadow = 0.4f;
	atmosphere.precipitation[1] = 0.5f;
	atmosphere.precipitation[2] = 0.5f;
	atmosphere.skyEnvironment.schemaVersion = WIRED_SKY_ENVIRONMENT_SCHEMA_VERSION;
	atmosphere.skyEnvironment.mode = ATMOSPHERE_SKY_AUTHORED;
	atmosphere.skyEnvironment.flags = ATMOSPHERE_SKY_AUTHORED_AVAILABLE
		| ATMOSPHERE_SKY_DISTANT_SCENE;
	atmosphere.skyEnvironment.sourceRevision = 1u;
	atmosphere.skyEnvironment.authoredSkyIdentity = 1u;
	atmosphere.skyEnvironment.distantSceneIdentity = distantIdentity;
	atmosphere.skyEnvironment.distantSceneScale = 16.0f;
	atmosphere.skyEnvironment.distantSceneLodBias = 1.0f;
	CHECK( RenderSubmission_SetAtmosphere( &frontend, &atmosphere ) );
	model = RenderSubmission_RegisterInlineModel( &frontend, "*1", 0u, 1u );
	CHECK( model > 0 );
	/* A loaded world can coexist with a UI-only loading frame before the first
	 * RenderScene call. That frame is presentable but owns no world draws. */
	CHECK( RenderSubmission_BeginFrame( &frontend, 70u ) );
	CHECK( RenderSubmission_AddUiQuad( &frontend, 0.0f, 0.0f, 1280.0f, 720.0f,
		0.0f, 0.0f, 1.0f, 1.0f, 0.0f, material ) );
	CHECK( RenderSubmission_EndFrame( &frontend, 70u, &submission ) );
	CHECK( submission.worldLoaded && !submission.sceneRendered );
	CHECK( RalOpenGl_FrontendPlanBuild( &core, &frontend, &submission, 71u,
		NULL, NULL, NULL, NULL, &plan ) );
	CHECK( plan.loweredWorldVertexCount == 0u
		&& plan.loweredWorldIndexCount == 0u
		&& plan.loweredWorldBatchCount == 0u
		&& plan.loweredUiPrimitiveCount == 1u
		&& plan.unresolvedCount == 0u );
	CHECK( RenderSubmission_BeginFrame( &frontend, 72u ) );
	memset( &view, 0, sizeof( view ) );
	view.width = 1280; view.height = 720; view.fov_x = 90.0f; view.fov_y = 60.0f;
	view.viewaxis[0][0] = view.viewaxis[1][1] = view.viewaxis[2][2] = 1.0f;
	CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
	memset( &modelEntity, 0, sizeof( modelEntity ) );
	modelEntity.reType = RT_MODEL; modelEntity.hModel = model;
	modelEntity.axis[0][0] = modelEntity.axis[1][1] = modelEntity.axis[2][2] = 1.0f;
	memset( modelEntity.shader.rgba, 255, sizeof( modelEntity.shader.rgba ) );
	CHECK( RenderSubmission_AddEntity( &frontend, &modelEntity, NULL ) );
	{
		ralIrradianceEntitySampleReceipt_t local = LocalIrradiance( 72u, 1u );
		CHECK( RenderSubmission_AttachEntityIrradiance( &frontend, 0u, &local ) );
	}
	memset( &spriteEntity, 0, sizeof( spriteEntity ) );
	spriteEntity.reType = RT_SPRITE; spriteEntity.radius = 1.0f;
	spriteEntity.customShader = material;
	memset( spriteEntity.shader.rgba, 255, sizeof( spriteEntity.shader.rgba ) );
	CHECK( RenderSubmission_AddEntity( &frontend, &spriteEntity, NULL ) );
	CHECK( RenderSubmission_AddUiQuad( &frontend, 0.0f, 0.0f, 1280.0f, 720.0f,
		0.0f, 0.0f, 1.0f, 1.0f, 0.0f, material ) );
	CHECK( RenderSubmission_EndFrame( &frontend, 72u, &submission ) );
	CHECK( RalOpenGl_FrontendPlanBuild( &core, &frontend, &submission, 73u,
		NULL, NULL, NULL, NULL, &plan ) );
	CHECK( plan.loweredMaterialCount == 1u
		&& plan.loweredWorldVertexCount == 3u
		&& plan.loweredWorldIndexCount == 3u
		&& plan.loweredWorldBatchCount == 1u
		&& plan.texturedWorldBatchCount == 1u
		&& plan.loweredModelEntityCount == 1u
		&& plan.loweredLocalIrradianceEntityCount == 1u
		&& plan.loweredPrimitiveEntityCount == 1u
		&& plan.loweredEntityIndexCount == 9u
		&& plan.loweredEntityBatchCount == 2u
		&& plan.loweredUiPrimitiveCount == 1u
		&& plan.unresolvedCount == 0u );
	exact = plan;
	CHECK( RalOpenGl_FrontendPlanReceiptExact( &plan, &exact ) );
	exact.loweringDigest++;
	CHECK( !RalOpenGl_FrontendPlanReceiptExact( &plan, &exact ) );
	memset( &plan, 0xa5, sizeof( plan ) ); before = plan;
	staleSubmission = submission; staleSubmission.frameDigest++;
	CHECK( !RalOpenGl_FrontendPlanBuild( &core, &frontend, &staleSubmission,
		74u, NULL, NULL, NULL, NULL, &plan )
		&& !memcmp( &plan, &before, sizeof( plan ) ) );
	msdfMaterial = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_MSDF, "fonts/opengl-msdf", qtrue, green, 2u, 2u );
	CHECK( msdfMaterial > 0 );
	CHECK( RenderSubmission_BeginFrame( &frontend, 75u ) );
	memset( &breathEmitter, 0, sizeof( breathEmitter ) );
	breathEmitter.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	breathEmitter.kind = ATMOSPHERE_EMITTER_BREATH;
	breathEmitter.id = 1u;
	breathEmitter.seed = 216u;
	breathEmitter.profile = 1u;
	breathEmitter.intensity = 1.0f;
	breathEmitter.direction[0] = 1.0f;
	breathEmitter.radius = 8.0f;
	breathEmitter.temperatureC = 37.0f;
	breathEmitter.humidity = 1.0f;
	CHECK( RenderSubmission_AddAtmosphereEmitter( &frontend, &breathEmitter ) );
	CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
	memset( &modelMotion, 0, sizeof( modelMotion ) );
	modelMotion.structSize = sizeof( modelMotion );
	modelMotion.version = REF_ENTITY_MOTION_VERSION;
	modelMotion.ownerId = 1u; modelMotion.generation = 75u;
	modelMotion.role = REF_ENTITY_MOTION_ROLE_PLAYER_BODY;
	CHECK( RenderSubmission_AddEntity( &frontend, &modelEntity, &modelMotion ) );
	{
		ralIrradianceEntitySampleReceipt_t local = LocalIrradiance( 75u, 1u );
		CHECK( RenderSubmission_AttachEntityIrradiance( &frontend, 0u, &local ) );
	}
	CHECK( RenderSubmission_AddEntity( &frontend, &spriteEntity, NULL ) );
	CHECK( RenderSubmission_AddUiQuad( &frontend, 0.0f, 0.0f, 1280.0f, 720.0f,
		0.0f, 0.0f, 1.0f, 1.0f, 15.0f, material ) );
	CHECK( RenderSubmission_AddUiLine( &frontend, 10.0f, 20.0f, 200.0f, 220.0f,
		2.0f, msdfMaterial ) );
	{
		polyVert_t poly[3];
		memset( poly, 0, sizeof( poly ) );
		CHECK( RenderSubmission_AddPoly( &frontend, material, 3, poly, 1 ) );
	}
	CHECK( RenderSubmission_AddLight( &frontend, spriteEntity.origin, NULL,
		64.0f, 1.0f, 0.5f, 0.25f ) );
	memset( &effectSprite, 0, sizeof( effectSprite ) );
	effectSprite.origin[0] = 8.0f;
	effectSprite.radius = 2.0f;
	effectSprite.rgba[0] = effectSprite.rgba[1] = effectSprite.rgba[2]
		= effectSprite.rgba[3] = 1.0f;
	effectSprite.shader = material;
	effectSprite.flags = PRIM_FLAG_ADDITIVE;
	CHECK( RenderSubmission_AddEffectSprite( &frontend, &effectSprite ) );
	memset( &effectDecal, 0, sizeof( effectDecal ) );
	effectDecal.origin[0] = 8.0f;
	effectDecal.normal[2] = 1.0f;
	effectDecal.radius = 2.0f;
	effectDecal.rgba[0] = effectDecal.rgba[3] = 1.0f;
	effectDecal.shader = material;
	effectDecal.lifetime = 1.0f;
	CHECK( RenderSubmission_AddEffectDecal( &frontend, &effectDecal ) );
	memset( effectRibbonPoints, 0, sizeof( effectRibbonPoints ) );
	for ( uint32_t point = 0u; point < 2u; ++point ) {
		effectRibbonPoints[point].pos[0] = 8.0f;
		effectRibbonPoints[point].pos[1] = point ? 4.0f : -4.0f;
		effectRibbonPoints[point].width = 1.0f;
		effectRibbonPoints[point].rgba[0] = 1.0f;
		effectRibbonPoints[point].rgba[3] = 1.0f;
		effectRibbonPoints[point].normal[2] = 1.0f;
	}
	memset( &effectRibbon, 0, sizeof( effectRibbon ) );
	effectRibbon.points = effectRibbonPoints;
	effectRibbon.numPoints = 2;
	effectRibbon.shader = material;
	effectRibbon.flags = PRIM_FLAG_CUSTOM_NORMAL | PRIM_FLAG_ADDITIVE;
	CHECK( RenderSubmission_AddEffectRibbon( &frontend, &effectRibbon ) );
	memset( &effectBeam, 0, sizeof( effectBeam ) );
	effectBeam.start[0] = effectBeam.end[0] = 8.0f;
	effectBeam.start[1] = -2.0f; effectBeam.end[1] = 2.0f;
	effectBeam.startWidth = 0.5f; effectBeam.endWidth = 0.25f;
	effectBeam.startColor[0] = effectBeam.startColor[3] = 1.0f;
	effectBeam.endColor[0] = effectBeam.endColor[3] = 1.0f;
	effectBeam.shader = material; effectBeam.duration = 1.0f;
	effectBeam.fadeOut = 0.25f; effectBeam.axialCopies = 1;
	effectBeam.startEntityNum = effectBeam.endEntityNum = -1;
	CHECK( RenderSubmission_AddEffectBeam( &frontend, &effectBeam ) );
	CHECK( RenderSubmission_EndFrame( &frontend, 75u, &submission ) );
	RenderEffectDecalPool_Reset( &decalPool );
	CHECK( RenderEffectDecalPool_Advance( &decalPool, 76u, qtrue, 0.0f,
		&effectDecal, 1u, &decalPoolCandidate, &decalPoolReceipt ) );
	RenderEffectBeamPool_Reset( &beamPool );
	CHECK( RenderEffectBeamPool_Advance( &beamPool, 76u, qtrue, 0.0f,
		&effectBeam, 1u, &beamPoolCandidate, &beamPoolReceipt ) );
	CHECK( RalOpenGl_FrontendPlanBuild( &core, &frontend, &submission, 76u,
		&decalPoolCandidate, &decalPoolReceipt,
		&beamPoolCandidate, &beamPoolReceipt, &plan ) );
	CHECK( plan.loweredPolygonCount == 1u && plan.loweredLightCount == 1u
		&& plan.loweredEffectIndexCount == 33u
		&& plan.loweredEffectBatchCount == 6u
		&& plan.loweredEffectSpriteCount == 1u
		&& plan.loweredEffectDecalCount == 1u
		&& plan.loweredEffectRibbonCount == 1u
		&& plan.loweredEffectBeamCount == 1u
		&& plan.loweredModelEntityCount == 1u
		&& plan.loweredPrimitiveEntityCount == 1u
		&& plan.loweredTemporalEntityCount == 1u
		&& plan.loweredLocalIrradianceEntityCount == 1u
		&& plan.loweredUiPrimitiveCount == 2u
		&& plan.texturedUiPrimitiveCount == 2u
		&& plan.msdfUiPrimitiveCount == 1u
		&& plan.unresolvedCount == 0u );
	memset( &lowerInfo, 0, sizeof( lowerInfo ) );
	lowerInfo.generation = 77u;
	lowerInfo.effectDecalPool = &decalPoolCandidate;
	lowerInfo.effectDecalPoolReceipt = decalPoolReceipt;
	lowerInfo.effectDecalPoolReceipt.frameGeneration = 77u;
	lowerInfo.effectBeamPool = &beamPoolCandidate;
	lowerInfo.effectBeamPoolReceipt = beamPoolReceipt;
	lowerInfo.effectBeamPoolReceipt.frameGeneration = 77u;
	memset( &worldReceipt, 0xa5, sizeof( worldReceipt ) ); worldExact = worldReceipt;
	CHECK( !RalOpenGl_WorldLower( coreOwner, &core, &frontend, &submission,
		&plan, &lowerInfo, &worldReceipt )
		&& !memcmp( &worldReceipt, &worldExact, sizeof( worldReceipt ) ) );
	lowerInfo.programName = 9001u;
	lowerInfo.outputWidth = 1280u;
	lowerInfo.outputHeight = 720u;
	lowerInfo.materialTextureNames = residentMaterialTextures;
	lowerInfo.materialTextureCount = 2u;
	memset( &atmosphereRequest, 0, sizeof( atmosphereRequest ) );
	atmosphereRequest.schemaVersion = RAL_ATMOSPHERE_PLAN_SCHEMA_VERSION;
	atmosphereRequest.backendType = RAL_BACKEND_OPENGL;
	atmosphereRequest.frameGeneration = lowerInfo.generation;
	atmosphereRequest.width = 1280u; atmosphereRequest.height = 720u;
	atmosphereRequest.requestedTier = RAL_ATMOSPHERE_TIER_FULL;
	atmosphereRequest.maxFroxelCount = 262144u;
	atmosphereRequest.maxLocalVolumes = RAL_ATMOSPHERE_MAX_VOLUMES;
	atmosphereRequest.maxLights = RAL_ATMOSPHERE_MAX_LIGHTS;
	atmosphereRequest.maxShadowedLights = RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS;
	atmosphereRequest.mediaActive = qtrue;
	atmosphereRequest.skyLightingActive = qtrue;
	atmosphereRequest.cloudsRequested = qtrue;
	atmosphereRequest.capabilities.analyticComposite = qtrue;
	atmosphereRequest.capabilities.compute = qtrue;
	atmosphereRequest.capabilities.storageBuffers = qtrue;
	atmosphereRequest.capabilities.temporalHistory = qtrue;
	atmosphereRequest.capabilities.fullClouds = qtrue;
	CHECK( Ral_AtmospherePlan( &atmosphereRequest, &lowerInfo.atmosphere ) );
	lowerInfo.atmosphereBufferName = 9002u;
	lowerInfo.directionalLighting = &lightingReceipt;
	CHECK( Ral_DisplayVisibilityPlanBuild( 1.0f,
		&lowerInfo.displayVisibility ) );
	/* Enter with the state left by an alpha/UI pass in the previous frame. */
	s_gl.depthWriteEnabled = 0u;
	CHECK( RalOpenGl_WorldLower( coreOwner, &core, &frontend, &submission,
		&plan, &lowerInfo, &worldReceipt ) );
	CHECK( s_gl.depthClearCount == 1u && s_gl.maskedDepthClearCount == 0u );
	/* Unbounded authored visibility must match the compute producer range,
	 * even when later UI draws overwrite the last generic draw uniforms. */
	CHECK( atmosphere.visibility == 0.0f && s_lastFullAtmosphereFar == 65536.0f );
	CHECK( worldReceipt.uploadedMaterialCount == 2u
		&& worldReceipt.uploadedWorldVertexCount == 3u
		&& worldReceipt.uploadedWorldIndexCount == 3u
		&& worldReceipt.worldDrawCount == 1u
		&& worldReceipt.legacyLightmapDrawCount == 0u
		&& worldReceipt.directionalStaticDrawCount == 1u
		&& worldReceipt.surfaceLightingBindingDigest != 0u
		&& worldReceipt.polygonCount == 1u
		&& worldReceipt.lightCount == 1u
		&& worldReceipt.effectIndexCount == 33u
		&& worldReceipt.effectDrawCount == 6u
		&& worldReceipt.effectSpriteDrawCount == 1u
		&& worldReceipt.effectDecalDrawCount == 1u
		&& worldReceipt.effectRibbonDrawCount == 1u
		&& worldReceipt.effectBeamDrawCount == 1u
		&& worldReceipt.modelEntityCount == 1u
		&& worldReceipt.primitiveEntityCount == 1u
		&& worldReceipt.temporalEntityCount == 1u
		&& worldReceipt.localIrradianceEntityCount == 1u
		&& worldReceipt.localIrradianceDrawCount == 1u
		&& worldReceipt.entityIndexCount == 9u
		&& worldReceipt.entityDrawCount == 2u
		&& worldReceipt.uiPrimitiveCount == 2u
		&& worldReceipt.texturedUiPrimitiveCount == 2u
		&& worldReceipt.msdfUiPrimitiveCount == 1u
		&& worldReceipt.uiDrawCount == 2u
		&& worldReceipt.nativeDrawCount == 11u
		&& worldReceipt.unresolvedCount == 0u
		&& s_gl.drawCount == 11u && s_gl.textureUploadCount == 0u
		&& s_gl.boundTextureByUnit[1] == 0u
		&& s_gl.boundTextureByUnit[2]
			== lightingReceipt.textureNames[0]
		&& s_gl.boundTextureByUnit[3]
			== lightingReceipt.textureNames[1]
		&& s_gl.boundTextureByUnit[4]
			== lightingReceipt.textureNames[2] );
	worldExact = worldReceipt;
	/* This fixture emits world, polygon/light/sprite/decal/ribbon/beam,
	 * then two entities and UI. Verify geometry offsets and native blending:
	 * additive primitives must preserve texture and lifetime alpha. */
	CHECK( s_firstDrawCount == 11u );
	CHECK( s_firstDraws[3].count == 6 && s_firstDraws[3].offset == 9u * sizeof(uint32_t) );
	CHECK( s_firstDraws[5].count == 6 && s_firstDraws[5].offset == 21u * sizeof(uint32_t) );
	CHECK( s_firstDraws[3].source == 0x0302u && s_firstDraws[3].destination == 1u );
	CHECK( s_firstDraws[5].source == 0x0302u && s_firstDraws[5].destination == 1u );
	CHECK( RalOpenGl_WorldReceiptExact( &worldReceipt, &worldExact ) );
	worldExact.nativeDrawCount++;
	CHECK( !RalOpenGl_WorldReceiptExact( &worldReceipt, &worldExact ) );
	s_gl.drawCount = 0u; s_gl.textureUploadCount = 0u;
	memset( &distantSkyRequest, 0, sizeof( distantSkyRequest ) );
	distantSkyRequest.schemaVersion = RAL_SKY_ENVIRONMENT_SCHEMA_VERSION;
	distantSkyRequest.backendType = RAL_BACKEND_OPENGL;
	distantSkyRequest.frameGeneration = 75u;
	distantSkyRequest.requestedTier = RAL_ATMOSPHERE_TIER_FULL;
	distantSkyRequest.mode = RAL_SKY_ENVIRONMENT_AUTHORED;
	distantSkyRequest.sourceRevision = 1u;
	distantSkyRequest.authoredSkyAvailable = qtrue;
	distantSkyRequest.distantSceneRequested = qtrue;
	distantSkyRequest.distantSceneIdentity = distantIdentity;
	distantSkyRequest.distantSceneScale = 16.0f;
	distantSkyRequest.distantSceneLodBias = 1.0f;
	distantSkyRequest.capabilities.distantScene = qtrue;
	CHECK( Ral_SkyEnvironmentPlan( &distantSkyRequest, &distantSkyReceipt ) );
	CHECK( distantSkyReceipt.distantSceneActive );
	CHECK( distantSkyReceipt.frameGeneration == frontend.sealedFrameGeneration );
	CHECK( frontend.frameSealed && !frontend.frameOpen );
	CHECK( frontend.activeWorldIndex == 0 );
	CHECK( frontend.worlds[RENDER_DISTANT_WORLD_BASE].loaded );
	CHECK( frontend.worlds[RENDER_DISTANT_WORLD_BASE].contentIdentity
		== distantSkyReceipt.distantSceneIdentity );
	CHECK( frontend.worlds[RENDER_DISTANT_WORLD_BASE].snapshot.batchCount == 1u );
	CHECK( isfinite( frontend.worlds[RENDER_DISTANT_WORLD_BASE]
		.snapshot.batches[0].boundsRadius ) );
	CHECK( RenderSubmission_UpdateWorldVisibilityForView( &frontend,
		RENDER_DISTANT_WORLD_BASE, &view ) );
	CHECK( RenderSubmission_DistantWorldBuildMetrics( &frontend,
		RENDER_DISTANT_WORLD_BASE, &distantSkyReceipt, &view,
		RAL_SKY_DISTANT_MAX_LOD_LEVELS, distantMetrics, 1u,
		&distantMetricCount ) && distantMetricCount == 1u );
	CHECK( RenderSubmission_DistantWorldDrawPlan( &frontend,
		RENDER_DISTANT_WORLD_BASE, &distantSkyReceipt, distantMetrics,
		distantMetricCount, 1u, distantCandidates, 1u, &distantDrawPlan ) );
	CHECK( RenderSubmission_DistantWorldSubmissionBuild( &frontend,
		RENDER_DISTANT_WORLD_BASE, &view, &distantDrawPlan,
		&distantSubmission ) );
	CHECK( RenderSubmission_DistantWorldSubmissionForActiveView( &frontend,
			&distantSkyReceipt, RAL_SKY_DISTANT_MAX_LOD_LEVELS, 1u,
			distantMetrics, 1u, distantCandidates, 1u, &distantSubmission ) );
	{
		/* A distant scene may end with a transparent, non-depth-writing batch.
		 * Its depth still must be discarded before drawing the gameplay world. */
		renderDistantWorldSubmission_t transparentDistant = distantSubmission;
		ralOpenGlWorldLowerInfo_t distantLower = lowerInfo;
		renderWorldBatch_t batch = distantSubmission.world.batches[0];
		batch.depthWrite = qfalse;
		transparentDistant.world.batches = &batch;
		distantLower.generation = transparentDistant.frameGeneration;
		distantLower.atmosphere.frameGeneration = distantLower.generation;
		distantLower.effectDecalPoolReceipt.frameGeneration = distantLower.generation;
		distantLower.effectBeamPoolReceipt.frameGeneration = distantLower.generation;
		distantLower.distantWorld = &transparentDistant;
		s_gl.depthWriteEnabled = 0u;
		s_gl.depthClearCount = s_gl.maskedDepthClearCount = 0u;
		CHECK( RalOpenGl_WorldLower( coreOwner, &core, &frontend, &submission,
			&plan, &distantLower, &worldReceipt ) );
		CHECK( s_gl.depthClearCount == 2u && s_gl.maskedDepthClearCount == 0u );
		s_gl.drawCount = 0u;
	}
	CHECK( RalOpenGl_ProductRender( product, &frontend, &submission, 75u,
		&productReceipt ) );
	CHECK( !CheckAtmospherePacket( &frontend, &productReceipt, qfalse ) );
	CHECK( productReceipt.atmosphere.admittedLightCount == 1u );
	CHECK( productReceipt.plan.loweredWorldBatchCount == 1u
		&& productReceipt.plan.loweredModelEntityCount == 1u
		&& productReceipt.plan.loweredUiPrimitiveCount == 2u
		&& productReceipt.plan.loweredPolygonCount == 1u
		&& productReceipt.plan.loweredLightCount == 1u
		&& productReceipt.native.nativeDrawCount == 13u
		&& productReceipt.native.effectDecalDrawCount == 1u
		&& productReceipt.native.effectBeamDrawCount == 1u
		&& productReceipt.native.distantWorldCandidateCount == 1u
		&& productReceipt.native.distantWorldDrawCount == 1u
		&& productReceipt.atmosphere.selectedTier == RAL_ATMOSPHERE_TIER_FULL
		&& productReceipt.atmosphere.fallbackReason
			== RAL_ATMOSPHERE_FALLBACK_NONE
		&& productReceipt.atmosphere.skyEnvironment.schemaVersion
			== RAL_SKY_ENVIRONMENT_RECEIPT_SCHEMA_VERSION
		&& productReceipt.atmosphere.skyEnvironment.selectedMode
			== RAL_SKY_ENVIRONMENT_AUTHORED
		&& productReceipt.atmosphere.skyEnvironment.fallbackReason
			== RAL_SKY_FALLBACK_NO_CAPTURE
		&& productReceipt.atmosphere.skyEnvironment.zeroWork == qtrue
		&& productReceipt.atmosphereDispatchCount == 4u
		&& productReceipt.atmosphereFroxelCount == 230400u
		&& productReceipt.atmosphereCompositeCount == 1u
		&& productReceipt.atmosphereCloudsActive == qtrue
		&& productReceipt.weather.familyMask == ( ( 1u << 1 ) | ( 1u << 2 ) )
		&& productReceipt.weather.activeParticleCount == 8192u
		&& productReceipt.weather.precipitationParticleCount == 8168u
		&& productReceipt.weather.semanticEmitterCount == 1u
		&& productReceipt.weather.semanticParticleCount == 24u
		&& productReceipt.weather.droppedParticleCount == 24u
		&& productReceipt.weatherDispatchCount == 1u
		&& productReceipt.weatherDrawCount == 1u
		&& productReceipt.native.weatherDrawCount == 1u
		&& productReceipt.native.weatherInstanceCount == 8192u
		&& productReceipt.unresolvedCount == 0u
		&& productReceipt.fallbackCount == 0u
		&& productReceipt.fatalCount == 0u
		&& s_gl.drawCount == 12u && s_gl.textureUploadCount == 2u
		&& s_gl.dispatchCount == 10u && s_gl.barrierCount == 8u
		&& productReceipt.postprocessDrawCount == 1u
		&& productReceipt.exposureDispatchCount == 0u && s_compositeCount == 1u
		&& s_hdrSceneDraws > 0u && s_uiOutputDraws == 2u
		&& s_gl.weatherDrawCount == 1u );
	productExact = productReceipt;
	CHECK( RalOpenGl_ProductFrameReceiptExact( &productReceipt, &productExact ) );
	productExact.atmosphereNativeDispatchCount--;
	CHECK( !RalOpenGl_ProductFrameReceiptExact( &productReceipt, &productExact ) );
	memset( &productExact, 0xa5, sizeof( productExact ) ); worldExact = worldReceipt;
	CHECK( !RalOpenGl_ProductRender( product, &frontend, &submission, 75u,
		&productExact ) );
	/* Handle zero is the established null-model sentinel. The canonical
	 * adapter lowers it as an explicit proxy primitive, not a fallback. */
	CHECK( RenderSubmission_BeginFrame( &frontend, 79u ) );
	CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
	memset( &modelEntity, 0, sizeof( modelEntity ) );
	modelEntity.reType = RT_MODEL; modelEntity.customShader = material;
	memset( modelEntity.shader.rgba, 255, sizeof( modelEntity.shader.rgba ) );
	CHECK( RenderSubmission_AddEntity( &frontend, &modelEntity, NULL ) );
	CHECK( RenderSubmission_EndFrame( &frontend, 79u, &submission ) );
	CHECK( RalOpenGl_ProductRender( product, &frontend, &submission, 79u,
		&productReceipt ) );
	CHECK( productReceipt.plan.loweredModelEntityCount == 0u
		&& productReceipt.plan.loweredPrimitiveEntityCount == 1u
		&& productReceipt.plan.loweredEntityIndexCount == 36u
		&& productReceipt.plan.loweredEntityBatchCount == 1u
		&& productReceipt.native.primitiveEntityCount == 1u
		&& productReceipt.native.nativeDrawCount == 6u
		&& productReceipt.native.effectDecalDrawCount == 1u
		&& productReceipt.native.effectBeamDrawCount == 1u
		&& productReceipt.native.distantWorldDrawCount == 1u
		&& productReceipt.fallbackCount == 0u
		&& productReceipt.unresolvedCount == 0u
		&& s_gl.textureUploadCount == 2u );
	/* Paired frames exercise reset/reuse and history ping-pong with translated
	 * cameras, short/default ranges, local volumes and point/tube lights. */
	for ( unsigned scenario = 0; scenario < 6; ++scenario ) {
		ralAtmosphereFixtureState_t authored;
		refdef_t camera = view;
		CHECK( Ral_AtmosphereConformanceFixture(RAL_ATMOSPHERE_FIXTURE_FOG, &authored) );
		authored.state.visibility = scenario%3u == 0 ? 0.0f : scenario%3u == 1 ? 1.0f : 4096.0f;
		authored.state.cloudCover = scenario & 1u ? 0.75f : 0.0f;
		authored.state.bounds[2] = -64;
		authored.state.bounds[5] = scenario & 1u ? 1984 : 128;
		authored.state.wind[0] = 3; authored.state.wind[1] = -7;
		authored.state.seed = 71u+scenario;
		camera.vieworg[0] = 12.0f*scenario; camera.vieworg[2] = 24.0f*scenario;
		camera.fov_x = 75+scenario*3; camera.fov_y = 60+scenario*2;
		if ( scenario & 1u ) {
			const float axes[3][3] = {{.8f,0,-.6f}, {0,1,0}, {.6f,0,.8f}};
			memcpy(camera.viewaxis, axes, sizeof(axes));
		}
		fakeName_t historyOut = 0;
		for ( unsigned frame = 0; frame < 2; ++frame ) {
			const uint64_t generation = 80u+scenario*2u+frame;
			authored.state.timelineSeconds = 10.0f+scenario+frame*.016f;
			CHECK( RenderSubmission_SetAtmosphere(&frontend, &authored.state) );
			CHECK( RenderSubmission_BeginFrame(&frontend, generation) );
			Ral_AtmosphereConformanceLocalMedia(&media);
			CHECK( RenderSubmission_AddAtmosphereMediaVolume(&frontend, &media) );
			const vec3_t origin = {64, 12, 48}, endpoint = {32, -7, 90};
			if ( scenario%3u ) {
				CHECK( RenderSubmission_AddLight(&frontend, origin, NULL, 72, .9f, .4f, .1f) );
				CHECK( RenderSubmission_AddLight(&frontend, endpoint, origin, 32, .1f, .6f, .8f) );
			}
			CHECK( RenderSubmission_RenderScene(&frontend, &camera, 0) );
			CHECK( RenderSubmission_EndFrame(&frontend, generation, &submission) );
			s_atm.count = 0;
			CHECK( RalOpenGl_ProductRender(product, &frontend, &submission, generation, &productReceipt) );
			CHECK( !CheckAtmospherePacket(&frontend, &productReceipt, qtrue) );
			CHECK( productReceipt.atmosphere.admittedLightCount == (scenario%3u ? 2u : 0u) );
			if ( frame ) {
				CHECK( productReceipt.atmosphere.temporalReuseCount == 1u );
				CHECK( s_atm.bindings[ATM_INTEGRATE][1] == historyOut );
			}
			historyOut = s_atm.bindings[ATM_INTEGRATE][3];
		}
	}
	for ( int fixtureIndex = RAL_ATMOSPHERE_FIXTURE_CLEAR;
			fixtureIndex < RAL_ATMOSPHERE_FIXTURE_COUNT; ++fixtureIndex ) {
		ralAtmosphereFixtureState_t authored;
		const uint64_t frontendGeneration = 100u + (uint64_t)fixtureIndex;
		const uint64_t productGeneration = 200u + (uint64_t)fixtureIndex;
		CHECK( Ral_AtmosphereConformanceFixture(
			(ralAtmosphereFixture_t)fixtureIndex, &authored ) );
		authored.state.timelineSeconds = (float)fixtureIndex;
		CHECK( RenderSubmission_SetAtmosphere( &frontend, &authored.state ) );
		CHECK( RenderSubmission_BeginFrame( &frontend, frontendGeneration ) );
		if ( authored.breathEmitter ) {
			Ral_AtmosphereConformanceBreathEmitter( 1u, &breathEmitter );
			CHECK( RenderSubmission_AddAtmosphereEmitter( &frontend,
				&breathEmitter ) );
		}
		if ( authored.localMedia ) {
			Ral_AtmosphereConformanceLocalMedia( &media );
			CHECK( RenderSubmission_AddAtmosphereMediaVolume( &frontend,
				&media ) );
		}
		CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
		CHECK( RenderSubmission_EndFrame( &frontend, frontendGeneration,
			&submission ) );
		s_atm.count = 0u;
		CHECK( RalOpenGl_ProductRender( product, &frontend, &submission,
			productGeneration, &productReceipt ) );
		CHECK( !CheckAtmospherePacket( &frontend, &productReceipt, qtrue ) );
		CHECK( productReceipt.atmosphere.requestedTier ==
			(ralAtmosphereTier_t)authored.state.qualityTier &&
			productReceipt.atmosphere.selectedTier ==
			(ralAtmosphereTier_t)authored.state.qualityTier &&
			productReceipt.atmosphere.fallbackReason ==
				RAL_ATMOSPHERE_FALLBACK_NONE &&
			productReceipt.weather.familyMask == authored.familyMask );
		if ( authored.breathEmitter )
			CHECK( productReceipt.weather.semanticEmitterCount == 1u &&
				productReceipt.weather.semanticParticleCount == 24u );
		if ( fixtureIndex == RAL_ATMOSPHERE_FIXTURE_CLEAR ||
				fixtureIndex == RAL_ATMOSPHERE_FIXTURE_FOG )
			CHECK( productReceipt.weather.zeroWork );
		if ( fixtureIndex == RAL_ATMOSPHERE_FIXTURE_FOG )
			CHECK( productReceipt.atmosphereDispatchCount == 3u &&
				productReceipt.atmosphereFroxelCount == 230400u &&
				productReceipt.atmosphereCompositeCount == 1u );
		if ( fixtureIndex == RAL_ATMOSPHERE_FIXTURE_STORM )
			CHECK( productReceipt.atmosphereDispatchCount == 4u &&
				productReceipt.atmosphereCloudsActive &&
				productReceipt.weather.activeParticleCount == 8192u );
	}
	{
		const uint32_t physicalWork[4] = {
			RAL_SKY_DIRTY_TRANSMITTANCE,
			RAL_SKY_DIRTY_MULTISCATTER,
			RAL_SKY_DIRTY_SKY_VIEW,
			0u
		};
		const uint32_t physicalMask = RAL_SKY_DIRTY_TRANSMITTANCE |
			RAL_SKY_DIRTY_MULTISCATTER | RAL_SKY_DIRTY_SKY_VIEW;
		const uint32_t imageBindStart = s_gl.imageBindCount;
		const uint32_t fenceStart = s_gl.fenceCount;
		const uint32_t completedFenceStart = s_gl.completedFenceCount;
		atmosphereFrameState_t physical = atmosphere;
		/* Full-tier physical sky without media must not dispatch a zero-sized
		 * froxel grid. Keep the real sky LUT/capture work enabled. */
		physical.mediaDensity = physical.visibility = physical.cloudCover = 0.0f;
		memset( physical.precipitation, 0, sizeof( physical.precipitation ) );

		memset( &physical.skyEnvironment, 0,
			sizeof( physical.skyEnvironment ) );
		physical.transitionSeconds = 0.35f;
		physical.skyEnvironment.schemaVersion =
			WIRED_SKY_ENVIRONMENT_SCHEMA_VERSION;
		physical.skyEnvironment.mode = ATMOSPHERE_SKY_PHYSICAL;
		physical.skyEnvironment.sourceRevision = 2u;
		physical.skyEnvironment.sunDirection[2] = 1.0f;
		physical.skyEnvironment.sunRadiance[0] = 4.0f;
		physical.skyEnvironment.sunRadiance[1] = 3.8f;
		physical.skyEnvironment.sunRadiance[2] = 3.5f;
		physical.skyEnvironment.sunAngularRadius = 0.00465f;
		physical.skyEnvironment.moonDirection[2] = -1.0f;
		physical.skyEnvironment.moonAngularRadius = 0.00436f;
		physical.skyEnvironment.ambientSh[0] = 0.12f;
		physical.skyEnvironment.ambientSh[1] = 0.16f;
		physical.skyEnvironment.ambientSh[2] = 0.22f;
		physical.skyEnvironment.cloudSunOcclusion = 0.4f;
		physical.skyEnvironment.skyLuminance = 1.0f;
		physical.skyEnvironment.exposure = 1.0f;
		physical.skyEnvironment.opticsVisibility = 0.6f;
		physical.skyEnvironment.distantSceneScale = 1.0f;
		CHECK( RenderSubmission_SetAtmosphere( &frontend, &physical ) );
		for ( uint32_t frame = 0u; frame < 4u; ++frame ) {
			const uint64_t frontendGeneration = 300u + frame;
			const uint64_t productGeneration = 400u + frame;
			CHECK( RenderSubmission_BeginFrame( &frontend,
				frontendGeneration ) );
			CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
			CHECK( RenderSubmission_EndFrame( &frontend,
				frontendGeneration, &submission ) );
			CHECK( RalOpenGl_ProductRender( product, &frontend, &submission,
				productGeneration, &productReceipt ) );
			CHECK( productReceipt.atmosphere.skyEnvironment.selectedMode ==
				RAL_SKY_ENVIRONMENT_PHYSICAL );
			CHECK( productReceipt.atmosphereDispatchCount == 0u &&
				productReceipt.atmosphereFroxelCount == 0u );
			CHECK( productReceipt.atmosphere.skyEnvironment.admittedWorkMask ==
				physicalWork[frame] );
		}
		CHECK( ( productReceipt.atmosphere.skyEnvironment.completedProductMask &
			physicalMask ) == physicalMask );
		/* Three physical-LUT images, then the first radiance capture slice
		 * only after LUT completion. Each product owns its own fence. */
		CHECK( s_gl.imageBindCount == imageBindStart + 4u );
		CHECK( s_gl.fenceCount == fenceStart + 4u );
		CHECK( s_gl.completedFenceCount >= completedFenceStart + 4u );
		CHECK( s_gl.queryBeginCount >= 7u );
		CHECK( s_gl.queryResultCount >= 6u );
		CHECK( productReceipt.skyGpuTelemetry.physicalTimingState ==
			RAL_SKY_GPU_TIMING_VALID );
		CHECK( productReceipt.skyGpuTelemetry.physicalGpuDurationNanoseconds ==
			250000u );
		CHECK( productReceipt.skyGpuTelemetry.radianceTimingState ==
			RAL_SKY_GPU_TIMING_PENDING );
		CHECK( productReceipt.skyGpuTelemetry.radianceGpuDurationNanoseconds ==
			0u );
	}
	{
		renderWorldSnapshot_t projection = { 0 };
		float point[3], radius, origin[3] = { 8, 0, 0 };
		projection.fovX = projection.fovY = 90.0f;
		projection.viewAxis[0][0] = projection.viewAxis[1][1] = projection.viewAxis[2][2] = 1.0f;
		CHECK( RalOpenGl_WorldProjectLens( &projection, origin, 720, point, &radius ) );
		CHECK( fabsf( point[0] - 0.5f ) < 0.0001f && fabsf( point[1] - 0.5f ) < 0.0001f
			&& fabsf( point[2] - 0.75f ) < 0.0001f && fabsf( radius - 45.0f ) < 0.0001f );
		origin[1] = origin[2] = 4.0f;
		CHECK( RalOpenGl_WorldProjectLens( &projection, origin, 720, point, &radius ) );
		CHECK( fabsf( point[0] - 0.25f ) < 0.0001f && fabsf( point[1] - 0.75f ) < 0.0001f );
		projection.viewOrigin[0] = origin[0];
		CHECK( !RalOpenGl_WorldProjectLens( &projection, origin, 720, point, &radius ) );
		projection.viewOrigin[0] = 0;
		projection.fovY = NAN;
		CHECK( !RalOpenGl_WorldProjectLens( &projection, origin, 720, point, &radius ) );
	}
	{
		unsigned pending[3];
		float visibility = -7.0f;
		uint32_t reads = s_lensReads, copies;
		s_holdLens = qtrue;
		view.time = 1000;
		for ( unsigned i = 0; i < 3; ++i ) {
			s_lensResult = 0.2f * ( i + 1 );
			CHECK( !LensFrame( &frontend, product, &view, 500u + i, 7, qtrue, &productReceipt ) );
			CHECK( productReceipt.lensSourceCount == 1 && productReceipt.lensDispatchCount == 1 );
			pending[i] = s_lastLensFence;
			CHECK( !RalOpenGl_ProductLensVisibility( product, 7, &visibility ) && visibility == -7.0f );
		}
		copies = s_lensCopies;
		CHECK( !LensFrame( &frontend, product, &view, 503, 7, qtrue, &productReceipt ) );
		CHECK( productReceipt.lensDeferredCount == 1 && productReceipt.lensDispatchCount == 0
			&& s_lensReads == reads && s_lensCopies == copies );
		/* Newest result first; older completed queries must never replace it. */
		s_fences[pending[2]].signaled = qtrue;
		CHECK( !LensFrame( &frontend, product, &view, 504, 7, qtrue, &productReceipt ) );
		CHECK( productReceipt.lensCompletedCount == 1 && s_lensReads == reads + 1 );
		CHECK( RalOpenGl_ProductLensVisibility( product, 7, &visibility ) && fabsf( visibility - 0.6f ) < 0.0001f );
		s_fences[pending[0]].signaled = s_fences[pending[1]].signaled = qtrue;
		CHECK( !LensFrame( &frontend, product, &view, 505, 7, qtrue, &productReceipt ) );
		CHECK( productReceipt.lensCompletedCount == 2 );
		CHECK( RalOpenGl_ProductLensVisibility( product, 7, &visibility ) && fabsf( visibility - 0.6f ) < 0.0001f );
		CHECK( !LensFrame( &frontend, product, &view, 506, -1, qtrue, &productReceipt ) );
		visibility = -7;
		CHECK( !RalOpenGl_ProductLensVisibility( product, 7, &visibility ) && visibility == -7 );
		CHECK( !RalOpenGl_ProductLensVisibility( product, -1, &visibility ) );
		CHECK( !RalOpenGl_ProductLensVisibility( product, LENS_MAX_CGSOURCES, &visibility ) );
		CHECK( !RalOpenGl_ProductLensVisibility( product, 7, NULL ) );
		CHECK( !LensFrame( &frontend, product, &view, 507, 8, qtrue, &productReceipt ) );
		CHECK( !RalOpenGl_ProductLensVisibility( product, 8, &visibility ) && visibility == -7 );
		RalOpenGl_ProductResetLensVisibility( product );
		for ( unsigned i = 0; i < 3; ++i ) CHECK( !s_lensBuffers[i].name );
		CHECK( !LensFrame( &frontend, product, &view, 508, 7, qtrue, &productReceipt ) );
		s_fences[s_lastLensFence].signaled = qtrue;
		s_failLensRead = qtrue;
		productExact = productReceipt;
		CHECK( !LensFrame( &frontend, product, &view, 509, 7, qfalse, &productReceipt ) );
		CHECK( !memcmp( &productExact, &productReceipt, sizeof( productExact ) ) );
		CHECK( !RalOpenGl_ProductLensVisibility( product, 7, &visibility ) );
		s_failLensRead = qfalse;
		CHECK( !LensFrame( &frontend, product, &view, 510, 7, qtrue, &productReceipt ) );
		unsigned oldFence = s_lastLensFence;
		view.time = 500; /* Timeline rewind cancels old in-flight publication. */
		CHECK( !LensFrame( &frontend, product, &view, 511, 7, qtrue, &productReceipt ) );
		CHECK( s_fences[oldFence].deleted && productReceipt.lensCompletedCount == 0 );
		CHECK( !RalOpenGl_ProductLensVisibility( product, 7, &visibility ) );
		oldFence = s_lastLensFence;
		CHECK( RalOpenGl_ProductSetOutputExtent( product, 1600, 900 ) );
		CHECK( s_fences[oldFence].deleted );
		CHECK( RalOpenGl_ProductSetOutputExtent( product, 1280, 720 ) );
		CHECK( RalOpenGl_ProductResetLevelResources( product ) );
		for ( unsigned i = 0; i < 3; ++i ) CHECK( !s_lensBuffers[i].name );
		s_holdLens = qfalse;
	}
	{
		/* Native command/lifecycle proof, not a CPU simulation of exposure pixels. */
		ralExposureSettings_t settings = { 1, 0.0123f, 0.5f, 0.2f, 1.5f, 0.5f, 0.25f, 8.0f };
		ralExposureSettings_t invalid = settings;
		CHECK( RalOpenGl_ProductSetExposure( product, &settings, 3u, 1.0f ) );
		invalid.key = NAN;
		CHECK( !RalOpenGl_ProductSetExposure( product, &invalid, 3u, 1.0f ) );
		CHECK( !RalOpenGl_ProductSetExposure( product, &settings, 5u, 1.0f ) );
		CHECK( !RalOpenGl_ProductSetExposure( product, &settings, 3u, INFINITY ) );
		s_drawFramebuffer = 71u; s_readFramebuffer = 72u;
		view.time = 1000;
		unsigned resets = s_accumulatorResets, clears = s_histogramClears;
		CHECK( !LensFrame( &frontend, product, &view, 600, -1, qtrue, &productReceipt ) );
		CHECK( productReceipt.exposureDispatchCount == 2u && productReceipt.exposureHistoryReset );
		CHECK( s_accumulatorResets == resets + 1u && s_histogramClears == clears + 1u );
		CHECK( s_drawFramebuffer == 71u && s_readFramebuffer == 72u );
		fakeName_t firstAccumulator = s_lastResetAccumulator;
		view.time = 1050;
		CHECK( !LensFrame( &frontend, product, &view, 601, -1, qtrue, &productReceipt ) );
		CHECK( !productReceipt.exposureHistoryReset && s_accumulatorResets == resets + 1u );
		CHECK( s_lastCopySource == firstAccumulator && s_lastCopyDestination != firstAccumulator );
		fakeName_t committedAccumulator = s_lastCopyDestination;
		productExact = productReceipt;
		view.time = 1100; s_rejectComposite = qtrue;
		CHECK( !LensFrame( &frontend, product, &view, 602, -1, qfalse, &productReceipt ) );
		CHECK( !memcmp( &productReceipt, &productExact, sizeof(productReceipt) ) );
		CHECK( s_drawFramebuffer == 71u && s_readFramebuffer == 72u );
		s_rejectComposite = qfalse; view.time = 1150;
		CHECK( !LensFrame( &frontend, product, &view, 603, -1, qtrue, &productReceipt ) );
		CHECK( s_lastCopySource == committedAccumulator && s_lastCopyDestination == firstAccumulator );
		view.time = 900;
		CHECK( !LensFrame( &frontend, product, &view, 604, -1, qtrue, &productReceipt ) );
		CHECK( productReceipt.exposureHistoryReset && s_accumulatorResets == resets + 2u );
		CHECK( RalOpenGl_ProductSetOutputExtent( product, 1600, 900 ) );
		s_rejectFramebuffer = qtrue; productExact = productReceipt;
		CHECK( !LensFrame( &frontend, product, &view, 605, -1, qfalse, &productReceipt ) );
		CHECK( !memcmp( &productReceipt, &productExact, sizeof(productReceipt) ) );
		CHECK( s_drawFramebuffer == 71u && s_readFramebuffer == 72u );
		s_rejectFramebuffer = qfalse;
		CHECK( RalOpenGl_ProductSetOutputExtent( product, 1280, 720 ) );
		CHECK( !LensFrame( &frontend, product, &view, 606, -1, qtrue, &productReceipt ) );
		CHECK( productReceipt.exposureHistoryReset );
		clears = s_histogramClears;
		CHECK( RenderSubmission_BeginFrame( &frontend, 607 ) );
		CHECK( RenderSubmission_EndFrame( &frontend, 607, &submission ) );
		CHECK( RalOpenGl_ProductRender( product, &frontend, &submission, 607, &productReceipt ) );
		CHECK( !productReceipt.exposureDispatchCount && s_histogramClears == clears );
		settings.enabled = 0;
		CHECK( RalOpenGl_ProductSetExposure( product, &settings, 0u, 5.2f ) );
		CHECK( s_toneMode == 0u && s_toneExposure == 1.0f );
		CHECK( !LensFrame( &frontend, product, &view, 608, -1, qtrue, &productReceipt ) );
		CHECK( !productReceipt.exposureDispatchCount && productReceipt.postprocessDrawCount == 1u );
		CHECK( !s_srgbEnabled && s_defaultEncodingQueries && s_customEncodingQueries );
		/* Linear HDR, sRGB final/UI, then restore caller state on either path. */
		s_srgbEnabled = 1;
		CHECK( !LensFrame( &frontend, product, &view, 609, -1, qtrue, &productReceipt ) );
		CHECK( s_srgbEnabled );
		productExact = productReceipt;
		s_rejectComposite = qtrue;
		CHECK( !LensFrame( &frontend, product, &view, 610, -1, qfalse, &productReceipt ) );
		CHECK( s_srgbEnabled && !memcmp( &productReceipt, &productExact, sizeof(productReceipt) ) );
		s_rejectComposite = qfalse;
		settings.enabled = 1;
		CHECK( RalOpenGl_ProductSetExposure( product, &settings, 3u, 1.0f ) );
		clears = s_histogramClears;
		unsigned composites = s_compositeCount;
		for ( unsigned invalidOutput = 0; invalidOutput < 2; ++invalidOutput ) {
			s_linearOutput = invalidOutput == 0;
			s_rejectEncodingQuery = invalidOutput == 1;
			CHECK( !LensFrame( &frontend, product, &view, 611 + invalidOutput, -1, qfalse, &productReceipt ) );
			CHECK( s_srgbEnabled && s_drawFramebuffer == 71u && s_readFramebuffer == 72u );
			CHECK( s_histogramClears == clears && s_compositeCount == composites );
			CHECK( !memcmp( &productReceipt, &productExact, sizeof(productReceipt) ) );
		}
		s_linearOutput = s_rejectEncodingQuery = qfalse;
		CHECK( !LensFrame( &frontend, product, &view, 613, -1, qtrue, &productReceipt ) );
		CHECK( s_srgbEnabled && productReceipt.exposureHistoryReset );
		/* A discarded cut must still reset the next accepted frame. */
		view.time = 1200; view.rdflags |= RDF_CAMERA_CUT;
		s_rejectComposite = qtrue;
		CHECK( !LensFrame( &frontend, product, &view, 614, -1, qfalse, &productReceipt ) );
		view.time = 1250; view.rdflags &= ~RDF_CAMERA_CUT;
		s_rejectComposite = qfalse;
		CHECK( !LensFrame( &frontend, product, &view, 615, -1, qtrue, &productReceipt ) );
		CHECK( productReceipt.exposureHistoryReset );
		view.time = 1300;
		CHECK( !LensFrame( &frontend, product, &view, 616, -1, qtrue, &productReceipt ) );
		CHECK( !productReceipt.exposureHistoryReset );
		view.rdflags |= RDF_HYPERSPACE;
		CHECK( !LensFrame( &frontend, product, &view, 617, -1, qtrue, &productReceipt ) );
		CHECK( productReceipt.exposureHistoryReset );
		view.rdflags &= ~RDF_HYPERSPACE;
		view.time = 1400;
		view.rdflags |= RDF_NOWORLDMODEL | RDF_HYPERSPACE | RDF_CAMERA_CUT;
		clears = s_histogramClears;
		CHECK( !LensFrame( &frontend, product, &view, 618, -1, qtrue, &productReceipt ) );
		CHECK( !productReceipt.exposureDispatchCount && s_histogramClears == clears );
		CHECK( !productReceipt.native.worldDrawCount
			&& !s_lastExposureBlock.autoEnabled
			&& !productReceipt.native.uploadedWorldVertexCount
			&& !productReceipt.native.uploadedWorldIndexCount );
		view.time = 1450;
		view.rdflags &= ~( RDF_NOWORLDMODEL | RDF_HYPERSPACE | RDF_CAMERA_CUT );
		CHECK( !LensFrame( &frontend, product, &view, 619, -1, qtrue, &productReceipt ) );
		CHECK( productReceipt.exposureHistoryReset && productReceipt.native.worldDrawCount
			&& s_lastExposureBlock.autoEnabled );
	}
	/* Exercise the production GL upload, not just a standalone clock helper.
	 * Absolute transient phase must survive resubmit; persistent phase starts
	 * at birth, pause holds, and rewind invalidates the retained beam. */
	for ( unsigned mode = 0u; mode < 2u; ++mode ) {
		const struct { int time; qboolean submit; float duration, phase; } cases[] = {
			{2000, qtrue, 0.0f, 2.0f}, {2500, qtrue, 0.0f, 2.5f},
			{3000, qtrue, 2.0f, 0.0f}, {3500, qfalse, 2.0f, 0.5f},
			{3500, qfalse, 2.0f, 0.5f}, {1000, qfalse, 0.0f, -1.0f},
			{1100, qtrue, 0.0f, 1.1f}
		};
		const float colors[2][4] = {{0.812345f, 0.412345f, 0.212345f, 0.251234f}, {0.212345f, 0.612345f, 1.0f, 0.512345f}};
		memcpy( effectBeam.startColor, colors[0], sizeof(colors[0]) );
		memcpy( effectBeam.endColor, colors[1], sizeof(colors[1]) );
		effectBeam.flags = mode ? 0 : PRIM_FLAG_ADDITIVE;
		for ( unsigned step = 0; step < sizeof(cases)/sizeof(cases[0]); ++step ) {
			const uint64_t generation = 620u + mode * 16u + step;
			view.time = cases[step].time;
			effectBeam.duration = cases[step].duration;
			effectBeam.uvScroll[0] = 2.0f; effectBeam.uvScroll[1] = -4.0f;
			CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
			CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
			if ( cases[step].submit ) CHECK( RenderSubmission_AddEffectBeam( &frontend, &effectBeam ) );
			CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
			s_beamUploadSeen = qfalse;
			CHECK( RalOpenGl_ProductRender( product, &frontend, &submission,
				generation, &productReceipt ) );
			if ( cases[step].phase < 0.0f ) {
				CHECK( !productReceipt.native.effectBeamDrawCount );
				continue;
			}
			CHECK( s_beamUploadSeen && productReceipt.native.effectBeamDrawCount == 1u );
			CHECK( productReceipt.native.effectBeamStageDrawCount == 1u );
			CHECK( !memcmp(s_beamMaterial + 8u, colors[0], 3u * sizeof(float)) );
			CHECK( !memcmp(s_beamMaterial + 12u, colors[1], 3u * sizeof(float)) );
			if (cases[step].submit) CHECK(s_beamMaterial[11] == colors[0][3] && s_beamMaterial[15] == colors[1][3]);
			CHECK(s_beamMaterial[16] == 1.0f && s_beamMaterial[17] == 1.0f);
			CHECK(fabsf(s_beamMaterial[18] - cases[step].phase * 2.0f) < .00001f);
			CHECK(fabsf(s_beamMaterial[19] + cases[step].phase * 4.0f) < .00001f);
			CHECK( s_beamBlendSource == 0x0302u ); /* SRC_ALPHA */
			CHECK( s_beamBlendDestination == (mode ? 0x0303u : 1u) );
			for ( unsigned corner = 0; corner < 4u; ++corner ) {
				CHECK(s_beamUpload[corner].texCoord[0] == (corner >= 2u ? 1.0f : 0.0f));
				CHECK(s_beamUpload[corner].texCoord[1] == (corner & 1u ? 1.0f : 0.0f));
			}
		}
	}
	{
		renderMaterialStageSnapshot_t stages[2] = {{0}};
		const byte pixels[2][4] = {{64,0,0,255},{0,128,0,128}};
		const qhandle_t staged = RenderSubmission_RegisterMaterialImage(&frontend,
			RENDER_ASSET_MATERIAL, "*staged-beam", qfalse, pixels[0], 1u, 1u);
		CHECK(staged > 0);
		for (unsigned i=0u; i<2u; ++i) {
			stages[i].material = RenderSubmission_RegisterStageTexture(&frontend,
				RENDER_ASSET_MATERIAL, i ? "*beam-green" : "*beam-red", qfalse, pixels[i], 1u, 1u);
			CHECK(stages[i].material > 0);
			stages[i].scaleScroll[0] = 2.0f+i; stages[i].scaleScroll[1] = 3.0f+i;
			stages[i].scaleScroll[2] = .25f+i; stages[i].scaleScroll[3] = -.5f-i;
		}
		CHECK(RenderSubmission_SetMaterialStages(&frontend, staged, stages, 2u));
		effectBeam.shader = staged; effectBeam.flags = 0; effectBeam.duration = 0;
		fakeName_t firstTextures[2];
		for (unsigned frame=0u; frame<3u; ++frame) {
			if (frame == 1u) {
				stages[1].animationFrameCount = 2u; stages[1].animationFrequency = 1.0f;
				stages[1].animationLooping = qtrue;
				stages[1].animationMaterials[0] = stages[0].material;
				stages[1].animationMaterials[1] = stages[1].material;
				CHECK(RenderSubmission_SetMaterialStages(&frontend, staged, stages, 2u));
			}
			view.time = 4000 + frame * 1000;
			CHECK(RenderSubmission_BeginFrame(&frontend, 680u+frame));
			CHECK(RenderSubmission_RenderScene(&frontend, &view, 0));
			CHECK(RenderSubmission_AddEffectBeam(&frontend, &effectBeam));
			CHECK(RenderSubmission_EndFrame(&frontend, 680u+frame, &submission));
			s_beamStageDraws = 0u;
			CHECK(RalOpenGl_ProductRender(product, &frontend, &submission, 680u+frame, &productReceipt));
			CHECK(productReceipt.native.effectBeamDrawCount == 1u && productReceipt.native.effectBeamStageDrawCount == 2u);
			CHECK(s_beamStageDraws == 2u && s_beamTextures[0] && s_beamTextures[1]);
			if (!frame) {
				CHECK(s_beamTextures[0] != s_beamTextures[1]);
				memcpy(firstTextures, s_beamTextures, sizeof(firstTextures));
			} else {
				CHECK(s_beamTextures[0] == firstTextures[0]);
				CHECK(s_beamTextures[1] == firstTextures[frame == 1u ? 1u : 0u]);
			}
			for (unsigned i=0u; i<2u; ++i) {
				CHECK(s_beamStageUv[i][0] == 2.0f+i && s_beamStageUv[i][1] == 3.0f+i);
				CHECK(fabsf(s_beamStageUv[i][2] - (2.25f+i)*(4.0f+frame)) < .00001f);
				CHECK(fabsf(s_beamStageUv[i][3] - (-4.5f-i)*(4.0f+frame)) < .00001f);
			}
		}
		worldExact = productReceipt.native; worldExact.effectBeamStageDrawCount = 0u;
		CHECK(!RalOpenGl_WorldReceiptExact(&worldExact, &worldExact));
	}
	/* Real product heightgrid upload, binding and ownership. GPU collision
	 * pixels are a separate native acceptance, not emulated by FakeGL. */
	{
		atmosphereFrameState_t weather = {0};
		float heights[4] = {-64.0f, 128.0f, 256.0f, 512.0f};
		fakeName_t previous = 0;
		weather.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
		weather.flags = ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_HEIGHTGRID;
		weather.qualityTier = ATMOSPHERE_QUALITY_WEATHER;
		weather.gridSize = 2;
		weather.worldMins[0] = -128.0f; weather.worldMins[1] = -64.0f;
		weather.worldMaxs[0] = 128.0f; weather.worldMaxs[1] = 64.0f;
		for (unsigned axis = 0; axis < 3; ++axis) {
			weather.bounds[axis] = -128.0f;
			weather.bounds[axis+3] = 128.0f;
		}
		weather.precipitation[0] = 1.0f;
		weather.indoorExposure = 1.0f; weather.distance = 1024.0f;
		for (unsigned step = 0; step < 8; ++step) {
			const uint64_t frame = 690u + step;
			const unsigned uploads = s_heightgridUploads;
			const unsigned deletesBefore = s_heightgridDeletes;
			CHECK(RenderSubmission_BeginFrame(&frontend, frame));
			weather.timelineSeconds = 10.0f + step * 0.016f;
			CHECK(RenderSubmission_SetAtmosphere(&frontend, &weather));
			if (step == 3 || step == 4) heights[0] += 1.0f;
			if (step) CHECK(RenderSubmission_SetAtmosphereHeightgrid(&frontend, heights, 4));
			CHECK(RenderSubmission_RenderScene(&frontend, &view, 0));
			CHECK(RenderSubmission_EndFrame(&frontend, frame, &submission));
			if (!step || step == 4) {
				const ralOpenGlProductFrameReceipt_t unchanged = productReceipt;
				s_rejectHeightgrid = step == 4;
				CHECK(!RalOpenGl_ProductRender(product, &frontend, &submission, frame, &productReceipt));
				CHECK(!memcmp(&unchanged, &productReceipt, sizeof(unchanged)));
				CHECK(s_heightgridUploads == uploads);
				if (!step) continue; /* Requested but absent: no silent ground fallback. */
				CHECK(s_heightgridTexture == previous);
				s_rejectHeightgrid = qfalse; /* Same sealed frame can retry. */
			}
			qboolean rendered = RalOpenGl_ProductRender(product, &frontend, &submission, frame, &productReceipt);
			if (!rendered) fprintf(stderr, "heightgrid step=%u uploads=%u texture=%u bound=%u error=%x\n",
				step, s_heightgridUploads, s_heightgridTexture, s_weatherHeightgridBinding, s_gl.error);
			CHECK(rendered);
			CHECK(productReceipt.weather.heightgridActive && productReceipt.weatherDispatchCount == 1u);
			CHECK(s_weatherHeightgridBinding && s_weatherHeightgridBinding == s_heightgridTexture);
			CHECK(!memcmp(s_heightgridSamples, heights, sizeof(heights)));
			/* std140 worldMin/max at byte160, inverse step176, grid size184. */
			CHECK(s_weatherFrame[40] == -128.0f && s_weatherFrame[41] == -64.0f);
			CHECK(s_weatherFrame[42] == 128.0f && s_weatherFrame[43] == 64.0f);
			CHECK(s_weatherFrame[44] == 1.0f/128.0f && s_weatherFrame[45] == 1.0f/64.0f);
			uint32_t edge; memcpy(&edge, s_weatherFrame+46, sizeof(edge));
			CHECK(edge == 2u);
			if (step == 2 || step == 6) {
				CHECK(s_heightgridUploads == uploads && s_heightgridTexture == previous);
			} else CHECK(s_heightgridUploads == uploads+1 && s_heightgridTexture != previous);
			CHECK(s_heightgridDeletes == deletesBefore + (step == 3 || step == 4 ? 1u : 0u));
			previous = s_heightgridTexture;
			const unsigned deletes = s_heightgridDeletes;
			RalOpenGl_ProductUnloadHeightgrid(product, RENDER_DISTANT_WORLD_BASE);
			CHECK(s_heightgridTexture == previous && s_heightgridDeletes == deletes);
			if (step == 4) {
				RalOpenGl_ProductUnloadHeightgrid(product, 0);
				CHECK(!s_heightgridTexture && s_heightgridDeletes == deletes+1);
			}
			if (step == 6) {
				CHECK(RalOpenGl_ProductResetLevelResources(product));
				CHECK(!s_heightgridTexture && s_heightgridDeletes == deletes+1);
			}
		}
	}
	/* A registration owns CPU snapshots and their native texture cache, but
	 * not the device/pipelines. Sibling worlds and an open UI frame veto reset. */
	CHECK( !RalOpenGl_ProductResetRegistrationAssets( product, &frontend ) );
	for ( int i = 0; i < MAX_RENDER_WORLDS; ++i )
		CHECK( RenderSubmission_UnloadWorld( &frontend, i ) );
	CHECK( RalOpenGl_ProductResetRegistrationAssets( product, &frontend ) );
	CHECK( !s_heightgridTexture );
	CHECK( s_heightgridUploads == s_heightgridDeletes );
	/* Eight authored stages must bind at the production draw, including the
	 * last animMap slot. Afterwards ordinary UI still uses the main program. */
	{
		renderMaterialStageSnapshot_t stages[8] = {{0}};
		fakeName_t initialTextures[8] = {0};
		material = RenderSubmission_RegisterMaterialImage(&frontend,
			RENDER_ASSET_MATERIAL, "textures/green", qtrue, green, 2u, 2u);
		CHECK(material > 0);
		for (unsigned i = 0; i < 8; ++i) {
			char name[32]; const byte pixel[4] = {(byte)(i*29), 71, 127, 255};
			snprintf(name, sizeof(name), "*sky-stage-%u", i);
			stages[i].material = RenderSubmission_RegisterStageTexture(&frontend,
				RENDER_ASSET_MATERIAL, name, qfalse, pixel, 1u, 1u);
			CHECK(stages[i].material > 0);
			stages[i].scaleScroll[0] = stages[i].scaleScroll[1] = 1.0f;
		}
		CHECK(RenderSubmission_SetMaterialSky(&frontend, material, qtrue));
		CHECK(RenderSubmission_SetMaterialSkyCloudHeight(&frontend, material, 256.0f));
		CHECK(RenderSubmission_SetMaterialStages(&frontend, material, stages, 8u));
		/* Keep a distinct ordinary surface beside the sky in this command
		 * fixture: sky rendering deliberately bypasses surface PBR. */
		const qhandle_t pbrWall = RenderSubmission_RegisterMaterialImage(&frontend,
			RENDER_ASSET_MATERIAL, "textures/pbr-wall", qtrue, green, 2u, 2u);
		CHECK(pbrWall > 0);
		renderMaterialFactorPublicationEntry_t factors = {0};
		factors.material = pbrWall;
		for (unsigned c = 0; c < 4; ++c) factors.factors.baseColor[c] = 1;
		factors.factors.roughness = 0.5f;
		factors.factors.normalScale = factors.factors.occlusionStrength = 1;
		CHECK(RenderSubmission_PublishMaterialFactors(&frontend, 1u, &factors, 1u));
		mapFile_t skyMap = map;
		dsurface_t skySurfaces[2] = {surface, surface};
		dshader_t skyShaders[2] = {shader, {0}};
		strcpy(skyShaders[1].shader, "textures/pbr-wall");
		skySurfaces[1].shaderNum = 1;
		skySurfaces[1].lightmapNum = -1;
		skyMap.numSurfaces = 2; skyMap.surfaces = skySurfaces;
		skyMap.numShaders = 2; skyMap.shaders = skyShaders;
		CHECK(RenderSubmission_LoadWorld(&frontend, &skyMap, 0));
		atmosphere.skyEnvironment.flags &= ~ATMOSPHERE_SKY_DISTANT_SCENE;
		atmosphere.skyEnvironment.distantSceneIdentity = 0;
		CHECK(RenderSubmission_SetAtmosphere(&frontend, &atmosphere));
		CHECK(RalOpenGl_ProductSetDirectionalLighting(product, NULL));
		view.rdflags = 0;
		for (unsigned frame = 0; frame < 4; ++frame) {
			if (frame) {
				stages[7].animationFrameCount = 2;
				stages[7].animationFrequency = 1.0f;
				stages[7].animationLooping = frame != 3;
				stages[7].animationMaterials[0] = stages[0].material;
				stages[7].animationMaterials[1] = stages[7].material;
				CHECK(RenderSubmission_SetMaterialStages(&frontend, material, stages, 8u));
			}
			view.time = 4000 + frame * 1000;
			CHECK(RenderSubmission_BeginFrame(&frontend, 720u + frame));
			CHECK(RenderSubmission_RenderScene(&frontend, &view, 0));
			CHECK(RenderSubmission_AddUiQuad(&frontend, 0, 0, 32, 32,
				0, 0, 1, 1, 0, material));
			CHECK(RenderSubmission_EndFrame(&frontend, 720u + frame, &submission));
			s_skyDraws = 0;
			CHECK(RalOpenGl_ProductRender(product, &frontend, &submission, 720u + frame, &productReceipt));
			CHECK(s_skyDraws == 1 && !s_skyDepthWrite && !s_skyPending);
			CHECK(s_skyParams.meta[0] == 8 && s_skyParams.timeHeight[1] == 256.0f);
			CHECK(fabsf(s_skyParams.timeHeight[0] - (4.0f + frame)) < 0.00001f);
			CHECK(s_skyProgram && s_activeProgram != s_skyProgram);
			CHECK(s_gl.boundTextureByUnit[5] == s_skyTextures[9]);
			if (!frame) {
				memcpy(initialTextures, s_skyTextures, sizeof(initialTextures));
				for (unsigned i = 0; i < 8; ++i) {
					CHECK(initialTextures[i]);
					for (unsigned j = 0; j < i; ++j) CHECK(initialTextures[i] != initialTextures[j]);
				}
			} else {
				for (unsigned i = 0; i < 7; ++i) CHECK(s_skyTextures[i] == initialTextures[i]);
				CHECK(s_skyTextures[7] == initialTextures[frame == 2 ? 0 : 7]);
			}
		}
		/* The radiance source uses that same ordered raster, not the old
		 * outer-only compute shader. Wait for physical LUT completion, admit
		 * only budgeted faces, and freeze animMap time across the six faces. */
		const ralMaterialSourceSky_t skySource = {0};
		CHECK(RenderSubmission_SetMaterialAuthoredSkySource(&frontend, material, &skySource));
		atmosphere.skyEnvironment.mode = ATMOSPHERE_SKY_PHYSICAL;
		atmosphere.skyEnvironment.sourceRevision = 3u;
		atmosphere.skyEnvironment.sunDirection[2] = 1.0f;
		atmosphere.skyEnvironment.sunRadiance[0] = 4.0f;
		atmosphere.skyEnvironment.sunRadiance[1] = 3.8f;
		atmosphere.skyEnvironment.sunRadiance[2] = 3.5f;
		atmosphere.skyEnvironment.sunAngularRadius = 0.00465f;
		atmosphere.skyEnvironment.moonDirection[2] = -1.0f;
		atmosphere.skyEnvironment.moonAngularRadius = 0.00436f;
		atmosphere.skyEnvironment.skyLuminance = 1.0f;
		atmosphere.skyEnvironment.exposure = 1.0f;
		atmosphere.skyEnvironment.opticsVisibility = 0.6f;
		CHECK(RenderSubmission_SetAtmosphere(&frontend, &atmosphere));
		stages[7].animationLooping = qtrue;
		CHECK(RenderSubmission_SetMaterialStages(&frontend, material, stages, 8u));
		s_captureDraws = s_captureMask = 0;
		for (unsigned frame = 0; frame < 16; ++frame) {
			const unsigned draws = s_captureDraws;
			const fakeName_t savedDraw = s_drawFramebuffer, savedRead = s_readFramebuffer;
			view.time = 8000 + frame * 1000;
			CHECK(RenderSubmission_BeginFrame(&frontend, 740u + frame));
			CHECK(RenderSubmission_RenderScene(&frontend, &view, 0));
			CHECK(RenderSubmission_EndFrame(&frontend, 740u + frame, &submission));
			CHECK(RalOpenGl_ProductRender(product, &frontend, &submission, 740u + frame, &productReceipt));
			CHECK(s_captureFace == -1); /* No retained source-cube attachment. */
			CHECK(s_viewport[2] == 1280 && s_viewport[3] == 720);
			CHECK(s_drawFramebuffer == savedDraw && s_readFramebuffer == savedRead);
			CHECK(s_captureDraws - draws <= RAL_SKY_TIER_MAX_RADIANCE_SLICES_PER_FRAME);
			if (!(productReceipt.atmosphere.skyEnvironment.completedProductMask & RAL_SKY_DIRTY_SKY_VIEW))
				CHECK(s_captureDraws == 0);
		}
		CHECK(s_captureDraws == 6u && s_captureMask == 63u);
		for (unsigned face = 0; face < 6; ++face) {
			CHECK(s_captureTimes[face] == s_captureTimes[0]);
			CHECK(s_capturePhysical[face] == 1.0f);
			for (unsigned slot = 0; slot < 10; ++slot)
				CHECK(s_captureTextures[face][slot] && s_captureTextures[face][slot] == s_captureTextures[0][slot]);
			for (unsigned slot = 0; slot < 7; ++slot)
				CHECK(s_captureTextures[face][slot] == initialTextures[slot]);
		}
		/* Finish the first publication with a held clock, then require a
		 * second cohort without a material revision or world reload. */
		for (unsigned frame = 0; frame < 32; ++frame) {
			atmosphere.timelineSeconds = frame < 16 ? 0.0f : 2.0f;
			if (frame >= 16) view.time = 30000;
			CHECK(RenderSubmission_SetAtmosphere(&frontend, &atmosphere));
			CHECK(RenderSubmission_BeginFrame(&frontend, 800u + frame));
			CHECK(RenderSubmission_RenderScene(&frontend, &view, 0));
			CHECK(RenderSubmission_EndFrame(&frontend, 800u + frame, &submission));
			CHECK(RalOpenGl_ProductRender(product, &frontend, &submission, 800u + frame, &productReceipt));
			if (frame == 15) CHECK(s_captureDraws == 6u);
		}
		CHECK(s_captureDraws == 12u);
		for (unsigned face = 0; face < 6; ++face) CHECK(fabsf(s_captureTimes[face] - 30.0f) < 0.00001f);
		/* Warm physical products must not remain bound when the selected
		 * tier withdraws sky lighting; returning to FULL retains ownership. */
		const atmosphereFrameState_t fullAtmosphere = atmosphere;
		for (unsigned phase = 0; phase < 3; ++phase) {
			atmosphere = fullAtmosphere;
			if (phase < 2) {
				atmosphere.qualityTier = phase ? ATMOSPHERE_QUALITY_ANALYTIC : ATMOSPHERE_QUALITY_OFF;
				atmosphere.flags = phase ? ATMOSPHERE_FLAG_ENABLED : 0u;
			}
			CHECK(RenderSubmission_SetAtmosphere(&frontend, &atmosphere));
			for (unsigned frame = 0; frame < 8; ++frame) {
				const unsigned captures = s_captureDraws;
				s_probeTierBindings = phase < 2;
				s_tierBindingsSeen = 0;
				CHECK(RenderSubmission_BeginFrame(&frontend, 840u + phase * 16u + frame));
				CHECK(RenderSubmission_RenderScene(&frontend, &view, 0));
				CHECK(RenderSubmission_EndFrame(&frontend, 840u + phase * 16u + frame, &submission));
				CHECK(RalOpenGl_ProductRender(product, &frontend, &submission,
					840u + phase * 16u + frame, &productReceipt));
				if (phase < 2) {
					CHECK(s_captureDraws == captures && s_tierBindingsSeen == 15u);
					for (unsigned slot = 0; slot < 4; ++slot) CHECK(s_firstTierBindings[slot] == 0u);
				} else CHECK(productReceipt.skyGpuTelemetry.currentRadianceRevision > 0u);
				s_probeTierBindings = qfalse;
			}
		}
		/* Live PBR is a surface-response control, not a sky-cache lifecycle
		 * operation. Observe native material uploads on the warm product. */
		uint64_t pbrRevision = 0;
		for (unsigned phase = 0; phase < 3; ++phase) {
			const unsigned captures = s_captureDraws;
			frontend.pbrEnabled = phase == 1 ? qfalse : qtrue;
			s_pbrMaterialUploads = 0;
			CHECK(RenderSubmission_BeginFrame(&frontend, 888u + phase));
			CHECK(RenderSubmission_RenderScene(&frontend, &view, 0));
			CHECK(RenderSubmission_EndFrame(&frontend, 888u + phase, &submission));
			CHECK(RalOpenGl_ProductRender(product, &frontend, &submission, 888u + phase, &productReceipt));
			if ((s_pbrMaterialUploads > 0) != (phase != 1))
				fprintf(stderr, "PBR phase=%u uploads=%u radiance=%llu\n", phase,
					s_pbrMaterialUploads, (unsigned long long)productReceipt.skyGpuTelemetry.currentRadianceRevision);
			CHECK((s_pbrMaterialUploads > 0) == (phase != 1));
			if (!phase) pbrRevision = productReceipt.skyGpuTelemetry.currentRadianceRevision;
			CHECK(pbrRevision && productReceipt.skyGpuTelemetry.currentRadianceRevision == pbrRevision);
			CHECK(s_captureDraws == captures);
		}
		CHECK(RenderSubmission_PublishMaterialFactors(&frontend, 0u, NULL, 0u));
		/* Mid-capture attachment failure must end its query and detach the
		 * cube even on the error path; no invisible analytic substitution. */
		atmosphere.skyEnvironment.sourceRevision++;
		CHECK(RenderSubmission_SetAtmosphere(&frontend, &atmosphere));
		s_rejectCaptureFace = 2;
		qboolean captureFailed = qfalse;
		for (unsigned frame = 0; frame < 16 && !captureFailed; ++frame) {
			/* Let the previous publication finish its crossfade before the
			 * injected failure may reuse the second product slot. */
			atmosphere.timelineSeconds = 2.0f + (frame + 1) * 0.1f;
			CHECK(RenderSubmission_SetAtmosphere(&frontend, &atmosphere));
			const fakeName_t savedDraw = s_drawFramebuffer, savedRead = s_readFramebuffer;
			const fakeName_t savedProgram = s_activeProgram, savedVao = s_captureVao;
			FakeEnable(0x0c11u);
			CHECK(RenderSubmission_BeginFrame(&frontend, 900u + frame));
			CHECK(RenderSubmission_RenderScene(&frontend, &view, 0));
			CHECK(RenderSubmission_EndFrame(&frontend, 900u + frame, &submission));
			captureFailed = !RalOpenGl_ProductRender(product, &frontend, &submission, 900u + frame, &productReceipt);
			if (captureFailed) {
				CHECK(s_captureFace == -1 && !s_gl.activeTimeElapsedQuery);
				CHECK(s_drawFramebuffer == savedDraw && s_readFramebuffer == savedRead);
				CHECK(s_viewport[2] == 1280 && s_viewport[3] == 720 && s_scissorEnabled);
				CHECK(s_activeProgram == savedProgram && s_captureVao == savedVao);
			}
		}
		CHECK(captureFailed);
		s_rejectCaptureFace = -1; FakeDisable(0x0c11u);
		CHECK(RenderSubmission_UnloadWorld(&frontend, 0));
		CHECK(RalOpenGl_ProductResetRegistrationAssets(product, &frontend));
	}
	{
		qhandle_t lastHandle = 0;
		for ( unsigned cycle = 0; cycle < 20u; ++cycle ) {
			const byte pixel[4] = {(byte)(cycle * 11u), 31, 97, 255};
			const uint32_t deletes = s_deletedTextures, uploads = s_gl.textureUploadCount;
			const qhandle_t handle = RenderSubmission_RegisterMaterialImage( &frontend,
				RENDER_ASSET_MATERIAL, "*registration", qtrue, pixel, 1u, 1u );
			CHECK( handle > lastHandle ); lastHandle = handle;
			CHECK( RenderSubmission_RegisterStageTexture( &frontend,
				RENDER_ASSET_MATERIAL, "*registration-stage", qtrue, pixel, 1u, 1u ) > handle );
			CHECK( RalOpenGl_ProductPrepareMaterialResources( product, &frontend ) );
			CHECK( s_gl.textureUploadCount == uploads + 2u );
			CHECK( RenderSubmission_BeginFrame( &frontend, 800u + cycle ) );
			CHECK( !RalOpenGl_ProductResetRegistrationAssets( product, &frontend ) );
			CHECK( s_deletedTextures == deletes && frontend.materialBytes == 4u );
			RenderSubmission_CancelFrame( &frontend );
			frontend.worlds[1].loaded = qtrue;
			CHECK( !RalOpenGl_ProductResetRegistrationAssets( product, &frontend ) );
			CHECK( s_deletedTextures == deletes );
			frontend.worlds[1].loaded = qfalse;
			CHECK( RalOpenGl_ProductResetRegistrationAssets( product, &frontend ) );
			CHECK( s_deletedTextures == deletes + 2u && !frontend.materialBytes
				&& !frontend.stageTextureBytes && !frontend.registeredAssetCount );
			CHECK( RalOpenGl_ProductResetRegistrationAssets( product, &frontend ) );
			CHECK( s_deletedTextures == deletes + 2u );
		}
	}
	RenderSubmission_Reset( &frontend );
	CHECK( RalOpenGl_ProductSetDirectionalLighting( product, NULL ) );
	CHECK( RalOpenGl_LightingDestroy( coreOwner, &core, lighting,
		&lightingReceipt ) );
	RalOpenGl_ProductDestroy( product );
	CHECK( !s_liveBuffers && !s_bufferLifetimeErrors );
	for ( unsigned weather = 0; weather < 2; ++weather )
		for ( unsigned recover = 0; recover < 2; ++recover )
			CHECK( !CheckAtmosphereAllocationLifecycle( coreOwner, &core,
				&map, (qboolean)weather, (qboolean)recover ) );
	RalOpenGl_CoreDestroy( coreOwner );
	puts( "atmosphere/weather allocation: 56 injected failures, retry/reuse/shutdown: ok" );
	puts( "ral opengl frontend plan contract: ok" );
	return 0;
}
