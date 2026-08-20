// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_resolved_hdr.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed: %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; } } while ( 0 )

static ralCaps_t caps;
static uintptr_t nextHandle = 0x10000u;
static uint32_t creates, capsQueries, formatQueries;
static uint32_t textureDestroys, viewDestroys, samplerDestroys, groupDestroys;
static uint32_t failStep;
static void *aliasAtStep;
static ralTextureCreateInfo_t lastTextureInfo;
static ralTextureViewCreateInfo_t lastViewInfo;
static ralSamplerCreateInfo_t lastSamplerInfo;
static ralBindGroupCreateInfo_t groupInfos[2];
static ralBindingValue_t groupValues[2][3];
typedef struct { char role; const void *pointer; } destroyEvent_t;
static destroyEvent_t destroyTrace[512];
static uint32_t destroyTraceCount;

static void TraceDestroy( char role, const void *pointer ) {
	if ( destroyTraceCount < 512 ) {
		destroyTrace[destroyTraceCount].role = role;
		destroyTrace[destroyTraceCount].pointer = pointer;
		++destroyTraceCount;
	}
}

static qboolean TraceContainsPointer( uint32_t start, const void *pointer ) {
	for ( uint32_t i = start; i < destroyTraceCount && i < 512; ++i )
		if ( destroyTrace[i].pointer == pointer ) return qtrue;
	return qfalse;
}

static uint32_t TracePointerCount( uint32_t start, const void *pointer ) {
	uint32_t count = 0;
	for ( uint32_t i = start; i < destroyTraceCount && i < 512; ++i )
		if ( destroyTrace[i].pointer == pointer ) ++count;
	return count;
}

static void *NewHandle( void ) {
	++creates;
	if ( aliasAtStep && failStep == creates ) return aliasAtStep;
	if ( failStep == creates ) return NULL;
	nextHandle += 0x100u;
	return (void *)nextHandle;
}

const ralCaps_t *Ral_GetCaps( ralBackend_t *backend ) {
	(void)backend; ++capsQueries; return &caps;
}
qboolean Ral_TextureFormatSupports( ralBackend_t *backend, ralFormat_t format,
		ralTextureUsage_t usage ) {
	(void)backend; ++formatQueries;
	return format == RAL_FORMAT_R16G16B16A16_SFLOAT &&
		usage == ( RAL_TEXTURE_USAGE_STORAGE | RAL_TEXTURE_USAGE_SAMPLED |
			RAL_TEXTURE_USAGE_COLOR_ATTACHMENT | RAL_TEXTURE_USAGE_TRANSFER_DST |
			RAL_TEXTURE_USAGE_TRANSFER_SRC ) ? qtrue : qfalse;
}
ralTexture_t *Ral_CreateTexture( ralBackend_t *backend,
		const ralTextureCreateInfo_t *ci ) {
	(void)backend; if ( ci ) lastTextureInfo = *ci;
	return (ralTexture_t *)NewHandle();
}
ralTextureView_t *Ral_CreateTextureView( ralBackend_t *backend,
		const ralTextureViewCreateInfo_t *ci ) {
	(void)backend; if ( ci ) lastViewInfo = *ci;
	return (ralTextureView_t *)NewHandle();
}
ralSampler_t *Ral_CreateSampler( ralBackend_t *backend,
		const ralSamplerCreateInfo_t *ci ) {
	(void)backend; if ( ci ) lastSamplerInfo = *ci;
	return (ralSampler_t *)NewHandle();
}
ralBindGroup_t *Ral_CreateBindGroup( ralBackend_t *backend,
		const ralBindGroupCreateInfo_t *ci ) {
	uint32_t index = creates == 3 ? 0u : 1u;
	(void)backend;
	if ( ci && index < 2 ) {
		groupInfos[index] = *ci;
		memcpy( groupValues[index], ci->values,
			ci->numValues * sizeof( ci->values[0] ) );
		groupInfos[index].values = groupValues[index];
	}
	return (ralBindGroup_t *)NewHandle();
}
void Ral_DestroyTexture( ralTexture_t *texture ) {
	TraceDestroy( 'T', texture ); ++textureDestroys;
}
void Ral_DestroyTextureView( ralTextureView_t *view ) {
	TraceDestroy( 'V', view ); ++viewDestroys;
}
void Ral_DestroySampler( ralSampler_t *sampler ) {
	TraceDestroy( 'S', sampler ); ++samplerDestroys;
}
void Ral_DestroyBindGroup( ralBindGroup_t *group ) {
	TraceDestroy( 'G', group ); ++groupDestroys;
}

static vkTemporalResolvedHdrInput_t MakeInput( void ) {
	vkTemporalResolvedHdrInput_t input;
	memset( &input, 0, sizeof( input ) );
	input.backend = (ralBackend_t *)(uintptr_t)0x100u;
	input.currentSceneColor = (ralTexture_t *)(uintptr_t)0x200u;
	input.currentPostprocessGroup = (ralBindGroup_t *)(uintptr_t)0x210u;
	input.currentHistogramGroup = (ralBindGroup_t *)(uintptr_t)0x220u;
	input.postprocessLayout = (ralBindGroupLayout_t *)(uintptr_t)0x300u;
	input.histogramLayout = (ralBindGroupLayout_t *)(uintptr_t)0x400u;
	input.histogramBuffer = (ralBuffer_t *)(uintptr_t)0x500u;
	input.worldIndex = 0;
	input.width = 1280; input.height = 720;
	input.topologyEpoch = 3; input.planGeneration = 7;
	input.sceneColorAttachmentGeneration = 11;
	input.sceneFormat = RAL_FORMAT_R16G16B16A16_SFLOAT;
	input.filter = RAL_FILTER_LINEAR;
	return input;
}

/* Field-wise equality for vkHdrPostprocessSource_t.
 *
 * NOT memcmp over sizeof, and the difference is not pedantry. The struct is 96
 * bytes but its fields occupy 92 (four pointers and two uint64_ts, then eleven
 * four-byte fields), so the last 4 bytes are tail padding. C11 6.2.6.1p6 leaves
 * padding bytes unspecified, and every copy here — `return source;` out of a
 * Make* helper, `source = *current;` and `*outSource = source;` inside
 * VK_TemporalResolvedHdrRouteSource — is free to leave them holding whatever
 * was already there.
 *
 * A byte comparison therefore tests the optimiser, not the routine. Measured on
 * this tree: VK_TemporalResolvedHdrRouteSource returned qtrue and all 92 field
 * bytes matched, while offsets 92-95 held cc cc cc cc against a0 15 00 00 —
 * a failure on bytes no field owns. gcc 16.1 copies the padding at -O0 and -Os
 * but not at -O1/-O2/-O3; clang copies it at every level, which is why this
 * passed on macOS and failed on Windows release builds.
 *
 * Comparing the fields the struct actually declares is both portable and a
 * stricter statement of intent: it says which values must agree, rather than
 * asserting that two objects are byte-identical, which C never promised. */
static qboolean SourceEquals( const vkHdrPostprocessSource_t *a,
		const vkHdrPostprocessSource_t *b ) {
	return (qboolean)( a->backend == b->backend
		&& a->attachment == b->attachment
		&& a->postprocessGroup == b->postprocessGroup
		&& a->histogramGroup == b->histogramGroup
		&& a->batchToken == b->batchToken
		&& a->frameId == b->frameId
		&& a->commandSlot == b->commandSlot
		&& a->frameCount == b->frameCount
		&& a->worldIndex == b->worldIndex
		&& a->width == b->width
		&& a->height == b->height
		&& a->topologyEpoch == b->topologyEpoch
		&& a->planGeneration == b->planGeneration
		&& a->sceneColorAttachmentGeneration == b->sceneColorAttachmentGeneration
		&& a->targetAllocationGeneration == b->targetAllocationGeneration
		&& a->sceneFormat == b->sceneFormat
		&& a->resolved == b->resolved );
}

static vkHdrPostprocessSource_t MakeCurrent(
		const vkTemporalResolvedHdrInput_t *input ) {
	vkHdrPostprocessSource_t source;
	memset( &source, 0, sizeof( source ) );
	source.backend = input->backend;
	source.attachment = input->currentSceneColor;
	source.postprocessGroup = input->currentPostprocessGroup;
	source.histogramGroup = input->currentHistogramGroup;
	source.batchToken = 101; source.frameId = 202;
	source.commandSlot = 1; source.frameCount = 3;
	source.worldIndex = input->worldIndex;
	source.width = input->width; source.height = input->height;
	source.topologyEpoch = input->topologyEpoch;
	source.planGeneration = input->planGeneration;
	source.sceneColorAttachmentGeneration =
		input->sceneColorAttachmentGeneration;
	source.sceneFormat = input->sceneFormat;
	return source;
}

static vkTemporalResolvedHdrContentReceipt_t MakeContent(
		const vkHdrPostprocessSource_t *current,
		const vkTemporalResolvedHdrReceipt_t *target ) {
	vkTemporalResolvedHdrContentReceipt_t content;
	memset( &content, 0, sizeof( content ) );
	content.batchToken = current->batchToken;
	content.frameId = current->frameId;
	content.contentSerial = 1;
	content.commandSlot = current->commandSlot;
	content.frameCount = current->frameCount;
	content.worldIndex = target->worldIndex;
	content.width = target->width; content.height = target->height;
	content.topologyEpoch = target->topologyEpoch;
	content.planGeneration = target->planGeneration;
	content.sceneColorAttachmentGeneration =
		target->sceneColorAttachmentGeneration;
	content.targetAllocationGeneration = target->allocationGeneration;
	content.backend = target->backend;
	content.sourceSceneColor = target->currentSceneColor;
	content.sourcePostprocessGroup = target->currentPostprocessGroup;
	content.sourceHistogramGroup = target->currentHistogramGroup;
	content.target = target->target;
	content.targetView = target->targetView;
	content.postprocessGroup = target->postprocessGroup;
	content.histogramGroup = target->histogramGroup;
	content.sceneFormat = target->sceneFormat;
	content.producer = VK_TEMPORAL_RESOLVED_HDR_PRODUCER_COPY;
	content.valid = qtrue;
	return content;
}

int main( void ) {
	vkTemporalResolvedHdrOwner_t owner, before;
	vkTemporalResolvedHdrInput_t input, changed;
	vkTemporalResolvedHdrReceipt_t receipt, receiptBefore, targetMutation;
	vkTemporalResolvedHdrContentReceipt_t content, contentMutation, submittedContent,
		submittedBefore;
	vkHdrPostprocessSource_t current, routed, routedBefore;
	uint32_t baseCaps, baseFormats, baseCreates;

	memset( &caps, 0, sizeof( caps ) );
	caps.maxTextureDimension2D = 8192;
	input = MakeInput();
	VK_TemporalResolvedHdrInit( &owner );
	CHECK( owner.initialized && !VK_TemporalResolvedHdrHasLive( &owner ) );
	memset( &receipt, 0xa5, sizeof( receipt ) ); receiptBefore = receipt;
	CHECK( !VK_TemporalResolvedHdrGetReceipt( &owner, &receipt ) );
	CHECK( memcmp( &receipt, &receiptBefore, sizeof( receipt ) ) == 0 );
	CHECK( !VK_TemporalResolvedHdrEnsureAfterFence( &owner, NULL, qfalse ) );
	changed = input; changed.sceneFormat = RAL_FORMAT_R8G8B8A8_UNORM;
	CHECK( !VK_TemporalResolvedHdrEnsureAfterFence( &owner, &changed, qfalse ) );
	CHECK( !VK_TemporalResolvedHdrHasLive( &owner ) );

	CHECK( VK_TemporalResolvedHdrEnsureAfterFence( &owner, &input, qfalse ) );
	CHECK( VK_TemporalResolvedHdrGetReceipt( &owner, &receipt ) );
	CHECK( receipt.ready && receipt.allocationGeneration == 1 );
	CHECK( lastTextureInfo.width == 1280 && lastTextureInfo.height == 720 );
	CHECK( lastTextureInfo.type == RAL_TEXTURE_2D
		&& lastTextureInfo.memory == RAL_MEMORY_DEVICE_LOCAL
		&& lastTextureInfo.depthOrArrayLayers == 1
		&& lastTextureInfo.mipLevels == 1 && lastTextureInfo.sampleCount == 1 );
	CHECK( lastTextureInfo.format == RAL_FORMAT_R16G16B16A16_SFLOAT );
	CHECK( lastTextureInfo.usage == ( RAL_TEXTURE_USAGE_STORAGE
		| RAL_TEXTURE_USAGE_SAMPLED | RAL_TEXTURE_USAGE_COLOR_ATTACHMENT
		| RAL_TEXTURE_USAGE_TRANSFER_DST | RAL_TEXTURE_USAGE_TRANSFER_SRC ) );
	CHECK( lastViewInfo.texture == receipt.target
		&& lastViewInfo.viewType == RAL_TEXTURE_2D
		&& lastViewInfo.format == RAL_FORMAT_UNDEFINED );
	CHECK( lastSamplerInfo.minFilter == RAL_FILTER_LINEAR
		&& lastSamplerInfo.magFilter == RAL_FILTER_LINEAR
		&& lastSamplerInfo.mipmapMode == RAL_MIPMAP_NEAREST
		&& lastSamplerInfo.addressU == RAL_ADDRESS_CLAMP_TO_EDGE
		&& lastSamplerInfo.addressV == RAL_ADDRESS_CLAMP_TO_EDGE
		&& lastSamplerInfo.addressW == RAL_ADDRESS_CLAMP_TO_EDGE
		&& lastSamplerInfo.maxAnisotropy == 1.0f );
	CHECK( groupInfos[0].layout == input.postprocessLayout
		&& groupInfos[0].numValues == 1
		&& groupValues[0][0].binding == 0
		&& groupValues[0][0].type == RAL_BIND_COMBINED_TEXTURE_SAMPLER
		&& groupValues[0][0].textureView == receipt.targetView
		&& groupValues[0][0].sampler == owner.sampler );
	CHECK( groupInfos[1].layout == input.histogramLayout
		&& groupInfos[1].numValues == 3
		&& groupValues[1][0].binding == 0
		&& groupValues[1][0].type == RAL_BIND_SAMPLED_TEXTURE
		&& groupValues[1][0].textureView == receipt.targetView
		&& groupValues[1][1].binding == 1
		&& groupValues[1][1].type == RAL_BIND_SAMPLER
		&& groupValues[1][1].sampler == owner.sampler
		&& groupValues[1][2].binding == 2
		&& groupValues[1][2].type == RAL_BIND_STORAGE_BUFFER
		&& groupValues[1][2].buffer == input.histogramBuffer );

	baseCaps = capsQueries; baseFormats = formatQueries; baseCreates = creates;
	CHECK( VK_TemporalResolvedHdrEnsureAfterFence( &owner, &input, qfalse ) );
	CHECK( capsQueries == baseCaps && formatQueries == baseFormats
		&& creates == baseCreates );
	changed = input; changed.worldIndex = 1; changed.planGeneration = 8;
	CHECK( VK_TemporalResolvedHdrEnsureAfterFence( &owner, &changed, qfalse ) );
	CHECK( owner.allocationGeneration == 1 && creates == baseCreates );
	CHECK( VK_TemporalResolvedHdrGetReceipt( &owner, &receipt )
		&& receipt.worldIndex == 1 && receipt.planGeneration == 8 );
	input = changed;

	changed = input; ++changed.width;
	before = owner;
	CHECK( VK_TemporalResolvedHdrNeedsIdle( &owner, &changed ) );
	CHECK( !VK_TemporalResolvedHdrEnsureAfterFence( &owner, &changed, qfalse ) );
	CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0 );
	failStep = creates + 2; aliasAtStep = NULL;
	CHECK( !VK_TemporalResolvedHdrEnsureAfterFence( &owner, &changed, qtrue ) );
	CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0 );
	failStep = 0;
	CHECK( VK_TemporalResolvedHdrEnsureAfterFence( &owner, &changed, qtrue ) );
	CHECK( owner.allocationGeneration == 2 && owner.key.width == changed.width );
	input = changed;

	changed = input;
	changed.sceneColorAttachmentGeneration++;
	before = owner;
	CHECK( VK_TemporalResolvedHdrNeedsIdle( &owner, &changed ) );
	CHECK( !VK_TemporalResolvedHdrEnsureAfterFence( &owner, &changed, qfalse ) );
	CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0 );
	changed = input; changed.width = caps.maxTextureDimension2D + 1u;
	before = owner; baseCreates = creates;
	CHECK( !VK_TemporalResolvedHdrEnsureAfterFence( &owner, &changed, qtrue ) );
	CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0
		&& creates == baseCreates );
	changed = input; changed.height = caps.maxTextureDimension2D + 1u;
	before = owner; baseCreates = creates;
	CHECK( !VK_TemporalResolvedHdrEnsureAfterFence( &owner, &changed, qtrue ) );
	CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0
		&& creates == baseCreates );
	{
		uint32_t savedGeneration = owner.allocationGeneration;
		owner.allocationGeneration = UINT32_MAX;
		CHECK( VK_TemporalResolvedHdrEnsureAfterFence( &owner, &input, qfalse ) );
		changed = input; ++changed.height; before = owner;
		CHECK( !VK_TemporalResolvedHdrEnsureAfterFence( &owner, &changed, qtrue ) );
		CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0 );
		owner.allocationGeneration = savedGeneration;
	}
	changed = input;
	changed.histogramBuffer = (ralBuffer_t *)changed.backend;
	before = owner;
	CHECK( !VK_TemporalResolvedHdrEnsureAfterFence( &owner, &changed, qtrue ) );
	CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0 );

	// Every create-stage failure preserves the live owner and destroys only the
	// candidate roles already created, in reverse dependency order.
	for ( uint32_t stage = 1; stage <= 5; ++stage ) {
		static const char *orders[5] = { "", "T", "VT", "SVT", "GSVT" };
		uint32_t trace0 = destroyTraceCount;
		changed = input; ++changed.height; before = owner;
		failStep = creates + stage; aliasAtStep = NULL;
		CHECK( !VK_TemporalResolvedHdrEnsureAfterFence( &owner, &changed, qtrue ) );
		CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0 );
		CHECK( destroyTraceCount - trace0 == strlen( orders[stage - 1] ) );
		for ( uint32_t i = 0; i < strlen( orders[stage - 1] ); ++i )
			CHECK( destroyTrace[trace0 + i].role == orders[stage - 1][i] );
		failStep = 0;
	}

	// A fake backend may return any borrowed/live identity for any role. It must
	// be rejected without routing that protected pointer to a destroy function.
	{
		const void *borrowed[7] = { input.backend, input.currentSceneColor,
			input.currentPostprocessGroup, input.currentHistogramGroup,
			input.postprocessLayout, input.histogramLayout, input.histogramBuffer };
		const void *live[5] = { owner.target, owner.targetView, owner.sampler,
			owner.postprocessGroup, owner.histogramGroup };
		for ( uint32_t set = 0; set < 2; ++set ) {
			const void *const *identities = set ? live : borrowed;
			uint32_t count = set ? 5u : 7u;
			for ( uint32_t identity = 0; identity < count; ++identity ) {
				for ( uint32_t stage = 1; stage <= 5; ++stage ) {
					uint32_t trace0 = destroyTraceCount;
					changed = input; ++changed.height; before = owner;
					failStep = creates + stage;
					aliasAtStep = (void *)identities[identity];
					CHECK( !VK_TemporalResolvedHdrEnsureAfterFence(
						&owner, &changed, qtrue ) );
					CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0 );
					CHECK( !TraceContainsPointer( trace0, identities[identity] ) );
					failStep = 0; aliasAtStep = NULL;
				}
			}
		}
	}

	// Cross-role candidate aliasing is also rejected and destroyed exactly once.
	for ( uint32_t stage = 2; stage <= 5; ++stage ) {
		uint32_t trace0 = destroyTraceCount;
		void *firstCandidate = (void *)( nextHandle + 0x100u );
		changed = input; ++changed.height; before = owner;
		failStep = creates + stage; aliasAtStep = firstCandidate;
		CHECK( !VK_TemporalResolvedHdrEnsureAfterFence( &owner, &changed, qtrue ) );
		CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0 );
		CHECK( TracePointerCount( trace0, firstCandidate ) == 1 );
		failStep = 0; aliasAtStep = NULL;
	}

	CHECK( VK_TemporalResolvedHdrGetReceipt( &owner, &receipt ) );
	current = MakeCurrent( &input );
	memset( &routed, 0xcc, sizeof( routed ) ); routedBefore = routed;
	CHECK( !VK_TemporalResolvedHdrRouteSource( NULL, &receipt, NULL, &routed ) );
	CHECK( memcmp( &routed, &routedBefore, sizeof( routed ) ) == 0 );
	CHECK( VK_TemporalResolvedHdrRouteSource( &current, &receipt, NULL, &routed )
		&& SourceEquals( &routed, &current ) );
	content = MakeContent( &current, &receipt );
	memset( &contentMutation, 0xcc, sizeof( contentMutation ) );
	CHECK( VK_TemporalResolvedHdrBuildContentReceipt(
		&current, &receipt, 1u, &contentMutation ) );
	CHECK( memcmp( &contentMutation, &content, sizeof( content ) ) == 0 );
	CHECK( contentMutation.producer == VK_TEMPORAL_RESOLVED_HDR_PRODUCER_COPY );
	CHECK( VK_TemporalResolvedHdrBuildContentReceiptForProducer(
		&current, &receipt, 2u,
		VK_TEMPORAL_RESOLVED_HDR_PRODUCER_TEMPORAL_RESOLVE,
		&contentMutation ) );
	CHECK( contentMutation.producer ==
		VK_TEMPORAL_RESOLVED_HDR_PRODUCER_TEMPORAL_RESOLVE
		&& contentMutation.contentSerial == 2u );
	contentMutation = content;
	CHECK( !VK_TemporalResolvedHdrBuildContentReceiptForProducer(
		&current, &receipt, 2u, VK_TEMPORAL_RESOLVED_HDR_PRODUCER_NONE,
		&contentMutation ) );
	CHECK( memcmp( &contentMutation, &content, sizeof( content ) ) == 0 );
	contentMutation = content;
	CHECK( !VK_TemporalResolvedHdrBuildContentReceipt(
		&current, &receipt, 0u, &contentMutation ) );
	CHECK( memcmp( &contentMutation, &content, sizeof( content ) ) == 0 );
	memset( &submittedContent, 0xcc, sizeof( submittedContent ) );
	CHECK( !VK_TemporalResolvedHdrResolveContentSubmit(
		&content, qfalse, &submittedContent ) );
	for ( size_t i = 0; i < sizeof( submittedContent ); ++i )
		CHECK( ( (const unsigned char *)&submittedContent )[i] == 0xcc );
	CHECK( VK_TemporalResolvedHdrResolveContentSubmit(
		&content, qtrue, &submittedContent ) );
	CHECK( submittedContent.valid && submittedContent.submitted );

#define MUTATE_SUBMIT(field, value) do { \
	contentMutation = content; contentMutation.field = (value); \
	submittedBefore = submittedContent; \
	CHECK( !VK_TemporalResolvedHdrResolveContentSubmit( \
		&contentMutation, qtrue, &submittedContent ) ); \
	CHECK( memcmp( &submittedContent, &submittedBefore, \
		sizeof( submittedContent ) ) == 0 ); \
} while ( 0 )
	MUTATE_SUBMIT( batchToken, 0 );
	MUTATE_SUBMIT( frameId, 0 );
	MUTATE_SUBMIT( contentSerial, 0 );
	MUTATE_SUBMIT( producer, VK_TEMPORAL_RESOLVED_HDR_PRODUCER_NONE );
	MUTATE_SUBMIT( commandSlot, content.frameCount );
	MUTATE_SUBMIT( frameCount, 0 );
	MUTATE_SUBMIT( frameCount, 5u );
	MUTATE_SUBMIT( worldIndex, -1 );
	MUTATE_SUBMIT( width, 0 );
	MUTATE_SUBMIT( height, 0 );
	MUTATE_SUBMIT( topologyEpoch, 0 );
	MUTATE_SUBMIT( planGeneration, 0 );
	MUTATE_SUBMIT( sceneColorAttachmentGeneration, 0 );
	MUTATE_SUBMIT( targetAllocationGeneration, 0 );
	MUTATE_SUBMIT( backend, NULL );
	MUTATE_SUBMIT( sourceSceneColor, NULL );
	MUTATE_SUBMIT( sourcePostprocessGroup, NULL );
	MUTATE_SUBMIT( sourceHistogramGroup, NULL );
	MUTATE_SUBMIT( target, NULL );
	MUTATE_SUBMIT( targetView, NULL );
	MUTATE_SUBMIT( postprocessGroup, NULL );
	MUTATE_SUBMIT( histogramGroup, NULL );
	MUTATE_SUBMIT( sceneFormat, RAL_FORMAT_R8G8B8A8_UNORM );
	MUTATE_SUBMIT( valid, qfalse );
	MUTATE_SUBMIT( submitted, qtrue );
#undef MUTATE_SUBMIT
	{
		const void *submitRoles[8] = { content.backend,
			content.sourceSceneColor, content.sourcePostprocessGroup,
			content.sourceHistogramGroup, content.target, content.targetView,
			content.postprocessGroup, content.histogramGroup };
		for ( uint32_t role = 0; role < 8; ++role ) {
			for ( uint32_t alias = 0; alias < 8; ++alias ) {
				if ( role == alias ) continue;
				contentMutation = content;
				if ( role == 0 ) contentMutation.backend =
					(ralBackend_t *)submitRoles[alias];
				if ( role == 1 ) contentMutation.sourceSceneColor =
					(ralTexture_t *)submitRoles[alias];
				if ( role == 2 ) contentMutation.sourcePostprocessGroup =
					(ralBindGroup_t *)submitRoles[alias];
				if ( role == 3 ) contentMutation.sourceHistogramGroup =
					(ralBindGroup_t *)submitRoles[alias];
				if ( role == 4 ) contentMutation.target =
					(ralTexture_t *)submitRoles[alias];
				if ( role == 5 ) contentMutation.targetView =
					(ralTextureView_t *)submitRoles[alias];
				if ( role == 6 ) contentMutation.postprocessGroup =
					(ralBindGroup_t *)submitRoles[alias];
				if ( role == 7 ) contentMutation.histogramGroup =
					(ralBindGroup_t *)submitRoles[alias];
				submittedBefore = submittedContent;
				CHECK( !VK_TemporalResolvedHdrResolveContentSubmit(
					&contentMutation, qtrue, &submittedContent ) );
				CHECK( memcmp( &submittedContent, &submittedBefore,
					sizeof( submittedContent ) ) == 0 );
			}
		}
	}
	CHECK( !VK_TemporalResolvedHdrRouteSource( &current, &receipt,
		&submittedContent, &routed ) || !routed.resolved );
	contentMutation = submittedContent;
	CHECK( !VK_TemporalResolvedHdrResolveContentSubmit(
		&contentMutation, qtrue, &submittedContent ) );
	{
		vkHdrPostprocessSource_t expected = current;
		expected.backend = receipt.backend;
		expected.attachment = receipt.target;
		expected.postprocessGroup = receipt.postprocessGroup;
		expected.histogramGroup = receipt.histogramGroup;
		expected.targetAllocationGeneration = receipt.allocationGeneration;
		expected.resolved = qtrue;
		CHECK( VK_TemporalResolvedHdrRouteSource( &current, &receipt,
			&content, &routed )
			&& SourceEquals( &routed, &expected ) );
	}

#define MUTATE_CONTENT(field, value) do { \
	contentMutation = content; contentMutation.field = (value); \
	CHECK( VK_TemporalResolvedHdrRouteSource( &current, &receipt, \
		&contentMutation, &routed ) && !routed.resolved \
		&& SourceEquals( &routed, &current ) ); \
} while ( 0 )
	MUTATE_CONTENT( batchToken, 0 );
	MUTATE_CONTENT( frameId, 0 );
	MUTATE_CONTENT( contentSerial, 0 );
	MUTATE_CONTENT( commandSlot, content.frameCount );
	MUTATE_CONTENT( frameCount, content.frameCount + 1 );
	MUTATE_CONTENT( worldIndex, content.worldIndex + 1 );
	MUTATE_CONTENT( width, content.width + 1 );
	MUTATE_CONTENT( height, content.height + 1 );
	MUTATE_CONTENT( topologyEpoch, content.topologyEpoch + 1 );
	MUTATE_CONTENT( planGeneration, content.planGeneration + 1 );
	MUTATE_CONTENT( sceneColorAttachmentGeneration,
		content.sceneColorAttachmentGeneration + 1 );
	MUTATE_CONTENT( targetAllocationGeneration,
		content.targetAllocationGeneration + 1 );
	MUTATE_CONTENT( backend, (ralBackend_t *)(uintptr_t)0x999u );
	MUTATE_CONTENT( sourceSceneColor, (ralTexture_t *)(uintptr_t)0x999u );
	MUTATE_CONTENT( sourcePostprocessGroup, (ralBindGroup_t *)(uintptr_t)0x999u );
	MUTATE_CONTENT( sourceHistogramGroup, (ralBindGroup_t *)(uintptr_t)0x999u );
	MUTATE_CONTENT( target, (ralTexture_t *)(uintptr_t)0x999u );
	MUTATE_CONTENT( targetView, (ralTextureView_t *)(uintptr_t)0x999u );
	MUTATE_CONTENT( postprocessGroup, (ralBindGroup_t *)(uintptr_t)0x999u );
	MUTATE_CONTENT( histogramGroup, (ralBindGroup_t *)(uintptr_t)0x999u );
	MUTATE_CONTENT( sceneFormat, RAL_FORMAT_R8G8B8A8_UNORM );
	MUTATE_CONTENT( valid, qfalse );
#undef MUTATE_CONTENT

	targetMutation = receipt; ++targetMutation.sceneColorAttachmentGeneration;
	CHECK( VK_TemporalResolvedHdrRouteSource( &current, &targetMutation,
		&content, &routed ) && !routed.resolved );
#define MUTATE_TARGET(field, value) do { \
	targetMutation = receipt; targetMutation.field = (value); \
	CHECK( VK_TemporalResolvedHdrRouteSource( &current, &targetMutation, \
		&content, &routed ) && !routed.resolved \
		&& SourceEquals( &routed, &current ) ); \
} while ( 0 )
	MUTATE_TARGET( backend, (ralBackend_t *)(uintptr_t)0x777u );
	MUTATE_TARGET( target, (ralTexture_t *)(uintptr_t)0x777u );
	MUTATE_TARGET( targetView, (ralTextureView_t *)(uintptr_t)0x777u );
	MUTATE_TARGET( postprocessGroup, (ralBindGroup_t *)(uintptr_t)0x777u );
	MUTATE_TARGET( histogramGroup, (ralBindGroup_t *)(uintptr_t)0x777u );
	MUTATE_TARGET( currentSceneColor, (ralTexture_t *)(uintptr_t)0x777u );
	MUTATE_TARGET( currentPostprocessGroup, (ralBindGroup_t *)(uintptr_t)0x777u );
	MUTATE_TARGET( currentHistogramGroup, (ralBindGroup_t *)(uintptr_t)0x777u );
	MUTATE_TARGET( worldIndex, receipt.worldIndex + 1 );
	MUTATE_TARGET( width, receipt.width + 1 );
	MUTATE_TARGET( height, receipt.height + 1 );
	MUTATE_TARGET( topologyEpoch, receipt.topologyEpoch + 1 );
	MUTATE_TARGET( planGeneration, receipt.planGeneration + 1 );
	MUTATE_TARGET( sceneColorAttachmentGeneration,
		receipt.sceneColorAttachmentGeneration + 1 );
	MUTATE_TARGET( allocationGeneration, receipt.allocationGeneration + 1 );
	MUTATE_TARGET( sceneFormat, RAL_FORMAT_R8G8B8A8_UNORM );
	MUTATE_TARGET( ready, qfalse );
#undef MUTATE_TARGET
	{
		const void *roles[8] = { receipt.backend, receipt.currentSceneColor,
			receipt.currentPostprocessGroup, receipt.currentHistogramGroup,
			receipt.target, receipt.targetView, receipt.postprocessGroup,
			receipt.histogramGroup };
		for ( uint32_t targetRole = 4; targetRole < 8; ++targetRole ) {
			for ( uint32_t aliasRole = 0; aliasRole < 8; ++aliasRole ) {
				if ( targetRole == aliasRole ) continue;
				targetMutation = receipt;
				contentMutation = content;
				if ( targetRole == 4 ) {
					targetMutation.target = (ralTexture_t *)roles[aliasRole];
					contentMutation.target = targetMutation.target;
				} else if ( targetRole == 5 ) {
					targetMutation.targetView = (ralTextureView_t *)roles[aliasRole];
					contentMutation.targetView = targetMutation.targetView;
				} else if ( targetRole == 6 ) {
					targetMutation.postprocessGroup = (ralBindGroup_t *)roles[aliasRole];
					contentMutation.postprocessGroup = targetMutation.postprocessGroup;
				} else {
					targetMutation.histogramGroup = (ralBindGroup_t *)roles[aliasRole];
					contentMutation.histogramGroup = targetMutation.histogramGroup;
				}
				CHECK( VK_TemporalResolvedHdrRouteSource( &current, &targetMutation,
					&contentMutation, &routed )
					&& SourceEquals( &routed, &current ) );
			}
		}
	}

#define MUTATE_CURRENT(field, value) do { \
	vkHdrPostprocessSource_t currentMutation = current; \
	currentMutation.field = (value); \
	CHECK( VK_TemporalResolvedHdrRouteSource( &currentMutation, &receipt, \
		&content, &routed ) && !routed.resolved \
		&& SourceEquals( &routed, &currentMutation ) ); \
} while ( 0 )
	MUTATE_CURRENT( backend, (ralBackend_t *)(uintptr_t)0x888u );
	MUTATE_CURRENT( attachment, (ralTexture_t *)(uintptr_t)0x888u );
	MUTATE_CURRENT( postprocessGroup, (ralBindGroup_t *)(uintptr_t)0x888u );
	MUTATE_CURRENT( histogramGroup, (ralBindGroup_t *)(uintptr_t)0x888u );
	MUTATE_CURRENT( worldIndex, current.worldIndex + 1 );
	MUTATE_CURRENT( width, current.width + 1 );
	MUTATE_CURRENT( height, current.height + 1 );
	MUTATE_CURRENT( topologyEpoch, current.topologyEpoch + 1 );
	MUTATE_CURRENT( planGeneration, current.planGeneration + 1 );
	MUTATE_CURRENT( sceneColorAttachmentGeneration,
		current.sceneColorAttachmentGeneration + 1 );
#undef MUTATE_CURRENT
	{
		vkHdrPostprocessSource_t invalidCurrent = current;
		const void *currentRoles[4] = { current.backend, current.attachment,
			current.postprocessGroup, current.histogramGroup };
		for ( uint32_t i = 0; i < 4; ++i ) for ( uint32_t j = 0; j < 4; ++j ) {
			if ( i == j ) continue;
			invalidCurrent = current;
			if ( i == 0 ) invalidCurrent.backend = (ralBackend_t *)currentRoles[j];
			if ( i == 1 ) invalidCurrent.attachment = (ralTexture_t *)currentRoles[j];
			if ( i == 2 ) invalidCurrent.postprocessGroup = (ralBindGroup_t *)currentRoles[j];
			if ( i == 3 ) invalidCurrent.histogramGroup = (ralBindGroup_t *)currentRoles[j];
			routedBefore = routed;
			CHECK( !VK_TemporalResolvedHdrRouteSource( &invalidCurrent, &receipt,
				&content, &routed ) );
			CHECK( memcmp( &routed, &routedBefore, sizeof( routed ) ) == 0 );
		}
		invalidCurrent = current; invalidCurrent.batchToken = 0;
		routedBefore = routed;
		CHECK( !VK_TemporalResolvedHdrRouteSource( &invalidCurrent, &receipt,
			&content, &routed ) );
		CHECK( memcmp( &routed, &routedBefore, sizeof( routed ) ) == 0 );
#define INVALID_CURRENT(field, value) do { \
	invalidCurrent = current; invalidCurrent.field = (value); \
	routedBefore = routed; \
	CHECK( !VK_TemporalResolvedHdrRouteSource( &invalidCurrent, &receipt, \
		&content, &routed ) ); \
	CHECK( memcmp( &routed, &routedBefore, sizeof( routed ) ) == 0 ); \
} while ( 0 )
		INVALID_CURRENT( frameId, 0 );
		INVALID_CURRENT( frameCount, 0 );
		INVALID_CURRENT( frameCount, 5 );
		INVALID_CURRENT( commandSlot, current.frameCount );
		INVALID_CURRENT( worldIndex, -1 );
		INVALID_CURRENT( width, 0 );
		INVALID_CURRENT( height, 0 );
		INVALID_CURRENT( topologyEpoch, 0 );
		INVALID_CURRENT( planGeneration, 0 );
		INVALID_CURRENT( sceneColorAttachmentGeneration, 0 );
		INVALID_CURRENT( sceneFormat, RAL_FORMAT_R8G8B8A8_UNORM );
		INVALID_CURRENT( targetAllocationGeneration, 1 );
		INVALID_CURRENT( resolved, qtrue );
#undef INVALID_CURRENT
	}

	CHECK( !VK_TemporalResolvedHdrReleaseAfterIdle( &owner, qfalse ) );
	CHECK( VK_TemporalResolvedHdrHasLive( &owner ) );
	{
		uint32_t trace0 = destroyTraceCount;
		const void *histogram = owner.histogramGroup;
		const void *postprocess = owner.postprocessGroup;
		const void *sampler = owner.sampler;
		const void *view = owner.targetView;
		const void *texture = owner.target;
	CHECK( VK_TemporalResolvedHdrReleaseAfterIdle( &owner, qtrue ) );
		CHECK( destroyTraceCount == trace0 + 5 );
		CHECK( destroyTrace[trace0 + 0].role == 'G'
			&& destroyTrace[trace0 + 0].pointer == histogram );
		CHECK( destroyTrace[trace0 + 1].role == 'G'
			&& destroyTrace[trace0 + 1].pointer == postprocess );
		CHECK( destroyTrace[trace0 + 2].role == 'S'
			&& destroyTrace[trace0 + 2].pointer == sampler );
		CHECK( destroyTrace[trace0 + 3].role == 'V'
			&& destroyTrace[trace0 + 3].pointer == view );
		CHECK( destroyTrace[trace0 + 4].role == 'T'
			&& destroyTrace[trace0 + 4].pointer == texture );
	}
	CHECK( !VK_TemporalResolvedHdrHasLive( &owner )
		&& owner.allocationGeneration == 2 );
	CHECK( VK_TemporalResolvedHdrReleaseAfterIdle( &owner, qtrue ) );
	puts( "vk temporal resolved HDR contract: PASS" );
	return 0;
}
