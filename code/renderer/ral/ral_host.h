// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_HOST_H
#define WIRED_RAL_HOST_H

#include "ral_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

qboolean RalHost_Validate( const ralHostImports_t *host,
	                       qboolean requireSurface );
qboolean RalHost_CreateSurface( const ralHostImports_t *host,
	                            void *platformHandle,
	                            void *nativeInstance,
	                            uint64_t *outNativeSurface );

#ifdef __cplusplus
}
#endif

#endif
