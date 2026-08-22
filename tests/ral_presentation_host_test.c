// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_presentation_host.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n", \
	__FILE__,__LINE__,#x); return 1; } } while (0)

static qboolean NoOpen( void *context, const ralPresentationHostOpenInfo_t *info,
		ralPresentationHostReceipt_t *receipt ) {
	(void)context; (void)info; (void)receipt; return qfalse;
}
static qboolean NoRefresh( void *context,
		const ralPresentationHostReceipt_t *current,
		ralPresentationHostReceipt_t *receipt ) {
	(void)context; (void)current; (void)receipt; return qfalse;
}
static qboolean NoBorrow( void *context,
		const ralPresentationHostReceipt_t *current,
		ralPresentationSurfaceBorrow_t *borrow ) {
	(void)context; (void)current; (void)borrow; return qfalse;
}
static qboolean NoClose( void *context,
		const ralPresentationHostReceipt_t *current,
		ralPresentationHostCloseMode_t mode ) {
	(void)context; (void)current; (void)mode; return qfalse;
}

int main( void ) {
	ralPresentationHostOpenInfo_t openInfo = {
		RAL_PRESENTATION_HOST_SCHEMA_VERSION, RAL_BACKEND_WEBGPU, qtrue
	};
	ralPresentationHostReceipt_t receipt = {
		RAL_PRESENTATION_HOST_SCHEMA_VERSION, RAL_BACKEND_WEBGPU,
		11u, 12u, 0x100u, 800u, 450u, 1600u, 900u,
		2.0f, 2.0f, qtrue, qtrue
	};
	ralPresentationSurfaceBorrow_t borrow = {
		RAL_PRESENTATION_HOST_SCHEMA_VERSION, RAL_BACKEND_WEBGPU,
		11u, 12u, 0x100u, 0x200u, qtrue
	};
	ralPresentationHostImports_t imports = {
		RAL_PRESENTATION_HOST_SCHEMA_VERSION, (void *)(uintptr_t)0x300u,
		NoOpen, NoRefresh, NoBorrow, NoClose
	};
	ralPresentationHostReceipt_t exact;
	ralPresentationSurfaceBorrow_t borrowExact;

	CHECK( Ral_PresentationExtentValid( 1280u, 720u ) );
	CHECK( !Ral_PresentationExtentValid( 640u, 480u ) );
	CHECK( !Ral_PresentationExtentValid( 0u, 720u ) );
	CHECK( Ral_PresentationHostOpenInfoValid( &openInfo ) );
	CHECK( Ral_PresentationHostReceiptExact( &receipt, &receipt ) );
	CHECK( Ral_PresentationSurfaceBorrowExact( &borrow, &borrow ) );
	CHECK( Ral_PresentationHostImportsValid( &imports ) );
#define MUTATE_RECEIPT(field) do { exact = receipt; exact.field++; \
	CHECK( !Ral_PresentationHostReceiptExact( &receipt, &exact ) ); } while (0)
	MUTATE_RECEIPT( backendType ); MUTATE_RECEIPT( ownerGeneration );
	MUTATE_RECEIPT( surfaceGeneration ); MUTATE_RECEIPT( ownerIdentity );
	MUTATE_RECEIPT( logicalWidth ); MUTATE_RECEIPT( logicalHeight );
	MUTATE_RECEIPT( pixelWidth ); MUTATE_RECEIPT( pixelHeight );
	MUTATE_RECEIPT( contentScaleX ); MUTATE_RECEIPT( contentScaleY );
	MUTATE_RECEIPT( visible ); MUTATE_RECEIPT( ready );
#undef MUTATE_RECEIPT
#define MUTATE_BORROW(field) do { borrowExact = borrow; borrowExact.field++; \
	CHECK( !Ral_PresentationSurfaceBorrowExact( &borrow, &borrowExact ) ); } while (0)
	MUTATE_BORROW( backendType ); MUTATE_BORROW( ownerGeneration );
	MUTATE_BORROW( surfaceGeneration ); MUTATE_BORROW( ownerIdentity );
	MUTATE_BORROW( surfaceIdentity ); MUTATE_BORROW( ready );
#undef MUTATE_BORROW
	borrowExact = borrow; borrowExact.surfaceIdentity = borrow.ownerIdentity;
	CHECK( !Ral_PresentationSurfaceBorrowValid( &borrowExact ) );
	imports.borrow = NULL; CHECK( !Ral_PresentationHostImportsValid( &imports ) );
	puts( "RAL presentation host: PASS" );
	return 0;
}
