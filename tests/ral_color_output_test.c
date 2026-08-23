// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_color_output.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if ( !(expr) ) { \
	fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); return 1; \
} } while ( 0 )

int main( void ) {
	ralColorOutputRequest_t request;
	ralColorOutputReceipt_t receipt, before, exact;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_COLOR_OUTPUT_SCHEMA_VERSION;
	request.presentationGeneration = 17u;
	request.sceneFormat = RAL_FORMAT_R16G16B16A16_SFLOAT;
	request.selectedOutput = (ralSurfaceFormat_t){
		RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR };
	request.toneMapOperator = RAL_TONEMAP_PBR_NEUTRAL;
	request.lutEnabled = qtrue;
	request.hdrPeakNits = 1000.0f;
	request.hdrMinNits = 0.01f;
	CHECK( Ral_ResolveColorOutput( &request, &receipt ) );
	CHECK( receipt.sceneTransfer == RAL_COLOR_TRANSFER_LINEAR
		&& receipt.uiTransfer == RAL_COLOR_TRANSFER_LINEAR
		&& receipt.uiReferenceWhiteNits == 100.0f
		&& receipt.presentationTransfer == RAL_COLOR_TRANSFER_SRGB
		&& receipt.screenshotTransfer == RAL_COLOR_TRANSFER_SRGB
		&& receipt.readbackTransfer == RAL_COLOR_TRANSFER_SRGB
		&& receipt.hdrActive == qfalse
		&& receipt.hdrFallback == RAL_HDR_FALLBACK_NOT_REQUESTED
		&& receipt.lutEnabled == qtrue );

	request.requestHdrOutput = qtrue;
	CHECK( Ral_ResolveColorOutput( &request, &receipt ) );
	CHECK( receipt.hdrFallback == RAL_HDR_FALLBACK_OUTPUT_UNAVAILABLE );
	request.selectedOutput = (ralSurfaceFormat_t){
		RAL_FORMAT_A2B10G10R10_UNORM, RAL_COLORSPACE_HDR10_ST2084 };
	CHECK( Ral_ResolveColorOutput( &request, &receipt ) );
	CHECK( receipt.hdrActive == qtrue
		&& receipt.hdrFallback == RAL_HDR_FALLBACK_NONE
		&& receipt.presentationTransfer == RAL_COLOR_TRANSFER_PQ );
	exact = receipt;
	CHECK( Ral_ColorOutputReceiptExact( &receipt, &exact ) );
#define MUTATE(field) do { exact = receipt; exact.field++; \
	CHECK( !Ral_ColorOutputReceiptExact( &receipt, &exact ) ); } while ( 0 )
	MUTATE( presentationGeneration ); MUTATE( sceneFormat );
	MUTATE( presentationTransfer ); MUTATE( screenshotTransfer );
	MUTATE( readbackTransfer ); MUTATE( toneMapOperator );
	MUTATE( lutEnabled ); MUTATE( hdrActive ); MUTATE( hdrFallback );
	MUTATE( hdrPeakNits ); MUTATE( hdrMinNits );
#undef MUTATE
	exact = receipt; exact.presentationTransfer = RAL_COLOR_TRANSFER_SRGB;
	CHECK( !Ral_ColorOutputReceiptValid( &exact ) );

	request.selectedOutput = (ralSurfaceFormat_t){
		RAL_FORMAT_R16G16B16A16_SFLOAT, RAL_COLORSPACE_DISPLAY_P3 };
	request.toneMapOperator = RAL_TONEMAP_AGX;
	CHECK( Ral_ResolveColorOutput( &request, &receipt )
		&& receipt.presentationTransfer == RAL_COLOR_TRANSFER_LINEAR );

	/* HDR output cannot be paired with an SDR scene; output remains untouched. */
	request.sceneFormat = RAL_FORMAT_R8G8B8A8_UNORM;
	memset( &receipt, 0x5a, sizeof( receipt ) ); before = receipt;
	CHECK( !Ral_ResolveColorOutput( &request, &receipt ) );
	CHECK( !memcmp( &receipt, &before, sizeof( receipt ) ) );
	request.selectedOutput = (ralSurfaceFormat_t){
		RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR };
	CHECK( Ral_ResolveColorOutput( &request, &receipt )
		&& receipt.hdrFallback == RAL_HDR_FALLBACK_SCENE_NOT_HDR );
	request.hdrPeakNits = 50.0f;
	before = receipt;
	CHECK( !Ral_ResolveColorOutput( &request, &receipt ) );
	CHECK( !memcmp( &receipt, &before, sizeof( receipt ) ) );

	puts( "RAL color output: PASS" );
	return 0;
}
