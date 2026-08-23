// SPDX-License-Identifier: GPL-3.0-or-later

#include "vk_temporal_motion_materialization.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL temporal materialization line %d: %s\n", __LINE__, #x); return 1; } } while (0)

struct ralBackend_s { int id; };
struct ralTexture_s { int id; };
struct ralTextureView_s { int id; };
struct ralBindGroupLayout_s { int id; };
struct ralPipelineLayout_s { int id; };

static ralCaps_t s_caps;
static struct ralTexture_s s_textures[32];
static struct ralTextureView_s s_views[32];
static int s_textureCreates, s_viewCreates, s_textureDestroys, s_viewDestroys;
static int s_capsCalls, s_supportCalls, s_failTextureAt, s_failViewAt;
static int s_layoutCreates, s_layoutDestroys, s_failLayout;
static const ralBindGroupLayout_t *s_capturedLayouts[3];
static const ralBindGroupLayout_t *s_capturedPayloadLayout;
static uint32_t s_capturedPayloadGeneration;
static qboolean s_capturedFog;

const ralCaps_t *Ral_GetCaps( ralBackend_t *backend ) {
	s_capsCalls++;
	return backend ? &s_caps : NULL;
}

qboolean Ral_TextureFormatSupports( ralBackend_t *backend, ralFormat_t format,
		ralTextureUsage_t usage ) {
	const ralTextureUsage_t exact = (ralTextureUsage_t)(
		RAL_TEXTURE_USAGE_COLOR_ATTACHMENT | RAL_TEXTURE_USAGE_SAMPLED
		| RAL_TEXTURE_USAGE_TRANSFER_SRC );
	s_supportCalls++;
	return backend && usage == exact && ( format == RAL_FORMAT_R16G16_SFLOAT
		|| format == RAL_FORMAT_R8_UNORM );
}

ralTexture_t *Ral_CreateTexture( ralBackend_t *backend,
		const ralTextureCreateInfo_t *ci ) {
	int call = ++s_textureCreates;
	(void)backend; (void)ci;
	if ( call == s_failTextureAt ) return NULL;
	s_textures[call].id = call;
	return &s_textures[call];
}

ralTextureView_t *Ral_CreateTextureView( ralBackend_t *backend,
		const ralTextureViewCreateInfo_t *ci ) {
	int call = ++s_viewCreates;
	(void)backend; (void)ci;
	if ( call == s_failViewAt ) return NULL;
	s_views[call].id = call;
	return &s_views[call];
}

void Ral_DestroyTexture( ralTexture_t *texture ) {
	(void)texture; s_textureDestroys++;
}
void Ral_DestroyTextureView( ralTextureView_t *view ) {
	(void)view; s_viewDestroys++;
}

qboolean R_TemporalMotionPayloadGetLayout(
		const temporalMotionPayloadOwner_t *owner,
		const ralBindGroupLayout_t **outLayout, uint32_t *outGeneration ) {
	if ( !owner || !owner->ready || !owner->layout
			|| !owner->layoutAllocationGeneration || !outLayout || !outGeneration )
		return qfalse;
	*outLayout = owner->layout;
	*outGeneration = owner->layoutAllocationGeneration;
	return qtrue;
}

void VK_TemporalPipelineLayoutInit( vkTemporalPipelineLayoutOwner_t *owner ) {
	if ( owner ) memset( owner, 0, sizeof( *owner ) );
}

qboolean VK_TemporalPipelineLayoutEnsure( vkTemporalPipelineLayoutOwner_t *owner,
		ralBackend_t *backend, VkDevice device,
		const ralBindGroupLayout_t *const borrowedLayouts[3],
		const ralBindGroupLayout_t *payloadLayout,
		uint32_t payloadLayoutGeneration, qboolean fog,
		const vkTemporalLayoutOps_t *ops ) {
	uint32_t i;
	(void)ops;
	if ( !owner || !backend || device == VK_NULL_HANDLE || !borrowedLayouts
			|| !payloadLayout || !payloadLayoutGeneration ) return qfalse;
	for ( i = 0; i < 3; ++i ) if ( !borrowedLayouts[i] ) return qfalse;
	if ( owner->ready && owner->backend == backend && owner->device == device
			&& owner->payloadLayout == payloadLayout
			&& owner->payloadLayoutGeneration == payloadLayoutGeneration
			&& owner->fog == fog
			&& memcmp( owner->borrowedLayouts, borrowedLayouts,
				sizeof( owner->borrowedLayouts ) ) == 0 ) return qtrue;
	if ( owner->allocationGeneration == UINT32_MAX || s_failLayout ) return qfalse;
	s_layoutCreates++;
	owner->backend = backend;
	owner->device = device;
	memcpy( owner->borrowedLayouts, borrowedLayouts,
		sizeof( owner->borrowedLayouts ) );
	owner->payloadLayout = payloadLayout;
	owner->payloadLayoutGeneration = payloadLayoutGeneration;
	owner->raw = (VkPipelineLayout)(uintptr_t)(0x100u + (uint32_t)s_layoutCreates);
	owner->adopted = (ralPipelineLayout_t *)(uintptr_t)(0x200u + (uint32_t)s_layoutCreates);
	owner->allocationGeneration++;
	owner->fog = fog;
	owner->ready = qtrue;
	memcpy( s_capturedLayouts, borrowedLayouts, sizeof( s_capturedLayouts ) );
	s_capturedPayloadLayout = payloadLayout;
	s_capturedPayloadGeneration = payloadLayoutGeneration;
	s_capturedFog = fog;
	return qtrue;
}

qboolean VK_TemporalPipelineLayoutRelease( vkTemporalPipelineLayoutOwner_t *owner,
		const vkTemporalLayoutOps_t *ops ) {
	uint32_t generation;
	(void)ops;
	if ( !owner || owner->leases ) return qfalse;
	generation = owner->allocationGeneration;
	if ( owner->raw != VK_NULL_HANDLE || owner->adopted ) s_layoutDestroys++;
	memset( owner, 0, sizeof( *owner ) );
	owner->allocationGeneration = generation;
	return qtrue;
}

static vkTemporalMotionMaterializationInput_t MakeInput(
		struct ralBackend_s *backend, temporalMotionPayloadOwner_t *payload ) {
	vkTemporalMotionMaterializationInput_t input;
	memset( &input, 0, sizeof( input ) );
	input.backend = backend;
	input.device = (VkDevice)(uintptr_t)0x55;
	input.borrowedLayouts[0] = (ralBindGroupLayout_t *)(uintptr_t)0x10;
	input.borrowedLayouts[1] = (ralBindGroupLayout_t *)(uintptr_t)0x11;
	input.borrowedLayouts[2] = (ralBindGroupLayout_t *)(uintptr_t)0x12;
	input.payload = payload;
	input.worldIndex = 1;
	input.width = 1280;
	input.height = 720;
	input.topologyEpoch = 3;
	input.planGeneration = 4;
	input.fog = qfalse;
	return input;
}

int main( void ) {
	struct ralBackend_s backend = { 1 };
	struct ralBindGroupLayout_s payloadLayoutA = { 1 }, payloadLayoutB = { 2 };
	temporalMotionPayloadOwner_t payload;
	vkTemporalMotionMaterialization_t owner, before;
	vkTemporalMotionMaterializationInput_t input;
	vkTemporalMotionMaterializationReceipt_t receipt, receiptBefore;
	vkTemporalMotionMaterializationProductView_t productView, productViewBefore;
	vkTemporalLayoutOps_t ops;
	uint32_t stableGeneration;
	int tc, vc, cc, sc, lc, td, vd, ld;

	memset( &s_caps, 0, sizeof( s_caps ) );
	s_caps.independentBlend = qtrue;
	s_caps.maxColorAttachments = 8;
	s_caps.maxTextureDimension2D = 4096;
	memset( &payload, 0, sizeof( payload ) );
	payload.ready = qtrue;
	payload.layout = &payloadLayoutA;
	payload.layoutAllocationGeneration = 1;
	memset( &ops, 0, sizeof( ops ) );
	input = MakeInput( &backend, &payload );
	VK_TemporalMotionMaterializationInit( &owner );
	CHECK( owner.initialized && !VK_TemporalMotionMaterializationHasLive( &owner ) );
	memset( &receipt, 0xa5, sizeof( receipt ) ); receiptBefore = receipt;
	CHECK( !VK_TemporalMotionMaterializationGetReceipt( &owner, &receipt )
		&& memcmp( &receipt, &receiptBefore, sizeof( receipt ) ) == 0 );

	CHECK( !VK_TemporalMotionMaterializationNeedsIdle( &owner, &input ) );
	CHECK( VK_TemporalMotionMaterializationEnsureAfterFence(
		&owner, &input, qfalse, &ops ) );
	CHECK( owner.ready && VK_TemporalMotionMaterializationHasLive( &owner ) );
	CHECK( owner.targets.ready && owner.pipelineLayout.ready
		&& owner.allocationGeneration == 1 );
	CHECK( s_capturedPayloadLayout == &payloadLayoutA
		&& s_capturedPayloadGeneration == 1 && !s_capturedFog );
	CHECK( memcmp( s_capturedLayouts, input.borrowedLayouts,
		sizeof( s_capturedLayouts ) ) == 0 );
	CHECK( VK_TemporalMotionMaterializationGetReceipt( &owner, &receipt ) );
	CHECK( receipt.ready && receipt.width == 1280 && receipt.height == 720
		&& receipt.payloadLayoutGeneration == 1
		&& receipt.targetAllocationGeneration == 1
		&& receipt.pipelineLayoutAllocationGeneration == 1
		&& receipt.allocationGeneration == 1 );
	memset( &productView, 0xa5, sizeof( productView ) );
	CHECK( VK_TemporalMotionMaterializationGetProductView( &owner, &receipt,
		(ralTexture_t *)(uintptr_t)0x7000, (ralTexture_t *)(uintptr_t)0x8000,
		&productView ) );
	CHECK( productView.scene == (ralTexture_t *)(uintptr_t)0x7000
		&& productView.depth == (ralTexture_t *)(uintptr_t)0x8000
		&& productView.velocity == owner.targets.velocity
		&& productView.velocityView == owner.targets.velocityView
		&& productView.validity == owner.targets.validity
		&& productView.validityView == owner.targets.validityView
		&& productView.pipelineLayout == owner.pipelineLayout.adopted
		&& productView.rawPipelineLayout == owner.pipelineLayout.raw );
	productViewBefore = productView; receipt.allocationGeneration++;
	CHECK( !VK_TemporalMotionMaterializationGetProductView( &owner, &receipt,
		(ralTexture_t *)(uintptr_t)0x7000, (ralTexture_t *)(uintptr_t)0x8000,
		&productView )
		&& memcmp( &productView, &productViewBefore, sizeof( productView ) ) == 0 );
	receipt.allocationGeneration--;

	tc=s_textureCreates;vc=s_viewCreates;cc=s_capsCalls;sc=s_supportCalls;lc=s_layoutCreates;
	CHECK( VK_TemporalMotionMaterializationEnsureAfterFence(
		&owner, &input, qfalse, &ops ) );
	CHECK( s_textureCreates==tc && s_viewCreates==vc && s_capsCalls==cc
		&& s_supportCalls==sc && s_layoutCreates==lc );
	stableGeneration = owner.allocationGeneration;
	input.worldIndex = 2; input.planGeneration = 5;
	CHECK( VK_TemporalMotionMaterializationEnsureAfterFence(
		&owner, &input, qfalse, &ops ) );
	CHECK( owner.allocationGeneration == stableGeneration
		&& s_textureCreates==tc && s_layoutCreates==lc );

	input.width = 1600;
	before = owner;
	CHECK( VK_TemporalMotionMaterializationNeedsIdle( &owner, &input ) );
	CHECK( !VK_TemporalMotionMaterializationEnsureAfterFence(
		&owner, &input, qfalse, &ops ) );
	CHECK( !owner.ready && owner.targets.velocity == before.targets.velocity );
	memset( &receipt, 0x5a, sizeof( receipt ) ); receiptBefore = receipt;
	CHECK( !VK_TemporalMotionMaterializationGetReceipt( &owner, &receipt )
		&& memcmp( &receipt, &receiptBefore, sizeof( receipt ) ) == 0 );
	s_failTextureAt = s_textureCreates + 1;
	CHECK( !VK_TemporalMotionMaterializationEnsureAfterFence(
		&owner, &input, qtrue, &ops ) );
	CHECK( !owner.ready && owner.targets.velocity == before.targets.velocity );
	s_failTextureAt = 0;
	CHECK( VK_TemporalMotionMaterializationEnsureAfterFence(
		&owner, &input, qtrue, &ops ) );
	CHECK( owner.ready && owner.targets.width == 1600
		&& owner.allocationGeneration == stableGeneration + 1 );

	payload.layout = &payloadLayoutB;
	payload.layoutAllocationGeneration = 2;
	input.planGeneration = 6;
	CHECK( VK_TemporalMotionMaterializationNeedsIdle( &owner, &input ) );
	s_failLayout = 1; before = owner;
	CHECK( !VK_TemporalMotionMaterializationEnsureAfterFence(
		&owner, &input, qtrue, &ops ) );
	CHECK( !owner.ready && owner.targets.ready
		&& owner.pipelineLayout.raw == before.pipelineLayout.raw );
	s_failLayout = 0;
	CHECK( VK_TemporalMotionMaterializationEnsureAfterFence(
		&owner, &input, qtrue, &ops ) );
	CHECK( owner.ready && owner.payloadLayout == &payloadLayoutB
		&& owner.payloadLayoutGeneration == 2
		&& owner.pipelineLayout.allocationGeneration == 2 );

	before = owner;
	CHECK( !VK_TemporalMotionMaterializationReleaseAfterIdle(
		&owner, qfalse, &ops ) && memcmp( &owner, &before, sizeof( owner ) ) == 0 );
	td=s_textureDestroys;vd=s_viewDestroys;ld=s_layoutDestroys;
	CHECK( VK_TemporalMotionMaterializationReleaseAfterIdle(
		&owner, qtrue, &ops ) );
	CHECK( !owner.ready && !VK_TemporalMotionMaterializationHasLive( &owner )
		&& s_layoutDestroys==ld+1 && s_textureDestroys==td+2
		&& s_viewDestroys==vd+2 );
	CHECK( owner.allocationGeneration == 3
		&& owner.targets.allocationGeneration == 2
		&& owner.pipelineLayout.allocationGeneration == 2 );
	CHECK( VK_TemporalMotionMaterializationReleaseAfterIdle(
		&owner, qtrue, &ops ) );

	CHECK( VK_TemporalMotionMaterializationEnsureAfterFence(
		&owner, &input, qfalse, &ops ) );
	CHECK( owner.allocationGeneration == 4
		&& owner.targets.allocationGeneration == 3
		&& owner.pipelineLayout.allocationGeneration == 3 );
	owner.allocationGeneration = UINT32_MAX;
	input.planGeneration++;
	tc=s_textureCreates;lc=s_layoutCreates;
	CHECK( VK_TemporalMotionMaterializationEnsureAfterFence(
		&owner, &input, qfalse, &ops ) );
	CHECK( owner.ready && owner.allocationGeneration == UINT32_MAX
		&& s_textureCreates==tc && s_layoutCreates==lc );
	input.width++;
	CHECK( !VK_TemporalMotionMaterializationEnsureAfterFence(
		&owner, &input, qtrue, &ops ) );
	CHECK( !owner.ready && s_textureCreates==tc && s_layoutCreates==lc );

	puts( "PASS inert temporal motion materialization contract" );
	return 0;
}
