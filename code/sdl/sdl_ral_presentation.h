// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_SDL_RAL_PRESENTATION_H
#define WIRED_SDL_RAL_PRESENTATION_H

#include "ral_presentation_host.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wiredSdlRalPresentationHost_s wiredSdlRalPresentationHost_t;

qboolean WiredSdlRalPresentationHost_Create( uint32_t logicalWidth,
	uint32_t logicalHeight, qboolean allowVisible, uint64_t firstGeneration,
	wiredSdlRalPresentationHost_t **outHost,
	ralPresentationHostImports_t *outImports );
qboolean WiredSdlRalPresentationHost_RequestResize(
	wiredSdlRalPresentationHost_t *host, uint32_t logicalWidth,
	uint32_t logicalHeight );
qboolean WiredSdlRalPresentationHost_GetReceipt(
	wiredSdlRalPresentationHost_t *host,
	ralPresentationHostReceipt_t *outReceipt );
void WiredSdlRalPresentationHost_Destroy(
	wiredSdlRalPresentationHost_t *host );

#ifdef __cplusplus
}
#endif

#endif
