// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "cl_display_catalog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static wiredDisplayCatalog_t s_activeCatalog;
static wiredDisplayResolutionReceipt_t s_activeResolutionReceipt;

static const wiredLegacyVideoMode_t s_legacyModes[] = {
	{ "Mode  0: 320x240",             320u,  240u,  1.0f },
	{ "Mode  1: 400x300",             400u,  300u,  1.0f },
	{ "Mode  2: 512x384",             512u,  384u,  1.0f },
	{ "Mode  3: 640x480",             640u,  480u,  1.0f },
	{ "Mode  4: 800x600",             800u,  600u,  1.0f },
	{ "Mode  5: 960x720",             960u,  720u,  1.0f },
	{ "Mode  6: 1024x768",           1024u,  768u,  1.0f },
	{ "Mode  7: 1152x864",           1152u,  864u,  1.0f },
	{ "Mode  8: 1280x1024 (5:4)",    1280u, 1024u,  1.0f },
	{ "Mode  9: 1600x1200",          1600u, 1200u,  1.0f },
	{ "Mode 10: 2048x1536",          2048u, 1536u,  1.0f },
	{ "Mode 11: 856x480 (wide)",      856u,  480u,  1.0f },
	{ "Mode 12: 1280x960",           1280u,  960u,  1.0f },
	{ "Mode 13: 1280x720",           1280u,  720u,  1.0f },
	{ "Mode 14: 1280x800 (16:10)",   1280u,  800u,  1.0f },
	{ "Mode 15: 1366x768",           1366u,  768u,  1.0f },
	{ "Mode 16: 1440x900 (16:10)",   1440u,  900u,  1.0f },
	{ "Mode 17: 1600x900",           1600u,  900u,  1.0f },
	{ "Mode 18: 1680x1050 (16:10)",  1680u, 1050u,  1.0f },
	{ "Mode 19: 1920x1080",          1920u, 1080u,  1.0f },
	{ "Mode 20: 1920x1200 (16:10)",  1920u, 1200u,  1.0f },
	{ "Mode 21: 2560x1080 (21:9)",   2560u, 1080u,  1.0f },
	{ "Mode 22: 3440x1440 (21:9)",   3440u, 1440u,  1.0f },
	{ "Mode 23: 3840x2160",          3840u, 2160u,  1.0f },
	{ "Mode 24: 4096x2160 (DCI 4K)", 4096u, 2160u,  1.0f },
	{ "Mode 25: 2560x1440 (QHD)",    2560u, 1440u,  1.0f },
	{ "Mode 26: 2560x1600 (16:10)",  2560u, 1600u,  1.0f },
	{ "Mode 27: 3840x1600 (24:10)",  3840u, 1600u,  1.0f },
	{ "Mode 28: 5120x1440 (32:9)",   5120u, 1440u,  1.0f },
	{ "Mode 29: 5120x2160 (21:9)",   5120u, 2160u,  1.0f },
	{ "Mode 30: 5120x2880 (5K)",     5120u, 2880u,  1.0f },
	{ "Mode 31: 7680x4320 (8K)",     7680u, 4320u,  1.0f }
};

static uint32_t Gcd( uint32_t a, uint32_t b ) {
	while ( b != 0u ) {
		const uint32_t remainder = a % b;
		a = b;
		b = remainder;
	}
	return a;
}

static void CopyText( char *destination, size_t capacity, const char *source ) {
	if ( !destination || capacity == 0u ) return;
	if ( !source ) source = "";
	strncpy( destination, source, capacity - 1u );
	destination[capacity - 1u] = '\0';
}

static int FractionValid( uint32_t numerator, uint32_t denominator,
		int allowUnspecified ) {
	if ( numerator == 0u || denominator == 0u )
		return allowUnspecified && numerator == 0u && denominator == 0u;
	return 1;
}

static void ReduceFraction( uint32_t *numerator, uint32_t *denominator ) {
	uint32_t divisor;
	if ( !numerator || !denominator || *numerator == 0u || *denominator == 0u )
		return;
	divisor = Gcd( *numerator, *denominator );
	*numerator /= divisor;
	*denominator /= divisor;
}

static int ModeCompare( const void *leftValue, const void *rightValue ) {
	const wiredDisplayMode_t *left = (const wiredDisplayMode_t *)leftValue;
	const wiredDisplayMode_t *right = (const wiredDisplayMode_t *)rightValue;
	uint64_t leftRefresh;
	uint64_t rightRefresh;
	if ( left->displayIndex != right->displayIndex )
		return left->displayIndex < right->displayIndex ? -1 : 1;
	if ( left->width != right->width ) return left->width < right->width ? -1 : 1;
	if ( left->height != right->height ) return left->height < right->height ? -1 : 1;
	if ( left->refreshDenominator == 0u && right->refreshDenominator != 0u ) return -1;
	if ( left->refreshDenominator != 0u && right->refreshDenominator == 0u ) return 1;
	leftRefresh = (uint64_t)left->refreshNumerator * right->refreshDenominator;
	rightRefresh = (uint64_t)right->refreshNumerator * left->refreshDenominator;
	if ( leftRefresh != rightRefresh ) return leftRefresh < rightRefresh ? -1 : 1;
	return 0;
}

static int ModeSameIdentity( const wiredDisplayMode_t *left,
		const wiredDisplayMode_t *right ) {
	return left->displayIndex == right->displayIndex
		&& left->width == right->width && left->height == right->height
		&& left->refreshNumerator == right->refreshNumerator
		&& left->refreshDenominator == right->refreshDenominator;
}

static int CatalogHeaderValid( const wiredDisplayCatalog_t *catalog ) {
	return catalog
		&& catalog->schemaVersion == WIRED_DISPLAY_CATALOG_SCHEMA_VERSION
		&& catalog->providerKind >= WIRED_DISPLAY_PROVIDER_INTERNAL
		&& catalog->providerKind <= WIRED_DISPLAY_PROVIDER_HEADLESS
		&& catalog->displayCount > 0u
		&& catalog->displayCount <= WIRED_DISPLAY_MAX_DISPLAYS
		&& catalog->modeCount <= WIRED_DISPLAY_MAX_MODES;
}

size_t WiredDisplay_LegacyModeCount( void ) {
	return sizeof( s_legacyModes ) / sizeof( s_legacyModes[0] );
}

const wiredLegacyVideoMode_t *WiredDisplay_LegacyModeAt( size_t index ) {
	return index < WiredDisplay_LegacyModeCount() ? &s_legacyModes[index] : NULL;
}

int WiredDisplay_ResolveLegacyMode( int mode, uint32_t desktopWidth,
		uint32_t desktopHeight, uint32_t customWidth, uint32_t customHeight,
		float customPixelAspect, uint32_t *outWidth, uint32_t *outHeight,
		float *outPixelAspect ) {
	const wiredLegacyVideoMode_t *preset;
	if ( !outWidth || !outHeight || !outPixelAspect || mode < -2
		|| mode >= (int)WiredDisplay_LegacyModeCount() ) return 0;
	if ( mode == -2 ) {
		if ( desktopWidth == 0u || desktopHeight == 0u ) mode = 13;
		else {
			*outWidth = desktopWidth; *outHeight = desktopHeight;
			*outPixelAspect = customPixelAspect; return 1;
		}
	}
	if ( mode == -1 ) {
		if ( customWidth == 0u || customHeight == 0u ) return 0;
		*outWidth = customWidth; *outHeight = customHeight;
		*outPixelAspect = customPixelAspect; return 1;
	}
	preset = WiredDisplay_LegacyModeAt( (size_t)mode );
	if ( !preset ) return 0;
	*outWidth = preset->width; *outHeight = preset->height;
	*outPixelAspect = preset->pixelAspect;
	return 1;
}

int WiredDisplay_CatalogNormalize( const wiredDisplayCatalog_t *input,
		wiredDisplayCatalog_t *outCatalog ) {
	wiredDisplayCatalog_t candidate;
	uint32_t i;
	uint32_t writeCount = 0u;
	if ( !outCatalog || !CatalogHeaderValid( input ) ) return 0;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	candidate.generation = input->generation ? input->generation : 1u;
	candidate.providerKind = input->providerKind;
	candidate.providerCapabilities = input->providerCapabilities;
	candidate.flags = input->flags;
	candidate.displayCount = input->displayCount;
	for ( i = 0u; i < input->displayCount; ++i ) {
		candidate.displays[i] = input->displays[i];
		candidate.displays[i].schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
		candidate.displays[i].firstMode = 0u;
		candidate.displays[i].modeCount = 0u;
		candidate.displays[i].persistentKey[WIRED_DISPLAY_KEY_MAX - 1u] = '\0';
		candidate.displays[i].name[WIRED_DISPLAY_NAME_MAX - 1u] = '\0';
		if ( candidate.displays[i].persistentKey[0] == '\0' ) return 0;
		if ( !FractionValid( candidate.displays[i].densityNumerator,
			candidate.displays[i].densityDenominator, 0 ) ) {
			candidate.displays[i].densityNumerator = 1u;
			candidate.displays[i].densityDenominator = 1u;
		}
		ReduceFraction( &candidate.displays[i].densityNumerator,
			&candidate.displays[i].densityDenominator );
	}
	for ( i = 0u; i < input->modeCount; ++i ) {
		wiredDisplayMode_t mode = input->modes[i];
		if ( mode.displayIndex >= candidate.displayCount
			|| mode.width == 0u || mode.height == 0u
			|| mode.width > 32768u || mode.height > 32768u
			|| !FractionValid( mode.refreshNumerator,
				mode.refreshDenominator, 1 ) ) continue;
		if ( !FractionValid( mode.densityNumerator, mode.densityDenominator, 0 ) ) {
			mode.densityNumerator = candidate.displays[mode.displayIndex].densityNumerator;
			mode.densityDenominator = candidate.displays[mode.displayIndex].densityDenominator;
		}
		mode.schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
		ReduceFraction( &mode.refreshNumerator, &mode.refreshDenominator );
		ReduceFraction( &mode.densityNumerator, &mode.densityDenominator );
		candidate.modes[writeCount++] = mode;
	}
	if ( writeCount == 0u ) return 0;
	qsort( candidate.modes, writeCount, sizeof( candidate.modes[0] ), ModeCompare );
	candidate.modeCount = 0u;
	for ( i = 0u; i < writeCount; ++i ) {
		wiredDisplayMode_t *previous = candidate.modeCount
			? &candidate.modes[candidate.modeCount - 1u] : NULL;
		if ( previous && ModeSameIdentity( previous, &candidate.modes[i] ) ) {
			previous->flags |= candidate.modes[i].flags;
			if ( previous->legacyModeIndex < 0
				&& candidate.modes[i].legacyModeIndex >= 0 )
				previous->legacyModeIndex = candidate.modes[i].legacyModeIndex;
			continue;
		}
		candidate.modes[candidate.modeCount++] = candidate.modes[i];
	}
	for ( i = 0u; i < candidate.modeCount; ++i ) {
		wiredDisplayDescriptor_t *display =
			&candidate.displays[candidate.modes[i].displayIndex];
		if ( display->modeCount == 0u ) display->firstMode = i;
		display->modeCount++;
	}
	*outCatalog = candidate;
	return 1;
}

int WiredDisplay_CatalogEquivalent( const wiredDisplayCatalog_t *left,
		const wiredDisplayCatalog_t *right ) {
	uint32_t i;
	if ( !CatalogHeaderValid( left ) || !CatalogHeaderValid( right )
		|| left->providerKind != right->providerKind
		|| left->providerCapabilities != right->providerCapabilities
		|| left->flags != right->flags
		|| left->displayCount != right->displayCount
		|| left->modeCount != right->modeCount ) return 0;
	for ( i = 0u; i < left->displayCount; ++i ) {
		const wiredDisplayDescriptor_t *a = &left->displays[i];
		const wiredDisplayDescriptor_t *b = &right->displays[i];
		if ( a->sessionHandle != b->sessionHandle
			|| strcmp( a->persistentKey, b->persistentKey ) != 0
			|| strcmp( a->name, b->name ) != 0
			|| a->desktopWidth != b->desktopWidth
			|| a->desktopHeight != b->desktopHeight
			|| a->densityNumerator != b->densityNumerator
			|| a->densityDenominator != b->densityDenominator
			|| a->flags != b->flags
			|| a->firstMode != b->firstMode
			|| a->modeCount != b->modeCount ) return 0;
	}
	for ( i = 0u; i < left->modeCount; ++i ) {
		const wiredDisplayMode_t *a = &left->modes[i];
		const wiredDisplayMode_t *b = &right->modes[i];
		if ( a->displayIndex != b->displayIndex
			|| a->width != b->width || a->height != b->height
			|| a->refreshNumerator != b->refreshNumerator
			|| a->refreshDenominator != b->refreshDenominator
			|| a->densityNumerator != b->densityNumerator
			|| a->densityDenominator != b->densityDenominator
			|| a->flags != b->flags
			|| a->legacyModeIndex != b->legacyModeIndex ) return 0;
	}
	return 1;
}

void WiredDisplay_BuildFallbackCatalog( wiredDisplayProviderKind_t kind,
		wiredDisplayCatalog_t *outCatalog ) {
	wiredDisplayCatalog_t candidate;
	size_t i;
	if ( !outCatalog ) return;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	candidate.generation = 1u;
	candidate.providerKind = kind;
	candidate.flags = WIRED_DISPLAY_CATALOG_FALLBACK;
	candidate.displayCount = 1u;
	candidate.modeCount = (uint32_t)WiredDisplay_LegacyModeCount();
	candidate.displays[0].schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	CopyText( candidate.displays[0].persistentKey,
		sizeof( candidate.displays[0].persistentKey ), "internal:primary" );
	CopyText( candidate.displays[0].name,
		sizeof( candidate.displays[0].name ), "Internal fallback" );
	candidate.displays[0].desktopWidth = 1280u;
	candidate.displays[0].desktopHeight = 720u;
	candidate.displays[0].densityNumerator = 1u;
	candidate.displays[0].densityDenominator = 1u;
	candidate.displays[0].flags = WIRED_DISPLAY_FLAG_PRIMARY
		| WIRED_DISPLAY_FLAG_CURRENT;
	for ( i = 0u; i < WiredDisplay_LegacyModeCount(); ++i ) {
		candidate.modes[i].schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
		candidate.modes[i].displayIndex = 0u;
		candidate.modes[i].width = s_legacyModes[i].width;
		candidate.modes[i].height = s_legacyModes[i].height;
		candidate.modes[i].densityNumerator = 1u;
		candidate.modes[i].densityDenominator = 1u;
		candidate.modes[i].legacyModeIndex = (int32_t)i;
		if ( i == 13u ) candidate.modes[i].flags =
			WIRED_DISPLAY_MODE_DESKTOP | WIRED_DISPLAY_MODE_CURRENT;
	}
	(void)WiredDisplay_CatalogNormalize( &candidate, outCatalog );
}

int WiredDisplay_CaptureProvider( const wiredDisplayProvider_t *provider,
		wiredDisplayCatalog_t *outCatalog ) {
	wiredDisplayCatalog_t raw;
	wiredDisplayProviderKind_t fallbackKind = WIRED_DISPLAY_PROVIDER_HEADLESS;
	if ( !outCatalog ) return 0;
	if ( provider && provider->kind >= WIRED_DISPLAY_PROVIDER_INTERNAL
		&& provider->kind <= WIRED_DISPLAY_PROVIDER_HEADLESS ) fallbackKind = provider->kind;
	memset( &raw, 0, sizeof( raw ) );
	if ( !provider || provider->schemaVersion != WIRED_DISPLAY_CATALOG_SCHEMA_VERSION
		|| !provider->snapshot || !provider->snapshot( provider->context, &raw )
		|| !WiredDisplay_CatalogNormalize( &raw, outCatalog ) ) {
		WiredDisplay_BuildFallbackCatalog( fallbackKind, outCatalog );
		return 0;
	}
	return 1;
}

int WiredDisplay_ExtentDomainsValid(
		const wiredDisplayExtentDomains_t *extents ) {
	if ( !extents ) return 0;
#define VALID_EXTENT_PAIR(w, h, limit) ((w) > 0u && (h) > 0u \
	&& (w) <= (limit) && (h) <= (limit))
	return VALID_EXTENT_PAIR( extents->logicalWidth, extents->logicalHeight, 16384u )
		&& VALID_EXTENT_PAIR( extents->presentationWidth,
			extents->presentationHeight, 32768u )
		&& VALID_EXTENT_PAIR( extents->renderWidth, extents->renderHeight, 32768u )
		&& VALID_EXTENT_PAIR( extents->uiWidth, extents->uiHeight, 32768u );
#undef VALID_EXTENT_PAIR
}

int WiredDisplay_FormatModeValue( const wiredDisplayMode_t *mode,
		char *buffer, size_t capacity ) {
	int written;
	if ( !mode || !buffer || capacity == 0u || mode->width == 0u
		|| mode->height == 0u
		|| !FractionValid( mode->refreshNumerator,
			mode->refreshDenominator, 1 ) ) return 0;
	if ( mode->refreshNumerator != 0u )
		written = snprintf( buffer, capacity, "%ux%u@%u/%u", mode->width,
			mode->height, mode->refreshNumerator, mode->refreshDenominator );
	else
		written = snprintf( buffer, capacity, "%ux%u", mode->width, mode->height );
	return written > 0 && (size_t)written < capacity;
}

int WiredDisplay_ParseModeValue( const char *value, uint32_t *outWidth,
		uint32_t *outHeight, uint32_t *outRefreshNumerator,
		uint32_t *outRefreshDenominator ) {
	unsigned int width = 0u, height = 0u, numerator = 0u, denominator = 0u;
	char trailing = '\0';
	int fields;
	if ( !value || !outWidth || !outHeight || !outRefreshNumerator
		|| !outRefreshDenominator ) return 0;
	fields = sscanf( value, "%ux%u@%u/%u%c", &width, &height, &numerator,
		&denominator, &trailing );
	if ( fields != 4 ) {
		fields = sscanf( value, "%ux%u%c", &width, &height, &trailing );
		if ( fields != 2 ) return 0;
		numerator = denominator = 0u;
	}
	if ( width == 0u || height == 0u || width > 32768u || height > 32768u
		|| !FractionValid( numerator, denominator, 1 ) ) return 0;
	ReduceFraction( &numerator, &denominator );
	*outWidth = width;
	*outHeight = height;
	*outRefreshNumerator = numerator;
	*outRefreshDenominator = denominator;
	return 1;
}

int WiredDisplay_FormatRefreshValue( uint32_t numerator, uint32_t denominator,
		char *buffer, size_t capacity ) {
	int written;
	if ( !buffer || capacity == 0u ) return 0;
	if ( numerator == 0u && denominator == 0u ) {
		written = snprintf( buffer, capacity, "auto" );
	} else {
		if ( !FractionValid( numerator, denominator, 0 ) ) return 0;
		ReduceFraction( &numerator, &denominator );
		written = snprintf( buffer, capacity, "%u/%u", numerator, denominator );
	}
	return written > 0 && (size_t)written < capacity;
}

int WiredDisplay_ParseRefreshValue( const char *value, uint32_t *outNumerator,
		uint32_t *outDenominator ) {
	unsigned int numerator = 0u, denominator = 0u;
	char trailing = '\0';
	if ( !value || !outNumerator || !outDenominator ) return 0;
	if ( value[0] == '\0' || strcmp( value, "auto" ) == 0 ) {
		*outNumerator = *outDenominator = 0u;
		return 1;
	}
	if ( sscanf( value, "%u/%u%c", &numerator, &denominator, &trailing ) != 2
			|| !FractionValid( numerator, denominator, 0 ) ) return 0;
	ReduceFraction( &numerator, &denominator );
	*outNumerator = numerator;
	*outDenominator = denominator;
	return 1;
}

int WiredDisplay_BuildResolutionReceipt( uint64_t catalogGeneration,
		uint64_t surfaceGeneration, uint32_t changeFlags,
		uint32_t logicalWidth, uint32_t logicalHeight,
		uint32_t presentationWidth, uint32_t presentationHeight,
		uint32_t renderWidth, uint32_t renderHeight,
		wiredDisplayResolutionReceipt_t *outReceipt ) {
	wiredDisplayResolutionReceipt_t receipt;
	if ( !outReceipt || catalogGeneration == 0u || surfaceGeneration == 0u ) return 0;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	receipt.catalogGeneration = catalogGeneration;
	receipt.surfaceGeneration = surfaceGeneration;
	receipt.changeFlags = changeFlags;
	receipt.extents.logicalWidth = logicalWidth;
	receipt.extents.logicalHeight = logicalHeight;
	receipt.extents.presentationWidth = presentationWidth;
	receipt.extents.presentationHeight = presentationHeight;
	receipt.extents.renderWidth = renderWidth ? renderWidth : presentationWidth;
	receipt.extents.renderHeight = renderHeight ? renderHeight : presentationHeight;
	/* UI composition is an output-domain concern, not a render-scale target. */
	receipt.extents.uiWidth = presentationWidth;
	receipt.extents.uiHeight = presentationHeight;
	if ( !WiredDisplay_ExtentDomainsValid( &receipt.extents ) ) return 0;
	*outReceipt = receipt;
	return 1;
}

void WiredDisplay_ReconcilerInit( wiredDisplayReconciler_t *reconciler,
		uint32_t initialFlags ) {
	if ( reconciler ) reconciler->pendingFlags = initialFlags;
}

void WiredDisplay_ReconcilerMark( wiredDisplayReconciler_t *reconciler,
		uint32_t flags ) {
	if ( reconciler ) reconciler->pendingFlags |= flags;
}

uint32_t WiredDisplay_ReconcilerTake( wiredDisplayReconciler_t *reconciler ) {
	uint32_t flags;
	if ( !reconciler ) return 0u;
	flags = reconciler->pendingFlags;
	reconciler->pendingFlags = 0u;
	return flags;
}

wiredPresentationAction_t WiredDisplay_PresentationAction( uint32_t changeFlags,
		int presentSupported ) {
	if ( !presentSupported ) return WIRED_PRESENTATION_ACTION_RESELECT_DEVICE;
	if ( changeFlags & (WIRED_DISPLAY_CHANGE_TOPOLOGY
			| WIRED_DISPLAY_CHANGE_ACTIVE_OUTPUT | WIRED_DISPLAY_CHANGE_SCALE
			| WIRED_DISPLAY_CHANGE_FULLSCREEN | WIRED_DISPLAY_CHANGE_EXTENT) )
		return WIRED_PRESENTATION_ACTION_REBUILD;
	if ( changeFlags & WIRED_DISPLAY_CHANGE_COLOR )
		return WIRED_PRESENTATION_ACTION_REFRESH_METADATA;
	return WIRED_PRESENTATION_ACTION_NONE;
}

const wiredDisplayCatalog_t *WiredDisplay_GetActiveCatalog( void ) {
	return s_activeCatalog.displayCount != 0u ? &s_activeCatalog : NULL;
}

int WiredDisplay_PublishCatalog( const wiredDisplayCatalog_t *catalog ) {
	wiredDisplayCatalog_t candidate;
	uint64_t nextGeneration;
	if ( !WiredDisplay_CatalogNormalize( catalog, &candidate ) ) return -1;
	if ( s_activeCatalog.displayCount != 0u
		&& WiredDisplay_CatalogEquivalent( &s_activeCatalog, &candidate ) ) return 0;
	if ( s_activeCatalog.generation == UINT64_MAX ) return -1;
	nextGeneration = s_activeCatalog.displayCount != 0u
		? s_activeCatalog.generation + 1u : 1u;
	if ( nextGeneration == 0u ) return -1;
	candidate.generation = nextGeneration;
	s_activeCatalog = candidate;
	return 1;
}

const wiredDisplayResolutionReceipt_t *WiredDisplay_GetActiveResolutionReceipt( void ) {
	return s_activeResolutionReceipt.surfaceGeneration != 0u
		? &s_activeResolutionReceipt : NULL;
}

int WiredDisplay_PublishResolutionReceipt(
		const wiredDisplayResolutionReceipt_t *receipt ) {
	if ( !receipt || receipt->schemaVersion != WIRED_DISPLAY_CATALOG_SCHEMA_VERSION
			|| receipt->catalogGeneration == 0u || receipt->surfaceGeneration == 0u
			|| !WiredDisplay_ExtentDomainsValid( &receipt->extents )
			|| (s_activeResolutionReceipt.surfaceGeneration != 0u
				&& receipt->surfaceGeneration
				<= s_activeResolutionReceipt.surfaceGeneration) ) return 0;
	s_activeResolutionReceipt = *receipt;
	return 1;
}

int WiredDisplay_BuildWebCurrentScreen( uint32_t cssWidth, uint32_t cssHeight,
		uint32_t pixelWidth, uint32_t pixelHeight, uint32_t densityNumerator,
		uint32_t densityDenominator, int fullscreenAvailable,
		wiredDisplayCatalog_t *outCatalog,
		wiredDisplayExtentDomains_t *outExtents ) {
	wiredDisplayCatalog_t raw;
	if ( !outCatalog || !outExtents || cssWidth == 0u || cssHeight == 0u
		|| pixelWidth == 0u || pixelHeight == 0u
		|| !FractionValid( densityNumerator, densityDenominator, 0 ) ) {
		if ( outCatalog ) WiredDisplay_BuildFallbackCatalog(
			WIRED_DISPLAY_PROVIDER_WEB, outCatalog );
		return 0;
	}
	memset( &raw, 0, sizeof( raw ) );
	raw.schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	raw.generation = 1u;
	raw.providerKind = WIRED_DISPLAY_PROVIDER_WEB;
	raw.providerCapabilities = WIRED_DISPLAY_CAP_DENSITY_SCALE
		| WIRED_DISPLAY_CAP_PERMISSION_GATED_SCREENS
		| (fullscreenAvailable ? WIRED_DISPLAY_CAP_FULLSCREEN : 0u);
	raw.displayCount = 1u;
	raw.modeCount = 1u;
	raw.displays[0].schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	CopyText( raw.displays[0].persistentKey,
		sizeof( raw.displays[0].persistentKey ), "web:current-screen" );
	CopyText( raw.displays[0].name,
		sizeof( raw.displays[0].name ), "Browser current screen" );
	raw.displays[0].desktopWidth = pixelWidth;
	raw.displays[0].desktopHeight = pixelHeight;
	raw.displays[0].densityNumerator = densityNumerator;
	raw.displays[0].densityDenominator = densityDenominator;
	raw.displays[0].flags = WIRED_DISPLAY_FLAG_PRIMARY | WIRED_DISPLAY_FLAG_CURRENT;
	raw.modes[0].schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	raw.modes[0].displayIndex = 0u;
	raw.modes[0].width = pixelWidth;
	raw.modes[0].height = pixelHeight;
	raw.modes[0].densityNumerator = densityNumerator;
	raw.modes[0].densityDenominator = densityDenominator;
	raw.modes[0].flags = WIRED_DISPLAY_MODE_CURRENT;
	raw.modes[0].legacyModeIndex = -1;
	if ( !WiredDisplay_CatalogNormalize( &raw, outCatalog ) ) return 0;
	outExtents->logicalWidth = cssWidth;
	outExtents->logicalHeight = cssHeight;
	outExtents->presentationWidth = pixelWidth;
	outExtents->presentationHeight = pixelHeight;
	outExtents->renderWidth = pixelWidth;
	outExtents->renderHeight = pixelHeight;
	outExtents->uiWidth = pixelWidth;
	outExtents->uiHeight = pixelHeight;
	return WiredDisplay_ExtentDomainsValid( outExtents );
}

static int SelectorScore( const wiredDisplaySelector_t *selector,
		const wiredDisplayDescriptor_t *display ) {
	int score = 0;
	if ( selector->persistentKey[0] != '\0'
		&& strcmp( selector->persistentKey, display->persistentKey ) == 0 ) return 100;
	if ( selector->name[0] != '\0' && strcmp( selector->name, display->name ) == 0 )
		score += 10;
	if ( selector->desktopWidth != 0u && selector->desktopHeight != 0u
		&& selector->desktopWidth == display->desktopWidth
		&& selector->desktopHeight == display->desktopHeight ) score += 5;
	if ( selector->densityNumerator != 0u && selector->densityDenominator != 0u
		&& (uint64_t)selector->densityNumerator * display->densityDenominator
		== (uint64_t)display->densityNumerator * selector->densityDenominator ) score += 2;
	return score;
}

static const wiredDisplayMode_t *DefaultModeForDisplay(
		const wiredDisplayCatalog_t *catalog, uint32_t displayIndex ) {
	const wiredDisplayMode_t *fallback = NULL;
	uint32_t i;
	for ( i = 0u; i < catalog->modeCount; ++i ) {
		const wiredDisplayMode_t *mode = &catalog->modes[i];
		if ( mode->displayIndex != displayIndex ) continue;
		if ( !fallback ) fallback = mode;
		if ( mode->flags & WIRED_DISPLAY_MODE_CURRENT ) return mode;
		if ( mode->flags & WIRED_DISPLAY_MODE_DESKTOP ) fallback = mode;
	}
	return fallback;
}

int WiredDisplay_ResolveRequest( const wiredDisplayCatalog_t *catalog,
		const wiredDisplayRequest_t *request,
		wiredDisplayEffectiveState_t *outState ) {
	uint32_t displayIndex = UINT32_MAX;
	uint32_t i;
	int bestScore = 0;
	const wiredDisplayMode_t *selectedMode = NULL;
	wiredWindowPolicy_t effectivePolicy;
	if ( !CatalogHeaderValid( catalog ) || catalog->modeCount == 0u
		|| !request || !outState
		|| request->schemaVersion != WIRED_DISPLAY_CATALOG_SCHEMA_VERSION
		|| request->display.schemaVersion != WIRED_DISPLAY_CATALOG_SCHEMA_VERSION
		|| request->windowPolicy < WIRED_WINDOW_POLICY_WINDOWED
		|| request->windowPolicy > WIRED_WINDOW_POLICY_EXCLUSIVE
		|| !FractionValid( request->refreshNumerator,
			request->refreshDenominator, 1 ) ) return 0;
	for ( i = 0u; i < catalog->displayCount; ++i ) {
		const int score = SelectorScore( &request->display, &catalog->displays[i] );
		if ( score > bestScore && catalog->displays[i].modeCount != 0u ) {
			bestScore = score; displayIndex = i;
		}
	}
	if ( displayIndex == UINT32_MAX ) {
		for ( i = 0u; i < catalog->displayCount; ++i )
			if ( catalog->displays[i].modeCount != 0u
				&& (catalog->displays[i].flags & WIRED_DISPLAY_FLAG_CURRENT) ) {
				displayIndex = i; break;
			}
	}
	if ( displayIndex == UINT32_MAX ) {
		for ( i = 0u; i < catalog->displayCount; ++i )
			if ( catalog->displays[i].modeCount != 0u
				&& (catalog->displays[i].flags & WIRED_DISPLAY_FLAG_PRIMARY) ) {
				displayIndex = i; break;
			}
	}
	if ( displayIndex == UINT32_MAX ) {
		for ( i = 0u; i < catalog->displayCount; ++i )
			if ( catalog->displays[i].modeCount != 0u ) { displayIndex = i; break; }
	}
	if ( displayIndex == UINT32_MAX ) return 0;
	for ( i = 0u; i < catalog->modeCount; ++i ) {
		const wiredDisplayMode_t *mode = &catalog->modes[i];
		if ( mode->displayIndex != displayIndex
			|| request->width == 0u || request->height == 0u
			|| mode->width != request->width || mode->height != request->height ) continue;
		if ( request->refreshDenominator != 0u
			&& mode->refreshDenominator == 0u ) continue;
		if ( request->refreshDenominator != 0u
			&& (uint64_t)mode->refreshNumerator * request->refreshDenominator
			!= (uint64_t)request->refreshNumerator * mode->refreshDenominator ) continue;
		if ( request->windowPolicy == WIRED_WINDOW_POLICY_EXCLUSIVE
			&& !(mode->flags & WIRED_DISPLAY_MODE_EXCLUSIVE) ) continue;
		selectedMode = mode;
		if ( mode->flags & WIRED_DISPLAY_MODE_CURRENT ) break;
	}
	effectivePolicy = request->windowPolicy;
	if ( !selectedMode ) {
		selectedMode = DefaultModeForDisplay( catalog, displayIndex );
		if ( request->windowPolicy == WIRED_WINDOW_POLICY_EXCLUSIVE )
			effectivePolicy = WIRED_WINDOW_POLICY_BORDERLESS;
	}
	if ( !selectedMode ) return 0;
	memset( outState, 0, sizeof( *outState ) );
	outState->schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	outState->generation = catalog->generation;
	outState->requested = *request;
	outState->effectiveWindowPolicy = effectivePolicy;
	outState->display = catalog->displays[displayIndex];
	outState->mode = *selectedMode;
	outState->extents.logicalWidth = selectedMode->width;
	outState->extents.logicalHeight = selectedMode->height;
	outState->extents.presentationWidth = selectedMode->width;
	outState->extents.presentationHeight = selectedMode->height;
	outState->extents.renderWidth = selectedMode->width;
	outState->extents.renderHeight = selectedMode->height;
	outState->extents.uiWidth = selectedMode->width;
	outState->extents.uiHeight = selectedMode->height;
	outState->usedFallback = (catalog->flags & WIRED_DISPLAY_CATALOG_FALLBACK) != 0u
		|| bestScore == 0;
	return WiredDisplay_ExtentDomainsValid( &outState->extents );
}
