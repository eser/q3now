// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_opengl_core.h"
#include "ral_opengl_internal.h"

#include "ral_allocation.h"
#include "ral_command_lifecycle.h"
#include "ral_transfer.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define RAL_GL_APIENTRY __stdcall
#else
#define RAL_GL_APIENTRY
#endif

// GL declarations stay private to the adapter. The platform-facing resolver
// supplies these OpenGL 4.6 Core entry points after making its context current.
typedef unsigned int glEnum_t;
typedef unsigned int glName_t;
typedef int glInt_t;
typedef int glSize_t;
typedef long glSizePtr_t;
typedef long glIntPtr_t;
typedef unsigned int glBitfield_t;
typedef unsigned char glBool_t;

enum {
	GL_NO_ERROR_VALUE = 0,
	GL_VENDOR_VALUE = 0x1F00,
	GL_RENDERER_VALUE = 0x1F01,
	GL_VERSION_VALUE = 0x1F02,
	GL_MAJOR_VERSION_VALUE = 0x821B,
	GL_MINOR_VERSION_VALUE = 0x821C,
	GL_CONTEXT_PROFILE_MASK_VALUE = 0x9126,
	GL_CONTEXT_CORE_PROFILE_BIT_VALUE = 0x00000001,
	GL_MAX_COLOR_ATTACHMENTS_VALUE = 0x8CDF,
	GL_MAX_TEXTURE_SIZE_VALUE = 0x0D33,
	GL_MAX_3D_TEXTURE_SIZE_VALUE = 0x8073,
	GL_MAX_ARRAY_TEXTURE_LAYERS_VALUE = 0x88FF,
	GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS_VALUE = 0x90EB,
	GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT_VALUE = 0x8A34,
	GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT_VALUE = 0x90DF,
	GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_VALUE = 0x8B4D,
	GL_MAP_READ_BIT_VALUE = 0x0001,
	GL_MAP_WRITE_BIT_VALUE = 0x0002,
	GL_DYNAMIC_STORAGE_BIT_VALUE = 0x0100,
	GL_TEXTURE_2D_VALUE = 0x0DE1,
	GL_RGBA8_VALUE = 0x8058,
	GL_TEXTURE_MIN_FILTER_VALUE = 0x2801,
	GL_TEXTURE_MAG_FILTER_VALUE = 0x2800,
	GL_LINEAR_VALUE = 0x2601
};

typedef void ( RAL_GL_APIENTRY *getIntegerFn )( glEnum_t, glInt_t * );
typedef const unsigned char *( RAL_GL_APIENTRY *getStringFn )( glEnum_t );
typedef glEnum_t ( RAL_GL_APIENTRY *getErrorFn )( void );
typedef void ( RAL_GL_APIENTRY *createBuffersFn )( glSize_t, glName_t * );
typedef void ( RAL_GL_APIENTRY *namedBufferStorageFn )( glName_t, glSizePtr_t, const void *,
	glBitfield_t );
typedef void *( RAL_GL_APIENTRY *mapNamedBufferRangeFn )( glName_t, glIntPtr_t, glSizePtr_t,
	glBitfield_t );
typedef glBool_t ( RAL_GL_APIENTRY *unmapNamedBufferFn )( glName_t );
typedef void ( RAL_GL_APIENTRY *copyNamedBufferSubDataFn )( glName_t, glName_t, glIntPtr_t,
	glIntPtr_t, glSizePtr_t );
typedef void ( RAL_GL_APIENTRY *deleteBuffersFn )( glSize_t, const glName_t * );
typedef void ( RAL_GL_APIENTRY *createTexturesFn )( glEnum_t, glSize_t, glName_t * );
typedef void ( RAL_GL_APIENTRY *textureStorage2DFn )( glName_t, glSize_t, glEnum_t, glSize_t,
	glSize_t );
typedef void ( RAL_GL_APIENTRY *deleteTexturesFn )( glSize_t, const glName_t * );
typedef void ( RAL_GL_APIENTRY *createSamplersFn )( glSize_t, glName_t * );
typedef void ( RAL_GL_APIENTRY *samplerParameteriFn )( glName_t, glEnum_t, glInt_t );
typedef void ( RAL_GL_APIENTRY *deleteSamplersFn )( glSize_t, const glName_t * );
typedef void ( RAL_GL_APIENTRY *finishFn )( void );

typedef struct {
	getIntegerFn GetIntegerv;
	getStringFn GetString;
	getErrorFn GetError;
	createBuffersFn CreateBuffers;
	namedBufferStorageFn NamedBufferStorage;
	mapNamedBufferRangeFn MapNamedBufferRange;
	unmapNamedBufferFn UnmapNamedBuffer;
	copyNamedBufferSubDataFn CopyNamedBufferSubData;
	deleteBuffersFn DeleteBuffers;
	createTexturesFn CreateTextures;
	textureStorage2DFn TextureStorage2D;
	deleteTexturesFn DeleteTextures;
	createSamplersFn CreateSamplers;
	samplerParameteriFn SamplerParameteri;
	deleteSamplersFn DeleteSamplers;
	finishFn Finish;
} ralOpenGlDispatch_t;

struct ralOpenGlCore_s {
	ralOpenGlDispatch_t gl;
	uint64_t generation;
	uint64_t allocationGeneration;
	uint64_t submissionGeneration;
	uint64_t commandToken;
	uint64_t queueToken;
	uintptr_t contextIdentity;
	ralCaps_t caps;
	ralCapabilityProfile_t capabilityProfile;
	ralMemoryFailureLedger_t failureLedger;
	qboolean deviceLost;
	ralOpenGlResolveProcFn resolveProc;
	void *resolveUserData;
};

static qboolean LoadDispatch( const ralOpenGlCoreCreateInfo_t *info,
		ralOpenGlDispatch_t *gl ) {
#define LOAD_GL( member, symbol, type ) do { \
	ralOpenGlProc_t proc = info->resolveProc( symbol, info->resolveUserData ); \
	if ( !proc ) return qfalse; \
	gl->member = (type)proc; \
} while ( 0 )
	memset( gl, 0, sizeof( *gl ) );
	LOAD_GL( GetIntegerv, "glGetIntegerv", getIntegerFn );
	LOAD_GL( GetString, "glGetString", getStringFn );
	LOAD_GL( GetError, "glGetError", getErrorFn );
	LOAD_GL( CreateBuffers, "glCreateBuffers", createBuffersFn );
	LOAD_GL( NamedBufferStorage, "glNamedBufferStorage", namedBufferStorageFn );
	LOAD_GL( MapNamedBufferRange, "glMapNamedBufferRange", mapNamedBufferRangeFn );
	LOAD_GL( UnmapNamedBuffer, "glUnmapNamedBuffer", unmapNamedBufferFn );
	LOAD_GL( CopyNamedBufferSubData, "glCopyNamedBufferSubData",
		copyNamedBufferSubDataFn );
	LOAD_GL( DeleteBuffers, "glDeleteBuffers", deleteBuffersFn );
	LOAD_GL( CreateTextures, "glCreateTextures", createTexturesFn );
	LOAD_GL( TextureStorage2D, "glTextureStorage2D", textureStorage2DFn );
	LOAD_GL( DeleteTextures, "glDeleteTextures", deleteTexturesFn );
	LOAD_GL( CreateSamplers, "glCreateSamplers", createSamplersFn );
	LOAD_GL( SamplerParameteri, "glSamplerParameteri", samplerParameteriFn );
	LOAD_GL( DeleteSamplers, "glDeleteSamplers", deleteSamplersFn );
	LOAD_GL( Finish, "glFinish", finishFn );
#undef LOAD_GL
	return qtrue;
}

static void CopyString( char *out, size_t outSize,
		const unsigned char *value ) {
	const char *text = (const char *)value;
	memset( out, 0, outSize );
	if ( text ) {
		strncpy( out, text, outSize - 1u );
		out[outSize - 1u] = '\0';
	}
}

static qboolean QueryPositive( const ralOpenGlDispatch_t *gl, glEnum_t name,
		uint32_t *out ) {
	glInt_t value = 0;
	gl->GetIntegerv( name, &value );
	if ( value <= 0 || gl->GetError() != GL_NO_ERROR_VALUE ) return qfalse;
	*out = (uint32_t)value;
	return qtrue;
}

static qboolean BuildCaps( ralOpenGlCore_t *core ) {
	uint32_t value;
	ralCaps_t caps;
	memset( &caps, 0, sizeof( caps ) );
	caps.dynamicRendering = qtrue;
	if ( !QueryPositive( &core->gl, GL_MAX_COLOR_ATTACHMENTS_VALUE,
			&caps.maxColorAttachments )
			|| !QueryPositive( &core->gl, GL_MAX_TEXTURE_SIZE_VALUE,
				&caps.maxTextureDimension2D )
			|| !QueryPositive( &core->gl, GL_MAX_3D_TEXTURE_SIZE_VALUE,
				&caps.maxTextureDimension3D )
			|| !QueryPositive( &core->gl, GL_MAX_ARRAY_TEXTURE_LAYERS_VALUE,
				&caps.maxTextureArrayLayers )
			|| !QueryPositive( &core->gl,
				GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS_VALUE,
				&caps.maxComputeWorkgroupSize )
			|| !QueryPositive( &core->gl,
				GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT_VALUE, &value ) ) return qfalse;
	caps.minUniformBufferAlignment = value;
	if ( !QueryPositive( &core->gl,
			GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT_VALUE, &value ) ) return qfalse;
	caps.minStorageBufferAlignment = value;
	if ( !QueryPositive( &core->gl, GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_VALUE,
			&caps.maxSampledTexturesPerShaderStage ) ) return qfalse;
	if ( caps.maxColorAttachments < 4u || caps.maxTextureDimension2D < 4096u )
		return qfalse;
	caps.maxBindGroups = 8u;
	caps.maxBindlessTextures = 64u;
	caps.maxPushConstantSize = 256u;
	caps.maxStorageBufferRange = UINT64_C(134217728);
	caps.maxSamplerAnisotropy = 1.0f;
	caps.independentBlend = qtrue;
	caps.textureCompressionBC = qtrue;
	caps.offscreenPresentation = qtrue;
	caps.adapterType = RAL_ADAPTER_TYPE_UNKNOWN;
	CopyString( caps.vendorName, sizeof( caps.vendorName ),
		core->gl.GetString( GL_VENDOR_VALUE ) );
	CopyString( caps.deviceName, sizeof( caps.deviceName ),
		core->gl.GetString( GL_RENDERER_VALUE ) );
	CopyString( caps.driverVersion, sizeof( caps.driverVersion ),
		core->gl.GetString( GL_VERSION_VALUE ) );
	strncpy( caps.apiVersion, "OpenGL 4.6 Core", sizeof( caps.apiVersion ) - 1u );
	if ( caps.vendorName[0] == '\0' || caps.deviceName[0] == '\0'
			|| caps.driverVersion[0] == '\0'
			|| core->gl.GetError() != GL_NO_ERROR_VALUE ) return qfalse;
	core->caps = caps;
	return Ral_CapabilityProfileFromCaps( RAL_BACKEND_OPENGL, &core->caps,
		core->generation, &core->capabilityProfile );
}

static qboolean ReceiptValid( const ralOpenGlCoreReceipt_t *receipt ) {
	return receipt && receipt->schemaVersion == RAL_OPENGL_CORE_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_OPENGL
		&& receipt->generation != 0u && receipt->generation != UINT64_MAX
		&& receipt->backendIdentity != (uintptr_t)0
		&& receipt->contextIdentity != (uintptr_t)0
		&& receipt->queueIdentity != (uintptr_t)0
		&& receipt->backendIdentity != receipt->contextIdentity
		&& receipt->backendIdentity != receipt->queueIdentity
		&& receipt->versionMajor == RAL_OPENGL_REQUIRED_MAJOR
		&& receipt->versionMinor >= RAL_OPENGL_REQUIRED_MINOR
		&& receipt->coreProfile == qtrue
		&& receipt->caps.dynamicRendering == qtrue
		&& receipt->caps.maxColorAttachments >= 4u
		&& receipt->caps.maxTextureDimension2D >= 4096u
		&& Ral_CapabilityProfileExact( &receipt->capabilityProfile,
			&receipt->capabilityProfile )
		&& receipt->ready == qtrue;
}

qboolean RalOpenGl_CoreCreate( const ralOpenGlCoreCreateInfo_t *createInfo,
		ralOpenGlCore_t **outCore, ralOpenGlCoreReceipt_t *outReceipt ) {
	ralOpenGlCore_t *candidate;
	ralOpenGlCoreReceipt_t receipt;
	glInt_t major = 0, minor = 0, profile = 0;
	if ( !createInfo || !outCore || !outReceipt || !createInfo->resolveProc
			|| !createInfo->contextIdentity || createInfo->generation == 0u
			|| createInfo->generation == UINT64_MAX ) return qfalse;
	candidate = (ralOpenGlCore_t *)calloc( 1u, sizeof( *candidate ) );
	if ( !candidate ) return qfalse;
	if ( !LoadDispatch( createInfo, &candidate->gl ) ) goto fail;
	candidate->generation = createInfo->generation;
	candidate->contextIdentity = createInfo->contextIdentity;
	candidate->resolveProc = createInfo->resolveProc;
	candidate->resolveUserData = createInfo->resolveUserData;
	candidate->queueToken = candidate->generation ^ UINT64_C(0x4f50454e474c3436);
	candidate->gl.GetIntegerv( GL_MAJOR_VERSION_VALUE, &major );
	candidate->gl.GetIntegerv( GL_MINOR_VERSION_VALUE, &minor );
	candidate->gl.GetIntegerv( GL_CONTEXT_PROFILE_MASK_VALUE, &profile );
	if ( candidate->gl.GetError() != GL_NO_ERROR_VALUE
			|| major != (glInt_t)RAL_OPENGL_REQUIRED_MAJOR
			|| minor < (glInt_t)RAL_OPENGL_REQUIRED_MINOR
			|| !( profile & GL_CONTEXT_CORE_PROFILE_BIT_VALUE )
			|| !BuildCaps( candidate ) ) goto fail;
	Ral_MemoryFailureLedgerInit( &candidate->failureLedger );
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_OPENGL_CORE_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_OPENGL;
	receipt.generation = candidate->generation;
	receipt.backendIdentity = (uintptr_t)candidate;
	receipt.contextIdentity = candidate->contextIdentity;
	receipt.queueIdentity = (uintptr_t)&candidate->queueToken;
	receipt.versionMajor = (uint32_t)major;
	receipt.versionMinor = (uint32_t)minor;
	receipt.coreProfile = qtrue;
	receipt.caps = candidate->caps;
	receipt.capabilityProfile = candidate->capabilityProfile;
	receipt.ready = qtrue;
	if ( !ReceiptValid( &receipt ) ) goto fail;
	*outCore = candidate;
	*outReceipt = receipt;
	return qtrue;
fail:
	memset( candidate, 0, sizeof( *candidate ) );
	free( candidate );
	return qfalse;
}

qboolean RalOpenGl_CoreMatchesReceipt( const ralOpenGlCore_t *core,
		const ralOpenGlCoreReceipt_t *receipt ) {
	return core && core->deviceLost != qtrue && ReceiptValid( receipt )
		&& receipt->generation == core->generation
		&& receipt->backendIdentity == (uintptr_t)core
		&& receipt->contextIdentity == core->contextIdentity
		&& receipt->queueIdentity == (uintptr_t)&core->queueToken;
}

ralOpenGlProc_t RalOpenGl_CoreResolve( const ralOpenGlCore_t *core,
		const char *name ) {
	if ( !core || !name || !name[0] || core->deviceLost == qtrue
			|| !core->resolveProc ) return NULL;
	return core->resolveProc( name, core->resolveUserData );
}

void RalOpenGl_CoreDestroy( ralOpenGlCore_t *core ) {
	if ( !core ) return;
	memset( core, 0, sizeof( *core ) );
	free( core );
}

qboolean RalOpenGl_CoreReceiptExact( const ralOpenGlCoreReceipt_t *a,
		const ralOpenGlCoreReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& a->backendType == b->backendType && a->generation == b->generation
		&& a->backendIdentity == b->backendIdentity
		&& a->contextIdentity == b->contextIdentity
		&& a->queueIdentity == b->queueIdentity
		&& a->versionMajor == b->versionMajor
		&& a->versionMinor == b->versionMinor
		&& a->coreProfile == b->coreProfile
		&& !memcmp( &a->caps, &b->caps, sizeof( a->caps ) )
		&& Ral_CapabilityProfileExact( &a->capabilityProfile,
			&b->capabilityProfile );
}

static qboolean BuildAllocation( ralOpenGlCore_t *core, uintptr_t identity,
		ralAllocationClass_t memoryClass, uint64_t size, uint64_t alignment,
		ralAllocationReceipt_t *out ) {
	ralAllocationRequest_t request;
	ralAllocationFacts_t facts;
	memset( &request, 0, sizeof( request ) );
	memset( &facts, 0, sizeof( facts ) );
	request.memoryClass = memoryClass;
	request.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	request.size = size;
	request.alignment = alignment;
	request.ownerIdentity = identity;
	request.ownerGeneration = core->generation;
	request.allowFallback = qtrue;
	facts.backendType = RAL_BACKEND_OPENGL;
	facts.placement = RAL_ALLOCATION_PLACEMENT_MANAGED;
	facts.committedSize = ( size + alignment - 1u ) & ~( alignment - 1u );
	facts.actualAlignment = alignment;
	facts.allocationGeneration = ++core->allocationGeneration;
	facts.deviceLocal = memoryClass == RAL_ALLOCATION_DEVICE_LOCAL ? qtrue : qfalse;
	facts.hostVisible = memoryClass == RAL_ALLOCATION_UPLOAD
		|| memoryClass == RAL_ALLOCATION_READBACK ? qtrue : qfalse;
	facts.hostCoherent = facts.hostVisible;
	return Ral_AllocationReceiptBuild( &request, &facts, out );
}

static uint64_t DigestBytes( const unsigned char *bytes, uint64_t count ) {
	uint64_t digest = UINT64_C(1469598103934665603);
	uint64_t i;
	for ( i = 0u; i < count; ++i ) {
		digest ^= bytes[i];
		digest *= UINT64_C(1099511628211);
	}
	return digest;
}

qboolean RalOpenGl_OffscreenConformance( ralOpenGlCore_t *core,
		uint64_t byteCount, ralBackendConformanceReceipt_t *outReceipt ) {
	ralBackendConformanceReceipt_t receipt;
	ralBackendConformanceFacts_t conformance;
	ralAllocationReceipt_t uploadAllocation, readbackAllocation, textureAllocation;
	ralCommandLifecycle_t command;
	ralSubmissionLifecycle_t submissionLifecycle;
	ralCommandLifecycle_t *commands[1];
	ralCommandReceipt_t recording, executable;
	ralSubmissionReceipt_t submission;
	ralTransferRequest_t transferRequest;
	ralTransferReceipt_t prepared, transfer;
	glName_t upload = 0u, readback = 0u, texture = 0u, sampler = 0u;
	unsigned char *mappedUpload = NULL, *mappedReadback = NULL;
	uint64_t digest;
	uint64_t i;
	if ( !core || !outReceipt || byteCount == 0u || byteCount > UINT64_C(1048576)
			|| core->deviceLost == qtrue
			|| core->allocationGeneration > UINT64_MAX - 4u
			|| core->submissionGeneration == UINT64_MAX ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	core->gl.CreateBuffers( 1, &upload );
	core->gl.CreateBuffers( 1, &readback );
	core->gl.CreateTextures( GL_TEXTURE_2D_VALUE, 1, &texture );
	core->gl.CreateSamplers( 1, &sampler );
	if ( !upload || !readback || !texture || !sampler
			|| core->gl.GetError() != GL_NO_ERROR_VALUE ) goto fail;
	core->gl.NamedBufferStorage( upload, (glSizePtr_t)byteCount, NULL,
		GL_MAP_WRITE_BIT_VALUE | GL_DYNAMIC_STORAGE_BIT_VALUE );
	core->gl.NamedBufferStorage( readback, (glSizePtr_t)byteCount, NULL,
		GL_MAP_READ_BIT_VALUE | GL_DYNAMIC_STORAGE_BIT_VALUE );
	core->gl.TextureStorage2D( texture, 1, GL_RGBA8_VALUE, 4, 4 );
	core->gl.SamplerParameteri( sampler, GL_TEXTURE_MIN_FILTER_VALUE, GL_LINEAR_VALUE );
	core->gl.SamplerParameteri( sampler, GL_TEXTURE_MAG_FILTER_VALUE, GL_LINEAR_VALUE );
	if ( core->gl.GetError() != GL_NO_ERROR_VALUE ) goto fail;
	mappedUpload = (unsigned char *)core->gl.MapNamedBufferRange( upload, 0,
		(glSizePtr_t)byteCount, GL_MAP_WRITE_BIT_VALUE );
	if ( !mappedUpload ) goto fail;
	for ( i = 0u; i < byteCount; ++i )
		mappedUpload[i] = (unsigned char)( ( i * 37u + 11u ) & 0xffu );
	if ( !core->gl.UnmapNamedBuffer( upload ) ) goto fail;
	mappedUpload = NULL;
	if ( !BuildAllocation( core, (uintptr_t)upload, RAL_ALLOCATION_UPLOAD,
			byteCount, 16u, &uploadAllocation )
			|| !BuildAllocation( core, (uintptr_t)readback, RAL_ALLOCATION_READBACK,
				byteCount, 16u, &readbackAllocation )
			|| !BuildAllocation( core, (uintptr_t)texture,
				RAL_ALLOCATION_DEVICE_LOCAL, 64u, 4u, &textureAllocation ) ) goto fail;
	core->commandToken++;
	Ral_CommandLifecycleInit( &command, (const ralBackend_t *)core,
		(const ralCommandBuffer_t *)&core->commandToken, RAL_QUEUE_GRAPHICS );
	if ( Ral_CommandLifecyclePublishBegin( &command, &recording ) != ralSuccess )
		goto fail;
	core->gl.CopyNamedBufferSubData( upload, readback, 0, 0,
		(glSizePtr_t)byteCount );
	if ( Ral_CommandLifecyclePublishEnd( &command, &recording, &executable )
			!= ralSuccess ) goto fail;
	core->gl.Finish();
	if ( core->gl.GetError() != GL_NO_ERROR_VALUE ) goto fail;
	mappedReadback = (unsigned char *)core->gl.MapNamedBufferRange( readback, 0,
		(glSizePtr_t)byteCount, GL_MAP_READ_BIT_VALUE );
	if ( !mappedReadback ) goto fail;
	for ( i = 0u; i < byteCount; ++i )
		if ( mappedReadback[i] != (unsigned char)( ( i * 37u + 11u ) & 0xffu ) )
			goto fail;
	digest = DigestBytes( mappedReadback, byteCount );
	if ( !core->gl.UnmapNamedBuffer( readback ) ) goto fail;
	mappedReadback = NULL;
	Ral_SubmissionLifecycleInit( &submissionLifecycle, (const ralBackend_t *)core,
		RAL_QUEUE_GRAPHICS );
	submissionLifecycle.generation = core->submissionGeneration;
	commands[0] = &command;
	if ( Ral_SubmissionLifecyclePublish( &submissionLifecycle, commands,
			&executable, 1u, &submission ) != ralSuccess ) goto fail;
	memset( &transferRequest, 0, sizeof( transferRequest ) );
	transferRequest.backendType = RAL_BACKEND_OPENGL;
	transferRequest.direction = RAL_TRANSFER_READBACK;
	transferRequest.resourceKind = RAL_TRANSFER_BUFFER;
	transferRequest.resourceIdentity = (uintptr_t)readback;
	transferRequest.resourceGeneration = readbackAllocation.allocationGeneration;
	transferRequest.byteSize = byteCount;
	transferRequest.byteBudget = byteCount;
	transferRequest.queue = RAL_QUEUE_GRAPHICS;
	if ( !Ral_TransferPrepare( &transferRequest, submission.generation, &prepared )
			|| !Ral_TransferPublish( &prepared, RAL_TRANSFER_OUTCOME_SYNCHRONOUS,
				submission.generation, &transfer ) ) goto fail;
	memset( &conformance, 0, sizeof( conformance ) );
	conformance.backendType = RAL_BACKEND_OPENGL;
	conformance.backendGeneration = core->generation;
	conformance.backendIdentity = (uintptr_t)core;
	conformance.deviceIdentity = core->contextIdentity;
	conformance.graphicsQueueIdentity = (uintptr_t)&core->queueToken;
	conformance.capabilities = &core->capabilityProfile;
	conformance.uploadAllocation = &uploadAllocation;
	conformance.readbackAllocation = &readbackAllocation;
	conformance.textureAllocation = &textureAllocation;
	conformance.samplerIdentity = (uintptr_t)sampler;
	conformance.samplerGeneration = textureAllocation.allocationGeneration + 1u;
	conformance.submission = &submission;
	conformance.transfer = &transfer;
	conformance.completionGeneration = submission.generation;
	conformance.copiedByteCount = byteCount;
	conformance.copiedByteDigest = digest;
	if ( !Ral_BackendConformanceBuild( &conformance, &receipt )
			|| !Ral_BackendConformanceReceiptExact( &receipt, &receipt ) ) goto fail;
	core->submissionGeneration = submissionLifecycle.generation;
	core->gl.DeleteSamplers( 1, &sampler );
	core->gl.DeleteTextures( 1, &texture );
	core->gl.DeleteBuffers( 1, &readback );
	core->gl.DeleteBuffers( 1, &upload );
	if ( core->gl.GetError() != GL_NO_ERROR_VALUE ) return qfalse;
	*outReceipt = receipt;
	return qtrue;
fail:
	if ( mappedReadback ) core->gl.UnmapNamedBuffer( readback );
	if ( mappedUpload ) core->gl.UnmapNamedBuffer( upload );
	if ( sampler ) core->gl.DeleteSamplers( 1, &sampler );
	if ( texture ) core->gl.DeleteTextures( 1, &texture );
	if ( readback ) core->gl.DeleteBuffers( 1, &readback );
	if ( upload ) core->gl.DeleteBuffers( 1, &upload );
	return qfalse;
}

qboolean RalOpenGl_CorePublishDeviceLoss( ralOpenGlCore_t *core,
		const ralMemoryFailureEvent_t *event, ralMemoryFailureReceipt_t *outReceipt ) {
	ralMemoryFailureLedger_t candidate;
	ralMemoryFailureReceipt_t receipt;
	if ( !core || !event || !outReceipt
			|| event->backendType != RAL_BACKEND_OPENGL
			|| event->cause != RAL_MEMORY_FAILURE_DEVICE_LOST
			|| core->deviceLost == qtrue ) return qfalse;
	candidate = core->failureLedger;
	if ( !Ral_MemoryFailureLedgerPublish( &candidate, event )
			|| !Ral_MemoryFailureLedgerGet( &candidate, &receipt )
			|| receipt.action != RAL_MEMORY_RECOVERY_RECREATE_BACKEND
			|| !Ral_MemoryFailureReceiptExact( &receipt, &receipt ) ) return qfalse;
	core->failureLedger = candidate;
	core->deviceLost = qtrue;
	*outReceipt = receipt;
	return qtrue;
}
