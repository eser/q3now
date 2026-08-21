// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_present.h"
#include "ral_metal_internal.h"

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct ralMetalPresent_s {
	ralMetalCore_t *core;
	CAMetalLayer *layer;
	id<CAMetalDrawable> drawable;
	ralMetalPresentLayerReceipt_t receipt;
	ralMetalDrawableReceipt_t drawableReceipt;
	uint64_t lastAcquireGeneration;
	uint64_t lastPresentGeneration;
	qboolean ownsLayer;
};

static qboolean BoolValid( qboolean value ) {
	return value == qfalse || value == qtrue ? qtrue : qfalse;
}

static qboolean FormatPairValid( ralFormat_t format, ralColorSpace_t colorSpace ) {
	return ( ( format == RAL_FORMAT_B8G8R8A8_UNORM
			&& colorSpace == RAL_COLORSPACE_SRGB_NONLINEAR )
		|| ( format == RAL_FORMAT_R16G16B16A16_SFLOAT
			&& colorSpace == RAL_COLORSPACE_DISPLAY_P3 ) ) ? qtrue : qfalse;
}

static qboolean PresentModeValid( ralPresentMode_t mode ) {
	return mode == RAL_PRESENT_FIFO || mode == RAL_PRESENT_IMMEDIATE ? qtrue : qfalse;
}

static qboolean SelectPresentation( const ralSwapchainCreateInfo_t *createInfo,
		uint64_t generation, ralSwapchainInfo_t *outSelected ) {
	ralSwapchainInfo_t selected;
	uint32_t i;
	qboolean foundFormat = qfalse, foundMode = qfalse;
	if ( !createInfo || !outSelected || !createInfo->formatPreferences
			|| createInfo->formatPreferenceCount == 0u
			|| !createInfo->presentModePreferences
			|| createInfo->presentModePreferenceCount == 0u
			|| createInfo->desiredWidth == 0u || createInfo->desiredHeight == 0u
			|| createInfo->desiredWidth > 16384u || createInfo->desiredHeight > 16384u
			|| ( createInfo->desiredImageCount != 0u
				&& createInfo->desiredImageCount != 2u
				&& createInfo->desiredImageCount != 3u )
			|| createInfo->requiredUsage != RAL_TEXTURE_USAGE_COLOR_ATTACHMENT
			|| createInfo->backendExtensionChain ) return qfalse;
	memset( &selected, 0, sizeof( selected ) );
	for ( i = 0u; i < createInfo->formatPreferenceCount; ++i ) {
		if ( FormatPairValid( createInfo->formatPreferences[i].format,
				createInfo->formatPreferences[i].colorSpace ) ) {
			selected.format = createInfo->formatPreferences[i].format;
			selected.colorSpace = createInfo->formatPreferences[i].colorSpace;
			foundFormat = qtrue; break;
		}
	}
	for ( i = 0u; i < createInfo->presentModePreferenceCount; ++i ) {
		if ( PresentModeValid( createInfo->presentModePreferences[i] ) ) {
			selected.presentMode = createInfo->presentModePreferences[i];
			foundMode = qtrue; break;
		}
	}
	if ( !foundFormat || !foundMode ) return qfalse;
	selected.generation = generation;
	selected.width = createInfo->desiredWidth; selected.height = createInfo->desiredHeight;
	selected.imageCount = createInfo->desiredImageCount ? createInfo->desiredImageCount : 3u;
	selected.usage = createInfo->requiredUsage;
	*outSelected = selected;
	return qtrue;
}

static qboolean LayerReceiptValid( const ralMetalPresentLayerReceipt_t *r ) {
	return ( r && r->schemaVersion == RAL_METAL_PRESENT_SCHEMA_VERSION
		&& r->backendType == RAL_BACKEND_METAL
		&& r->coreGeneration != 0u && r->coreGeneration != UINT64_MAX
		&& r->presentationGeneration != 0u && r->presentationGeneration != UINT64_MAX
		&& r->ownerIdentity != (uintptr_t)0 && r->layerIdentity != (uintptr_t)0
		&& r->ownerIdentity != r->layerIdentity
		&& r->selected.generation == r->presentationGeneration
		&& r->selected.width != 0u && r->selected.height != 0u
		&& FormatPairValid( r->selected.format, r->selected.colorSpace )
		&& PresentModeValid( r->selected.presentMode )
		&& ( r->selected.imageCount == 2u || r->selected.imageCount == 3u )
		&& r->selected.usage == RAL_TEXTURE_USAGE_COLOR_ATTACHMENT
		&& BoolValid( r->displaySyncEnabled ) && BoolValid( r->extendedDynamicRange )
		&& r->displaySyncEnabled
			== ( r->selected.presentMode == RAL_PRESENT_FIFO ? qtrue : qfalse )
		&& r->extendedDynamicRange
			== ( r->selected.format == RAL_FORMAT_R16G16B16A16_SFLOAT ? qtrue : qfalse )
		&& r->framebufferOnly == qfalse && BoolValid( r->ownsLayer )
		&& r->ready == qtrue ) ? qtrue : qfalse;
}

qboolean RalMetal_PresentLayerReceiptExact( const ralMetalPresentLayerReceipt_t *a,
		const ralMetalPresentLayerReceipt_t *b ) {
	return ( LayerReceiptValid( a ) && LayerReceiptValid( b )
		&& a->backendType == b->backendType && a->coreGeneration == b->coreGeneration
		&& a->presentationGeneration == b->presentationGeneration
		&& a->ownerIdentity == b->ownerIdentity && a->layerIdentity == b->layerIdentity
		&& !memcmp( &a->selected, &b->selected, sizeof( a->selected ) )
		&& a->displaySyncEnabled == b->displaySyncEnabled
		&& a->extendedDynamicRange == b->extendedDynamicRange
		&& a->framebufferOnly == b->framebufferOnly
		&& a->ownsLayer == b->ownsLayer && a->ready == b->ready )
		? qtrue : qfalse;
}

static qboolean OwnerMatches( const ralMetalPresent_t *present,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalPresentLayerReceipt_t *layerReceipt ) {
	CGColorSpaceRef nativeColorSpace;
	CFStringRef nativeName, expectedName;
	MTLPixelFormat expectedFormat;
	if ( !present || !present->layer || !layerReceipt ) return qfalse;
	nativeColorSpace = present->layer.colorspace;
	nativeName = nativeColorSpace ? CGColorSpaceGetName( nativeColorSpace ) : NULL;
	expectedName = layerReceipt->selected.format == RAL_FORMAT_B8G8R8A8_UNORM
		? kCGColorSpaceSRGB : kCGColorSpaceExtendedLinearDisplayP3;
	expectedFormat = layerReceipt->selected.format == RAL_FORMAT_B8G8R8A8_UNORM
		? MTLPixelFormatBGRA8Unorm : MTLPixelFormatRGBA16Float;
	return ( present && coreReceipt && layerReceipt
		&& present->core && present->layer
		&& RalMetal_CoreMatchesReceipt( present->core, coreReceipt )
		&& layerReceipt->ownerIdentity == (uintptr_t)present
		&& layerReceipt->layerIdentity == (uintptr_t)(void *)present->layer
		&& layerReceipt->ownsLayer == present->ownsLayer
		&& RalMetal_PresentLayerReceiptExact( layerReceipt, &present->receipt )
		&& present->layer.device == RalMetal_CoreNativeDevice( present->core )
		&& present->layer.pixelFormat == expectedFormat
		&& present->layer.drawableSize.width == layerReceipt->selected.width
		&& present->layer.drawableSize.height == layerReceipt->selected.height
		&& present->layer.maximumDrawableCount == layerReceipt->selected.imageCount
		&& ( present->layer.displaySyncEnabled ? qtrue : qfalse )
			== layerReceipt->displaySyncEnabled
		&& ( present->layer.framebufferOnly ? qtrue : qfalse )
			== layerReceipt->framebufferOnly
		&& ( present->layer.wantsExtendedDynamicRangeContent ? qtrue : qfalse )
			== layerReceipt->extendedDynamicRange
		&& nativeName && CFEqual( nativeName, expectedName ) )
		? qtrue : qfalse;
}

static qboolean BorrowedOwnerMatchesResize( const ralMetalPresent_t *present,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalPresentLayerReceipt_t *currentReceipt,
		const ralSwapchainCreateInfo_t *createInfo, uint64_t generation ) {
	ralSwapchainInfo_t selected;
	CGColorSpaceRef nativeColorSpace;
	CFStringRef nativeName, expectedName;
	MTLPixelFormat expectedFormat;
	if ( !present || present->ownsLayer != qfalse || !present->layer
			|| !coreReceipt || !currentReceipt
			|| !SelectPresentation( createInfo, generation, &selected ) ) return qfalse;
	nativeColorSpace = present->layer.colorspace;
	nativeName = nativeColorSpace ? CGColorSpaceGetName( nativeColorSpace ) : NULL;
	expectedName = currentReceipt->selected.format == RAL_FORMAT_B8G8R8A8_UNORM
		? kCGColorSpaceSRGB : kCGColorSpaceExtendedLinearDisplayP3;
	expectedFormat = currentReceipt->selected.format == RAL_FORMAT_B8G8R8A8_UNORM
		? MTLPixelFormatBGRA8Unorm : MTLPixelFormatRGBA16Float;
	return ( RalMetal_CoreMatchesReceipt( present->core, coreReceipt )
		&& currentReceipt->ownerIdentity == (uintptr_t)present
		&& currentReceipt->layerIdentity == (uintptr_t)(void *)present->layer
		&& RalMetal_PresentLayerReceiptExact( currentReceipt, &present->receipt )
		&& currentReceipt->ownsLayer == qfalse
		&& present->layer.device == RalMetal_CoreNativeDevice( present->core )
		&& present->layer.pixelFormat == expectedFormat
		&& present->layer.drawableSize.width == selected.width
		&& present->layer.drawableSize.height == selected.height
		&& present->layer.maximumDrawableCount == currentReceipt->selected.imageCount
		&& ( present->layer.displaySyncEnabled ? qtrue : qfalse )
			== currentReceipt->displaySyncEnabled
		&& ( present->layer.framebufferOnly ? qtrue : qfalse )
			== currentReceipt->framebufferOnly
		&& ( present->layer.wantsExtendedDynamicRangeContent ? qtrue : qfalse )
			== currentReceipt->extendedDynamicRange
		&& nativeName && CFEqual( nativeName, expectedName ) ) ? qtrue : qfalse;
}

static qboolean ConfigureLayer( ralMetalCore_t *core,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralSwapchainCreateInfo_t *createInfo, uint64_t generation,
		uintptr_t ownerIdentity, CAMetalLayer *layer, qboolean ownsLayer,
		ralMetalPresentLayerReceipt_t *outReceipt ) {
	ralSwapchainInfo_t selected;
	ralMetalPresentLayerReceipt_t receipt;
	CGColorSpaceRef colorSpace;
	if ( !core || !coreReceipt || !layer || !outReceipt || !BoolValid( ownsLayer )
			|| ownerIdentity == (uintptr_t)0 || generation == 0u || generation == UINT64_MAX
			|| !RalMetal_CoreMatchesReceipt( core, coreReceipt )
			|| !SelectPresentation( createInfo, generation, &selected )
			|| ![layer isKindOfClass:[CAMetalLayer class]] ) return qfalse;
	colorSpace = CGColorSpaceCreateWithName( selected.format == RAL_FORMAT_B8G8R8A8_UNORM
		? kCGColorSpaceSRGB : kCGColorSpaceExtendedLinearDisplayP3 );
	if ( !colorSpace ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_METAL_PRESENT_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_METAL; receipt.coreGeneration = coreReceipt->generation;
	receipt.presentationGeneration = generation; receipt.ownerIdentity = ownerIdentity;
	receipt.layerIdentity = (uintptr_t)(void *)layer; receipt.selected = selected;
	receipt.displaySyncEnabled = selected.presentMode == RAL_PRESENT_FIFO ? qtrue : qfalse;
	receipt.extendedDynamicRange = selected.format == RAL_FORMAT_R16G16B16A16_SFLOAT
		? qtrue : qfalse;
	receipt.framebufferOnly = qfalse; receipt.ownsLayer = ownsLayer; receipt.ready = qtrue;
	if ( !LayerReceiptValid( &receipt ) ) { CGColorSpaceRelease( colorSpace ); return qfalse; }
	layer.device = RalMetal_CoreNativeDevice( core );
	layer.pixelFormat = selected.format == RAL_FORMAT_B8G8R8A8_UNORM
		? MTLPixelFormatBGRA8Unorm : MTLPixelFormatRGBA16Float;
	layer.drawableSize = CGSizeMake( selected.width, selected.height );
	layer.maximumDrawableCount = selected.imageCount;
	layer.displaySyncEnabled = selected.presentMode == RAL_PRESENT_FIFO ? YES : NO;
	layer.framebufferOnly = NO;
	layer.presentsWithTransaction = NO;
	layer.allowsNextDrawableTimeout = YES;
	layer.wantsExtendedDynamicRangeContent = selected.format
		== RAL_FORMAT_R16G16B16A16_SFLOAT ? YES : NO;
	layer.colorspace = colorSpace;
	CGColorSpaceRelease( colorSpace );
	*outReceipt = receipt;
	return qtrue;
}

static qboolean BuildOwnedLayer( ralMetalCore_t *core,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralSwapchainCreateInfo_t *createInfo, uint64_t generation,
		uintptr_t ownerIdentity, CAMetalLayer **outLayer,
		ralMetalPresentLayerReceipt_t *outReceipt ) {
	CAMetalLayer *layer;
	if ( !outLayer || !outReceipt ) return qfalse;
	layer = [[CAMetalLayer alloc] init];
	if ( !layer ) return qfalse;
	if ( !ConfigureLayer( core, coreReceipt, createInfo, generation, ownerIdentity,
			layer, qtrue, outReceipt ) ) { [layer release]; return qfalse; }
	*outLayer = layer;
	return qtrue;
}

qboolean RalMetal_PresentCreate( ralMetalCore_t *core,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralSwapchainCreateInfo_t *createInfo, uint64_t generation,
		ralMetalPresent_t **outPresent, ralMetalPresentLayerReceipt_t *outReceipt ) {
	ralMetalPresent_t *candidate;
	ralMetalPresentLayerReceipt_t receipt;
	CAMetalLayer *layer = nil;
	if ( !outPresent || !outReceipt ) return qfalse;
	candidate = (ralMetalPresent_t *)calloc( 1u, sizeof( *candidate ) );
	if ( !candidate ) return qfalse;
	if ( !BuildOwnedLayer( core, coreReceipt, createInfo, generation, (uintptr_t)candidate,
			&layer, &receipt ) ) { free( candidate ); return qfalse; }
	candidate->core = core; candidate->layer = layer; candidate->receipt = receipt;
	candidate->ownsLayer = qtrue;
	*outPresent = candidate; *outReceipt = receipt;
	return qtrue;
}

qboolean RalMetal_PresentAdoptBorrowedLayer( ralMetalCore_t *core,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralSwapchainCreateInfo_t *createInfo, uint64_t generation,
		void *borrowedLayer, ralMetalPresent_t **outPresent,
		ralMetalPresentLayerReceipt_t *outReceipt ) {
	ralMetalPresent_t *candidate;
	ralMetalPresentLayerReceipt_t receipt;
	CAMetalLayer *layer = (CAMetalLayer *)borrowedLayer;
	if ( !outPresent || !outReceipt || !layer ) return qfalse;
	candidate = (ralMetalPresent_t *)calloc( 1u, sizeof( *candidate ) );
	if ( !candidate ) return qfalse;
	if ( !ConfigureLayer( core, coreReceipt, createInfo, generation,
			(uintptr_t)candidate, layer, qfalse, &receipt ) ) {
		free( candidate ); return qfalse;
	}
	candidate->core = core; candidate->layer = layer; candidate->receipt = receipt;
	candidate->ownsLayer = qfalse;
	*outPresent = candidate; *outReceipt = receipt;
	return qtrue;
}

qboolean RalMetal_PresentReconfigure( ralMetalPresent_t *present,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalPresentLayerReceipt_t *currentReceipt,
		const ralSwapchainCreateInfo_t *createInfo, uint64_t generation,
		ralMetalPresentLayerReceipt_t *outReceipt ) {
	ralMetalPresentLayerReceipt_t receipt;
	CAMetalLayer *candidate = nil, *old;
	if ( !outReceipt || ( !OwnerMatches( present, coreReceipt, currentReceipt )
			&& !BorrowedOwnerMatchesResize( present, coreReceipt, currentReceipt,
				createInfo, generation ) )
			|| present->drawable || generation <= currentReceipt->presentationGeneration
			|| generation == UINT64_MAX ) return qfalse;
	if ( present->ownsLayer ) {
		if ( !BuildOwnedLayer( present->core, coreReceipt, createInfo, generation,
				(uintptr_t)present, &candidate, &receipt ) ) return qfalse;
		old = present->layer; present->layer = candidate; present->receipt = receipt;
		[old release];
	} else {
		if ( !ConfigureLayer( present->core, coreReceipt, createInfo, generation,
				(uintptr_t)present, present->layer, qfalse, &receipt ) ) return qfalse;
		present->receipt = receipt;
	}
	*outReceipt = receipt;
	return qtrue;
}

void RalMetal_PresentDestroy( ralMetalPresent_t *present ) {
	if ( !present ) return;
	[present->drawable release];
	if ( present->ownsLayer ) [present->layer release];
	memset( present, 0, sizeof( *present ) );
	free( present );
}

static qboolean DrawableReceiptValid( const ralMetalDrawableReceipt_t *r ) {
	return ( r && r->schemaVersion == RAL_METAL_PRESENT_SCHEMA_VERSION
		&& r->backendType == RAL_BACKEND_METAL
		&& r->coreGeneration != 0u && r->coreGeneration != UINT64_MAX
		&& r->presentationGeneration != 0u && r->presentationGeneration != UINT64_MAX
		&& r->acquireGeneration != 0u && r->acquireGeneration != UINT64_MAX
		&& r->ownerIdentity != (uintptr_t)0 && r->layerIdentity != (uintptr_t)0
		&& r->drawableIdentity != (uintptr_t)0 && r->textureIdentity != (uintptr_t)0
		&& r->ownerIdentity != r->layerIdentity && r->drawableIdentity != r->textureIdentity
		&& r->width != 0u && r->height != 0u
		&& ( r->format == RAL_FORMAT_B8G8R8A8_UNORM
			|| r->format == RAL_FORMAT_R16G16B16A16_SFLOAT )
		&& r->acquired == qtrue ) ? qtrue : qfalse;
}

qboolean RalMetal_DrawableReceiptExact( const ralMetalDrawableReceipt_t *a,
		const ralMetalDrawableReceipt_t *b ) {
	return ( DrawableReceiptValid( a ) && DrawableReceiptValid( b )
		&& a->backendType == b->backendType && a->coreGeneration == b->coreGeneration
		&& a->presentationGeneration == b->presentationGeneration
		&& a->acquireGeneration == b->acquireGeneration
		&& a->ownerIdentity == b->ownerIdentity && a->layerIdentity == b->layerIdentity
		&& a->drawableIdentity == b->drawableIdentity && a->textureIdentity == b->textureIdentity
		&& a->width == b->width && a->height == b->height && a->format == b->format
		&& a->acquired == b->acquired ) ? qtrue : qfalse;
}

qboolean RalMetal_PresentAcquire( ralMetalPresent_t *present,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalPresentLayerReceipt_t *layerReceipt, uint64_t acquireGeneration,
		ralMetalDrawableReceipt_t *outReceipt ) {
	ralMetalDrawableReceipt_t receipt;
	id<CAMetalDrawable> drawable;
	id<MTLTexture> texture;
	if ( !outReceipt || !OwnerMatches( present, coreReceipt, layerReceipt )
			|| present->drawable || acquireGeneration == 0u
			|| acquireGeneration == UINT64_MAX
			|| acquireGeneration <= present->lastAcquireGeneration ) return qfalse;
	drawable = [[present->layer nextDrawable] retain];
	if ( !drawable ) return qfalse;
	texture = drawable.texture;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_METAL_PRESENT_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_METAL; receipt.coreGeneration = coreReceipt->generation;
	receipt.presentationGeneration = layerReceipt->presentationGeneration;
	receipt.acquireGeneration = acquireGeneration; receipt.ownerIdentity = (uintptr_t)present;
	receipt.layerIdentity = (uintptr_t)(void *)present->layer;
	receipt.drawableIdentity = (uintptr_t)(void *)drawable;
	receipt.textureIdentity = (uintptr_t)(void *)texture;
	receipt.width = (uint32_t)texture.width; receipt.height = (uint32_t)texture.height;
	receipt.format = layerReceipt->selected.format; receipt.acquired = qtrue;
	if ( !texture || texture.width != layerReceipt->selected.width
			|| texture.height != layerReceipt->selected.height
			|| texture.pixelFormat != present->layer.pixelFormat
			|| !DrawableReceiptValid( &receipt ) ) { [drawable release]; return qfalse; }
	present->drawable = drawable; present->drawableReceipt = receipt;
	present->lastAcquireGeneration = acquireGeneration;
	*outReceipt = receipt;
	return qtrue;
}

static qboolean PresentReceiptValid( const ralMetalPresentReceipt_t *r ) {
	return ( r && r->schemaVersion == RAL_METAL_PRESENT_SCHEMA_VERSION
		&& r->backendType == RAL_BACKEND_METAL
		&& r->coreGeneration != 0u && r->coreGeneration != UINT64_MAX
		&& r->presentationGeneration != 0u && r->presentationGeneration != UINT64_MAX
		&& r->acquireGeneration != 0u && r->acquireGeneration != UINT64_MAX
		&& r->presentGeneration != 0u && r->presentGeneration != UINT64_MAX
		&& r->ownerIdentity != (uintptr_t)0 && r->layerIdentity != (uintptr_t)0
		&& r->drawableIdentity != (uintptr_t)0 && r->textureIdentity != (uintptr_t)0
		&& FormatPairValid( r->format, r->colorSpace ) && PresentModeValid( r->presentMode )
		&& r->width != 0u && r->height != 0u
		&& isfinite( r->clearColor[0] ) && isfinite( r->clearColor[1] )
		&& isfinite( r->clearColor[2] ) && isfinite( r->clearColor[3] )
		&& Ral_CommandReceiptValid( &r->recording )
		&& Ral_CommandReceiptValid( &r->executable )
		&& Ral_SubmissionReceiptValid( &r->submission )
		&& r->recording.state == RAL_COMMAND_RECORDING
		&& r->executable.state == RAL_COMMAND_EXECUTABLE
		&& r->recording.generation == r->presentGeneration
		&& r->executable.generation == r->presentGeneration
		&& r->recording.backendIdentity == r->executable.backendIdentity
		&& r->recording.commandIdentity == r->executable.commandIdentity
		&& r->submission.backendIdentity == r->executable.backendIdentity
		&& r->submission.queue == r->executable.queue && r->submission.commandCount == 1u
		&& r->submission.commands[0].commandIdentity == r->executable.commandIdentity
		&& r->submission.commands[0].generation == r->presentGeneration
		&& r->submission.commands[0].state == RAL_COMMAND_SUBMITTED
		&& r->completionGeneration == r->submission.generation
		&& r->clearDigest != 0u && r->presented == qtrue ) ? qtrue : qfalse;
}

qboolean RalMetal_PresentReceiptExact( const ralMetalPresentReceipt_t *a,
		const ralMetalPresentReceipt_t *b ) {
	return ( PresentReceiptValid( a ) && PresentReceiptValid( b )
		&& a->backendType == b->backendType && a->coreGeneration == b->coreGeneration
		&& a->presentationGeneration == b->presentationGeneration
		&& a->acquireGeneration == b->acquireGeneration
		&& a->presentGeneration == b->presentGeneration
		&& a->ownerIdentity == b->ownerIdentity && a->layerIdentity == b->layerIdentity
		&& a->drawableIdentity == b->drawableIdentity && a->textureIdentity == b->textureIdentity
		&& a->format == b->format && a->colorSpace == b->colorSpace
		&& a->presentMode == b->presentMode && a->width == b->width && a->height == b->height
		&& !memcmp( a->clearColor, b->clearColor, sizeof( a->clearColor ) )
		&& Ral_CommandReceiptExact( &a->recording, &b->recording )
		&& Ral_CommandReceiptExact( &a->executable, &b->executable )
		&& Ral_SubmissionReceiptExact( &a->submission, &b->submission )
		&& a->completionGeneration == b->completionGeneration
		&& a->clearDigest == b->clearDigest && a->presented == b->presented ) ? qtrue : qfalse;
}

static uint64_t DigestClear( const float clearColor[4] ) {
	const uint8_t *bytes = (const uint8_t *)clearColor;
	uint64_t digest = UINT64_C(1469598103934665603);
	for ( uint32_t i = 0u; i < sizeof( float ) * 4u; ++i ) {
		digest ^= bytes[i]; digest *= UINT64_C(1099511628211);
	}
	return digest;
}

qboolean RalMetal_PresentClearAndSubmit( ralMetalPresent_t *present,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalPresentLayerReceipt_t *layerReceipt,
		const ralMetalDrawableReceipt_t *drawableReceipt,
		const float clearColor[4], uint64_t presentGeneration,
		ralMetalPresentReceipt_t *outReceipt ) {
	ralMetalPresentReceipt_t receipt;
	ralCommandLifecycle_t lifecycle;
	ralCommandReceipt_t recording, executable;
	id<MTLCommandQueue> queue;
	uint32_t i;
	float maxRgb;
	if ( !outReceipt || !clearColor || !OwnerMatches( present, coreReceipt, layerReceipt )
			|| !present->drawable || !drawableReceipt
			|| !RalMetal_DrawableReceiptExact( drawableReceipt, &present->drawableReceipt )
			|| presentGeneration == 0u || presentGeneration == UINT64_MAX
			|| presentGeneration <= drawableReceipt->acquireGeneration
			|| presentGeneration <= present->lastPresentGeneration ) return qfalse;
	maxRgb = layerReceipt->extendedDynamicRange == qtrue ? 16.0f : 1.0f;
	for ( i = 0u; i < 4u; ++i ) if ( !isfinite( clearColor[i] ) ) return qfalse;
	if ( clearColor[0] < 0.0f || clearColor[0] > maxRgb
			|| clearColor[1] < 0.0f || clearColor[1] > maxRgb
			|| clearColor[2] < 0.0f || clearColor[2] > maxRgb
			|| clearColor[3] < 0.0f || clearColor[3] > 1.0f ) return qfalse;
	queue = RalMetal_CoreNativeQueue( present->core );
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_METAL_PRESENT_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_METAL; receipt.coreGeneration = coreReceipt->generation;
	receipt.presentationGeneration = layerReceipt->presentationGeneration;
	receipt.acquireGeneration = drawableReceipt->acquireGeneration;
	receipt.presentGeneration = presentGeneration; receipt.ownerIdentity = (uintptr_t)present;
	receipt.layerIdentity = layerReceipt->layerIdentity;
	receipt.drawableIdentity = drawableReceipt->drawableIdentity;
	receipt.textureIdentity = drawableReceipt->textureIdentity;
	receipt.format = layerReceipt->selected.format; receipt.colorSpace = layerReceipt->selected.colorSpace;
	receipt.presentMode = layerReceipt->selected.presentMode;
	receipt.width = drawableReceipt->width; receipt.height = drawableReceipt->height;
	memcpy( receipt.clearColor, clearColor, sizeof( receipt.clearColor ) );
	@autoreleasepool {
		id<MTLCommandBuffer> command = [queue commandBuffer];
		if ( !command || !RalMetal_CoreBeginCommand( present->core, presentGeneration,
				&lifecycle, &recording ) ) return qfalse;
		MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
		pass.colorAttachments[0].texture = present->drawable.texture;
		pass.colorAttachments[0].loadAction = MTLLoadActionClear;
		pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		pass.colorAttachments[0].clearColor = MTLClearColorMake( clearColor[0], clearColor[1],
			clearColor[2], clearColor[3] );
		id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
		if ( !encoder ) return qfalse;
		[encoder endEncoding];
		if ( Ral_CommandLifecyclePublishEnd( &lifecycle, &recording,
				&executable ) != ralSuccess ) return qfalse;
		[command presentDrawable:present->drawable];
		[command commit]; [command waitUntilCompleted];
		if ( command.status != MTLCommandBufferStatusCompleted
				|| !RalMetal_CorePublishSubmission( present->core, &lifecycle, &executable,
					&receipt.submission ) ) {
			[present->drawable release]; present->drawable = nil;
			memset( &present->drawableReceipt, 0, sizeof( present->drawableReceipt ) );
			return qfalse;
		}
	}
	receipt.recording = recording; receipt.executable = executable;
	receipt.completionGeneration = receipt.submission.generation;
	receipt.clearDigest = DigestClear( clearColor ); receipt.presented = qtrue;
	present->lastPresentGeneration = presentGeneration;
	[present->drawable release]; present->drawable = nil;
	memset( &present->drawableReceipt, 0, sizeof( present->drawableReceipt ) );
	if ( !PresentReceiptValid( &receipt ) ) return qfalse;
	*outReceipt = receipt;
	return qtrue;
}
