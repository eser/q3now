// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_color_output.h"

#include <limits.h>
#include <math.h>
#include <string.h>

static qboolean BoolValid( qboolean value ) {
	return value == qfalse || value == qtrue ? qtrue : qfalse;
}

static qboolean ToneMapValid( ralToneMapOperator_t value ) {
	return value >= RAL_TONEMAP_IDENTITY && value <= RAL_TONEMAP_REINHARD
		? qtrue : qfalse;
}

static qboolean SceneFormatValid( ralFormat_t format ) {
	return format == RAL_FORMAT_R16G16B16A16_SFLOAT
		|| format == RAL_FORMAT_R16G16B16A16_UNORM
		|| format == RAL_FORMAT_R8G8B8A8_UNORM
		|| format == RAL_FORMAT_B8G8R8A8_UNORM
		|| format == RAL_FORMAT_R8G8B8A8_SRGB
		|| format == RAL_FORMAT_B8G8R8A8_SRGB ? qtrue : qfalse;
}

static qboolean SdrOutput( ralSurfaceFormat_t output ) {
	return output.colorSpace == RAL_COLORSPACE_SRGB_NONLINEAR
		&& ( output.format == RAL_FORMAT_B8G8R8A8_UNORM
			|| output.format == RAL_FORMAT_R8G8B8A8_UNORM
			|| output.format == RAL_FORMAT_B8G8R8A8_SRGB
			|| output.format == RAL_FORMAT_R8G8B8A8_SRGB ) ? qtrue : qfalse;
}

static qboolean HdrOutput( ralSurfaceFormat_t output,
		ralColorTransfer_t *transfer ) {
	if ( output.colorSpace == RAL_COLORSPACE_HDR10_ST2084
			&& output.format == RAL_FORMAT_A2B10G10R10_UNORM ) {
		*transfer = RAL_COLOR_TRANSFER_PQ; return qtrue;
	}
	if ( output.colorSpace == RAL_COLORSPACE_HDR10_HLG
			&& output.format == RAL_FORMAT_A2B10G10R10_UNORM ) {
		*transfer = RAL_COLOR_TRANSFER_HLG; return qtrue;
	}
	if ( ( output.colorSpace == RAL_COLORSPACE_EXTENDED_SRGB_LINEAR
				|| output.colorSpace == RAL_COLORSPACE_DISPLAY_P3 )
			&& output.format == RAL_FORMAT_R16G16B16A16_SFLOAT ) {
		*transfer = RAL_COLOR_TRANSFER_LINEAR; return qtrue;
	}
	return qfalse;
}

qboolean Ral_ResolveColorOutput( const ralColorOutputRequest_t *request,
		ralColorOutputReceipt_t *outReceipt ) {
	ralColorOutputReceipt_t receipt;
	ralColorTransfer_t hdrTransfer = RAL_COLOR_TRANSFER_LINEAR;
	qboolean selectedHdr;
	qboolean sceneHdr;
	if ( !request || !outReceipt
			|| request->schemaVersion != RAL_COLOR_OUTPUT_SCHEMA_VERSION
			|| request->presentationGeneration == 0u
			|| request->presentationGeneration == UINT64_MAX
			|| !BoolValid( request->requestHdrOutput )
			|| !BoolValid( request->lutEnabled )
			|| !ToneMapValid( request->toneMapOperator )
			|| !isfinite( request->hdrPeakNits )
			|| !isfinite( request->hdrMinNits )
			|| request->hdrPeakNits < 100.0f || request->hdrPeakNits > 10000.0f
			|| request->hdrMinNits < 0.0001f || request->hdrMinNits > 1.0f
			|| request->hdrMinNits >= request->hdrPeakNits ) return qfalse;
	if ( !SceneFormatValid( request->sceneFormat ) ) return qfalse;
	sceneHdr = request->sceneFormat == RAL_FORMAT_R16G16B16A16_SFLOAT
		? qtrue : qfalse;
	selectedHdr = HdrOutput( request->selectedOutput, &hdrTransfer );
	if ( !selectedHdr && !SdrOutput( request->selectedOutput ) ) return qfalse;
	if ( selectedHdr && ( !request->requestHdrOutput || !sceneHdr ) )
		return qfalse;

	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_COLOR_OUTPUT_SCHEMA_VERSION;
	receipt.presentationGeneration = request->presentationGeneration;
	receipt.sceneFormat = request->sceneFormat;
	receipt.sceneTransfer = RAL_COLOR_TRANSFER_LINEAR;
	receipt.uiTransfer = RAL_COLOR_TRANSFER_LINEAR;
	receipt.uiReferenceWhiteNits = 100.0f;
	receipt.presentation = request->selectedOutput;
	receipt.presentationTransfer = selectedHdr
		? hdrTransfer : RAL_COLOR_TRANSFER_SRGB;
	/* Screenshots and ordinary CPU readback are stable display-referred sRGB,
	 * never raw PQ/scRGB bytes. Explicit raw-HDR export is a separate request. */
	receipt.screenshotTransfer = RAL_COLOR_TRANSFER_SRGB;
	receipt.readbackTransfer = RAL_COLOR_TRANSFER_SRGB;
	receipt.toneMapOperator = request->toneMapOperator;
	receipt.lutEnabled = request->lutEnabled;
	receipt.hdrActive = selectedHdr;
	if ( selectedHdr ) receipt.hdrFallback = RAL_HDR_FALLBACK_NONE;
	else if ( !request->requestHdrOutput )
		receipt.hdrFallback = RAL_HDR_FALLBACK_NOT_REQUESTED;
	else if ( !sceneHdr )
		receipt.hdrFallback = RAL_HDR_FALLBACK_SCENE_NOT_HDR;
	else receipt.hdrFallback = RAL_HDR_FALLBACK_OUTPUT_UNAVAILABLE;
	receipt.hdrPeakNits = request->hdrPeakNits;
	receipt.hdrMinNits = request->hdrMinNits;
	if ( !Ral_ColorOutputReceiptValid( &receipt ) ) return qfalse;
	*outReceipt = receipt;
	return qtrue;
}

qboolean Ral_ColorOutputReceiptValid( const ralColorOutputReceipt_t *receipt ) {
	ralColorTransfer_t expectedTransfer = RAL_COLOR_TRANSFER_SRGB;
	qboolean hdrOutput;
	if ( !receipt ) return qfalse;
	hdrOutput = HdrOutput( receipt->presentation, &expectedTransfer );
	if ( !hdrOutput && !SdrOutput( receipt->presentation ) ) return qfalse;
	return receipt->schemaVersion == RAL_COLOR_OUTPUT_SCHEMA_VERSION
		&& receipt->presentationGeneration != 0u
		&& receipt->presentationGeneration != UINT64_MAX
		&& SceneFormatValid( receipt->sceneFormat )
		&& receipt->sceneTransfer == RAL_COLOR_TRANSFER_LINEAR
		&& receipt->uiTransfer == RAL_COLOR_TRANSFER_LINEAR
		&& receipt->uiReferenceWhiteNits == 100.0f
		&& receipt->presentationTransfer == expectedTransfer
		&& receipt->screenshotTransfer == RAL_COLOR_TRANSFER_SRGB
		&& receipt->readbackTransfer == RAL_COLOR_TRANSFER_SRGB
		&& ToneMapValid( receipt->toneMapOperator )
		&& BoolValid( receipt->lutEnabled )
		&& BoolValid( receipt->hdrActive )
		&& receipt->hdrActive == hdrOutput
		&& ( !hdrOutput
			|| receipt->sceneFormat == RAL_FORMAT_R16G16B16A16_SFLOAT )
		&& receipt->hdrFallback >= RAL_HDR_FALLBACK_NONE
		&& receipt->hdrFallback <= RAL_HDR_FALLBACK_OUTPUT_UNAVAILABLE
		&& ( receipt->hdrActive
			? receipt->hdrFallback == RAL_HDR_FALLBACK_NONE
			: receipt->hdrFallback != RAL_HDR_FALLBACK_NONE )
		&& isfinite( receipt->hdrPeakNits ) && isfinite( receipt->hdrMinNits )
		&& receipt->hdrPeakNits >= 100.0f && receipt->hdrPeakNits <= 10000.0f
		&& receipt->hdrMinNits >= 0.0001f && receipt->hdrMinNits <= 1.0f
		&& receipt->hdrMinNits < receipt->hdrPeakNits ? qtrue : qfalse;
}

qboolean Ral_ColorOutputReceiptExact( const ralColorOutputReceipt_t *a,
		const ralColorOutputReceipt_t *b ) {
	return Ral_ColorOutputReceiptValid( a ) && Ral_ColorOutputReceiptValid( b )
		&& a->schemaVersion == b->schemaVersion
		&& a->presentationGeneration == b->presentationGeneration
		&& a->sceneFormat == b->sceneFormat
		&& a->sceneTransfer == b->sceneTransfer
		&& a->uiTransfer == b->uiTransfer
		&& a->uiReferenceWhiteNits == b->uiReferenceWhiteNits
		&& a->presentation.format == b->presentation.format
		&& a->presentation.colorSpace == b->presentation.colorSpace
		&& a->presentationTransfer == b->presentationTransfer
		&& a->screenshotTransfer == b->screenshotTransfer
		&& a->readbackTransfer == b->readbackTransfer
		&& a->toneMapOperator == b->toneMapOperator
		&& a->lutEnabled == b->lutEnabled
		&& a->hdrActive == b->hdrActive
		&& a->hdrFallback == b->hdrFallback
		&& a->hdrPeakNits == b->hdrPeakNits
		&& a->hdrMinNits == b->hdrMinNits ? qtrue : qfalse;
}
