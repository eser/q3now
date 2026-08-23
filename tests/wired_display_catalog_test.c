// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "cl_display_catalog.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n", \
	__FILE__,__LINE__,#x); return 1; } } while (0)

static int FailedProvider( void *context, wiredDisplayCatalog_t *outCatalog ) {
	(void)context; (void)outCatalog; return 0;
}

static int CatalogProvider( void *context, wiredDisplayCatalog_t *outCatalog ) {
	if ( !context || !outCatalog ) return 0;
	*outCatalog = *(const wiredDisplayCatalog_t *)context;
	return 1;
}

static void InitDisplay( wiredDisplayDescriptor_t *display, const char *key,
		const char *name, uint32_t flags ) {
	memset( display, 0, sizeof( *display ) );
	display->schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	strncpy( display->persistentKey, key, sizeof( display->persistentKey ) - 1u );
	strncpy( display->name, name, sizeof( display->name ) - 1u );
	display->desktopWidth = 1920u; display->desktopHeight = 1080u;
	display->densityNumerator = 1u; display->densityDenominator = 1u;
	display->flags = flags;
}

static wiredDisplayMode_t Mode( uint32_t displayIndex, uint32_t width,
		uint32_t height, uint32_t refreshNumerator,
		uint32_t refreshDenominator, uint32_t flags ) {
	wiredDisplayMode_t mode;
	memset( &mode, 0, sizeof( mode ) );
	mode.schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	mode.displayIndex = displayIndex; mode.width = width; mode.height = height;
	mode.refreshNumerator = refreshNumerator;
	mode.refreshDenominator = refreshDenominator;
	mode.densityNumerator = 1u; mode.densityDenominator = 1u;
	mode.flags = flags; mode.legacyModeIndex = -1;
	return mode;
}

int main( void ) {
	wiredDisplayCatalog_t raw, normalized, fallback, web, reordered, removed;
	wiredDisplayProvider_t provider = {
		WIRED_DISPLAY_CATALOG_SCHEMA_VERSION,
		WIRED_DISPLAY_PROVIDER_WEB, NULL, FailedProvider
	};
	wiredDisplayExtentDomains_t extents;
	wiredDisplayRequest_t request;
	wiredDisplayEffectiveState_t effective;
	wiredDisplayResolutionReceipt_t receipt;
	wiredDisplayReconciler_t reconciler;
	char modeValue[64];
	uint32_t width, height;
	uint32_t refreshNumerator, refreshDenominator;
	float pixelAspect;

	CHECK( WiredDisplay_LegacyModeCount() == 32u );
	CHECK( WiredDisplay_LegacyModeAt( 3u )->width == 640u );
	CHECK( WiredDisplay_LegacyModeAt( 13u )->height == 720u );
	CHECK( WiredDisplay_LegacyModeAt( 31u )->width == 7680u );
	CHECK( WiredDisplay_ResolveLegacyMode( -1, 0u, 0u, 640u, 480u, 1.0f,
		&width, &height, &pixelAspect ) );
	CHECK( width == 640u && height == 480u && pixelAspect == 1.0f );
	CHECK( WiredDisplay_ResolveLegacyMode( -2, 0u, 0u, 1u, 1u, 1.0f,
		&width, &height, &pixelAspect ) );
	CHECK( width == 1280u && height == 720u );

	WiredDisplay_BuildFallbackCatalog( WIRED_DISPLAY_PROVIDER_HEADLESS, &fallback );
	CHECK( fallback.displayCount == 1u && fallback.modeCount == 32u );
	CHECK( fallback.flags & WIRED_DISPLAY_CATALOG_FALLBACK );
	CHECK( fallback.modes[fallback.displays[0].firstMode].width == 320u );
	CHECK( !WiredDisplay_CaptureProvider( &provider, &fallback ) );
	CHECK( fallback.providerKind == WIRED_DISPLAY_PROVIDER_WEB );
	CHECK( fallback.flags & WIRED_DISPLAY_CATALOG_FALLBACK );

	memset( &raw, 0, sizeof( raw ) );
	raw.schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	raw.generation = 7u; raw.providerKind = WIRED_DISPLAY_PROVIDER_SDL;
	raw.displayCount = 2u; raw.modeCount = 6u;
	InitDisplay( &raw.displays[0], "display:b", "Same", WIRED_DISPLAY_FLAG_PRIMARY );
	InitDisplay( &raw.displays[1], "display:a", "Same", WIRED_DISPLAY_FLAG_CURRENT );
	raw.modes[0] = Mode( 0u, 1920u, 1080u, 120000u, 1000u,
		WIRED_DISPLAY_MODE_EXCLUSIVE );
	raw.modes[1] = Mode( 0u, 1280u, 720u, 60u, 1u, 0u );
	raw.modes[2] = Mode( 0u, 1920u, 1080u, 60000u, 1000u,
		WIRED_DISPLAY_MODE_DESKTOP );
	raw.modes[3] = Mode( 0u, 1920u, 1080u, 60u, 1u,
		WIRED_DISPLAY_MODE_CURRENT );
	raw.modes[4] = Mode( 1u, 2560u, 1440u, 144u, 1u,
		WIRED_DISPLAY_MODE_CURRENT | WIRED_DISPLAY_MODE_DESKTOP );
	raw.modes[5] = Mode( 1u, 0u, 1440u, 60u, 1u, 0u );
	CHECK( WiredDisplay_CatalogNormalize( &raw, &normalized ) );
	CHECK( normalized.modeCount == 4u );
	CHECK( normalized.modes[0].width == 1280u );
	CHECK( normalized.modes[1].refreshNumerator == 60u );
	CHECK( normalized.modes[1].flags
		& (WIRED_DISPLAY_MODE_DESKTOP | WIRED_DISPLAY_MODE_CURRENT) );
	CHECK( normalized.modes[2].refreshNumerator == 120u );
	CHECK( normalized.displays[0].modeCount == 3u );
	CHECK( normalized.displays[1].modeCount == 1u );
	CHECK( WiredDisplay_FormatModeValue( &normalized.modes[1], modeValue,
		sizeof( modeValue ) ) );
	CHECK( WiredDisplay_ParseModeValue( modeValue, &width, &height,
		&refreshNumerator, &refreshDenominator ) );
	CHECK( width == 1920u && height == 1080u );
	CHECK( refreshNumerator == 60u && refreshDenominator == 1u );
	CHECK( WiredDisplay_FormatRefreshValue( 60000u, 1000u, modeValue,
		sizeof( modeValue ) ) && !strcmp( modeValue, "60/1" ) );
	CHECK( WiredDisplay_ParseRefreshValue( modeValue, &refreshNumerator,
		&refreshDenominator ) && refreshNumerator == 60u
		&& refreshDenominator == 1u );
	CHECK( WiredDisplay_ParseRefreshValue( "auto", &refreshNumerator,
		&refreshDenominator ) && refreshNumerator == 0u
		&& refreshDenominator == 0u );
	CHECK( WiredDisplay_ParseModeValue( "640x480", &width, &height,
		&refreshNumerator, &refreshDenominator ) );
	CHECK( width == 640u && height == 480u && refreshNumerator == 0u
		&& refreshDenominator == 0u );
	CHECK( !WiredDisplay_ParseModeValue( "640x480 trailing", &width, &height,
		&refreshNumerator, &refreshDenominator ) );
	CHECK( WiredDisplay_PublishCatalog( &normalized ) == 1 );
	CHECK( WiredDisplay_GetActiveCatalog()->generation == 1u );
	CHECK( WiredDisplay_PublishCatalog( &normalized ) == 0 );
	provider.kind = WIRED_DISPLAY_PROVIDER_SDL;
	provider.context = &raw;
	provider.snapshot = CatalogProvider;
	CHECK( WiredDisplay_CaptureProvider( &provider, &fallback ) );
	CHECK( fallback.providerKind == WIRED_DISPLAY_PROVIDER_SDL
		&& fallback.modeCount == normalized.modeCount );

	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	request.display.schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	strncpy( request.display.persistentKey, "display:a",
		sizeof( request.display.persistentKey ) - 1u );
	request.windowPolicy = WIRED_WINDOW_POLICY_WINDOWED;
	request.width = 2560u; request.height = 1440u;
	CHECK( WiredDisplay_ResolveRequest( &normalized, &request, &effective ) );
	CHECK( strcmp( effective.display.persistentKey, "display:a" ) == 0 );
	CHECK( effective.mode.width == 2560u && !effective.usedFallback );
	CHECK( effective.extents.uiWidth == effective.extents.presentationWidth );

	/* Provider reorder never changes the serializable selector's meaning. */
	memset( &reordered, 0, sizeof( reordered ) );
	reordered.schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	reordered.providerKind = WIRED_DISPLAY_PROVIDER_SDL;
	reordered.displayCount = 2u; reordered.modeCount = 2u;
	reordered.displays[0] = raw.displays[1];
	reordered.displays[1] = raw.displays[0];
	reordered.modes[0] = Mode( 0u, 2560u, 1440u, 144u, 1u,
		WIRED_DISPLAY_MODE_CURRENT | WIRED_DISPLAY_MODE_DESKTOP );
	reordered.modes[1] = Mode( 1u, 1920u, 1080u, 60u, 1u,
		WIRED_DISPLAY_MODE_DESKTOP );
	CHECK( WiredDisplay_CatalogNormalize( &reordered, &reordered ) );
	CHECK( WiredDisplay_ResolveRequest( &reordered, &request, &effective ) );
	CHECK( !strcmp( effective.display.persistentKey, "display:a" )
		&& effective.mode.width == 2560u );

	/* Removing the requested output falls back current -> primary atomically,
	 * while keeping the original request untouched for a future explicit Apply. */
	memset( &removed, 0, sizeof( removed ) );
	removed.schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	removed.providerKind = WIRED_DISPLAY_PROVIDER_SDL;
	removed.displayCount = 1u; removed.modeCount = 1u;
	InitDisplay( &removed.displays[0], "display:b", "Remaining",
		WIRED_DISPLAY_FLAG_PRIMARY | WIRED_DISPLAY_FLAG_CURRENT );
	removed.displays[0].densityNumerator = 3u;
	removed.displays[0].densityDenominator = 2u;
	removed.modes[0] = Mode( 0u, 1920u, 1080u, 60u, 1u,
		WIRED_DISPLAY_MODE_CURRENT | WIRED_DISPLAY_MODE_DESKTOP );
	removed.modes[0].densityNumerator = 3u;
	removed.modes[0].densityDenominator = 2u;
	CHECK( WiredDisplay_CatalogNormalize( &removed, &removed ) );
	CHECK( WiredDisplay_ResolveRequest( &removed, &request, &effective ) );
	CHECK( !strcmp( effective.display.persistentKey, "display:b" )
		&& !strcmp( effective.requested.display.persistentKey, "display:a" )
		&& effective.usedFallback );

	request.display.persistentKey[0] = '\0';
	strncpy( request.display.name, "Same", sizeof( request.display.name ) - 1u );
	request.display.desktopWidth = 1920u; request.display.desktopHeight = 1080u;
	request.width = 1920u; request.height = 1080u;
	request.windowPolicy = WIRED_WINDOW_POLICY_EXCLUSIVE;
	request.refreshNumerator = 75u; request.refreshDenominator = 1u;
	CHECK( WiredDisplay_ResolveRequest( &normalized, &request, &effective ) );
	CHECK( effective.requested.windowPolicy == WIRED_WINDOW_POLICY_EXCLUSIVE );
	CHECK( effective.effectiveWindowPolicy == WIRED_WINDOW_POLICY_BORDERLESS );

	CHECK( WiredDisplay_BuildWebCurrentScreen( 800u, 600u, 1600u, 1200u,
		2u, 1u, 1, &web, &extents ) );
	CHECK( web.providerKind == WIRED_DISPLAY_PROVIDER_WEB );
	CHECK( !(web.providerCapabilities & WIRED_DISPLAY_CAP_EXCLUSIVE_MODES) );
	CHECK( extents.logicalWidth == 800u && extents.presentationWidth == 1600u );
	CHECK( extents.uiWidth == 1600u && extents.renderWidth == 1600u );
	CHECK( WiredDisplay_ExtentDomainsValid( &extents ) );
	CHECK( WiredDisplay_BuildResolutionReceipt( 9u, 4u,
		WIRED_DISPLAY_CHANGE_ACTIVE_OUTPUT | WIRED_DISPLAY_CHANGE_SCALE,
		1280u, 720u, 2560u, 1440u, 1600u, 900u, &receipt ) );
	CHECK( receipt.extents.logicalWidth == 1280u
		&& receipt.extents.presentationWidth == 2560u
		&& receipt.extents.renderWidth == 1600u
		&& receipt.extents.uiWidth == 2560u );
	CHECK( WiredDisplay_PublishResolutionReceipt( &receipt ) );
	CHECK( WiredDisplay_GetActiveResolutionReceipt()->surfaceGeneration == 4u );
	CHECK( !WiredDisplay_PublishResolutionReceipt( &receipt ) );

	/* A burst is observed once with its union of reasons. */
	WiredDisplay_ReconcilerInit( &reconciler, 0u );
	WiredDisplay_ReconcilerMark( &reconciler, WIRED_DISPLAY_CHANGE_TOPOLOGY );
	WiredDisplay_ReconcilerMark( &reconciler, WIRED_DISPLAY_CHANGE_SCALE );
	WiredDisplay_ReconcilerMark( &reconciler, WIRED_DISPLAY_CHANGE_COLOR );
	CHECK( WiredDisplay_ReconcilerTake( &reconciler )
		== (WIRED_DISPLAY_CHANGE_TOPOLOGY | WIRED_DISPLAY_CHANGE_SCALE
			| WIRED_DISPLAY_CHANGE_COLOR) );
	CHECK( WiredDisplay_ReconcilerTake( &reconciler ) == 0u );
	CHECK( WiredDisplay_PresentationAction( WIRED_DISPLAY_CHANGE_COLOR, 1 )
		== WIRED_PRESENTATION_ACTION_REFRESH_METADATA );
	CHECK( WiredDisplay_PresentationAction( WIRED_DISPLAY_CHANGE_ACTIVE_OUTPUT, 1 )
		== WIRED_PRESENTATION_ACTION_REBUILD );
	CHECK( WiredDisplay_PresentationAction( WIRED_DISPLAY_CHANGE_TOPOLOGY, 0 )
		== WIRED_PRESENTATION_ACTION_RESELECT_DEVICE );
	extents.logicalWidth = 640u; extents.logicalHeight = 480u;
	extents.presentationWidth = 1920u; extents.presentationHeight = 1080u;
	extents.renderWidth = 1280u; extents.renderHeight = 720u;
	extents.uiWidth = 1920u; extents.uiHeight = 1080u;
	CHECK( WiredDisplay_ExtentDomainsValid( &extents ) );

	puts( "Wired display catalog: PASS" );
	return 0;
}
