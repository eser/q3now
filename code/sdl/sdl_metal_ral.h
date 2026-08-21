// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_SDL_METAL_RAL_H
#define WIRED_SDL_METAL_RAL_H

#include "ral_metal_present.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIRED_METAL_SDL_SCHEMA_VERSION 1u

typedef struct wiredMetalSdl_s wiredMetalSdl_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t coreGeneration;
	uint64_t adapterGeneration;
	uintptr_t ownerIdentity;
	uint32_t pixelWidth;
	uint32_t pixelHeight;
	ralMetalPresentLayerReceipt_t presentation;
	qboolean hidden;
	qboolean ready;
} wiredMetalSdlReceipt_t;

qboolean WiredMetalSdl_Create( ralMetalCore_t *core,
	const ralMetalCoreReceipt_t *coreReceipt,
	const ralSwapchainCreateInfo_t *createInfo, uint64_t adapterGeneration,
	wiredMetalSdl_t **outAdapter, wiredMetalSdlReceipt_t *outReceipt );
qboolean WiredMetalSdl_Resize( wiredMetalSdl_t *adapter,
	const ralMetalCoreReceipt_t *coreReceipt,
	const wiredMetalSdlReceipt_t *currentReceipt,
	uint32_t logicalWidth, uint32_t logicalHeight, uint64_t adapterGeneration,
	wiredMetalSdlReceipt_t *outReceipt );
qboolean WiredMetalSdl_PresentClear( wiredMetalSdl_t *adapter,
	const ralMetalCoreReceipt_t *coreReceipt,
	const wiredMetalSdlReceipt_t *adapterReceipt,
	uint64_t acquireGeneration, uint64_t presentGeneration,
	const float clearColor[4], ralMetalDrawableReceipt_t *outDrawable,
	ralMetalPresentReceipt_t *outPresent );
void WiredMetalSdl_Destroy( wiredMetalSdl_t *adapter );

qboolean WiredMetalSdl_ReceiptExact( const wiredMetalSdlReceipt_t *a,
	const wiredMetalSdlReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
