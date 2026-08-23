// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_CL_DISPLAY_CATALOG_H
#define WIRED_CL_DISPLAY_CATALOG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIRED_DISPLAY_CATALOG_SCHEMA_VERSION 1u
#define WIRED_DISPLAY_MAX_DISPLAYS 16u
#define WIRED_DISPLAY_MAX_MODES 256u
#define WIRED_DISPLAY_KEY_MAX 64u
#define WIRED_DISPLAY_NAME_MAX 96u

typedef enum {
	WIRED_DISPLAY_PROVIDER_INTERNAL = 1,
	WIRED_DISPLAY_PROVIDER_SDL,
	WIRED_DISPLAY_PROVIDER_WEB,
	WIRED_DISPLAY_PROVIDER_HEADLESS
} wiredDisplayProviderKind_t;

enum {
	WIRED_DISPLAY_CAP_MULTIPLE_DISPLAYS = 1u << 0,
	WIRED_DISPLAY_CAP_EXCLUSIVE_MODES = 1u << 1,
	WIRED_DISPLAY_CAP_DENSITY_SCALE = 1u << 2,
	WIRED_DISPLAY_CAP_FULLSCREEN = 1u << 3,
	WIRED_DISPLAY_CAP_PERMISSION_GATED_SCREENS = 1u << 4
};

enum {
	WIRED_DISPLAY_FLAG_PRIMARY = 1u << 0,
	WIRED_DISPLAY_FLAG_CURRENT = 1u << 1
};

enum {
	WIRED_DISPLAY_MODE_DESKTOP = 1u << 0,
	WIRED_DISPLAY_MODE_CURRENT = 1u << 1,
	WIRED_DISPLAY_MODE_EXCLUSIVE = 1u << 2
};

enum {
	WIRED_DISPLAY_CATALOG_FALLBACK = 1u << 0
};

/* Backend-neutral invalidation reasons. Providers may OR many events into one
 * frame-boundary reconciliation; consumers act on the resulting generation. */
enum {
	WIRED_DISPLAY_CHANGE_TOPOLOGY = 1u << 0,
	WIRED_DISPLAY_CHANGE_ACTIVE_OUTPUT = 1u << 1,
	WIRED_DISPLAY_CHANGE_SCALE = 1u << 2,
	WIRED_DISPLAY_CHANGE_COLOR = 1u << 3,
	WIRED_DISPLAY_CHANGE_FULLSCREEN = 1u << 4,
	WIRED_DISPLAY_CHANGE_EXTENT = 1u << 5
};

typedef enum {
	WIRED_WINDOW_POLICY_WINDOWED = 1,
	WIRED_WINDOW_POLICY_BORDERLESS,
	WIRED_WINDOW_POLICY_EXCLUSIVE
} wiredWindowPolicy_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t sessionHandle;
	char persistentKey[WIRED_DISPLAY_KEY_MAX];
	char name[WIRED_DISPLAY_NAME_MAX];
	uint32_t desktopWidth;
	uint32_t desktopHeight;
	uint32_t densityNumerator;
	uint32_t densityDenominator;
	uint32_t flags;
	uint32_t firstMode;
	uint32_t modeCount;
} wiredDisplayDescriptor_t;

typedef struct {
	uint32_t schemaVersion;
	uint32_t displayIndex;
	uint32_t width;
	uint32_t height;
	uint32_t refreshNumerator;
	uint32_t refreshDenominator;
	uint32_t densityNumerator;
	uint32_t densityDenominator;
	uint32_t flags;
	int32_t legacyModeIndex;
} wiredDisplayMode_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t generation;
	wiredDisplayProviderKind_t providerKind;
	uint32_t providerCapabilities;
	uint32_t flags;
	uint32_t displayCount;
	uint32_t modeCount;
	wiredDisplayDescriptor_t displays[WIRED_DISPLAY_MAX_DISPLAYS];
	wiredDisplayMode_t modes[WIRED_DISPLAY_MAX_MODES];
} wiredDisplayCatalog_t;

typedef int (*wiredDisplayProviderSnapshotFn)( void *context,
	wiredDisplayCatalog_t *outCatalog );

typedef struct {
	uint32_t schemaVersion;
	wiredDisplayProviderKind_t kind;
	void *context;
	wiredDisplayProviderSnapshotFn snapshot;
} wiredDisplayProvider_t;

/* Persistent config identity. sessionHandle and provider order never enter it. */
typedef struct {
	uint32_t schemaVersion;
	char persistentKey[WIRED_DISPLAY_KEY_MAX];
	char name[WIRED_DISPLAY_NAME_MAX];
	uint32_t desktopWidth;
	uint32_t desktopHeight;
	uint32_t densityNumerator;
	uint32_t densityDenominator;
} wiredDisplaySelector_t;

typedef struct {
	uint32_t schemaVersion;
	wiredDisplaySelector_t display;
	wiredWindowPolicy_t windowPolicy;
	uint32_t width;
	uint32_t height;
	uint32_t refreshNumerator;
	uint32_t refreshDenominator;
} wiredDisplayRequest_t;

typedef struct {
	uint32_t logicalWidth;
	uint32_t logicalHeight;
	uint32_t presentationWidth;
	uint32_t presentationHeight;
	uint32_t renderWidth;
	uint32_t renderHeight;
	uint32_t uiWidth;
	uint32_t uiHeight;
} wiredDisplayExtentDomains_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t catalogGeneration;
	uint64_t surfaceGeneration;
	uint32_t changeFlags;
	wiredDisplayExtentDomains_t extents;
} wiredDisplayResolutionReceipt_t;

typedef struct {
	uint32_t pendingFlags;
} wiredDisplayReconciler_t;

typedef enum {
	WIRED_PRESENTATION_ACTION_NONE = 0,
	WIRED_PRESENTATION_ACTION_REFRESH_METADATA,
	WIRED_PRESENTATION_ACTION_REBUILD,
	WIRED_PRESENTATION_ACTION_RESELECT_DEVICE
} wiredPresentationAction_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t generation;
	wiredDisplayRequest_t requested;
	wiredWindowPolicy_t effectiveWindowPolicy;
	wiredDisplayDescriptor_t display;
	wiredDisplayMode_t mode;
	wiredDisplayExtentDomains_t extents;
	uint32_t usedFallback;
} wiredDisplayEffectiveState_t;

typedef struct {
	const char *description;
	uint32_t width;
	uint32_t height;
	float pixelAspect;
} wiredLegacyVideoMode_t;

size_t WiredDisplay_LegacyModeCount( void );
const wiredLegacyVideoMode_t *WiredDisplay_LegacyModeAt( size_t index );
int WiredDisplay_ResolveLegacyMode( int mode, uint32_t desktopWidth,
	uint32_t desktopHeight, uint32_t customWidth, uint32_t customHeight,
	float customPixelAspect, uint32_t *outWidth, uint32_t *outHeight,
	float *outPixelAspect );

int WiredDisplay_CatalogNormalize( const wiredDisplayCatalog_t *input,
	wiredDisplayCatalog_t *outCatalog );
int WiredDisplay_CatalogEquivalent( const wiredDisplayCatalog_t *left,
	const wiredDisplayCatalog_t *right );
void WiredDisplay_BuildFallbackCatalog( wiredDisplayProviderKind_t kind,
	wiredDisplayCatalog_t *outCatalog );
int WiredDisplay_CaptureProvider( const wiredDisplayProvider_t *provider,
	wiredDisplayCatalog_t *outCatalog );
int WiredDisplay_BuildWebCurrentScreen( uint32_t cssWidth, uint32_t cssHeight,
	uint32_t pixelWidth, uint32_t pixelHeight, uint32_t densityNumerator,
	uint32_t densityDenominator, int fullscreenAvailable,
	wiredDisplayCatalog_t *outCatalog,
	wiredDisplayExtentDomains_t *outExtents );
int WiredDisplay_ResolveRequest( const wiredDisplayCatalog_t *catalog,
	const wiredDisplayRequest_t *request,
	wiredDisplayEffectiveState_t *outState );
int WiredDisplay_ExtentDomainsValid(
	const wiredDisplayExtentDomains_t *extents );
int WiredDisplay_FormatModeValue( const wiredDisplayMode_t *mode,
	char *buffer, size_t capacity );
int WiredDisplay_ParseModeValue( const char *value, uint32_t *outWidth,
	uint32_t *outHeight, uint32_t *outRefreshNumerator,
	uint32_t *outRefreshDenominator );
int WiredDisplay_FormatRefreshValue( uint32_t numerator, uint32_t denominator,
	char *buffer, size_t capacity );
int WiredDisplay_ParseRefreshValue( const char *value, uint32_t *outNumerator,
	uint32_t *outDenominator );
int WiredDisplay_BuildResolutionReceipt( uint64_t catalogGeneration,
	uint64_t surfaceGeneration, uint32_t changeFlags,
	uint32_t logicalWidth, uint32_t logicalHeight,
	uint32_t presentationWidth, uint32_t presentationHeight,
	uint32_t renderWidth, uint32_t renderHeight,
	wiredDisplayResolutionReceipt_t *outReceipt );
void WiredDisplay_ReconcilerInit( wiredDisplayReconciler_t *reconciler,
	uint32_t initialFlags );
void WiredDisplay_ReconcilerMark( wiredDisplayReconciler_t *reconciler,
	uint32_t flags );
uint32_t WiredDisplay_ReconcilerTake( wiredDisplayReconciler_t *reconciler );
wiredPresentationAction_t WiredDisplay_PresentationAction( uint32_t changeFlags,
	int presentSupported );

/* Main-thread runtime publication seam. Providers publish normalized snapshots;
 * client/UI/RAL consumers observe one immutable generation at a time. */
const wiredDisplayCatalog_t *WiredDisplay_GetActiveCatalog( void );
int WiredDisplay_PublishCatalog( const wiredDisplayCatalog_t *catalog );
const wiredDisplayResolutionReceipt_t *WiredDisplay_GetActiveResolutionReceipt( void );
int WiredDisplay_PublishResolutionReceipt(
	const wiredDisplayResolutionReceipt_t *receipt );

#ifdef __cplusplus
}
#endif

#endif
