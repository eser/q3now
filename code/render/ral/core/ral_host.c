// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_host.h"

qboolean RalHost_Validate( const ralHostImports_t *host,
	                       qboolean requireSurface ) {
	if ( !host || !host->getProcAddress ) return qfalse;
	if ( requireSurface && !host->createSurface ) return qfalse;
	// Logging is optional by contract: diagnostics may be silent, but backend
	// behavior and teardown must remain unchanged.
	return qtrue;
}

qboolean RalHost_CreateSurface( const ralHostImports_t *host,
	                            void *platformHandle,
	                            void *nativeInstance,
	                            uint64_t *outNativeSurface ) {
	uint64_t candidate = 0;
	if ( outNativeSurface ) *outNativeSurface = 0;
	if ( !outNativeSurface || !host || !host->createSurface ) return qfalse;
	if ( !host->createSurface( host->userData, platformHandle, nativeInstance,
	                          &candidate ) || candidate == 0 ) return qfalse;
	*outNativeSurface = candidate;
	return qtrue;
}
