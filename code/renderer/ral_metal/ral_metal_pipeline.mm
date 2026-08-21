// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_pipeline.h"
#include "ral_metal_internal.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct ralMetalPipeline_s {
	id<MTLLibrary> vertexLibrary;
	id<MTLLibrary> fragmentLibrary;
	id<MTLRenderPipelineState> pipelineState;
	id<MTLDepthStencilState> depthStencilState;
	ralMetalPipelineReceipt_t receipt;
};

static qboolean ReadDigest( const char *path, NSData **outData,
		ralShaderDigest_t *outDigest ) {
	NSData *data;
	if ( !path || !outData || !outDigest ) return qfalse;
	data = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:path]];
	if ( !data || data.length == 0u || data.length > UINT32_MAX
			|| Ral_ShaderArtifactDigest( data.bytes, (uint32_t)data.length,
				outDigest ) != ralSuccess ) return qfalse;
	*outData = data;
	return qtrue;
}

static qboolean LayoutIsCanonicalOverlay( const ralMetalPipelineCreateInfo_t *info ) {
	const ralMetalBindLayoutReceipt_t *layout;
	if ( !info || !info->fragmentLayout || !info->fragmentLayoutReceipt ) return qfalse;
	layout = info->fragmentLayoutReceipt;
	return ( RalMetal_BindLayoutMatchesReceipt( info->fragmentLayout, layout )
		&& layout->layoutIdentity == (uintptr_t)info->fragmentLayout
		&& layout->entryCount == 2u && layout->totalArgumentCount == 2u
		&& layout->entries[0].binding == 0u
		&& layout->entries[0].type == RAL_BIND_SAMPLED_TEXTURE
		&& layout->entries[0].count == 1u
		&& layout->entries[0].stageFlags == RAL_STAGE_FRAGMENT
		&& layout->entries[0].textureViewType == RAL_BIND_TEXTURE_VIEW_2D
		&& layout->entries[0].argumentIndex == 0u
		&& layout->entries[1].binding == 32u
		&& layout->entries[1].type == RAL_BIND_SAMPLER
		&& layout->entries[1].count == 1u
		&& layout->entries[1].stageFlags == RAL_STAGE_FRAGMENT
		&& layout->entries[1].argumentIndex == 1u ) ? qtrue : qfalse;
}

static qboolean FormatValid( ralFormat_t color, ralFormat_t depth ) {
	return ( color == RAL_FORMAT_R8G8B8A8_UNORM
		&& ( depth == RAL_FORMAT_UNDEFINED || depth == RAL_FORMAT_D32_SFLOAT ) )
		? qtrue : qfalse;
}

static qboolean PipelineReceiptValid( const ralMetalPipelineReceipt_t *receipt ) {
	return ( receipt && receipt->schemaVersion == RAL_METAL_PIPELINE_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_METAL
		&& receipt->coreGeneration != 0u && receipt->coreGeneration != UINT64_MAX
		&& receipt->pipelineGeneration != 0u && receipt->pipelineGeneration != UINT64_MAX
		&& receipt->pipelineIdentity != (uintptr_t)0
		&& Ral_ShaderDigestValid( &receipt->vertexSourceDigest )
		&& Ral_ShaderDigestValid( &receipt->fragmentSourceDigest )
		&& Ral_ShaderDigestValid( &receipt->vertexLibraryDigest )
		&& Ral_ShaderDigestValid( &receipt->fragmentLibraryDigest )
		&& receipt->vertexSourceBytes != 0u && receipt->fragmentSourceBytes != 0u
		&& receipt->vertexLibraryBytes != 0u && receipt->fragmentLibraryBytes != 0u
		&& receipt->fragmentLayoutIdentity != (uintptr_t)0
		&& receipt->pipelineIdentity != receipt->fragmentLayoutIdentity
		&& receipt->fragmentLayoutGeneration != 0u
		&& receipt->fragmentLayoutGeneration != UINT64_MAX
		&& FormatValid( receipt->colorFormat, receipt->depthFormat )
		&& receipt->sampleCount == 1u && receipt->vertexStride == 20u
		&& receipt->vertexInputs[0].location == 0u
		&& receipt->vertexInputs[0].format == RAL_FORMAT_R32G32_SFLOAT
		&& receipt->vertexInputOffsets[0] == 0u
		&& receipt->vertexInputs[1].location == 1u
		&& receipt->vertexInputs[1].format == RAL_FORMAT_R8G8B8A8_UNORM
		&& receipt->vertexInputOffsets[1] == 8u
		&& receipt->vertexInputs[2].location == 2u
		&& receipt->vertexInputs[2].format == RAL_FORMAT_R32G32_SFLOAT
		&& receipt->vertexInputOffsets[2] == 12u
		&& receipt->blendEnabled == qtrue && receipt->ready == qtrue ) ? qtrue : qfalse;
}

qboolean RalMetal_PipelineReceiptExact( const ralMetalPipelineReceipt_t *a,
		const ralMetalPipelineReceipt_t *b ) {
	return ( PipelineReceiptValid( a ) && PipelineReceiptValid( b )
		&& a->backendType == b->backendType && a->coreGeneration == b->coreGeneration
		&& a->pipelineGeneration == b->pipelineGeneration
		&& a->pipelineIdentity == b->pipelineIdentity
		&& Ral_ShaderDigestExact( &a->vertexSourceDigest, &b->vertexSourceDigest )
		&& Ral_ShaderDigestExact( &a->fragmentSourceDigest, &b->fragmentSourceDigest )
		&& Ral_ShaderDigestExact( &a->vertexLibraryDigest, &b->vertexLibraryDigest )
		&& Ral_ShaderDigestExact( &a->fragmentLibraryDigest, &b->fragmentLibraryDigest )
		&& a->vertexSourceBytes == b->vertexSourceBytes
		&& a->fragmentSourceBytes == b->fragmentSourceBytes
		&& a->vertexLibraryBytes == b->vertexLibraryBytes
		&& a->fragmentLibraryBytes == b->fragmentLibraryBytes
		&& a->fragmentLayoutIdentity == b->fragmentLayoutIdentity
		&& a->fragmentLayoutGeneration == b->fragmentLayoutGeneration
		&& a->colorFormat == b->colorFormat && a->depthFormat == b->depthFormat
		&& a->sampleCount == b->sampleCount && a->vertexStride == b->vertexStride
		&& !memcmp( a->vertexInputs, b->vertexInputs, sizeof( a->vertexInputs ) )
		&& !memcmp( a->vertexInputOffsets, b->vertexInputOffsets,
			sizeof( a->vertexInputOffsets ) )
		&& a->blendEnabled == b->blendEnabled && a->ready == b->ready ) ? qtrue : qfalse;
}

qboolean RalMetal_PipelineMatchesReceipt( const ralMetalPipeline_t *pipeline,
		const ralMetalPipelineReceipt_t *receipt ) {
	return ( pipeline && receipt && receipt->pipelineIdentity == (uintptr_t)pipeline
		&& RalMetal_PipelineReceiptExact( receipt, &pipeline->receipt ) ) ? qtrue : qfalse;
}

id<MTLRenderPipelineState> RalMetal_PipelineNativeState( ralMetalPipeline_t *pipeline ) {
	return pipeline ? pipeline->pipelineState : nil;
}

id<MTLDepthStencilState> RalMetal_PipelineNativeDepthState( ralMetalPipeline_t *pipeline ) {
	return pipeline ? pipeline->depthStencilState : nil;
}

qboolean RalMetal_PipelineCreate( ralMetalCore_t *core,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalPipelineCreateInfo_t *createInfo,
		ralMetalPipeline_t **outPipeline, ralMetalPipelineReceipt_t *outReceipt ) {
	ralMetalPipeline_t *candidate;
	ralMetalPipelineReceipt_t receipt;
	NSData *vertexSource, *fragmentSource, *vertexLibraryData, *fragmentLibraryData;
	ralShaderDigest_t vertexSourceDigest, fragmentSourceDigest;
	id<MTLDevice> device;
	id<MTLFunction> vertexFunction = nil, fragmentFunction = nil;
	MTLVertexDescriptor *vertexDescriptor = nil;
	MTLRenderPipelineDescriptor *pipelineInfo = nil;
	MTLDepthStencilDescriptor *depthInfo = nil;
	if ( !core || !coreReceipt || !createInfo || !outPipeline || !outReceipt
			|| (uintptr_t)createInfo->fragmentLayout == (uintptr_t)core
			|| createInfo->generation == 0u || createInfo->generation == UINT64_MAX
			|| createInfo->sampleCount != 1u
			|| !FormatValid( createInfo->colorFormat, createInfo->depthFormat )
			|| !LayoutIsCanonicalOverlay( createInfo )
			|| !RalMetal_CoreMatchesReceipt( core, coreReceipt ) ) return qfalse;
	device = RalMetal_CoreNativeDevice( core );
	memset( &receipt, 0, sizeof( receipt ) );
	@autoreleasepool {
		if ( !ReadDigest( createInfo->vertexSourcePath, &vertexSource, &vertexSourceDigest )
				|| !ReadDigest( createInfo->fragmentSourcePath, &fragmentSource,
					&fragmentSourceDigest )
				|| !Ral_ShaderDigestExact( &vertexSourceDigest,
					&createInfo->expectedVertexSourceDigest )
				|| !Ral_ShaderDigestExact( &fragmentSourceDigest,
					&createInfo->expectedFragmentSourceDigest )
				|| !ReadDigest( createInfo->vertexLibraryPath, &vertexLibraryData,
					&receipt.vertexLibraryDigest )
				|| !ReadDigest( createInfo->fragmentLibraryPath, &fragmentLibraryData,
					&receipt.fragmentLibraryDigest ) ) return qfalse;
		candidate = (ralMetalPipeline_t *)calloc( 1u, sizeof( *candidate ) );
		if ( !candidate ) return qfalse;
		NSError *error = nil;
		candidate->vertexLibrary = [device newLibraryWithURL:
			[NSURL fileURLWithPath:[NSString stringWithUTF8String:createInfo->vertexLibraryPath]]
			error:&error];
		if ( !candidate->vertexLibrary || error ) goto fail;
		error = nil;
		candidate->fragmentLibrary = [device newLibraryWithURL:
			[NSURL fileURLWithPath:[NSString stringWithUTF8String:createInfo->fragmentLibraryPath]]
			error:&error];
		if ( !candidate->fragmentLibrary || error ) goto fail;
		vertexFunction = [candidate->vertexLibrary newFunctionWithName:@"main0"];
		fragmentFunction = [candidate->fragmentLibrary newFunctionWithName:@"main0"];
		if ( !vertexFunction || !fragmentFunction
				|| vertexFunction.functionType != MTLFunctionTypeVertex
				|| fragmentFunction.functionType != MTLFunctionTypeFragment ) {
			[vertexFunction release]; [fragmentFunction release]; goto fail;
		}
		vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
		vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2;
		vertexDescriptor.attributes[0].offset = 0u; vertexDescriptor.attributes[0].bufferIndex = 0u;
		vertexDescriptor.attributes[1].format = MTLVertexFormatUChar4Normalized;
		vertexDescriptor.attributes[1].offset = 8u; vertexDescriptor.attributes[1].bufferIndex = 0u;
		vertexDescriptor.attributes[2].format = MTLVertexFormatFloat2;
		vertexDescriptor.attributes[2].offset = 12u; vertexDescriptor.attributes[2].bufferIndex = 0u;
		vertexDescriptor.layouts[0].stride = 20u;
		vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		pipelineInfo = [[[MTLRenderPipelineDescriptor alloc] init] autorelease];
		pipelineInfo.vertexFunction = vertexFunction; pipelineInfo.fragmentFunction = fragmentFunction;
		pipelineInfo.vertexDescriptor = vertexDescriptor; pipelineInfo.rasterSampleCount = 1u;
		pipelineInfo.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
		pipelineInfo.colorAttachments[0].blendingEnabled = YES;
		pipelineInfo.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
		pipelineInfo.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
		pipelineInfo.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
		pipelineInfo.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
		pipelineInfo.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
		pipelineInfo.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
		pipelineInfo.depthAttachmentPixelFormat = createInfo->depthFormat == RAL_FORMAT_D32_SFLOAT
			? MTLPixelFormatDepth32Float : MTLPixelFormatInvalid;
		error = nil;
		candidate->pipelineState = [device newRenderPipelineStateWithDescriptor:pipelineInfo error:&error];
		[vertexFunction release]; [fragmentFunction release];
		if ( !candidate->pipelineState || error ) goto fail;
		depthInfo = [[[MTLDepthStencilDescriptor alloc] init] autorelease];
		depthInfo.depthCompareFunction = MTLCompareFunctionAlways;
		depthInfo.depthWriteEnabled = NO;
		candidate->depthStencilState = [device newDepthStencilStateWithDescriptor:depthInfo];
		if ( !candidate->depthStencilState ) goto fail;
		receipt.schemaVersion = RAL_METAL_PIPELINE_SCHEMA_VERSION;
		receipt.backendType = RAL_BACKEND_METAL; receipt.coreGeneration = coreReceipt->generation;
		receipt.pipelineGeneration = createInfo->generation;
		receipt.pipelineIdentity = (uintptr_t)candidate;
		receipt.vertexSourceDigest = vertexSourceDigest;
		receipt.fragmentSourceDigest = fragmentSourceDigest;
		receipt.vertexSourceBytes = (uint32_t)vertexSource.length;
		receipt.fragmentSourceBytes = (uint32_t)fragmentSource.length;
		receipt.vertexLibraryBytes = (uint32_t)vertexLibraryData.length;
		receipt.fragmentLibraryBytes = (uint32_t)fragmentLibraryData.length;
		receipt.fragmentLayoutIdentity = createInfo->fragmentLayoutReceipt->layoutIdentity;
		receipt.fragmentLayoutGeneration = createInfo->fragmentLayoutReceipt->layoutGeneration;
		receipt.colorFormat = createInfo->colorFormat; receipt.depthFormat = createInfo->depthFormat;
		receipt.sampleCount = 1u; receipt.vertexStride = 20u;
		receipt.vertexInputs[0] = { 0u, RAL_FORMAT_R32G32_SFLOAT };
		receipt.vertexInputs[1] = { 1u, RAL_FORMAT_R8G8B8A8_UNORM };
		receipt.vertexInputs[2] = { 2u, RAL_FORMAT_R32G32_SFLOAT };
		receipt.vertexInputOffsets[0] = 0u; receipt.vertexInputOffsets[1] = 8u;
		receipt.vertexInputOffsets[2] = 12u; receipt.blendEnabled = qtrue;
		receipt.ready = qtrue;
		if ( !PipelineReceiptValid( &receipt ) ) goto fail;
		candidate->receipt = receipt;
		*outPipeline = candidate; *outReceipt = receipt;
		return qtrue;
fail:
		[candidate->depthStencilState release]; [candidate->pipelineState release];
		[candidate->fragmentLibrary release]; [candidate->vertexLibrary release];
		free( candidate );
		return qfalse;
	}
}

void RalMetal_PipelineDestroy( ralMetalPipeline_t *pipeline ) {
	if ( !pipeline ) return;
	[pipeline->depthStencilState release]; [pipeline->pipelineState release];
	[pipeline->fragmentLibrary release]; [pipeline->vertexLibrary release];
	memset( pipeline, 0, sizeof( *pipeline ) ); free( pipeline );
}
