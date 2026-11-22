// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_bootstrap_aa.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../code/render/ral/backends/vulkan/include/vulkan/vulkan.h"

#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL BootstrapAa %d: %s\n",__LINE__,#x);return 1;} }while(0)

static uintptr_t nextHandle = 0x100000;
static uint32_t creates, destroys, failStep, eventCount;
static ralBindingValue_t values[2][6];
static uint32_t groupCount;
static vkTemporalHistoryStorePush_t gotPush;
static struct { char k; const void *o; uint32_t a, b, c; } events[24];

static void *H(uintptr_t x) { return (void *)x; }
static void *New(void) {
	++creates;
	if ( creates == failStep ) return NULL;
	nextHandle += 0x100;
	return (void *)nextHandle;
}
static void E(char k, const void *o, uint32_t a, uint32_t b, uint32_t c) {
	if ( eventCount < 24 ) { events[eventCount].k = k; events[eventCount].o = o;
		events[eventCount].a = a; events[eventCount].b = b; events[eventCount].c = c; }
	++eventCount;
}
ralTextureView_t *Ral_CreateTextureView( ralBackend_t *b, const ralTextureViewCreateInfo_t *c ) { (void)b; (void)c; return New(); }
ralSampler_t *Ral_CreateSampler( ralBackend_t *b, const ralSamplerCreateInfo_t *c ) { (void)b; (void)c; return New(); }
ralBindGroupLayout_t *Ral_CreateBindGroupLayout( ralBackend_t *b, const ralBindGroupLayoutCreateInfo_t *c ) {
	(void)b; if ( !c || c->numEntries != 6 ) return NULL;
	if ( c->entries[3].type != RAL_BIND_STORAGE_TEXTURE || c->entries[4].type != RAL_BIND_STORAGE_TEXTURE
			|| c->entries[5].type != RAL_BIND_STORAGE_TEXTURE ) return NULL;
	return New();
}
ralBindGroup_t *Ral_CreateBindGroup( ralBackend_t *b, const ralBindGroupCreateInfo_t *c ) {
	(void)b;
	if ( c && groupCount < 2 && c->numValues == 6 ) memcpy( values[groupCount++], c->values, sizeof( values[0] ) );
	return New();
}
ralPipeline_t *Ral_CreateComputePipeline( ralBackend_t *b, const ralComputePipelineCreateInfo_t *c ) {
	(void)b; if ( !c || c->pushConstantSize != sizeof( vkTemporalHistoryStorePush_t ) ) return NULL;
	return New();
}
void Ral_DestroyTextureView( ralTextureView_t *p ) { (void)p; ++destroys; }
void Ral_DestroySampler( ralSampler_t *p ) { (void)p; ++destroys; }
void Ral_DestroyBindGroupLayout( ralBindGroupLayout_t *p ) { (void)p; ++destroys; }
void Ral_DestroyBindGroup( ralBindGroup_t *p ) { (void)p; ++destroys; }
void Ral_DestroyPipeline( ralPipeline_t *p ) { (void)p; ++destroys; }
void Ral_CmdTransitionTexture( ralCommandBuffer_t *c, ralTexture_t *t, uint32_t a, uint32_t b, VkImageLayout l ) { (void)c; E( 'T', t, a, b, (uint32_t)l ); }
void Ral_CmdBindPipeline( ralCommandBuffer_t *c, ralPipeline_t *p ) { (void)c; E( 'P', p, 0, 0, 0 ); }
void Ral_CmdBindBindGroup( ralCommandBuffer_t *c, uint32_t s, ralBindGroup_t *g ) { (void)c; E( 'G', g, s, 0, 0 ); }
void Ral_CmdPushConstants( ralCommandBuffer_t *c, uint32_t st, uint32_t o, uint32_t n, const void *d ) {
	(void)c; E( 'U', NULL, (uint32_t)st, n, o ); if ( n == sizeof( gotPush ) ) memcpy( &gotPush, d, n );
}
void Ral_CmdDispatch( ralCommandBuffer_t *c, uint32_t x, uint32_t y, uint32_t z ) { (void)c; E( 'D', NULL, x, y, z ); }

static vkTemporalBootstrapAaKey_t Key( uintptr_t base ) {
	vkTemporalBootstrapAaKey_t k;
	uint32_t i;
	static const uint32_t spirv[4] = { 1, 2, 3, 4 };
	memset( &k, 0, sizeof( k ) );
	k.backend = H( base + 1 ); k.width = 1280; k.height = 720;
	k.historyAllocationGeneration = 2; k.sceneColorAttachmentGeneration = 3;
	k.resolvedTargetAllocationGeneration = 4;
	k.currentColor = H( base + 0x10 ); k.currentDepth = H( base + 0x20 );
	k.resolvedTarget = H( base + 0x30 ); k.resolvedTargetView = H( base + 0x40 );
	for ( i = 0; i < 2; ++i ) {
		k.historyColor[i] = H( base + 0x100 + i * 0x10 ); k.historyColorView[i] = H( base + 0x108 + i * 0x10 );
		k.historyDepth[i] = H( base + 0x200 + i * 0x10 ); k.historyDepthView[i] = H( base + 0x208 + i * 0x10 );
	}
	k.computeSpirv = spirv; k.computeSpirvSize = sizeof( spirv );
	return k;
}

int main( void ) {
	vkTemporalBootstrapAaOwner_t o, before;
	vkTemporalBootstrapAaKey_t k = Key( 0x1000 ), k2;
	vkTemporalHistoryStorePush_t p = { { 1280, 720 }, 4.0f, 8192.0f };
	uint32_t c0;

	VK_TemporalBootstrapAaInit( &o );
	CHECK( o.initialized && !VK_TemporalBootstrapAaHasLive( &o ) );
	CHECK( !VK_TemporalBootstrapAaEnsureAfterFence( &o, NULL, qfalse ) );
	CHECK( VK_TemporalBootstrapAaEnsureAfterFence( &o, &k, qfalse ) );
	CHECK( o.ready && groupCount == 2 && VK_TemporalBootstrapAaMatches( &o, &k ) );
	// Each group commits the history pair of its own index; the resolved target is written by both.
	CHECK( values[0][0].textureView == o.currentColorView && values[0][1].textureView == o.currentDepthView
		&& values[0][2].sampler == o.sampler && values[0][3].textureView == k.resolvedTargetView
		&& values[0][4].textureView == k.historyColorView[0] && values[0][5].textureView == k.historyDepthView[0]
		&& values[1][3].textureView == k.resolvedTargetView
		&& values[1][4].textureView == k.historyColorView[1] && values[1][5].textureView == k.historyDepthView[1] );
	before = o;
	CHECK( VK_TemporalBootstrapAaEnsureAfterFence( &o, &k, qfalse ) && memcmp( &o, &before, sizeof( o ) ) == 0 );

	eventCount = 0;
	CHECK( VK_TemporalBootstrapAaRecord( &o, &k, H( 0x9000 ), 0, &p ) );
	CHECK( eventCount == 10 );
	CHECK( events[0].k == 'T' && events[0].o == k.currentColor && events[0].c == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	CHECK( events[1].k == 'T' && events[1].o == k.resolvedTarget && events[1].c == VK_IMAGE_LAYOUT_GENERAL );
	CHECK( events[2].o == k.historyColor[0] && events[2].c == VK_IMAGE_LAYOUT_GENERAL );
	CHECK( events[3].o == k.historyDepth[0] && events[3].c == VK_IMAGE_LAYOUT_GENERAL );
	CHECK( events[4].k == 'P' && events[4].o == o.pipeline );
	CHECK( events[5].k == 'G' && events[5].o == o.groups[0] && events[5].a == 0 );
	CHECK( events[6].k == 'U' && events[6].a == RAL_STAGE_COMPUTE && events[6].b == sizeof( p ) && memcmp( &gotPush, &p, sizeof( p ) ) == 0 );
	CHECK( events[7].k == 'D' && events[7].a == 160 && events[7].b == 90 && events[7].c == 1 );
	CHECK( events[8].o == k.resolvedTarget && events[8].c == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	CHECK( events[9].o == k.currentColor && events[9].c == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	// The history pair stays in the storage layout on purpose: the store pass transitions it to the sampled layout.

	c0 = eventCount;
	CHECK( !VK_TemporalBootstrapAaRecord( &o, &k, H( 1 ), 2, &p ) && eventCount == c0 );
	p.zNear = NAN;
	CHECK( !VK_TemporalBootstrapAaRecord( &o, &k, H( 1 ), 0, &p ) && eventCount == c0 );
	p.zNear = 4.0f; p.extent[0]++;
	CHECK( !VK_TemporalBootstrapAaRecord( &o, &k, H( 1 ), 0, &p ) && eventCount == c0 );
	p.extent[0] = 1280;
	k2 = k; k2.sceneColorAttachmentGeneration++;
	CHECK( !VK_TemporalBootstrapAaRecord( &o, &k2, H( 1 ), 0, &p ) && eventCount == c0 );

	// A different key needs the device idle before the live resources may be replaced.
	k2 = Key( 0x2000 );
	CHECK( VK_TemporalBootstrapAaNeedsIdle( &o, &k2 ) );
	CHECK( !VK_TemporalBootstrapAaEnsureAfterFence( &o, &k2, qfalse ) && VK_TemporalBootstrapAaMatches( &o, &k ) );
	CHECK( VK_TemporalBootstrapAaEnsureAfterFence( &o, &k2, qtrue ) && VK_TemporalBootstrapAaMatches( &o, &k2 ) );
	CHECK( destroys == 7 );

	// A failed creation leaves nothing live.
	groupCount = 0; destroys = 0;
	k = Key( 0x3000 );
	failStep = creates + 4;
	CHECK( !VK_TemporalBootstrapAaEnsureAfterFence( &o, &k, qtrue ) );
	CHECK( !o.ready && !VK_TemporalBootstrapAaHasLive( &o ) );
	failStep = 0;

	// Duplicate roles are refused.
	k2 = Key( 0x4000 ); k2.resolvedTargetView = k2.historyColorView[0];
	CHECK( !VK_TemporalBootstrapAaEnsureAfterFence( &o, &k2, qtrue ) );

	CHECK( VK_TemporalBootstrapAaEnsureAfterFence( &o, &k, qtrue ) );
	CHECK( !VK_TemporalBootstrapAaReleaseAfterIdle( &o, qfalse ) && VK_TemporalBootstrapAaHasLive( &o ) );
	CHECK( VK_TemporalBootstrapAaReleaseAfterIdle( &o, qtrue ) && !VK_TemporalBootstrapAaHasLive( &o ) );
	printf( "vk temporal bootstrap AA owner: PASS\n" );
	return 0;
}
