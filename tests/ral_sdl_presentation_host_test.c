// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "sdl_ral_presentation.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n", \
	__FILE__,__LINE__,#x); return 1; } } while (0)

int main( void ) {
	wiredSdlRalPresentationHost_t *host = NULL;
	ralPresentationHostImports_t imports;
	ralPresentationHostOpenInfo_t info;
	ralPresentationHostReceipt_t receipt, resized, beforeReceipt, failedReceipt;
	ralPresentationSurfaceBorrow_t borrow, beforeBorrow;

	CHECK( WiredSdlRalPresentationHost_Create( 1280u, 720u, qfalse, 901u,
		&host, &imports ) );
	memset( &info, 0, sizeof( info ) );
	info.schemaVersion = RAL_PRESENTATION_HOST_SCHEMA_VERSION;
	info.backendType = RAL_BACKEND_WEBGPU;
	info.requestVisible = qtrue;
	memset( &receipt, 0x5a, sizeof( receipt ) ); beforeReceipt = receipt;
	CHECK( !imports.open( imports.context, &info, &receipt )
		&& memcmp( &receipt, &beforeReceipt, sizeof( receipt ) ) == 0 );
	info.backendType = RAL_BACKEND_METAL;
	CHECK( imports.open( imports.context, &info, &receipt )
		&& Ral_PresentationHostReceiptExact( &receipt, &receipt )
		&& receipt.logicalWidth == 1280u && receipt.logicalHeight == 720u
		&& receipt.visible == qfalse );
	memset( &borrow, 0x5a, sizeof( borrow ) );
	CHECK( imports.borrow( imports.context, &receipt, &borrow )
		&& Ral_PresentationSurfaceBorrowExact( &borrow, &borrow ) );
	beforeReceipt = receipt; beforeReceipt.surfaceGeneration++;
	memset( &resized, 0x5a, sizeof( resized ) );
	failedReceipt = resized;
	CHECK( !imports.refresh( imports.context, &beforeReceipt, &resized )
		&& memcmp( &resized, &failedReceipt, sizeof( resized ) ) == 0 );
	CHECK( WiredSdlRalPresentationHost_RequestResize( host, 1600u, 900u )
		&& imports.refresh( imports.context, &receipt, &resized )
		&& resized.logicalWidth == 1600u && resized.logicalHeight == 900u
		&& resized.surfaceGeneration > receipt.surfaceGeneration );
	CHECK( imports.borrow( imports.context, &resized, &beforeBorrow )
		&& beforeBorrow.surfaceIdentity == borrow.surfaceIdentity );
	CHECK( imports.close( imports.context, &resized,
		RAL_PRESENTATION_HOST_KEEP_OWNER )
		&& imports.open( imports.context, &info, &receipt )
		&& Ral_PresentationHostReceiptExact( &receipt, &resized ) );
	CHECK( imports.close( imports.context, &receipt,
		RAL_PRESENTATION_HOST_DESTROY_OWNER )
		&& !WiredSdlRalPresentationHost_GetReceipt( host, &resized ) );
	WiredSdlRalPresentationHost_Destroy( host );
	puts( "RAL SDL presentation host: PASS" );
	return 0;
}
