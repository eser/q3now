// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_presentation_host.h"

#include <limits.h>
#include <math.h>

static qboolean BoolValid( qboolean value ) {
	return ( value == qfalse || value == qtrue ) ? qtrue : qfalse;
}

static qboolean BackendValid( ralBackendType_t backendType ) {
	return ( backendType >= RAL_BACKEND_VULKAN
		&& backendType <= RAL_BACKEND_WEBGL2 ) ? qtrue : qfalse;
}

qboolean Ral_PresentationHostOpenInfoValid(
		const ralPresentationHostOpenInfo_t *info ) {
	return ( info
		&& info->schemaVersion == RAL_PRESENTATION_HOST_SCHEMA_VERSION
		&& BackendValid( info->backendType )
		&& BoolValid( info->requestVisible ) ) ? qtrue : qfalse;
}

qboolean Ral_PresentationHostReceiptValid(
		const ralPresentationHostReceipt_t *receipt ) {
	return ( receipt
		&& receipt->schemaVersion == RAL_PRESENTATION_HOST_SCHEMA_VERSION
		&& BackendValid( receipt->backendType )
		&& receipt->ownerGeneration != 0u
		&& receipt->ownerGeneration != UINT64_MAX
		&& receipt->surfaceGeneration != 0u
		&& receipt->surfaceGeneration != UINT64_MAX
		&& receipt->ownerIdentity != 0u
		&& receipt->logicalWidth != 0u && receipt->logicalHeight != 0u
		&& receipt->pixelWidth != 0u && receipt->pixelHeight != 0u
		&& receipt->logicalWidth <= 16384u && receipt->logicalHeight <= 16384u
		&& receipt->pixelWidth <= 32768u && receipt->pixelHeight <= 32768u
		&& isfinite( receipt->contentScaleX )
		&& isfinite( receipt->contentScaleY )
		&& receipt->contentScaleX > 0.0f && receipt->contentScaleX <= 16.0f
		&& receipt->contentScaleY > 0.0f && receipt->contentScaleY <= 16.0f
		&& BoolValid( receipt->visible )
		&& receipt->ready == qtrue ) ? qtrue : qfalse;
}

qboolean Ral_PresentationHostReceiptExact(
		const ralPresentationHostReceipt_t *a,
		const ralPresentationHostReceipt_t *b ) {
	return ( Ral_PresentationHostReceiptValid( a )
		&& Ral_PresentationHostReceiptValid( b )
		&& a->schemaVersion == b->schemaVersion
		&& a->backendType == b->backendType
		&& a->ownerGeneration == b->ownerGeneration
		&& a->surfaceGeneration == b->surfaceGeneration
		&& a->ownerIdentity == b->ownerIdentity
		&& a->logicalWidth == b->logicalWidth
		&& a->logicalHeight == b->logicalHeight
		&& a->pixelWidth == b->pixelWidth
		&& a->pixelHeight == b->pixelHeight
		&& a->contentScaleX == b->contentScaleX
		&& a->contentScaleY == b->contentScaleY
		&& a->visible == b->visible
		&& a->ready == b->ready ) ? qtrue : qfalse;
}

qboolean Ral_PresentationSurfaceBorrowValid(
		const ralPresentationSurfaceBorrow_t *borrow ) {
	return ( borrow
		&& borrow->schemaVersion == RAL_PRESENTATION_HOST_SCHEMA_VERSION
		&& BackendValid( borrow->backendType )
		&& borrow->ownerGeneration != 0u
		&& borrow->ownerGeneration != UINT64_MAX
		&& borrow->surfaceGeneration != 0u
		&& borrow->surfaceGeneration != UINT64_MAX
		&& borrow->ownerIdentity != 0u
		&& borrow->surfaceIdentity != 0u
		&& borrow->ownerIdentity != borrow->surfaceIdentity
		&& borrow->ready == qtrue ) ? qtrue : qfalse;
}

qboolean Ral_PresentationSurfaceBorrowExact(
		const ralPresentationSurfaceBorrow_t *a,
		const ralPresentationSurfaceBorrow_t *b ) {
	return ( Ral_PresentationSurfaceBorrowValid( a )
		&& Ral_PresentationSurfaceBorrowValid( b )
		&& a->schemaVersion == b->schemaVersion
		&& a->backendType == b->backendType
		&& a->ownerGeneration == b->ownerGeneration
		&& a->surfaceGeneration == b->surfaceGeneration
		&& a->ownerIdentity == b->ownerIdentity
		&& a->surfaceIdentity == b->surfaceIdentity
		&& a->ready == b->ready ) ? qtrue : qfalse;
}

qboolean Ral_PresentationHostImportsValid(
		const ralPresentationHostImports_t *imports ) {
	return ( imports
		&& imports->schemaVersion == RAL_PRESENTATION_HOST_SCHEMA_VERSION
		&& imports->open && imports->refresh && imports->borrow && imports->close )
		? qtrue : qfalse;
}
