// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_host.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	void    *expectedPlatform;
	void    *expectedInstance;
	uint64_t producedSurface;
	int      calls;
	qboolean accept;
} fixture_t;

static void *fake_get_proc( void *userData, void *instance, const char *name ) {
	(void)userData; (void)instance; (void)name;
	return (void *)(uintptr_t)1;
}

static qboolean fake_create_surface( void *userData, void *platform,
	                                 void *instance, uint64_t *outSurface ) {
	fixture_t *f = (fixture_t *)userData;
	f->calls++;
	if ( platform != f->expectedPlatform || instance != f->expectedInstance )
		return qfalse;
	// Deliberately scribble even on rejection. RalHost_CreateSurface must keep
	// its caller-visible output zero until the callback both accepts and
	// supplies a non-zero handle.
	*outSurface = f->producedSurface;
	return f->accept;
}

static int require_true( qboolean value, const char *what ) {
	if ( value ) return 0;
	fprintf( stderr, "FAIL: %s\n", what );
	return 1;
}

int main( void ) {
	ralHostImports_t host;
	fixture_t fixture;
	uint64_t surface;
	void *platform = (void *)(uintptr_t)0x1234;
	void *instance = (void *)(uintptr_t)0x5678;
	int failed = 0;

	memset( &host, 0, sizeof( host ) );
	failed += require_true( !RalHost_Validate( NULL, qfalse ), "NULL host rejected" );
	failed += require_true( !RalHost_Validate( &host, qfalse ), "missing loader rejected" );
	host.getProcAddress = fake_get_proc;
	failed += require_true( RalHost_Validate( &host, qfalse ), "loader-only offscreen host accepted" );
	failed += require_true( !RalHost_Validate( &host, qtrue ), "owned-surface host requires callback" );

	memset( &fixture, 0, sizeof( fixture ) );
	fixture.expectedPlatform = platform;
	fixture.expectedInstance = instance;
	fixture.producedSurface = UINT64_C( 0xAABBCCDD );
	host.userData = &fixture;
	host.createSurface = fake_create_surface;
	failed += require_true( RalHost_Validate( &host, qtrue ), "complete host accepted with NULL logger" );

	surface = UINT64_MAX;
	fixture.accept = qfalse;
	failed += require_true( !RalHost_CreateSurface( &host, platform, instance, &surface ), "rejected surface fails" );
	failed += require_true( surface == 0, "rejected surface is output-atomic" );

	surface = UINT64_MAX;
	fixture.accept = qtrue;
	fixture.producedSurface = 0;
	failed += require_true( !RalHost_CreateSurface( &host, platform, instance, &surface ), "zero surface fails" );
	failed += require_true( surface == 0, "zero surface is output-atomic" );

	surface = 0;
	fixture.producedSurface = UINT64_C( 0xAABBCCDD );
	failed += require_true( RalHost_CreateSurface( &host, platform, instance, &surface ), "valid surface accepted" );
	failed += require_true( surface == fixture.producedSurface, "surface handle forwarded exactly" );
	failed += require_true( fixture.calls == 3, "callback cardinality" );

	if ( failed ) return 1;
	puts( "ral host import contract: PASS" );
	return 0;
}
