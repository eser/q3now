// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_browser_bridge.h"
#include "ral_webgpu_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

typedef struct {
	uint32_t adapterPolls, devicePolls, configures, unconfigures;
	uint32_t acquires, presents, releasedDevice, releasedAdapter;
	uint32_t resourcesCreated, resourcesDestroyed, writes;
	uint32_t pipelineObjectsCreated, pipelineObjectsDestroyed;
	uint32_t draws, submissions, commandObjectsReleased;
	uint32_t width, height;
	uintptr_t nextIdentity;
	byte roundTrip[64];
	uint64_t roundTripBytes;
	ralWebGpuBrowserOpcode_t failOpcode;
	qboolean unavailable, lost;
	uint32_t lensStatus, lensMalformed, lensReleases;
	uint64_t lensScope;
	qboolean lensBusy;
	uint32_t targetMipLevel, targetArrayLayer;
} fakeBrowser_t;

static void ResponseHeader( ralWebGpuBrowserAbiHeader_t *header,
		ralWebGpuBrowserOpcode_t opcode, uint32_t bytes ) {
	header->schemaVersion = RAL_WEBGPU_BROWSER_ABI_SCHEMA_VERSION;
	header->opcode = opcode; header->byteCount = bytes; header->reserved = 0u;
}

static qboolean Dispatch( void *userData, ralWebGpuBrowserOpcode_t opcode,
		const void *request, uint32_t requestBytes, void *response,
		uint32_t responseBytes ) {
	fakeBrowser_t *fake = userData;
	const ralWebGpuBrowserAbiHeader_t *requestHeader = request;
	if ( !fake || !request || !response || requestBytes < sizeof( *requestHeader )
			|| requestHeader->schemaVersion != RAL_WEBGPU_BROWSER_ABI_SCHEMA_VERSION
			|| requestHeader->opcode != (uint32_t)opcode
			|| requestHeader->byteCount != requestBytes ) return qfalse;
	if ( fake->failOpcode == opcode ) return qfalse;
	memset( response, 0, responseBytes );
	switch ( opcode ) {
	case RAL_WEBGPU_BROWSER_OP_BEGIN_LENS_VISIBILITY: {
		const ralWebGpuBrowserLensBeginRequest_t *in = request;
		ralWebGpuBrowserIdentityResponse_t *out = response;
		if ( requestBytes != sizeof(*in) || responseBytes != sizeof(*out)
				|| in->deviceIdentity != 0x3000u || in->scopeGeneration != fake->lensScope
				|| in->frameGeneration != 5u || in->sourceCount != 1u
				|| !in->paramsOffset || !in->sourcesOffset ) return qfalse;
		ResponseHeader(&out->header, opcode, sizeof(*out));
		out->accepted = 1u; out->identity = fake->lensBusy ? 0u : 0x9000u;
		return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_POLL_LENS_VISIBILITY: {
		const ralWebGpuBrowserLensPollRequest_t *in = request;
		ralWebGpuBrowserAsyncResponse_t *out = response;
		if ( requestBytes != sizeof(*in) || responseBytes != sizeof(*out)
				|| in->deviceIdentity != 0x3000u || in->scopeGeneration != fake->lensScope
				|| in->frameGeneration != 5u || in->queryIdentity != 0x9000u
				|| in->capacity != RAL_WEBGPU_BROWSER_LENS_SOURCE_LIMIT ) return qfalse;
		ResponseHeader(&out->header, opcode, sizeof(*out));
		out->status = fake->lensStatus;
		out->byteSize = fake->lensMalformed == 2u ? 6u : 5u;
		if ( fake->lensStatus == RAL_WEBGPU_ASYNC_READY ) {
			float *values = (float *)(uintptr_t)in->outputOffset;
			uint32_t count = in->capacity - (fake->lensMalformed == 3u ? 1u : 0u);
			for ( uint32_t i = 0u; i < count; ++i ) values[i] = 0.25f;
			if ( fake->lensMalformed == 1u ) values[count - 1u] = NAN;
		}
		return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_RESET_LENS_VISIBILITY:
	case RAL_WEBGPU_BROWSER_OP_RELEASE_LENS_VISIBILITY: {
		const ralWebGpuBrowserLensControlRequest_t *in = request;
		ralWebGpuBrowserStatusResponse_t *out = response;
		if ( requestBytes != sizeof(*in) || responseBytes != sizeof(*out)
				|| in->deviceIdentity != 0x3000u || !in->identity ) return qfalse;
		if ( opcode == RAL_WEBGPU_BROWSER_OP_RESET_LENS_VISIBILITY ) fake->lensScope = in->identity;
		else fake->lensReleases++;
		ResponseHeader(&out->header, opcode, sizeof(*out)); out->accepted = 1u;
		return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_BEGIN_ADAPTER:
	case RAL_WEBGPU_BROWSER_OP_BEGIN_DEVICE: {
		ralWebGpuBrowserStatusResponse_t *out = response;
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		ResponseHeader( &out->header, opcode, sizeof( *out ) ); out->accepted = 1u;
		return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_POLL_ADAPTER: {
		ralWebGpuBrowserAdapterResponse_t *out = response;
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		ResponseHeader( &out->header, opcode, sizeof( *out ) );
		if ( fake->unavailable ) {
			out->status = RAL_WEBGPU_REQUEST_UNAVAILABLE; return qtrue;
		}
		if ( fake->adapterPolls++ == 0u ) {
			out->status = RAL_WEBGPU_REQUEST_PENDING; return qtrue;
		}
		out->status = RAL_WEBGPU_REQUEST_READY; out->adapterType = RAL_ADAPTER_TYPE_DISCRETE;
		out->adapterIdentity = UINT64_C(0x2000); out->vendorId = 0x1234u;
		out->deviceId = 0x5678u; strcpy( out->vendorName, "Wired" );
		strcpy( out->deviceName, "Browser WebGPU fixture" );
		out->limits.maxColorAttachments = 8u;
		out->limits.maxTextureDimension2D = 8192u;
		out->limits.maxTextureDimension3D = 2048u;
		out->limits.maxTextureArrayLayers = 2048u;
		out->limits.maxComputeInvocationsPerWorkgroup = 256u;
		out->limits.maxSampledTexturesPerShaderStage = 32u;
		out->limits.maxBindGroups = 4u; out->limits.maxBindingsPerBindGroup = 16u;
		out->limits.maxStorageBufferBindingSize = UINT64_C(134217728);
		out->limits.minUniformBufferOffsetAlignment = 256u;
		out->limits.minStorageBufferOffsetAlignment = 256u;
		out->limits.bindingArrays = 1u; return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_POLL_DEVICE: {
		ralWebGpuBrowserDeviceResponse_t *out = response;
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		ResponseHeader( &out->header, opcode, sizeof( *out ) );
		if ( fake->devicePolls++ == 0u ) {
			out->status = RAL_WEBGPU_REQUEST_PENDING; return qtrue;
		}
		out->status = RAL_WEBGPU_REQUEST_READY;
		out->deviceIdentity = UINT64_C(0x3000); out->queueIdentity = UINT64_C(0x4000);
		return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_RELEASE_DEVICE: {
		ralWebGpuBrowserStatusResponse_t *out = response;
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		ResponseHeader( &out->header, opcode, sizeof( *out ) );
		out->accepted = 1u; fake->releasedDevice++; return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_RELEASE_ADAPTER: {
		ralWebGpuBrowserStatusResponse_t *out = response;
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		ResponseHeader( &out->header, opcode, sizeof( *out ) );
		out->accepted = 1u; fake->releasedAdapter++; return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_POLL_DEVICE_LOSS: {
		ralWebGpuBrowserLossResponse_t *out = response;
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		ResponseHeader( &out->header, opcode, sizeof( *out ) );
		out->deviceIdentity = UINT64_C(0x3000); out->lost = fake->lost;
		out->recoverable = fake->lost; if ( fake->lost ) strcpy( out->reason, "fixture loss" );
		return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_CANVAS_CONFIGURE: {
		const ralWebGpuBrowserCanvasConfigureRequest_t *in = request;
		ralWebGpuBrowserStatusResponse_t *out = response;
		if ( requestBytes != sizeof( *in ) || responseBytes != sizeof( *out )
				|| in->canvasIdentity != UINT64_C(0x7000)
				|| in->deviceIdentity != UINT64_C(0x3000) ) return qfalse;
		fake->width = in->pixelWidth; fake->height = in->pixelHeight;
		fake->configures++; ResponseHeader( &out->header, opcode, sizeof( *out ) );
		out->accepted = 1u; return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_CANVAS_UNCONFIGURE: {
		ralWebGpuBrowserStatusResponse_t *out = response;
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		fake->unconfigures++; ResponseHeader( &out->header, opcode, sizeof( *out ) );
		out->accepted = 1u; return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_CANVAS_ACQUIRE: {
		ralWebGpuBrowserCanvasAcquireResponse_t *out = response;
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		fake->acquires++; ResponseHeader( &out->header, opcode, sizeof( *out ) );
		out->accepted = 1u; out->textureIdentity = UINT64_C(0x8000); return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_CANVAS_PRESENT: {
		ralWebGpuBrowserStatusResponse_t *out = response;
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		fake->presents++; ResponseHeader( &out->header, opcode, sizeof( *out ) );
		out->accepted = 1u; return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_CREATE_BUFFER:
	case RAL_WEBGPU_BROWSER_OP_CREATE_TEXTURE:
	case RAL_WEBGPU_BROWSER_OP_CREATE_SAMPLER: {
		ralWebGpuBrowserIdentityResponse_t *out = response;
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		ResponseHeader( &out->header, opcode, sizeof( *out ) );
		out->accepted = 1u; out->identity = ++fake->nextIdentity;
		fake->resourcesCreated++; return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_DESTROY_RESOURCE: {
		ralWebGpuBrowserStatusResponse_t *out = response;
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		ResponseHeader( &out->header, opcode, sizeof( *out ) );
		out->accepted = 1u; fake->resourcesDestroyed++; return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_WRITE_BUFFER:
	case RAL_WEBGPU_BROWSER_OP_WRITE_TEXTURE: {
		ralWebGpuBrowserStatusResponse_t *out = response;
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		ResponseHeader( &out->header, opcode, sizeof( *out ) );
		out->accepted = 1u; fake->writes++; return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_BEGIN_ROUND_TRIP: {
		const ralWebGpuBrowserRoundTripBeginRequest_t *in = request;
		ralWebGpuBrowserIdentityResponse_t *out = response;
		if ( requestBytes != sizeof( *in ) || responseBytes != sizeof( *out )
				|| !in->dataOffset || !in->byteSize || in->byteSize > sizeof( fake->roundTrip ) )
			return qfalse;
		memcpy( fake->roundTrip, (const void *)(uintptr_t)in->dataOffset,
			(size_t)in->byteSize ); fake->roundTripBytes = in->byteSize;
		ResponseHeader( &out->header, opcode, sizeof( *out ) );
		out->accepted = 1u; out->identity = ++fake->nextIdentity; return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_POLL_ROUND_TRIP: {
		const ralWebGpuBrowserRoundTripPollRequest_t *in = request;
		ralWebGpuBrowserAsyncResponse_t *out = response;
		if ( requestBytes != sizeof( *in ) || responseBytes != sizeof( *out )
				|| in->capacity < fake->roundTripBytes || !in->outputOffset ) return qfalse;
		memcpy( (void *)(uintptr_t)in->outputOffset, fake->roundTrip,
			(size_t)fake->roundTripBytes );
		ResponseHeader( &out->header, opcode, sizeof( *out ) );
		out->status = RAL_WEBGPU_ASYNC_READY; out->byteSize = fake->roundTripBytes;
		return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_RELEASE_OPERATION:
	case RAL_WEBGPU_BROWSER_OP_RECORD_INDEXED_DRAW:
	case RAL_WEBGPU_BROWSER_OP_END_PASS: {
		ralWebGpuBrowserStatusResponse_t *out = response;
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		ResponseHeader( &out->header, opcode, sizeof( *out ) ); out->accepted = 1u;
		if ( opcode == RAL_WEBGPU_BROWSER_OP_RECORD_INDEXED_DRAW ) fake->draws++;
		return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_CREATE_SHADER_MODULE:
	case RAL_WEBGPU_BROWSER_OP_CREATE_BIND_GROUP_LAYOUT:
	case RAL_WEBGPU_BROWSER_OP_CREATE_PIPELINE_LAYOUT:
	case RAL_WEBGPU_BROWSER_OP_CREATE_PIPELINE: {
		ralWebGpuBrowserIdentityResponse_t *out = response;
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		if ( opcode == RAL_WEBGPU_BROWSER_OP_CREATE_PIPELINE ) {
			const ralWebGpuBrowserPipelineRequest_t *in = request;
			if ( requestBytes != sizeof( *in )
					|| in->kind != RAL_SHADER_PIPELINE_GRAPHICS
					|| in->shaderModuleCount != 2u || in->vertexBindingCount != 1u
					|| in->vertexAttributeCount != 1u ) return qfalse;
			if (in->specValueCount != 4u || in->specStageMasks[0] != RAL_STAGE_VERTEX
				|| in->specStageMasks[1] != (RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT)
				|| in->specStageMasks[2] != RAL_STAGE_FRAGMENT
				|| in->specValueTypes[0] != RAL_SHADER_SPEC_I32
				|| in->specValueTypes[1] != RAL_SHADER_SPEC_U32
				|| in->specValueTypes[2] != RAL_SHADER_SPEC_I32
				|| in->specValueTypes[3] != RAL_SHADER_SPEC_F32
				|| in->specStageMasks[3] != RAL_STAGE_FRAGMENT
				|| in->specValues[3].constantId != 18u || in->specValues[3].value != 0x3fb33333u
				|| in->specValues[2].constantId != 17u || in->specValues[2].value != 3u)
				return qfalse;
		}
		ResponseHeader( &out->header, opcode, sizeof( *out ) ); out->accepted = 1u;
		out->identity = ++fake->nextIdentity; fake->pipelineObjectsCreated++;
		return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_DESTROY_PIPELINE_OBJECT: {
		ralWebGpuBrowserStatusResponse_t *out = response;
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		ResponseHeader( &out->header, opcode, sizeof( *out ) ); out->accepted = 1u;
		fake->pipelineObjectsDestroyed++; return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_BEGIN_ENCODER:
	case RAL_WEBGPU_BROWSER_OP_BEGIN_PASS:
	case RAL_WEBGPU_BROWSER_OP_FINISH_ENCODER:
	case RAL_WEBGPU_BROWSER_OP_SUBMIT: {
		ralWebGpuBrowserIdentityResponse_t *out = response;
		if (opcode == RAL_WEBGPU_BROWSER_OP_BEGIN_PASS) {
			const ralWebGpuBrowserBeginPassRequest_t *in = request;
			if (requestBytes != sizeof(*in) || in->reserved) return qfalse;
			fake->targetMipLevel = in->targetMipLevel;
			fake->targetArrayLayer = in->targetArrayLayer;
		}
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		ResponseHeader( &out->header, opcode, sizeof( *out ) ); out->accepted = 1u;
		out->identity = ++fake->nextIdentity;
		if ( opcode == RAL_WEBGPU_BROWSER_OP_SUBMIT ) fake->submissions++;
		return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_POLL_SUBMISSION: {
		ralWebGpuBrowserSubmissionResponse_t *out = response;
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		ResponseHeader( &out->header, opcode, sizeof( *out ) );
		out->status = RAL_WEBGPU_ASYNC_READY;
		out->timingState = RAL_WEBGPU_GPU_TIMING_UNAVAILABLE;
		return qtrue;
	}
	case RAL_WEBGPU_BROWSER_OP_RELEASE_COMMAND_OBJECT: {
		ralWebGpuBrowserStatusResponse_t *out = response;
		if ( responseBytes != sizeof( *out ) ) return qfalse;
		ResponseHeader( &out->header, opcode, sizeof( *out ) ); out->accepted = 1u;
		fake->commandObjectsReleased++; return qtrue;
	}
	default: return qfalse;
	}
}

typedef struct {
	uint32_t spirv[2][4];
	ralShaderModuleAbi_t modules[2];
	ralShaderVertexInputAbi_t input;
	ralShaderAbiManifest_t manifest;
	ralShaderVariantAbi_t variant;
	ralWebGpuWgslModule_t wgsl[2];
	ralVertexBinding_t vertexBinding;
	ralVertexAttribute_t vertexAttribute;
	ralColorBlendAttachment_t blend;
	ralGraphicsPipelineCreateInfo_t state;
} graphicsFixture_t;

static const char VERTEX_WGSL[] =
	"@vertex fn main(@location(0) p: vec3<f32>) -> @builtin(position) vec4<f32> { return vec4<f32>(p, 1.0); }\n";
static const char FRAGMENT_WGSL[] =
	"@fragment fn main() -> @location(0) vec4<f32> { return vec4<f32>(1.0); }\n";

static void Artifact( ralShaderArtifactAbi_t *artifact,
		ralShaderArtifactTarget_t target, const void *bytes, uint32_t byteCount ) {
	memset( artifact, 0, sizeof( *artifact ) ); artifact->target = target;
	if ( bytes ) {
		artifact->byteCount = byteCount;
		if ( Ral_ShaderArtifactDigest( bytes, byteCount, &artifact->digest )
				!= ralSuccess ) abort();
	}
}

static void ShaderModule( ralShaderModuleAbi_t *module, uint32_t stage,
		const uint32_t *spirv, const char *wgsl, uint32_t wgslBytes,
		uint64_t seed ) {
	memset( module, 0, sizeof( *module ) ); module->stage = stage;
	module->entryPoint = "main"; module->sourceDigest.lane0 = seed;
	module->sourceDigest.lane1 = seed + 1u;
	Artifact( &module->artifacts[RAL_SHADER_ARTIFACT_SPIRV],
		RAL_SHADER_ARTIFACT_SPIRV, spirv, 16u );
	Artifact( &module->artifacts[RAL_SHADER_ARTIFACT_MSL],
		RAL_SHADER_ARTIFACT_MSL, NULL, 0u );
	Artifact( &module->artifacts[RAL_SHADER_ARTIFACT_WGSL],
		RAL_SHADER_ARTIFACT_WGSL, wgsl, wgslBytes );
}

static void GraphicsSetup( graphicsFixture_t *fixture ) {
	static const ralShaderSpecConstantAbi_t specs[] = {
		{9, RAL_STAGE_VERTEX, 0, RAL_SHADER_SPEC_I32},
		{10, RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT, 0, RAL_SHADER_SPEC_U32},
		{17, RAL_STAGE_FRAGMENT, 1, RAL_SHADER_SPEC_I32},
		{18, RAL_STAGE_FRAGMENT, 0x3f800000u, RAL_SHADER_SPEC_F32}};
	static const ralShaderSpecValue_t values[] = {{9,1},{10,2},{17,3},{18,0x3fb33333u}};
	static const ralSpecConstant_t nativeValues[] = {{9,1},{10,2},{17,3},{18,0x3fb33333u}};
	memset( fixture, 0, sizeof( *fixture ) );
	fixture->manifest.specConstants = specs; fixture->manifest.specConstantCount = 4;
	fixture->variant.specValues = values; fixture->variant.specValueCount = 4;
	fixture->state.specConstants = nativeValues; fixture->state.numSpecConstants = 4;
	fixture->spirv[0][0] = 0x07230203u; fixture->spirv[1][0] = 0x07230203u;
	ShaderModule( &fixture->modules[0], RAL_STAGE_VERTEX, fixture->spirv[0],
		VERTEX_WGSL, sizeof( VERTEX_WGSL ) - 1u, 11u );
	ShaderModule( &fixture->modules[1], RAL_STAGE_FRAGMENT, fixture->spirv[1],
		FRAGMENT_WGSL, sizeof( FRAGMENT_WGSL ) - 1u, 21u );
	fixture->input.location = 0u; fixture->input.format = RAL_FORMAT_R32G32B32_SFLOAT;
	fixture->manifest.schemaVersion = RAL_SHADER_ABI_SCHEMA_VERSION;
	fixture->manifest.generation = 17u;
	fixture->manifest.pipelineKind = RAL_SHADER_PIPELINE_GRAPHICS;
	fixture->manifest.modules = fixture->modules; fixture->manifest.moduleCount = 2u;
	fixture->manifest.vertexInputs = &fixture->input;
	fixture->manifest.vertexInputCount = 1u;
	fixture->wgsl[0] = (ralWebGpuWgslModule_t){ VERTEX_WGSL,
		sizeof( VERTEX_WGSL ) - 1u };
	fixture->wgsl[1] = (ralWebGpuWgslModule_t){ FRAGMENT_WGSL,
		sizeof( FRAGMENT_WGSL ) - 1u };
	fixture->vertexBinding = (ralVertexBinding_t){ 0u, 12u,
		RAL_VERTEX_INPUT_PER_VERTEX };
	fixture->vertexAttribute = (ralVertexAttribute_t){ 0u, 0u,
		RAL_FORMAT_R32G32B32_SFLOAT, 0u };
	fixture->blend.writeMask = RAL_COLOR_WRITE_ALL;
	fixture->blend.writeMaskExplicit = qtrue;
	fixture->state.vertexBindings = &fixture->vertexBinding;
	fixture->state.numVertexBindings = 1u;
	fixture->state.vertexAttributes = &fixture->vertexAttribute;
	fixture->state.numVertexAttributes = 1u;
	fixture->state.topology = RAL_TOPOLOGY_TRIANGLE_LIST;
	fixture->state.raster.polygonMode = RAL_POLYGON_FILL;
	fixture->state.raster.lineWidth = 1.0f;
	fixture->state.colorBlends = &fixture->blend; fixture->state.numColorBlends = 1u;
	fixture->state.colorFormats[0] = RAL_FORMAT_R8G8B8A8_UNORM;
	fixture->state.numColorFormats = 1u; fixture->state.sampleCount = 1u;
	fixture->variant.artifactTarget = RAL_SHADER_ARTIFACT_WGSL;
	fixture->variant.generation = 3u;
	if ( Ral_GraphicsPipelineSemanticDigest( &fixture->state,
			&fixture->variant.semanticStateDigest ) != ralSuccess ) abort();
}

static int LensBridgeTest(ralWebGpuBrowserBridge_t *bridge, fakeBrowser_t *fake) {
	float params[24] = {0}, values[RAL_WEBGPU_BROWSER_LENS_SOURCE_LIMIT];
	ralWebGpuBrowserLensSource_t source = {0};
	uintptr_t query = 123u;
	CHECK(RalWebGpu_BrowserBridgeResetLensVisibility(bridge, 10u));
	CHECK(!RalWebGpu_BrowserBridgeBeginLensVisibility(bridge, 11u, 5u, params, &source, 1u, &query));
	CHECK(query == 123u);
	CHECK(RalWebGpu_BrowserBridgeBeginLensVisibility(bridge, 10u, 5u, params, &source, 1u, &query));
	CHECK(query == 0x9000u);
	for (uint32_t i=0; i<RAL_WEBGPU_BROWSER_LENS_SOURCE_LIMIT; ++i) values[i] = -7.0f;
	fake->lensStatus = RAL_WEBGPU_ASYNC_PENDING;
	CHECK(RalWebGpu_BrowserBridgePollLensVisibility(bridge,10u,5u,query,values) == RAL_WEBGPU_ASYNC_PENDING);
	for (uint32_t i=0; i<RAL_WEBGPU_BROWSER_LENS_SOURCE_LIMIT; ++i) CHECK(values[i] == -7.0f);
	fake->lensStatus = RAL_WEBGPU_ASYNC_READY;
	for (fake->lensMalformed=1u; fake->lensMalformed<=3u; fake->lensMalformed++) {
		CHECK(RalWebGpu_BrowserBridgePollLensVisibility(bridge,10u,5u,query,values) == RAL_WEBGPU_ASYNC_FAILED);
		for (uint32_t i=0; i<RAL_WEBGPU_BROWSER_LENS_SOURCE_LIMIT; ++i) CHECK(values[i] == -7.0f);
	}
	fake->lensMalformed = 0u;
	CHECK(RalWebGpu_BrowserBridgePollLensVisibility(bridge,10u,5u,query,values) == RAL_WEBGPU_ASYNC_READY);
	for (uint32_t i=0; i<RAL_WEBGPU_BROWSER_LENS_SOURCE_LIMIT; ++i) CHECK(values[i] == 0.25f);
	CHECK(RalWebGpu_BrowserBridgeResetLensVisibility(bridge,11u));
	CHECK(RalWebGpu_BrowserBridgePollLensVisibility(bridge,10u,5u,query,values) == RAL_WEBGPU_ASYNC_FAILED);
	RalWebGpu_BrowserBridgeReleaseLensVisibility(bridge,query); CHECK(fake->lensReleases == 1u);
	fake->lensBusy = qtrue;
	CHECK(RalWebGpu_BrowserBridgeBeginLensVisibility(bridge,11u,5u,params,&source,1u,&query));
	CHECK(query == 0u); fake->lensBusy = qfalse;
	return 0;
}

int main( void ) {
	fakeBrowser_t fake = { 0 }, replacement = { 0 };
	fakeBrowser_t unavailable = { .unavailable = qtrue };
	ralWebGpuBrowserBridgeCreateInfo_t createInfo;
	ralWebGpuBrowserBridge_t *bridge = NULL, *unsupportedBridge = NULL;
	ralWebGpuCoreCreateInfo_t coreInfo;
	ralWebGpuCore_t *core = NULL, *unsupportedCore = NULL;
	ralWebGpuCoreReceipt_t coreReceipt;
	ralWebGpuRuntimeCreateInfo_t runtimeInfo;
	ralWebGpuRuntime_t *runtime = NULL;
	ralWebGpuRuntimeReceipt_t runtimeReceipt;
	ralWebGpuPresentationCreateInfo_t presentationInfo;
	ralWebGpuPresentation_t *presentation = NULL;
	ralWebGpuPresentationReceipt_t configured, resized, suspended;
	ralWebGpuFrameReceipt_t frame;
	ralWebGpuBrowserLoss_t loss;
	ralMemoryFailureEvent_t lossEvent;
	ralMemoryFailureReceipt_t lossReceipt;
	ralWebGpuResourceLayerCreateInfo_t resourceInfo;
	ralWebGpuResourceLayer_t *resources = NULL;
	ralWebGpuResource_t *vertex = NULL, *index = NULL, *texture = NULL, *sampler = NULL;
	ralWebGpuResourceReceipt_t vertexReceipt, indexReceipt, textureReceipt, samplerReceipt;
	ralWebGpuBufferDesc_t bufferDesc;
	ralWebGpuTextureDesc_t textureDesc;
	ralWebGpuSamplerDesc_t samplerDesc;
	ralWebGpuWriteReceipt_t writeReceipt, unchangedWrite, writeSentinel;
	ralBackendConformanceReceipt_t conformanceReceipt;
	ralWebGpuPipelineCreateInfo_t pipelineInfo;
	ralWebGpuPipeline_t *pipeline = NULL;
	ralWebGpuPipelineReceipt_t pipelineReceipt;
	graphicsFixture_t graphics;
	ralWebGpuCommandCreateInfo_t commandInfo;
	ralWebGpuCommand_t *command = NULL;
	ralWebGpuCommandReceipt_t recording, recorded, executable;
	ralWebGpuSubmissionReceipt_t submission;
	ralWebGpuIndexedDraw_t draw;
	byte vertexBytes[36] = { 0u }, indexBytes[8] = { 0u }, textureBytes[256] = { 0u };
	CHECK( sizeof( ralWebGpuBrowserAbiHeader_t ) == 16u );
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.schemaVersion = RAL_WEBGPU_BROWSER_ABI_SCHEMA_VERSION;
	createInfo.generation = 7u; createInfo.canvasIdentity = 0x7000u;
	createInfo.userData = &fake; createInfo.dispatch = Dispatch;
	CHECK( RalWebGpu_BrowserBridgeCreate( &createInfo, &bridge ) );
	CHECK( RalWebGpu_BrowserBridgeBuildCoreInfo( bridge, &coreInfo ) );
	CHECK( !coreInfo.host.beginAdapter( coreInfo.userData, 8u ) );
	memset( &runtimeInfo, 0, sizeof( runtimeInfo ) ); runtimeInfo.core = coreInfo;
	CHECK( RalWebGpu_BrowserBridgeBuildPresentationInfo( bridge, &presentationInfo ) );
	CHECK( RalWebGpu_BrowserBridgeBuildResourceInfo( bridge, &resourceInfo ) );
	CHECK( RalWebGpu_BrowserBridgeBuildCommandInfo( bridge, &commandInfo ) );
	runtimeInfo.presentation = presentationInfo; runtimeInfo.resources = resourceInfo;
	runtimeInfo.command = commandInfo;
	CHECK( RalWebGpu_RuntimeBegin( &runtimeInfo, &runtime ) );
	while ( RalWebGpu_RuntimePoll( runtime, &runtimeReceipt )
			== RAL_WEBGPU_POLL_PENDING ) {}
	CHECK( RalWebGpu_RuntimeReceiptExact( &runtimeReceipt, &runtimeReceipt ) );
	coreReceipt = runtimeReceipt.core;
	CHECK( coreReceipt.deviceIdentity == 0x3000u && coreReceipt.queueIdentity == 0x4000u );
	CHECK( LensBridgeTest(bridge, &fake) == 0 );
	presentation = RalWebGpu_RuntimePresentation( runtime, &runtimeReceipt );
	resources = RalWebGpu_RuntimeResources( runtime, &runtimeReceipt );
	command = RalWebGpu_RuntimeCommand( runtime, &runtimeReceipt );
	CHECK( presentation && resources && command );
	CHECK( RalWebGpu_PresentationConfigure( presentation, 1280u, 720u,
		RAL_WEBGPU_DPR_ONE_Q16, RAL_FORMAT_B8G8R8A8_UNORM,
		RAL_COLORSPACE_SRGB_NONLINEAR, qtrue, &configured ) );
	CHECK( fake.width == 1280u && fake.height == 720u );
	CHECK( RalWebGpu_PresentationConfigure( presentation, 640u, 360u,
		RAL_WEBGPU_DPR_ONE_Q16 * 2u, RAL_FORMAT_B8G8R8A8_UNORM,
		RAL_COLORSPACE_SRGB_NONLINEAR, qtrue, &resized ) );
	CHECK( fake.width == 1280u && fake.height == 720u );
	CHECK( RalWebGpu_PresentationAcquire( presentation, &resized, &frame ) );
	CHECK( frame.textureIdentity == 0x8000u && fake.acquires == 1u );
	memset( &bufferDesc, 0, sizeof( bufferDesc ) ); bufferDesc.size = 36u;
	bufferDesc.usage = RAL_WEBGPU_BUFFER_VERTEX;
	bufferDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	CHECK( RalWebGpu_CreateBuffer( resources, &bufferDesc, &vertex, &vertexReceipt ) );
	bufferDesc.size = 8u; bufferDesc.usage = RAL_WEBGPU_BUFFER_INDEX;
	CHECK( RalWebGpu_CreateBuffer( resources, &bufferDesc, &index, &indexReceipt ) );
	CHECK( RalWebGpu_WriteBuffer( resources, vertex, &vertexReceipt, 0u,
		vertexBytes, sizeof( vertexBytes ), &writeReceipt ) );
	memset( &writeSentinel, 0xa5, sizeof( writeSentinel ) ); unchangedWrite = writeSentinel;
	fake.failOpcode = RAL_WEBGPU_BROWSER_OP_WRITE_BUFFER;
	CHECK( !RalWebGpu_WriteBuffer( resources, index, &indexReceipt, 0u,
		indexBytes, sizeof( indexBytes ), &unchangedWrite )
		&& !memcmp( &unchangedWrite, &writeSentinel, sizeof( unchangedWrite ) ) );
	fake.failOpcode = 0;
	CHECK( RalWebGpu_WriteBuffer( resources, index, &indexReceipt, 0u,
		indexBytes, sizeof( indexBytes ), &writeReceipt ) );
	memset( &textureDesc, 0, sizeof( textureDesc ) ); textureDesc.width = 64u;
	textureDesc.height = 1u; textureDesc.depth = 1u; textureDesc.mipLevelCount = 1u;
	textureDesc.format = RAL_FORMAT_R8G8B8A8_UNORM; textureDesc.bytesPerTexel = 4u;
	textureDesc.usage = RAL_WEBGPU_TEXTURE_COPY_SOURCE
		| RAL_WEBGPU_TEXTURE_COPY_DESTINATION | RAL_WEBGPU_TEXTURE_SAMPLED;
	CHECK( RalWebGpu_CreateTexture( resources, &textureDesc, &texture, &textureReceipt ) );
	CHECK( RalWebGpu_WriteTexture( resources, texture, &textureReceipt, textureBytes,
		sizeof( textureBytes ), 256u, 1u, &writeReceipt ) );
	memset( &samplerDesc, 0, sizeof( samplerDesc ) ); samplerDesc.clampToEdge = qtrue;
	CHECK( RalWebGpu_CreateSampler( resources, &samplerDesc, &sampler, &samplerReceipt ) );
	CHECK( RalWebGpu_OffscreenConformanceBegin( resources, 64u ) );
	CHECK( RalWebGpu_OffscreenConformancePoll( resources, &conformanceReceipt )
		== RAL_WEBGPU_ASYNC_READY );
	GraphicsSetup( &graphics ); memset( &pipelineInfo, 0, sizeof( pipelineInfo ) );
	pipelineInfo.manifest = &graphics.manifest; pipelineInfo.variant = &graphics.variant;
	pipelineInfo.modules = graphics.wgsl; pipelineInfo.moduleCount = 2u;
	pipelineInfo.graphicsState = &graphics.state; pipelineInfo.generation = 41u;
	CHECK( RalWebGpu_BrowserBridgeApplyPipelineInfo( bridge, &pipelineInfo ) );
	CHECK( RalWebGpu_RuntimeCreatePipeline( runtime, &runtimeReceipt, &pipelineInfo,
		&pipeline, &pipelineReceipt ) );
	CHECK( RalWebGpu_CommandBegin( command, RAL_WEBGPU_PASS_RENDER,
		frame.textureIdentity, &recording ) );
	memset( &draw, 0, sizeof( draw ) ); draw.kind = RAL_WEBGPU_DRAW_WORLD;
	draw.pipelineIdentity = pipelineReceipt.pipelineIdentity;
	draw.vertexBufferIdentity = vertexReceipt.resourceIdentity;
	draw.indexBufferIdentity = indexReceipt.resourceIdentity;
	draw.textureIdentity = textureReceipt.resourceIdentity;
	draw.samplerIdentity = samplerReceipt.resourceIdentity; draw.textured = qtrue;
	draw.indexCount = 3u; draw.instanceCount = 1u; draw.contentDigest = 0xabcdu;
	CHECK( RalWebGpu_CommandRecordIndexedDraw( command, &recording, &draw, &recorded ) );
	CHECK( RalWebGpu_CommandEnd( command, &recorded, &executable ) );
	CHECK( RalWebGpu_CommandSubmit( command, &executable, &submission ) );
	CHECK( RalWebGpu_CommandPoll( command, &submission ) == RAL_WEBGPU_ASYNC_READY );
	CHECK( RalWebGpu_PresentationPresent( presentation, &frame, &submission ) );
	CHECK( fake.presents == 1u );
	CHECK( RalWebGpu_RuntimeGetReceipt( runtime, &runtimeReceipt ) );
	CHECK( RalWebGpu_RuntimeOwnsPipeline( runtime, &runtimeReceipt, &pipelineReceipt ) );
	CHECK( RalWebGpu_RuntimeDestroyPipeline( runtime, pipeline, &pipelineReceipt ) );
	pipeline = NULL;
	CHECK( RalWebGpu_DestroyResource( resources, sampler, &samplerReceipt ) );
	CHECK( RalWebGpu_DestroyResource( resources, texture, &textureReceipt ) );
	CHECK( RalWebGpu_DestroyResource( resources, index, &indexReceipt ) );
	CHECK( RalWebGpu_DestroyResource( resources, vertex, &vertexReceipt ) );
	CHECK( fake.draws == 1u && fake.submissions == 1u
		&& fake.resourcesCreated == fake.resourcesDestroyed
		&& fake.pipelineObjectsCreated == fake.pipelineObjectsDestroyed
		&& fake.commandObjectsReleased == 4u );
	{
		ralWebGpuResource_t *cube = NULL;
		ralWebGpuResourceReceipt_t cubeReceipt;
		ralWebGpuTextureDesc_t desc = {0};
		desc.width = desc.height = 128; desc.depth = 6; desc.mipLevelCount = 3;
		desc.format = RAL_FORMAT_R16G16B16A16_SFLOAT; desc.bytesPerTexel = 8;
		desc.usage = RAL_WEBGPU_TEXTURE_RENDER_ATTACHMENT | RAL_WEBGPU_TEXTURE_SAMPLED;
		CHECK(RalWebGpu_CreateTexture(resources, &desc, &cube, &cubeReceipt));
		ralWebGpuPassDesc_t capture = {0};
		capture.kind = RAL_WEBGPU_PASS_RENDER; capture.targetIdentity = cubeReceipt.resourceIdentity;
		capture.colorLoadOp = RAL_WEBGPU_LOAD_CLEAR;
		capture.targetMipLevel = 2; capture.targetArrayLayer = 5;
		CHECK(RalWebGpu_CommandBeginWithDesc(command, &capture, &recording));
		CHECK(fake.targetMipLevel == 2 && fake.targetArrayLayer == 5);
		CHECK(RalWebGpu_CommandEnd(command, &recording, &executable));
		CHECK(RalWebGpu_CommandSubmit(command, &executable, &submission));
		CHECK(RalWebGpu_CommandPoll(command, &submission) == RAL_WEBGPU_ASYNC_READY);
		CHECK(RalWebGpu_DestroyResource(resources, cube, &cubeReceipt));
		CHECK(fake.resourcesCreated == fake.resourcesDestroyed);
	}
	CHECK( RalWebGpu_PresentationConfigure( presentation, 1280u, 720u,
		RAL_WEBGPU_DPR_ONE_Q16, RAL_FORMAT_B8G8R8A8_UNORM,
		RAL_COLORSPACE_SRGB_NONLINEAR, qtrue, &configured ) );
	CHECK( RalWebGpu_PresentationConfigure( presentation, 0u, 0u,
		RAL_WEBGPU_DPR_ONE_Q16, RAL_FORMAT_B8G8R8A8_UNORM,
		RAL_COLORSPACE_SRGB_NONLINEAR, qtrue, &suspended ) );
	CHECK( suspended.suspended == qtrue && suspended.configured == qfalse );
	CHECK( RalWebGpu_BrowserBridgePollLoss( bridge, &loss ) && !loss.lost );
	fake.lost = qtrue;
	CHECK( RalWebGpu_BrowserBridgePollLoss( bridge, &loss )
		&& loss.lost && loss.recoverable && !strcmp( loss.reason, "fixture loss" ) );
	memset( &lossEvent, 0, sizeof( lossEvent ) );
	lossEvent.backendType = RAL_BACKEND_WEBGPU;
	lossEvent.cause = RAL_MEMORY_FAILURE_DEVICE_LOST;
	lossEvent.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	lossEvent.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	lossEvent.criticality = RAL_MEMORY_CRITICALITY_REQUIRED;
	lossEvent.requestedBytes = 4096u; lossEvent.attempt = 1u;
	lossEvent.maxAttempts = 1u; lossEvent.liveParent = qtrue;
	CHECK( RalWebGpu_RuntimePublishDeviceLoss( runtime, &lossEvent, &lossReceipt ) );
	CHECK( lossReceipt.action == RAL_MEMORY_RECOVERY_RECREATE_BACKEND );
	CHECK( RalWebGpu_RuntimePoll( runtime, &runtimeReceipt )
		== RAL_WEBGPU_POLL_DEVICE_LOST );
	RalWebGpu_RuntimeDestroy( runtime ); runtime = NULL;
	CHECK( fake.releasedDevice == 1u && fake.releasedAdapter == 1u );
	RalWebGpu_BrowserBridgeDestroy( bridge );
	bridge = NULL; core = NULL;
	createInfo.generation = 8u; createInfo.userData = &replacement;
	CHECK( RalWebGpu_BrowserBridgeCreate( &createInfo, &bridge ) );
	CHECK( RalWebGpu_BrowserBridgeBuildCoreInfo( bridge, &coreInfo ) );
	CHECK( RalWebGpu_CoreBegin( &coreInfo, &core ) );
	while ( RalWebGpu_CorePoll( core, &coreReceipt ) == RAL_WEBGPU_POLL_PENDING ) {}
	CHECK( coreReceipt.generation == 8u && RalWebGpu_CoreMatchesReceipt( core,
		&coreReceipt ) );
	RalWebGpu_CoreDestroy( core ); RalWebGpu_BrowserBridgeDestroy( bridge );

	createInfo.generation = 9u; createInfo.userData = &unavailable;
	CHECK( RalWebGpu_BrowserBridgeCreate( &createInfo, &unsupportedBridge ) );
	CHECK( RalWebGpu_BrowserBridgeBuildCoreInfo( unsupportedBridge, &coreInfo ) );
	CHECK( RalWebGpu_CoreBegin( &coreInfo, &unsupportedCore ) );
	CHECK( RalWebGpu_CorePoll( unsupportedCore, NULL ) == RAL_WEBGPU_POLL_UNAVAILABLE );
	RalWebGpu_CoreDestroy( unsupportedCore );
	RalWebGpu_BrowserBridgeDestroy( unsupportedBridge );
	puts( "ral WebGPU browser core/canvas ABI contract: PASS" ); return 0;
}
