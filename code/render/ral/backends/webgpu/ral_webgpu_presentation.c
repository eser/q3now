// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_presentation.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct ralWebGpuPresentation_s {
	ralWebGpuCore_t *core;
	ralWebGpuCoreReceipt_t coreReceipt;
	void *userData;
	uintptr_t canvasIdentity;
	ralWebGpuPresentationHostOps_t host;
	ralWebGpuPresentationReceipt_t receipt;
	ralWebGpuFrameReceipt_t frame;
	uint64_t presentationGeneration;
	uint64_t frameGeneration;
	qboolean frameAcquired;
};

static qboolean BoolValid( qboolean value ) {
	return value == qfalse || value == qtrue;
}

static qboolean PresentationValid(
		const ralWebGpuPresentationReceipt_t *receipt ) {
	if ( !receipt || receipt->schemaVersion != RAL_WEBGPU_PRESENTATION_SCHEMA_VERSION
			|| receipt->backendType != RAL_BACKEND_WEBGPU
			|| !receipt->backendGeneration
			|| receipt->backendGeneration == UINT64_MAX
			|| !receipt->presentationGeneration
			|| receipt->presentationGeneration == UINT64_MAX
			|| !receipt->canvasIdentity || !receipt->dprQ16
			|| !BoolValid( receipt->opaqueAlpha )
			|| !BoolValid( receipt->configured )
			|| !BoolValid( receipt->suspended ) || receipt->ready != qtrue )
		return qfalse;
	if ( receipt->suspended ) return receipt->configured == qfalse
		&& receipt->cssWidth == 0u && receipt->cssHeight == 0u
		&& receipt->pixelWidth == 0u && receipt->pixelHeight == 0u
		&& receipt->format == RAL_FORMAT_UNDEFINED;
	return receipt->configured == qtrue && receipt->cssWidth
		&& receipt->cssHeight && receipt->pixelWidth && receipt->pixelHeight
		&& ( receipt->format == RAL_FORMAT_B8G8R8A8_UNORM
			|| receipt->format == RAL_FORMAT_R8G8B8A8_UNORM )
		&& ( receipt->colorSpace == RAL_COLORSPACE_SRGB_NONLINEAR
			|| receipt->colorSpace == RAL_COLORSPACE_DISPLAY_P3 );
}

static qboolean FrameValid( const ralWebGpuFrameReceipt_t *frame ) {
	return frame && frame->schemaVersion == RAL_WEBGPU_PRESENTATION_SCHEMA_VERSION
		&& frame->backendType == RAL_BACKEND_WEBGPU
		&& frame->backendGeneration && frame->backendGeneration != UINT64_MAX
		&& frame->presentationGeneration
		&& frame->presentationGeneration != UINT64_MAX
		&& frame->frameGeneration && frame->frameGeneration != UINT64_MAX
		&& frame->canvasIdentity && frame->textureIdentity
		&& frame->pixelWidth && frame->pixelHeight && frame->ready == qtrue;
}

qboolean RalWebGpu_PresentationCreate( ralWebGpuCore_t *core,
		const ralWebGpuCoreReceipt_t *coreReceipt,
		const ralWebGpuPresentationCreateInfo_t *createInfo,
		ralWebGpuPresentation_t **outPresentation ) {
	ralWebGpuPresentation_t *presentation;
	if ( !core || !coreReceipt || !createInfo || !outPresentation
			|| !RalWebGpu_CoreMatchesReceipt( core, coreReceipt )
			|| !createInfo->canvasIdentity || !createInfo->host.configure
			|| !createInfo->host.unconfigure || !createInfo->host.acquire
			|| !createInfo->host.present ) return qfalse;
	presentation = (ralWebGpuPresentation_t *)calloc( 1u,
		sizeof( *presentation ) );
	if ( !presentation ) return qfalse;
	presentation->core = core; presentation->coreReceipt = *coreReceipt;
	presentation->userData = createInfo->userData;
	presentation->canvasIdentity = createInfo->canvasIdentity;
	presentation->host = createInfo->host;
	presentation->presentationGeneration = coreReceipt->generation;
	presentation->frameGeneration = coreReceipt->generation;
	*outPresentation = presentation; return qtrue;
}

void RalWebGpu_PresentationDestroy( ralWebGpuPresentation_t *presentation ) {
	if ( !presentation ) return;
	if ( presentation->receipt.configured ) presentation->host.unconfigure(
		presentation->userData, presentation->canvasIdentity );
	memset( presentation, 0, sizeof( *presentation ) ); free( presentation );
}

qboolean RalWebGpu_PresentationConfigure(
		ralWebGpuPresentation_t *presentation, uint32_t cssWidth,
		uint32_t cssHeight, uint32_t dprQ16, ralFormat_t format,
		ralColorSpace_t colorSpace, qboolean opaqueAlpha,
		ralWebGpuPresentationReceipt_t *outReceipt ) {
	ralWebGpuPresentationReceipt_t candidate;
	uint64_t pixelWidth, pixelHeight;
	if ( !presentation || !outReceipt || !dprQ16 || !BoolValid( opaqueAlpha )
			|| !RalWebGpu_CoreMatchesReceipt( presentation->core,
				&presentation->coreReceipt )
			|| presentation->frameAcquired
			|| presentation->presentationGeneration == UINT64_MAX ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RAL_WEBGPU_PRESENTATION_SCHEMA_VERSION;
	candidate.backendType = RAL_BACKEND_WEBGPU;
	candidate.backendGeneration = presentation->coreReceipt.generation;
	candidate.presentationGeneration = presentation->presentationGeneration + 1u;
	candidate.canvasIdentity = presentation->canvasIdentity;
	candidate.dprQ16 = dprQ16; candidate.opaqueAlpha = opaqueAlpha;
	candidate.colorSpace = colorSpace; candidate.ready = qtrue;
	if ( !cssWidth || !cssHeight ) {
		if ( presentation->receipt.configured ) presentation->host.unconfigure(
			presentation->userData, presentation->canvasIdentity );
		candidate.suspended = qtrue;
		if ( !PresentationValid( &candidate ) ) return qfalse;
		presentation->presentationGeneration = candidate.presentationGeneration;
		presentation->receipt = candidate; *outReceipt = candidate; return qtrue;
	}
	if ( format != RAL_FORMAT_B8G8R8A8_UNORM
			&& format != RAL_FORMAT_R8G8B8A8_UNORM ) return qfalse;
	if ( colorSpace != RAL_COLORSPACE_SRGB_NONLINEAR
			&& colorSpace != RAL_COLORSPACE_DISPLAY_P3 ) return qfalse;
	pixelWidth = ( (uint64_t)cssWidth * dprQ16 + RAL_WEBGPU_DPR_ONE_Q16 - 1u )
		/ RAL_WEBGPU_DPR_ONE_Q16;
	pixelHeight = ( (uint64_t)cssHeight * dprQ16 + RAL_WEBGPU_DPR_ONE_Q16 - 1u )
		/ RAL_WEBGPU_DPR_ONE_Q16;
	if ( !pixelWidth || !pixelHeight || pixelWidth > UINT32_MAX
			|| pixelHeight > UINT32_MAX
			|| pixelWidth > presentation->coreReceipt.caps.maxTextureDimension2D
			|| pixelHeight > presentation->coreReceipt.caps.maxTextureDimension2D
			|| !presentation->host.configure( presentation->userData,
				presentation->canvasIdentity,
				presentation->coreReceipt.deviceIdentity, (uint32_t)pixelWidth,
				(uint32_t)pixelHeight, format, colorSpace, opaqueAlpha ) ) return qfalse;
	candidate.cssWidth = cssWidth; candidate.cssHeight = cssHeight;
	candidate.pixelWidth = (uint32_t)pixelWidth;
	candidate.pixelHeight = (uint32_t)pixelHeight;
	candidate.format = format; candidate.configured = qtrue;
	if ( !PresentationValid( &candidate ) ) return qfalse;
	presentation->presentationGeneration = candidate.presentationGeneration;
	presentation->receipt = candidate; *outReceipt = candidate; return qtrue;
}

qboolean RalWebGpu_PresentationAcquire(
		ralWebGpuPresentation_t *presentation,
		const ralWebGpuPresentationReceipt_t *authority,
		ralWebGpuFrameReceipt_t *outFrame ) {
	ralWebGpuFrameReceipt_t frame;
	uintptr_t texture = (uintptr_t)0;
	if ( !presentation || !authority || !outFrame || presentation->frameAcquired
			|| !RalWebGpu_CoreMatchesReceipt( presentation->core,
				&presentation->coreReceipt )
			|| !RalWebGpu_PresentationReceiptExact( authority,
				&presentation->receipt ) || authority->configured != qtrue
			|| presentation->frameGeneration == UINT64_MAX ) return qfalse;
	if ( !presentation->host.acquire( presentation->userData,
			presentation->canvasIdentity, presentation->frameGeneration + 1u,
			&texture ) || !texture ) return qfalse;
	memset( &frame, 0, sizeof( frame ) );
	frame.schemaVersion = RAL_WEBGPU_PRESENTATION_SCHEMA_VERSION;
	frame.backendType = RAL_BACKEND_WEBGPU;
	frame.backendGeneration = presentation->coreReceipt.generation;
	frame.presentationGeneration = authority->presentationGeneration;
	frame.frameGeneration = presentation->frameGeneration + 1u;
	frame.canvasIdentity = presentation->canvasIdentity;
	frame.textureIdentity = texture;
	frame.pixelWidth = authority->pixelWidth; frame.pixelHeight = authority->pixelHeight;
	frame.ready = qtrue;
	if ( !FrameValid( &frame ) ) return qfalse;
	presentation->frameGeneration = frame.frameGeneration;
	presentation->frame = frame; presentation->frameAcquired = qtrue;
	*outFrame = frame; return qtrue;
}

qboolean RalWebGpu_PresentationPresent(
		ralWebGpuPresentation_t *presentation,
		const ralWebGpuFrameReceipt_t *frame,
		const ralWebGpuSubmissionReceipt_t *submission ) {
	if ( !presentation || !frame || !submission || !presentation->frameAcquired
			|| !RalWebGpu_CoreMatchesReceipt( presentation->core,
				&presentation->coreReceipt )
			|| !RalWebGpu_FrameReceiptExact( frame, &presentation->frame )
			|| !RalWebGpu_SubmissionReceiptExact( submission, submission )
			|| submission->backendGeneration != frame->backendGeneration
			|| !presentation->host.present( presentation->userData,
				presentation->canvasIdentity, frame->textureIdentity,
				frame->frameGeneration, submission->submission.generation ) )
		return qfalse;
	presentation->frameAcquired = qfalse;
	memset( &presentation->frame, 0, sizeof( presentation->frame ) );
	return qtrue;
}

qboolean RalWebGpu_PresentationReceiptExact(
		const ralWebGpuPresentationReceipt_t *a,
		const ralWebGpuPresentationReceipt_t *b ) {
	return PresentationValid( a ) && PresentationValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

qboolean RalWebGpu_FrameReceiptExact( const ralWebGpuFrameReceipt_t *a,
		const ralWebGpuFrameReceipt_t *b ) {
	return FrameValid( a ) && FrameValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}
