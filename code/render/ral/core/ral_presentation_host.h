// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_PRESENTATION_HOST_H
#define WIRED_RAL_PRESENTATION_HOST_H

#include "ral_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_PRESENTATION_HOST_SCHEMA_VERSION 1u

typedef enum {
	RAL_PRESENTATION_HOST_KEEP_OWNER = 1,
	RAL_PRESENTATION_HOST_DESTROY_OWNER
} ralPresentationHostCloseMode_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	qboolean requestVisible;
} ralPresentationHostOpenInfo_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t ownerGeneration;
	uint64_t surfaceGeneration;
	uintptr_t ownerIdentity;
	uint32_t logicalWidth;
	uint32_t logicalHeight;
	uint32_t pixelWidth;
	uint32_t pixelHeight;
	float contentScaleX;
	float contentScaleY;
	qboolean visible;
	qboolean ready;
} ralPresentationHostReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t ownerGeneration;
	uint64_t surfaceGeneration;
	uintptr_t ownerIdentity;
	uintptr_t surfaceIdentity;
	qboolean ready;
} ralPresentationSurfaceBorrow_t;

typedef qboolean (*ralPresentationHostOpenFn)( void *context,
	const ralPresentationHostOpenInfo_t *info,
	ralPresentationHostReceipt_t *outReceipt );
typedef qboolean (*ralPresentationHostRefreshFn)( void *context,
	const ralPresentationHostReceipt_t *currentReceipt,
	ralPresentationHostReceipt_t *outReceipt );
typedef qboolean (*ralPresentationHostBorrowFn)( void *context,
	const ralPresentationHostReceipt_t *currentReceipt,
	ralPresentationSurfaceBorrow_t *outBorrow );
typedef qboolean (*ralPresentationHostCloseFn)( void *context,
	const ralPresentationHostReceipt_t *currentReceipt,
	ralPresentationHostCloseMode_t mode );

typedef struct {
	uint32_t schemaVersion;
	void *context;
	ralPresentationHostOpenFn open;
	ralPresentationHostRefreshFn refresh;
	ralPresentationHostBorrowFn borrow;
	ralPresentationHostCloseFn close;
} ralPresentationHostImports_t;

qboolean Ral_PresentationExtentValid( uint32_t width, uint32_t height );
qboolean Ral_PresentationHostOpenInfoValid(
	const ralPresentationHostOpenInfo_t *info );
qboolean Ral_PresentationHostReceiptValid(
	const ralPresentationHostReceipt_t *receipt );
qboolean Ral_PresentationHostReceiptExact(
	const ralPresentationHostReceipt_t *a,
	const ralPresentationHostReceipt_t *b );
qboolean Ral_PresentationSurfaceBorrowValid(
	const ralPresentationSurfaceBorrow_t *borrow );
qboolean Ral_PresentationSurfaceBorrowExact(
	const ralPresentationSurfaceBorrow_t *a,
	const ralPresentationSurfaceBorrow_t *b );
qboolean Ral_PresentationHostImportsValid(
	const ralPresentationHostImports_t *imports );

#ifdef __cplusplus
}
#endif

#endif
