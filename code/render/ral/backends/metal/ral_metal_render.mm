// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_render.h"
#include "ral_metal_internal.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <limits.h>
#include <math.h>
#include <string.h>

static qboolean MetalPixelFormat( ralFormat_t format, MTLPixelFormat *out,
		qboolean *outDepth ) {
	switch ( format ) {
	case RAL_FORMAT_R8_UNORM: *out = MTLPixelFormatR8Unorm; *outDepth = qfalse; return qtrue;
	case RAL_FORMAT_R8G8_UNORM: *out = MTLPixelFormatRG8Unorm; *outDepth = qfalse; return qtrue;
	case RAL_FORMAT_R8G8B8A8_UNORM: *out = MTLPixelFormatRGBA8Unorm; *outDepth = qfalse; return qtrue;
	case RAL_FORMAT_R8G8B8A8_SRGB: *out = MTLPixelFormatRGBA8Unorm_sRGB; *outDepth = qfalse; return qtrue;
	case RAL_FORMAT_B8G8R8A8_UNORM: *out = MTLPixelFormatBGRA8Unorm; *outDepth = qfalse; return qtrue;
	case RAL_FORMAT_B8G8R8A8_SRGB: *out = MTLPixelFormatBGRA8Unorm_sRGB; *outDepth = qfalse; return qtrue;
	case RAL_FORMAT_R16G16_SFLOAT: *out = MTLPixelFormatRG16Float; *outDepth = qfalse; return qtrue;
	case RAL_FORMAT_R16G16_SNORM: *out = MTLPixelFormatRG16Snorm; *outDepth = qfalse; return qtrue;
	case RAL_FORMAT_E5B9G9R9_UFLOAT: *out = MTLPixelFormatRGB9E5Float; *outDepth = qfalse; return qtrue;
	case RAL_FORMAT_R16G16B16A16_SFLOAT: *out = MTLPixelFormatRGBA16Float; *outDepth = qfalse; return qtrue;
	case RAL_FORMAT_R32_SFLOAT: *out = MTLPixelFormatR32Float; *outDepth = qfalse; return qtrue;
	case RAL_FORMAT_D16_UNORM: *out = MTLPixelFormatDepth16Unorm; *outDepth = qtrue; return qtrue;
	case RAL_FORMAT_D32_SFLOAT: *out = MTLPixelFormatDepth32Float; *outDepth = qtrue; return qtrue;
	default: return qfalse;
	}
}

static qboolean LoadStoreValid( ralLoadOp_t loadOp, ralStoreOp_t storeOp ) {
	return ( loadOp >= RAL_LOAD_OP_LOAD && loadOp <= RAL_LOAD_OP_DONT_CARE
		&& storeOp >= RAL_STORE_OP_STORE && storeOp <= RAL_STORE_OP_DONT_CARE )
		? qtrue : qfalse;
}

static qboolean AttachmentInputValid( const ralMetalRenderAttachment_t *attachment,
		uint32_t width, uint32_t height, qboolean expectDepth ) {
	id resource;
	MTLPixelFormat expectedFormat;
	qboolean isDepth;
	uint32_t i;
	if ( !attachment || !attachment->nativeTexture
			|| attachment->textureIdentity != (uintptr_t)attachment->nativeTexture
			|| attachment->textureGeneration == 0u
			|| attachment->textureGeneration == UINT64_MAX
			|| attachment->width != width || attachment->height != height
			|| attachment->sampleCount == 0u || !LoadStoreValid( attachment->loadOp,
				attachment->storeOp )
			|| !MetalPixelFormat( attachment->format, &expectedFormat, &isDepth )
			|| isDepth != expectDepth ) return qfalse;
	resource = (id)attachment->nativeTexture;
	if ( ![resource conformsToProtocol:@protocol(MTLTexture)] ) return qfalse;
	id<MTLTexture> texture = (id<MTLTexture>)resource;
	if ( texture.textureType != MTLTextureType2D || texture.width != width
			|| texture.height != height || texture.sampleCount != attachment->sampleCount
			|| texture.pixelFormat != expectedFormat
			|| ( texture.usage & MTLTextureUsageRenderTarget ) == 0u ) return qfalse;
	if ( attachment->loadOp == RAL_LOAD_OP_CLEAR ) {
		if ( expectDepth ) {
			if ( !isfinite( attachment->clearValue.depthStencil.depth )
					|| attachment->clearValue.depthStencil.depth < 0.0f
					|| attachment->clearValue.depthStencil.depth > 1.0f ) return qfalse;
		} else for ( i = 0u; i < 4u; ++i )
			if ( !isfinite( attachment->clearValue.color[i] ) ) return qfalse;
	}
	if ( expectDepth ) {
		if ( !isfinite( attachment->clearValue.depthStencil.depth )
				|| attachment->clearValue.depthStencil.depth < 0.0f
				|| attachment->clearValue.depthStencil.depth > 1.0f ) return qfalse;
	} else for ( i = 0u; i < 4u; ++i )
		if ( !isfinite( attachment->clearValue.color[i] ) ) return qfalse;
	return qtrue;
}

static void BuildAttachmentReceipt( const ralMetalRenderAttachment_t *source,
		ralMetalRenderAttachmentReceipt_t *target ) {
	memset( target, 0, sizeof( *target ) );
	target->textureIdentity = source->textureIdentity;
	target->textureGeneration = source->textureGeneration;
	target->format = source->format; target->width = source->width;
	target->height = source->height; target->sampleCount = source->sampleCount;
	target->loadOp = source->loadOp; target->storeOp = source->storeOp;
	target->clearValue = source->clearValue;
}

static qboolean AttachmentReceiptValid( const ralMetalRenderAttachmentReceipt_t *attachment,
		qboolean expectDepth ) {
	MTLPixelFormat ignored;
	qboolean isDepth;
	uint32_t i;
	if ( !attachment || attachment->textureIdentity == (uintptr_t)0
			|| attachment->textureGeneration == 0u
			|| attachment->textureGeneration == UINT64_MAX
			|| attachment->width == 0u || attachment->height == 0u
			|| attachment->sampleCount == 0u
			|| !LoadStoreValid( attachment->loadOp, attachment->storeOp )
			|| !MetalPixelFormat( attachment->format, &ignored, &isDepth )
			|| isDepth != expectDepth ) return qfalse;
	if ( attachment->loadOp == RAL_LOAD_OP_CLEAR ) {
		if ( expectDepth ) return ( isfinite( attachment->clearValue.depthStencil.depth )
			&& attachment->clearValue.depthStencil.depth >= 0.0f
			&& attachment->clearValue.depthStencil.depth <= 1.0f ) ? qtrue : qfalse;
		for ( i = 0u; i < 4u; ++i ) if ( !isfinite( attachment->clearValue.color[i] ) ) return qfalse;
	}
	return qtrue;
}

static qboolean AttachmentReceiptExact( const ralMetalRenderAttachmentReceipt_t *a,
		const ralMetalRenderAttachmentReceipt_t *b ) {
	return ( a->textureIdentity == b->textureIdentity
		&& a->textureGeneration == b->textureGeneration && a->format == b->format
		&& a->width == b->width && a->height == b->height
		&& a->sampleCount == b->sampleCount && a->loadOp == b->loadOp
		&& a->storeOp == b->storeOp
		&& !memcmp( &a->clearValue, &b->clearValue, sizeof( a->clearValue ) ) ) ? qtrue : qfalse;
}

static qboolean RenderReceiptValid( const ralMetalRenderReceipt_t *receipt ) {
	uint32_t i;
	if ( !receipt || receipt->schemaVersion != RAL_METAL_RENDER_SCHEMA_VERSION
			|| receipt->backendType != RAL_BACKEND_METAL
			|| receipt->coreGeneration == 0u || receipt->coreGeneration == UINT64_MAX
			|| receipt->renderGeneration == 0u || receipt->renderGeneration == UINT64_MAX
			|| receipt->colorAttachmentCount == 0u
			|| receipt->colorAttachmentCount > RAL_MAX_COLOR_ATTACHMENTS
			|| ( receipt->hasDepthAttachment != qfalse && receipt->hasDepthAttachment != qtrue )
			|| !Ral_CommandReceiptValid( &receipt->recording )
			|| !Ral_CommandReceiptValid( &receipt->executable )
			|| !Ral_SubmissionReceiptValid( &receipt->submission )
			|| receipt->recording.generation != receipt->renderGeneration
			|| receipt->executable.generation != receipt->renderGeneration
			|| receipt->recording.state != RAL_COMMAND_RECORDING
			|| receipt->executable.state != RAL_COMMAND_EXECUTABLE
			|| receipt->submission.commandCount != 1u
			|| receipt->submission.commands[0].generation != receipt->renderGeneration
			|| receipt->submission.commands[0].state != RAL_COMMAND_SUBMITTED
			|| receipt->recording.backendIdentity != receipt->executable.backendIdentity
			|| receipt->recording.commandIdentity != receipt->executable.commandIdentity
			|| receipt->recording.queue != receipt->executable.queue
			|| receipt->submission.backendIdentity != receipt->executable.backendIdentity
			|| receipt->submission.queue != receipt->executable.queue
			|| receipt->submission.commands[0].commandIdentity
				!= receipt->executable.commandIdentity
			|| receipt->completionGeneration == 0u
			|| receipt->completionGeneration != receipt->submission.generation
			|| receipt->ready != qtrue ) return qfalse;
	for ( i = 0u; i < receipt->colorAttachmentCount; ++i ) {
		uint32_t j;
		if ( !AttachmentReceiptValid( &receipt->colorAttachments[i], qfalse ) ) return qfalse;
		for ( j = 0u; j < i; ++j ) if ( receipt->colorAttachments[i].textureIdentity
				== receipt->colorAttachments[j].textureIdentity ) return qfalse;
	}
	if ( receipt->hasDepthAttachment ) {
		if ( !AttachmentReceiptValid( &receipt->depthAttachment, qtrue ) ) return qfalse;
		for ( i = 0u; i < receipt->colorAttachmentCount; ++i )
			if ( receipt->depthAttachment.textureIdentity
					== receipt->colorAttachments[i].textureIdentity ) return qfalse;
	}
	return qtrue;
}

qboolean RalMetal_RenderReceiptExact( const ralMetalRenderReceipt_t *a,
		const ralMetalRenderReceipt_t *b ) {
	uint32_t i;
	if ( !RenderReceiptValid( a ) || !RenderReceiptValid( b )
			|| a->backendType != b->backendType || a->coreGeneration != b->coreGeneration
			|| a->renderGeneration != b->renderGeneration
			|| a->colorAttachmentCount != b->colorAttachmentCount
			|| a->hasDepthAttachment != b->hasDepthAttachment
			|| !Ral_CommandReceiptExact( &a->recording, &b->recording )
			|| !Ral_CommandReceiptExact( &a->executable, &b->executable )
			|| !Ral_SubmissionReceiptExact( &a->submission, &b->submission )
			|| a->completionGeneration != b->completionGeneration ) return qfalse;
	for ( i = 0u; i < a->colorAttachmentCount; ++i )
		if ( !AttachmentReceiptExact( &a->colorAttachments[i], &b->colorAttachments[i] ) ) return qfalse;
	if ( a->hasDepthAttachment
			&& !AttachmentReceiptExact( &a->depthAttachment, &b->depthAttachment ) ) return qfalse;
	return qtrue;
}

static MTLLoadAction MetalLoadAction( ralLoadOp_t op ) {
	return op == RAL_LOAD_OP_LOAD ? MTLLoadActionLoad
		: ( op == RAL_LOAD_OP_CLEAR ? MTLLoadActionClear : MTLLoadActionDontCare );
}

static MTLStoreAction MetalStoreAction( ralStoreOp_t op ) {
	return op == RAL_STORE_OP_STORE ? MTLStoreActionStore : MTLStoreActionDontCare;
}

qboolean RalMetal_RenderPlanValid( const ralMetalRenderPlan_t *plan ) {
	uint32_t i, j;
	if ( !plan || !plan->colorAttachments || plan->colorAttachmentCount == 0u
			|| plan->colorAttachmentCount > RAL_MAX_COLOR_ATTACHMENTS
			|| plan->width == 0u || plan->height == 0u ) return qfalse;
	for ( i = 0u; i < plan->colorAttachmentCount; ++i ) {
		if ( !AttachmentInputValid( &plan->colorAttachments[i], plan->width,
				plan->height, qfalse ) ) return qfalse;
		for ( j = 0u; j < i; ++j ) if ( plan->colorAttachments[i].textureIdentity
				== plan->colorAttachments[j].textureIdentity ) return qfalse;
	}
	if ( plan->depthAttachment ) {
		if ( !AttachmentInputValid( plan->depthAttachment, plan->width, plan->height,
				qtrue ) ) return qfalse;
		for ( i = 0u; i < plan->colorAttachmentCount; ++i )
			if ( plan->depthAttachment->textureIdentity
					== plan->colorAttachments[i].textureIdentity ) return qfalse;
	}
	return qtrue;
}

qboolean RalMetal_RenderClearPass( ralMetalCore_t *core,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalRenderPlan_t *plan, uint64_t renderGeneration,
		ralMetalRenderReceipt_t *outReceipt ) {
	ralMetalRenderReceipt_t receipt;
	ralCommandLifecycle_t lifecycle;
	ralCommandReceipt_t recording, executable;
	id<MTLDevice> device;
	id<MTLCommandQueue> queue;
	uint32_t i;
	if ( !core || !coreReceipt || !plan || !outReceipt
			|| !RalMetal_RenderPlanValid( plan )
			|| renderGeneration == 0u || renderGeneration == UINT64_MAX
			|| !RalMetal_CoreMatchesReceipt( core, coreReceipt ) ) return qfalse;
	device = RalMetal_CoreNativeDevice( core ); queue = RalMetal_CoreNativeQueue( core );
	if ( !device || !queue || coreReceipt->backendIdentity != (uintptr_t)core
			|| coreReceipt->deviceIdentity != (uintptr_t)(void *)device
			|| coreReceipt->queueIdentity != (uintptr_t)(void *)queue ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_METAL_RENDER_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_METAL; receipt.coreGeneration = coreReceipt->generation;
	receipt.renderGeneration = renderGeneration;
	receipt.colorAttachmentCount = plan->colorAttachmentCount;
	for ( i = 0u; i < plan->colorAttachmentCount; ++i )
		BuildAttachmentReceipt( &plan->colorAttachments[i], &receipt.colorAttachments[i] );
	receipt.hasDepthAttachment = plan->depthAttachment ? qtrue : qfalse;
	if ( plan->depthAttachment ) BuildAttachmentReceipt( plan->depthAttachment,
		&receipt.depthAttachment );
	@autoreleasepool {
		MTLRenderPassDescriptor *descriptor = [MTLRenderPassDescriptor renderPassDescriptor];
		for ( i = 0u; i < plan->colorAttachmentCount; ++i ) {
			const ralMetalRenderAttachment_t *source = &plan->colorAttachments[i];
			MTLRenderPassColorAttachmentDescriptor *target = descriptor.colorAttachments[i];
			target.texture = (id<MTLTexture>)source->nativeTexture;
			target.loadAction = MetalLoadAction( source->loadOp );
			target.storeAction = MetalStoreAction( source->storeOp );
			target.clearColor = MTLClearColorMake( source->clearValue.color[0],
				source->clearValue.color[1], source->clearValue.color[2],
				source->clearValue.color[3] );
		}
		if ( plan->depthAttachment ) {
			const ralMetalRenderAttachment_t *source = plan->depthAttachment;
			descriptor.depthAttachment.texture = (id<MTLTexture>)source->nativeTexture;
			descriptor.depthAttachment.loadAction = MetalLoadAction( source->loadOp );
			descriptor.depthAttachment.storeAction = MetalStoreAction( source->storeOp );
			descriptor.depthAttachment.clearDepth = source->clearValue.depthStencil.depth;
		}
		id<MTLCommandBuffer> nativeCommand = [queue commandBuffer];
		if ( !nativeCommand || !RalMetal_CoreBeginCommand( core, renderGeneration,
				&lifecycle, &recording ) ) return qfalse;
		id<MTLRenderCommandEncoder> encoder = [nativeCommand
			renderCommandEncoderWithDescriptor:descriptor];
		if ( !encoder ) return qfalse;
		[encoder endEncoding];
		if ( Ral_CommandLifecyclePublishEnd( &lifecycle, &recording,
				&executable ) != ralSuccess ) return qfalse;
		[nativeCommand commit];
		[nativeCommand waitUntilCompleted];
		if ( nativeCommand.status != MTLCommandBufferStatusCompleted ) return qfalse;
		if ( !RalMetal_CorePublishSubmission( core, &lifecycle, &executable,
				&receipt.submission ) ) return qfalse;
	}
	receipt.recording = recording; receipt.executable = executable;
	receipt.completionGeneration = receipt.submission.generation;
	receipt.ready = qtrue;
	if ( !RenderReceiptValid( &receipt ) ) return qfalse;
	*outReceipt = receipt;
	return qtrue;
}
