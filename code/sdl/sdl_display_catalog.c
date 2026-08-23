// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include <SDL3/SDL.h>

#include "../client/client.h"
#include "../client/cl_display_catalog.h"
#include "sdl_glw.h"

#include <stdio.h>
#include <string.h>

LOG_DECLARE_CHANNEL( ch_client, "client" );

static wiredDisplayReconciler_t s_reconciler = { GLIMP_DISPLAY_DIRTY_TOPOLOGY };

_Static_assert( REF_PRESENTATION_CHANGE_TOPOLOGY == WIRED_DISPLAY_CHANGE_TOPOLOGY,
	"presentation topology flag drift" );
_Static_assert( REF_PRESENTATION_CHANGE_ACTIVE_OUTPUT == WIRED_DISPLAY_CHANGE_ACTIVE_OUTPUT,
	"presentation active-output flag drift" );
_Static_assert( REF_PRESENTATION_CHANGE_SCALE == WIRED_DISPLAY_CHANGE_SCALE,
	"presentation scale flag drift" );
_Static_assert( REF_PRESENTATION_CHANGE_COLOR == WIRED_DISPLAY_CHANGE_COLOR,
	"presentation color flag drift" );
_Static_assert( REF_PRESENTATION_CHANGE_FULLSCREEN == WIRED_DISPLAY_CHANGE_FULLSCREEN,
	"presentation fullscreen flag drift" );
_Static_assert( REF_PRESENTATION_CHANGE_EXTENT == WIRED_DISPLAY_CHANGE_EXTENT,
	"presentation extent flag drift" );
static uint64_t s_presentationGeneration;

static uint64_t HashBytes( uint64_t hash, const void *data, size_t size ) {
	const unsigned char *bytes = (const unsigned char *)data;
	size_t i;
	for ( i = 0u; i < size; ++i ) {
		hash ^= bytes[i];
		hash *= UINT64_C( 1099511628211 );
	}
	return hash;
}

static uint64_t HashText( uint64_t hash, const char *text ) {
	return HashBytes( hash, text ? text : "", text ? strlen( text ) : 0u );
}

static void ScaleFraction( float value, uint32_t *numerator,
		uint32_t *denominator ) {
	if ( !(value > 0.0f) || value > 16.0f ) value = 1.0f;
	*numerator = (uint32_t)( value * 1000.0f + 0.5f );
	*denominator = 1000u;
}

static int LegacyModeIndex( int width, int height ) {
	size_t i;
	for ( i = 0u; i < WiredDisplay_LegacyModeCount(); ++i ) {
		const wiredLegacyVideoMode_t *mode = WiredDisplay_LegacyModeAt( i );
		if ( mode && mode->width == (uint32_t)width
			&& mode->height == (uint32_t)height ) return (int)i;
	}
	return -1;
}

static int SameMode( const SDL_DisplayMode *left,
		const SDL_DisplayMode *right ) {
	if ( !left || !right ) return 0;
	return left->w == right->w && left->h == right->h
		&& left->refresh_rate_numerator == right->refresh_rate_numerator
		&& left->refresh_rate_denominator == right->refresh_rate_denominator;
}

static void AppendMode( wiredDisplayCatalog_t *catalog, uint32_t displayIndex,
		const SDL_DisplayMode *source, uint32_t flags ) {
	wiredDisplayMode_t *mode;
	if ( !catalog || !source || source->w <= 0 || source->h <= 0
		|| catalog->modeCount >= WIRED_DISPLAY_MAX_MODES ) return;
	mode = &catalog->modes[catalog->modeCount++];
	memset( mode, 0, sizeof( *mode ) );
	mode->schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	mode->displayIndex = displayIndex;
	mode->width = (uint32_t)source->w;
	mode->height = (uint32_t)source->h;
	if ( source->refresh_rate_numerator > 0
		&& source->refresh_rate_denominator > 0 ) {
		mode->refreshNumerator = (uint32_t)source->refresh_rate_numerator;
		mode->refreshDenominator = (uint32_t)source->refresh_rate_denominator;
	}
	ScaleFraction( source->pixel_density,
		&mode->densityNumerator, &mode->densityDenominator );
	mode->flags = flags;
	mode->legacyModeIndex = LegacyModeIndex( source->w, source->h );
}

static int SdlSnapshot( void *context, wiredDisplayCatalog_t *outCatalog ) {
	SDL_DisplayID *displayIds;
	SDL_DisplayID primary;
	SDL_DisplayID current;
	int displayCount = 0;
	int i;
	char baseNames[WIRED_DISPLAY_MAX_DISPLAYS][WIRED_DISPLAY_NAME_MAX];
	(void)context;
	if ( !outCatalog || !SDL_WasInit( SDL_INIT_VIDEO ) ) return 0;
	displayIds = SDL_GetDisplays( &displayCount );
	if ( !displayIds || displayCount <= 0 ) {
		SDL_free( displayIds );
		return 0;
	}
	if ( displayCount > (int)WIRED_DISPLAY_MAX_DISPLAYS )
		displayCount = (int)WIRED_DISPLAY_MAX_DISPLAYS;
	memset( outCatalog, 0, sizeof( *outCatalog ) );
	outCatalog->schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	outCatalog->generation = 1u;
	outCatalog->providerKind = WIRED_DISPLAY_PROVIDER_SDL;
	outCatalog->providerCapabilities = WIRED_DISPLAY_CAP_DENSITY_SCALE
		| WIRED_DISPLAY_CAP_FULLSCREEN;
	if ( displayCount > 1 ) outCatalog->providerCapabilities
		|= WIRED_DISPLAY_CAP_MULTIPLE_DISPLAYS;
	outCatalog->displayCount = (uint32_t)displayCount;
	primary = SDL_GetPrimaryDisplay();
	current = SDL_window ? SDL_GetDisplayForWindow( SDL_window ) : primary;
	for ( i = 0; i < displayCount; ++i ) {
		wiredDisplayDescriptor_t *display = &outCatalog->displays[i];
		const SDL_DisplayID id = displayIds[i];
		const SDL_DisplayMode *desktop = SDL_GetDesktopDisplayMode( id );
		const SDL_DisplayMode *active = SDL_GetCurrentDisplayMode( id );
		SDL_DisplayMode **modes;
		SDL_Rect bounds = { 0, 0, 0, 0 };
		const char *name = SDL_GetDisplayName( id );
		float contentScale = SDL_GetDisplayContentScale( id );
		uint64_t fingerprint = UINT64_C( 1469598103934665603 );
		int modeCount = 0;
		int j;
		display->schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
		display->sessionHandle = (uint64_t)id;
		if ( !name || !*name ) name = "Display";
		SDL_GetDisplayBounds( id, &bounds );
		fingerprint = HashText( fingerprint, name );
		fingerprint = HashBytes( fingerprint, &bounds, sizeof( bounds ) );
		if ( desktop ) {
			fingerprint = HashBytes( fingerprint, &desktop->w, sizeof( desktop->w ) );
			fingerprint = HashBytes( fingerprint, &desktop->h, sizeof( desktop->h ) );
			fingerprint = HashBytes( fingerprint, &desktop->pixel_density,
				sizeof( desktop->pixel_density ) );
			fingerprint = HashBytes( fingerprint, &desktop->refresh_rate_numerator,
				sizeof( desktop->refresh_rate_numerator ) );
			fingerprint = HashBytes( fingerprint, &desktop->refresh_rate_denominator,
				sizeof( desktop->refresh_rate_denominator ) );
		}
		snprintf( display->persistentKey, sizeof( display->persistentKey ),
			"sdl-soft:%016llx", (unsigned long long)fingerprint );
		snprintf( display->name, sizeof( display->name ), "%s", name );
		snprintf( baseNames[i], sizeof( baseNames[i] ), "%s", name );
		if ( desktop ) {
			display->desktopWidth = (uint32_t)desktop->w;
			display->desktopHeight = (uint32_t)desktop->h;
		}
		ScaleFraction( contentScale,
			&display->densityNumerator, &display->densityDenominator );
		if ( id == primary ) display->flags |= WIRED_DISPLAY_FLAG_PRIMARY;
		if ( id == current ) display->flags |= WIRED_DISPLAY_FLAG_CURRENT;
		AppendMode( outCatalog, (uint32_t)i, desktop,
			WIRED_DISPLAY_MODE_DESKTOP | ( SameMode( desktop, active )
				? WIRED_DISPLAY_MODE_CURRENT : 0u ) );
		if ( active && !SameMode( desktop, active ) )
			AppendMode( outCatalog, (uint32_t)i, active, WIRED_DISPLAY_MODE_CURRENT );
		modes = SDL_GetFullscreenDisplayModes( id, &modeCount );
		if ( modes && modeCount > 0 ) outCatalog->providerCapabilities
			|= WIRED_DISPLAY_CAP_EXCLUSIVE_MODES;
		for ( j = 0; modes && j < modeCount; ++j )
			AppendMode( outCatalog, (uint32_t)i, modes[j],
				WIRED_DISPLAY_MODE_EXCLUSIVE
				| ( SameMode( modes[j], desktop ) ? WIRED_DISPLAY_MODE_DESKTOP : 0u )
				| ( SameMode( modes[j], active ) ? WIRED_DISPLAY_MODE_CURRENT : 0u ) );
		SDL_free( modes );
	}
	for ( i = 0; i < displayCount; ++i ) {
		int duplicateCount = 0;
		int ordinal = 0;
		int j;
		char baseName[WIRED_DISPLAY_NAME_MAX];
		snprintf( baseName, sizeof( baseName ), "%s", baseNames[i] );
		for ( j = 0; j < displayCount; ++j )
			if ( strcmp( baseName, baseNames[j] ) == 0 ) {
				duplicateCount++;
				if ( j <= i ) ordinal++;
			}
		if ( duplicateCount > 1 )
			snprintf( outCatalog->displays[i].name,
				sizeof( outCatalog->displays[i].name ), "%.*s [%d]",
				(int)sizeof( outCatalog->displays[i].name ) - 8,
				baseName, ordinal );
	}
	SDL_free( displayIds );
	return outCatalog->modeCount > 0u;
}

const wiredDisplayCatalog_t *GLimp_GetDisplayCatalog( void ) {
	return WiredDisplay_GetActiveCatalog();
}

SDL_DisplayID GLimp_ResolveConfiguredDisplay( void ) {
	const wiredDisplayCatalog_t *catalog = WiredDisplay_GetActiveCatalog();
	const char *requestedKey = Cvar_VariableString( "r_output" );
	uint32_t i;
	if ( !catalog || !requestedKey || !requestedKey[0] ) return 0;
	for ( i = 0u; i < catalog->displayCount; ++i )
		if ( strcmp( requestedKey, catalog->displays[i].persistentKey ) == 0 )
			return (SDL_DisplayID)catalog->displays[i].sessionHandle;
	for ( i = 0u; i < catalog->displayCount; ++i )
		if ( catalog->displays[i].flags & WIRED_DISPLAY_FLAG_CURRENT )
			return (SDL_DisplayID)catalog->displays[i].sessionHandle;
	for ( i = 0u; i < catalog->displayCount; ++i )
		if ( catalog->displays[i].flags & WIRED_DISPLAY_FLAG_PRIMARY )
			return (SDL_DisplayID)catalog->displays[i].sessionHandle;
	return catalog->displayCount != 0u
		? (SDL_DisplayID)catalog->displays[0].sessionHandle : 0;
}

void GLimp_DisplayCatalogMarkDirty( uint32_t flags ) {
	WiredDisplay_ReconcilerMark( &s_reconciler, flags );
}

static void NotifyPresentationChanged( uint32_t dirty,
		const wiredDisplayCatalog_t *catalog ) {
	wiredDisplayResolutionReceipt_t resolution;
	refPresentationChange_t change;
	const glconfig_t *config = re.GetConfig ? re.GetConfig() : NULL;
	int logicalWidth = 0, logicalHeight = 0;
	int pixelWidth = 0, pixelHeight = 0;
	uint32_t renderWidth, renderHeight;
	if ( !SDL_window || !catalog
			|| s_presentationGeneration == UINT64_MAX
			|| !SDL_GetWindowSize( SDL_window, &logicalWidth, &logicalHeight )
			|| !SDL_GetWindowSizeInPixels( SDL_window, &pixelWidth, &pixelHeight ) ) return;
	renderWidth = config && config->vidWidth > 0
		? (uint32_t)config->vidWidth : (uint32_t)pixelWidth;
	renderHeight = config && config->vidHeight > 0
		? (uint32_t)config->vidHeight : (uint32_t)pixelHeight;
	if ( !WiredDisplay_BuildResolutionReceipt( catalog->generation,
			s_presentationGeneration + 1u, dirty,
			(uint32_t)logicalWidth, (uint32_t)logicalHeight,
			(uint32_t)pixelWidth, (uint32_t)pixelHeight,
			renderWidth, renderHeight, &resolution ) ) return;
	if ( !WiredDisplay_PublishResolutionReceipt( &resolution ) ) return;
	glw_state.window_width = pixelWidth;
	glw_state.window_height = pixelHeight;
	s_presentationGeneration++;
	memset( &change, 0, sizeof( change ) );
	change.schemaVersion = REF_PRESENTATION_CHANGE_SCHEMA_VERSION;
	change.generation = s_presentationGeneration;
	change.catalogGeneration = resolution.catalogGeneration;
	change.changeFlags = resolution.changeFlags;
	change.logicalWidth = resolution.extents.logicalWidth;
	change.logicalHeight = resolution.extents.logicalHeight;
	change.presentationWidth = resolution.extents.presentationWidth;
	change.presentationHeight = resolution.extents.presentationHeight;
	change.renderWidth = resolution.extents.renderWidth;
	change.renderHeight = resolution.extents.renderHeight;
	change.uiWidth = resolution.extents.uiWidth;
	change.uiHeight = resolution.extents.uiHeight;
	Com_Log( SEV_INFO, LOG_CH(ch_client),
		"presentation-resolution schema=%u generation=%llu catalog=%llu flags=0x%x logical=%ux%u presentation=%ux%u render=%ux%u ui=%ux%u\n",
		resolution.schemaVersion,
		(unsigned long long)resolution.surfaceGeneration,
		(unsigned long long)resolution.catalogGeneration, resolution.changeFlags,
		resolution.extents.logicalWidth, resolution.extents.logicalHeight,
		resolution.extents.presentationWidth, resolution.extents.presentationHeight,
		resolution.extents.renderWidth, resolution.extents.renderHeight,
		resolution.extents.uiWidth, resolution.extents.uiHeight );
	if ( re.PresentationChanged ) re.PresentationChanged( &change );
}

void GLimp_DisplayCatalogReconcile( void ) {
	wiredDisplayCatalog_t candidate;
	wiredDisplayProvider_t provider = {
		WIRED_DISPLAY_CATALOG_SCHEMA_VERSION,
		WIRED_DISPLAY_PROVIDER_SDL,
		NULL,
		SdlSnapshot
	};
	uint32_t dirty;
	uint32_t i;
	if ( s_reconciler.pendingFlags == 0u || !SDL_WasInit( SDL_INIT_VIDEO ) ) return;
	dirty = WiredDisplay_ReconcilerTake( &s_reconciler );
	(void)WiredDisplay_CaptureProvider( &provider, &candidate );
	if ( WiredDisplay_PublishCatalog( &candidate ) < 0 ) {
		if ( !WiredDisplay_GetActiveCatalog() )
			WiredDisplay_ReconcilerMark( &s_reconciler, dirty );
		return;
	}
	{
		const wiredDisplayCatalog_t *catalog = WiredDisplay_GetActiveCatalog();
		if ( !catalog ) {
			WiredDisplay_ReconcilerMark( &s_reconciler, dirty );
		return;
		}
		glw_state.monitorCount = (int)catalog->displayCount;
		for ( i = 0u; i < catalog->displayCount; ++i ) {
			if ( catalog->displays[i].flags & WIRED_DISPLAY_FLAG_CURRENT ) {
				if ( catalog->displays[i].desktopWidth != 0u )
					glw_state.desktop_width = (int)catalog->displays[i].desktopWidth;
				if ( catalog->displays[i].desktopHeight != 0u )
					glw_state.desktop_height = (int)catalog->displays[i].desktopHeight;
				break;
			}
		}
		Com_Log( SEV_INFO, LOG_CH(ch_client),
			"display-catalog schema=%u generation=%llu provider=%u displays=%u modes=%u fallback=%u dirty=0x%x\n",
			catalog->schemaVersion, (unsigned long long)catalog->generation,
			(unsigned)catalog->providerKind, catalog->displayCount,
			catalog->modeCount,
			(catalog->flags & WIRED_DISPLAY_CATALOG_FALLBACK) != 0u, dirty );
		NotifyPresentationChanged( dirty, catalog );
	}
}
