// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_SDL_OPENGL46_RAL_H
#define WIRED_SDL_OPENGL46_RAL_H

#include "ral_opengl_core.h"
#include "ral_presentation_host.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIRED_SDL_OPENGL46_SCHEMA_VERSION 1u

typedef struct wiredSdlOpenGl46_s wiredSdlOpenGl46_t;

typedef enum {
	WIRED_SDL_OPENGL46_READY = 1,
	WIRED_SDL_OPENGL46_UNSUPPORTED,
	WIRED_SDL_OPENGL46_FAILED
} wiredSdlOpenGl46Status_t;

typedef struct {
	uint32_t logicalWidth;
	uint32_t logicalHeight;
	uint64_t firstGeneration;
	qboolean visible;
} wiredSdlOpenGl46CreateInfo_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t adapterGeneration;
	uint64_t presentedFrames;
	ralPresentationHostReceipt_t host;
	ralOpenGlCoreReceipt_t core;
	qboolean ready;
} wiredSdlOpenGl46Receipt_t;

wiredSdlOpenGl46Status_t WiredSdlOpenGl46_Create(
	const wiredSdlOpenGl46CreateInfo_t *createInfo,
	wiredSdlOpenGl46_t **outAdapter, wiredSdlOpenGl46Receipt_t *outReceipt );
void WiredSdlOpenGl46_Destroy( wiredSdlOpenGl46_t *adapter );
qboolean WiredSdlOpenGl46_ReceiptExact( const wiredSdlOpenGl46Receipt_t *a,
	const wiredSdlOpenGl46Receipt_t *b );
qboolean WiredSdlOpenGl46_RequestResize( wiredSdlOpenGl46_t *adapter,
	const wiredSdlOpenGl46Receipt_t *current, uint32_t logicalWidth,
	uint32_t logicalHeight, wiredSdlOpenGl46Receipt_t *outReceipt );
qboolean WiredSdlOpenGl46_Present( wiredSdlOpenGl46_t *adapter,
	const wiredSdlOpenGl46Receipt_t *current,
	wiredSdlOpenGl46Receipt_t *outReceipt );
qboolean WiredSdlOpenGl46_RecreateAfterLoss( wiredSdlOpenGl46_t *adapter,
	const wiredSdlOpenGl46Receipt_t *current,
	const ralMemoryFailureEvent_t *lossEvent,
	wiredSdlOpenGl46Receipt_t *outReceipt );
qboolean WiredSdlOpenGl46_BorrowCore( wiredSdlOpenGl46_t *adapter,
	const wiredSdlOpenGl46Receipt_t *current, ralOpenGlCore_t **outCore,
	ralOpenGlCoreReceipt_t *outReceipt );

#ifdef __cplusplus
}
#endif

#endif
