// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_module.h"
#include "tr_public.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	refimport_t imports;
	refexport_t exports;
	ralMetalCore_t *core;
	ralMetalCoreReceipt_t coreReceipt;
	ralPresentationHostReceipt_t hostReceipt;
	ralPresentationSurfaceBorrow_t surfaceBorrow;
	ralMetalPresent_t *presentation;
	ralMetalPresentLayerReceipt_t presentationReceipt;
	ralColorOutputReceipt_t colorOutputReceipt;
	ralFrameShell_t frameShell;
	ralFrameShellReceipt_t frameReceipt;
	ralMetalModuleFrameReceipt_t published;
	glconfig_t config;
	uint64_t moduleGeneration;
	uint64_t nextGeneration;
	qboolean loaded;
	qboolean registered;
	qboolean frameOpen;
} wiredMetalModuleState_t;

#ifdef __cplusplus
#define WIRED_METAL_MODULE_EXPORT extern "C" Q_EXPORT
#else
#define WIRED_METAL_MODULE_EXPORT Q_EXPORT
#endif

static wiredMetalModuleState_t s_module;
static uint64_t s_moduleCounter;

static qboolean ModuleReceiptValid( const ralMetalModuleFrameReceipt_t *receipt ) {
	return ( receipt && receipt->schemaVersion == RAL_METAL_MODULE_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_METAL
		&& receipt->moduleGeneration != 0u && receipt->moduleGeneration != UINT64_MAX
		&& receipt->frameGeneration != 0u && receipt->frameGeneration != UINT64_MAX
		&& receipt->frame.backendType == RAL_BACKEND_METAL
		&& receipt->frame.ownerGeneration == receipt->moduleGeneration
		&& receipt->frame.frameGeneration == receipt->frameGeneration
		&& receipt->frame.state == RAL_FRAME_SHELL_PRESENTED
		&& Ral_FrameShellReceiptExact( &receipt->frame, &receipt->frame )
		&& Ral_PresentationHostReceiptExact( &receipt->host, &receipt->host )
		&& Ral_PresentationSurfaceBorrowExact( &receipt->surface,
			&receipt->surface )
		&& RalMetal_DrawableReceiptExact( &receipt->drawable, &receipt->drawable )
		&& RalMetal_PresentReceiptExact( &receipt->presentation,
			&receipt->presentation )
		&& receipt->host.backendType == receipt->backendType
		&& receipt->surface.backendType == receipt->backendType
		&& receipt->surface.ownerGeneration == receipt->host.ownerGeneration
		&& receipt->surface.surfaceGeneration == receipt->host.surfaceGeneration
		&& receipt->surface.ownerIdentity == receipt->host.ownerIdentity
		&& receipt->drawable.ownerIdentity == receipt->presentation.ownerIdentity
		&& receipt->drawable.layerIdentity == receipt->surface.surfaceIdentity
		&& receipt->presentation.layerIdentity == receipt->surface.surfaceIdentity
		&& receipt->drawable.drawableIdentity == receipt->presentation.drawableIdentity
		&& receipt->drawable.textureIdentity == receipt->presentation.textureIdentity
		&& receipt->drawable.acquireGeneration == receipt->presentation.acquireGeneration
		&& receipt->ready == qtrue ) ? qtrue : qfalse;
}

WIRED_METAL_MODULE_EXPORT qboolean RalMetal_ModuleFrameReceiptExact(
		const ralMetalModuleFrameReceipt_t *a,
		const ralMetalModuleFrameReceipt_t *b ) {
	return ( ModuleReceiptValid( a ) && ModuleReceiptValid( b )
		&& a->backendType == b->backendType
		&& a->moduleGeneration == b->moduleGeneration
		&& a->frameGeneration == b->frameGeneration
		&& Ral_FrameShellReceiptExact( &a->frame, &b->frame )
		&& Ral_PresentationHostReceiptExact( &a->host, &b->host )
		&& Ral_PresentationSurfaceBorrowExact( &a->surface, &b->surface )
		&& RalMetal_DrawableReceiptExact( &a->drawable, &b->drawable )
		&& RalMetal_PresentReceiptExact( &a->presentation, &b->presentation )
		&& a->ready == b->ready ) ? qtrue : qfalse;
}

WIRED_METAL_MODULE_EXPORT qboolean WiredMetal_GetFrameReceipt(
		ralMetalModuleFrameReceipt_t *outReceipt ) {
	if ( !outReceipt || !ModuleReceiptValid( &s_module.published ) ) return qfalse;
	*outReceipt = s_module.published;
	return qtrue;
}

static uint64_t NextGeneration( void ) {
	if ( s_module.nextGeneration == UINT64_MAX - 1u ) return 0u;
	return ++s_module.nextGeneration;
}

static void MarkFailed( void ) {
	s_module.exports.initFailed = qtrue;
}

static qhandle_t RegisterHandle( const char *name ) { (void)name; return 0; }
static qhandle_t RegisterLightMap( const char *name, int lightmap ) {
	(void)name; (void)lightmap; return 0;
}
static qhandle_t RegisterMsdf( const char *name, float range, int width, int height ) {
	(void)name; (void)range; (void)width; (void)height; return 0;
}
static void NoopHandle( qhandle_t handle ) { (void)handle; }
static void NoopVoid( void ) {}
static void NoopBool( qboolean value ) { (void)value; }
static void NoopWorld( const mapFile_t *bsp, int worldIndex ) {
	(void)bsp; (void)worldIndex;
}
static void NoopBytes( const byte *bytes ) { (void)bytes; }
static void NoopEntity( const refEntity_t *entity, qboolean shaderTime ) {
	(void)entity; (void)shaderTime;
}
static void NoopEntityTemporal( const refEntity_t *entity,
		const refEntityMotion_t *motion ) { (void)entity; (void)motion; }
static void NoopPoly( qhandle_t shader, int vertices, const polyVert_t *data, int count ) {
	(void)shader; (void)vertices; (void)data; (void)count;
}
static int NoLight( vec3_t point, vec3_t ambient, vec3_t directed, vec3_t direction ) {
	(void)point;
	if ( ambient ) memset( ambient, 0, sizeof( vec3_t ) );
	if ( directed ) memset( directed, 0, sizeof( vec3_t ) );
	if ( direction ) memset( direction, 0, sizeof( vec3_t ) );
	return 0;
}
static void NoopLight( const vec3_t origin, float intensity,
		float red, float green, float blue ) {
	(void)origin; (void)intensity; (void)red; (void)green; (void)blue;
}
static void NoopLinearLight( const vec3_t start, const vec3_t end, float intensity,
		float red, float green, float blue ) {
	(void)start; (void)end; (void)intensity; (void)red; (void)green; (void)blue;
}
static void NoopRibbon( const ribbonDesc_t *desc ) { (void)desc; }
static void NoopRailRibbon( const railRibbonDesc_t *desc ) { (void)desc; }
static void NoopBeam( const beamDesc_t *desc ) { (void)desc; }
static void NoopSprite( const spriteDesc_t *desc ) { (void)desc; }
static void NoopEmitter( const emitterDesc_t *desc ) { (void)desc; }
static void NoopDecal( const decalDesc_t *desc ) { (void)desc; }
static void NoopParticleClass( particleClassHandle_t handle,
		const particleClass_t *particleClass ) { (void)handle; (void)particleClass; }
static void NoopAtmosphere( const atmosphericDesc_t *desc ) { (void)desc; }
static void NoopHeightgrid( const float *grid, int count ) { (void)grid; (void)count; }
static void NoopLens( const lensSourceDesc_t *desc ) { (void)desc; }
static qboolean NoLensVisibility( int id, float *visibility ) {
	(void)id; if ( visibility ) *visibility = 0.0f; return qfalse;
}
static void NoopScene( const refdef_t *refdef, int worldIndex ) {
	(void)refdef; (void)worldIndex;
}
static void NoopColor( const float *color ) { (void)color; }
static void NoopMsdfOutline( float width, const float *color,
		float glowWidth, const float *glowColor ) {
	(void)width; (void)color; (void)glowWidth; (void)glowColor;
}
static void NoopMsdfShadow( float x, float y, const float *color ) {
	(void)x; (void)y; (void)color;
}
static void NoopPic( float x, float y, float width, float height,
		float s1, float t1, float s2, float t2, qhandle_t shader ) {
	(void)x; (void)y; (void)width; (void)height; (void)s1; (void)t1;
	(void)s2; (void)t2; (void)shader;
}
static void NoopBackdrop( float x, float y, float width, float height,
		float time, float mouseX, float mouseY, float transition ) {
	(void)x; (void)y; (void)width; (void)height; (void)time;
	(void)mouseX; (void)mouseY; (void)transition;
}
static void NoopRotatedPic( float x, float y, float width, float height,
		float s1, float t1, float s2, float t2, float angle, qhandle_t shader ) {
	(void)x; (void)y; (void)width; (void)height; (void)s1; (void)t1;
	(void)s2; (void)t2; (void)angle; (void)shader;
}
static void NoopLine( float x1, float y1, float x2, float y2,
		float width, qhandle_t shader ) {
	(void)x1; (void)y1; (void)x2; (void)y2; (void)width; (void)shader;
}
static void NoopRaw( int x, int y, int width, int height, int columns, int rows,
		byte *data, int client, qboolean dirty ) {
	(void)x; (void)y; (void)width; (void)height; (void)columns; (void)rows;
	(void)data; (void)client; (void)dirty;
}
static void NoopUpload( int width, int height, int columns, int rows,
		byte *data, int client, qboolean dirty ) {
	(void)width; (void)height; (void)columns; (void)rows;
	(void)data; (void)client; (void)dirty;
}

static qboolean BuildPresentationCreateInfo( uint32_t pixelWidth,
		uint32_t pixelHeight, ralSwapchainCreateInfo_t *createInfo,
		ralSurfaceFormat_t *format, ralPresentationPolicy_t *policy ) {
	ralPresentationPolicyRequest_t request;
	memset( createInfo, 0, sizeof( *createInfo ) );
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_PRESENTATION_POLICY_SCHEMA_VERSION;
	request.intent = RAL_PRESENTATION_INTENT_SYNCHRONIZED;
	request.maxFramesInFlight = 2u;
	request.allowTearingWhenLate = qfalse;
	request.preferVrr = qtrue;
	if ( !Ral_ResolvePresentationPolicy( &request, policy ) ) return qfalse;
	format->format = RAL_FORMAT_B8G8R8A8_UNORM;
	format->colorSpace = RAL_COLORSPACE_SRGB_NONLINEAR;
	createInfo->desiredWidth = pixelWidth;
	createInfo->desiredHeight = pixelHeight;
	createInfo->formatPreferences = format;
	createInfo->formatPreferenceCount = 1u;
	createInfo->presentPreferences = policy->preferences;
	createInfo->presentPreferenceCount = policy->preferenceCount;
	createInfo->requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	return qtrue;
}

static qboolean BuildColorOutputReceipt(
		const ralMetalPresentLayerReceipt_t *presentation,
		ralColorOutputReceipt_t *outReceipt ) {
	ralColorOutputRequest_t request;
	qboolean hdr;
	if ( !presentation || !outReceipt ) return qfalse;
	hdr = presentation->selected.colorSpace != RAL_COLORSPACE_SRGB_NONLINEAR
		? qtrue : qfalse;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_COLOR_OUTPUT_SCHEMA_VERSION;
	request.presentationGeneration = presentation->selected.generation;
	request.sceneFormat = hdr ? RAL_FORMAT_R16G16B16A16_SFLOAT
		: RAL_FORMAT_R8G8B8A8_UNORM;
	request.requestHdrOutput = hdr;
	request.selectedOutput.format = presentation->selected.format;
	request.selectedOutput.colorSpace = presentation->selected.colorSpace;
	request.toneMapOperator = RAL_TONEMAP_PBR_NEUTRAL;
	request.lutEnabled = qfalse;
	request.hdrPeakNits = 1000.0f;
	request.hdrMinNits = 0.01f;
	return Ral_ResolveColorOutput( &request, outReceipt );
}

static qboolean HostSurfaceJoinValid(
		const ralPresentationHostReceipt_t *host,
		const ralPresentationSurfaceBorrow_t *surface ) {
	return ( Ral_PresentationHostReceiptValid( host )
		&& Ral_PresentationSurfaceBorrowValid( surface )
		&& host->backendType == RAL_BACKEND_METAL
		&& surface->backendType == host->backendType
		&& surface->ownerGeneration == host->ownerGeneration
		&& surface->surfaceGeneration == host->surfaceGeneration
		&& surface->ownerIdentity == host->ownerIdentity ) ? qtrue : qfalse;
}

static qboolean InitializeOwners( void ) {
	ralMetalCoreCreateInfo_t coreInfo;
	ralPresentationHostOpenInfo_t openInfo;
	ralSurfaceFormat_t format;
	ralPresentationPolicy_t presentationPolicy;
	ralSwapchainCreateInfo_t createInfo;
	uint64_t coreGeneration = NextGeneration();
	uint64_t presentationGeneration = NextGeneration();
	uint64_t shellGeneration = s_module.moduleGeneration;
	qboolean hostOpened = qfalse;
	if ( !coreGeneration || !presentationGeneration
			|| !Ral_PresentationHostImportsValid(
				&s_module.imports.PresentationHost ) ) return qfalse;
	memset( &coreInfo, 0, sizeof( coreInfo ) ); coreInfo.generation = coreGeneration;
	if ( !RalMetal_CoreCreate( &coreInfo, &s_module.core, &s_module.coreReceipt ) ) {
		return qfalse;
	}
	memset( &openInfo, 0, sizeof( openInfo ) );
	openInfo.schemaVersion = RAL_PRESENTATION_HOST_SCHEMA_VERSION;
	openInfo.backendType = RAL_BACKEND_METAL;
	openInfo.requestVisible = qtrue;
	if ( !s_module.imports.PresentationHost.open(
			s_module.imports.PresentationHost.context, &openInfo,
			&s_module.hostReceipt ) ) goto fail;
	hostOpened = qtrue;
	if ( !s_module.imports.PresentationHost.borrow(
			s_module.imports.PresentationHost.context, &s_module.hostReceipt,
			&s_module.surfaceBorrow )
			|| !HostSurfaceJoinValid( &s_module.hostReceipt,
				&s_module.surfaceBorrow ) ) goto fail;
	if ( !BuildPresentationCreateInfo( s_module.hostReceipt.pixelWidth,
			s_module.hostReceipt.pixelHeight, &createInfo, &format,
			&presentationPolicy ) ) goto fail;
	if ( !RalMetal_PresentAdoptBorrowedLayer( s_module.core,
			&s_module.coreReceipt, &createInfo, presentationGeneration,
			(void *)s_module.surfaceBorrow.surfaceIdentity,
			&s_module.presentation, &s_module.presentationReceipt )
			|| !BuildColorOutputReceipt( &s_module.presentationReceipt,
				&s_module.colorOutputReceipt )
			|| !Ral_FrameShellInit( &s_module.frameShell, RAL_BACKEND_METAL,
				shellGeneration, &s_module.frameReceipt ) ) goto fail;
	memset( &s_module.config, 0, sizeof( s_module.config ) );
	snprintf( s_module.config.renderer_string, sizeof( s_module.config.renderer_string ),
		"Wired native Metal RAL" );
	snprintf( s_module.config.vendor_string, sizeof( s_module.config.vendor_string ),
		"Apple Metal" );
	snprintf( s_module.config.version_string, sizeof( s_module.config.version_string ),
		"RAL module schema %u", RAL_METAL_MODULE_SCHEMA_VERSION );
	s_module.config.maxTextureSize = 16384; s_module.config.numTextureUnits = 32;
	s_module.config.colorBits = 32; s_module.config.depthBits = 24;
	s_module.config.stencilBits = 8; s_module.config.driverType = GLDRV_ICD;
	s_module.config.hardwareType = GLHW_GENERIC;
	s_module.config.deviceSupportsGamma = qfalse;
	s_module.config.textureCompression = TC_NONE;
	s_module.config.textureEnvAddAvailable = qtrue;
	s_module.config.vidWidth = (int)s_module.hostReceipt.pixelWidth;
	s_module.config.vidHeight = (int)s_module.hostReceipt.pixelHeight;
	s_module.config.vidWidthLogical = (int)s_module.hostReceipt.logicalWidth;
	s_module.config.vidHeightLogical = (int)s_module.hostReceipt.logicalHeight;
	s_module.config.windowAspect = (float)s_module.config.vidWidth
		/ (float)s_module.config.vidHeight;
	s_module.config.displayFrequency = 0; s_module.config.isFullscreen = qfalse;
	return qtrue;
fail:
	RalMetal_PresentDestroy( s_module.presentation );
	s_module.presentation = NULL;
	if ( hostOpened ) s_module.imports.PresentationHost.close(
		s_module.imports.PresentationHost.context, &s_module.hostReceipt,
		RAL_PRESENTATION_HOST_DESTROY_OWNER );
	RalMetal_CoreDestroy( s_module.core ); s_module.core = NULL;
	memset( &s_module.hostReceipt, 0, sizeof( s_module.hostReceipt ) );
	memset( &s_module.surfaceBorrow, 0, sizeof( s_module.surfaceBorrow ) );
	memset( &s_module.presentationReceipt, 0,
		sizeof( s_module.presentationReceipt ) );
	return qfalse;
}

static void BeginRegistration( glconfig_t *config ) {
	if ( !config || !s_module.loaded || s_module.frameOpen ) { MarkFailed(); return; }
	if ( !s_module.core && !InitializeOwners() ) { MarkFailed(); return; }
	s_module.registered = qtrue; *config = s_module.config;
}

static void EndRegistration( void ) {
	if ( !s_module.registered || s_module.frameOpen ) MarkFailed();
}

static void BeginFrame( stereoFrame_t stereoFrame ) {
	ralFrameShellReceipt_t recording;
	uint64_t generation = NextGeneration();
	(void)stereoFrame;
	if ( !generation || !s_module.registered || s_module.frameOpen
			|| !Ral_FrameShellBegin( &s_module.frameShell, &s_module.frameReceipt,
				generation, &recording ) ) { MarkFailed(); return; }
	s_module.frameReceipt = recording; s_module.frameOpen = qtrue;
}

static qboolean RefreshPresentation( void ) {
	ralPresentationHostReceipt_t host;
	ralPresentationSurfaceBorrow_t surface;
	ralMetalPresentLayerReceipt_t presentation;
	ralMetalPresent_t *candidate = NULL;
	ralSurfaceFormat_t format;
	ralPresentationPolicy_t presentationPolicy;
	ralColorOutputReceipt_t colorOutput;
	ralSwapchainCreateInfo_t createInfo;
	uint64_t generation;
	if ( !s_module.imports.PresentationHost.refresh(
			s_module.imports.PresentationHost.context, &s_module.hostReceipt,
			&host )
			|| !s_module.imports.PresentationHost.borrow(
				s_module.imports.PresentationHost.context, &host, &surface )
			|| !HostSurfaceJoinValid( &host, &surface ) ) return qfalse;
	if ( Ral_PresentationHostReceiptExact( &host, &s_module.hostReceipt )
			&& Ral_PresentationSurfaceBorrowExact( &surface,
				&s_module.surfaceBorrow ) ) return qtrue;
	generation = NextGeneration();
	if ( !generation ) return qfalse;
	if ( !BuildPresentationCreateInfo( host.pixelWidth, host.pixelHeight,
			&createInfo, &format, &presentationPolicy ) ) return qfalse;
	if ( surface.surfaceIdentity != s_module.surfaceBorrow.surfaceIdentity ) {
		if ( !RalMetal_PresentAdoptBorrowedLayer( s_module.core,
				&s_module.coreReceipt, &createInfo, generation,
				(void *)surface.surfaceIdentity, &candidate, &presentation ) ) {
			return qfalse;
		}
		RalMetal_PresentDestroy( s_module.presentation );
		s_module.presentation = candidate;
	} else if ( host.pixelWidth != s_module.hostReceipt.pixelWidth
			|| host.pixelHeight != s_module.hostReceipt.pixelHeight
			|| host.surfaceGeneration != s_module.hostReceipt.surfaceGeneration ) {
		if ( !RalMetal_PresentReconfigure( s_module.presentation,
				&s_module.coreReceipt, &s_module.presentationReceipt,
				&createInfo, generation, &presentation ) ) return qfalse;
	} else {
		presentation = s_module.presentationReceipt;
	}
	if ( !BuildColorOutputReceipt( &presentation, &colorOutput ) ) return qfalse;
	s_module.hostReceipt = host;
	s_module.surfaceBorrow = surface;
	s_module.presentationReceipt = presentation;
	s_module.colorOutputReceipt = colorOutput;
	s_module.config.vidWidth = (int)host.pixelWidth;
	s_module.config.vidHeight = (int)host.pixelHeight;
	s_module.config.vidWidthLogical = (int)host.logicalWidth;
	s_module.config.vidHeightLogical = (int)host.logicalHeight;
	s_module.config.windowAspect = (float)host.pixelWidth / (float)host.pixelHeight;
	return qtrue;
}

static void EndFrame( int *frontEndMsec, int *backEndMsec ) {
	ralMetalModuleFrameReceipt_t published;
	ralFrameShellReceipt_t completed, canceled;
	uint64_t acquireGeneration = NextGeneration();
	uint64_t presentGeneration = NextGeneration();
	float clearColor[4] = { 0.0625f, 0.125f, 0.25f, 1.0f };
	if ( frontEndMsec ) *frontEndMsec = 0;
	if ( backEndMsec ) *backEndMsec = 0;
	if ( !s_module.frameOpen || !acquireGeneration || !presentGeneration ) {
		MarkFailed(); return;
	}
	if ( !RefreshPresentation() ) {
		if ( Ral_FrameShellCancel( &s_module.frameShell, &s_module.frameReceipt,
				&canceled ) ) s_module.frameReceipt = canceled;
		s_module.frameOpen = qfalse; MarkFailed(); return;
	}
	memset( &published, 0, sizeof( published ) );
	published.schemaVersion = RAL_METAL_MODULE_SCHEMA_VERSION;
	published.backendType = RAL_BACKEND_METAL;
	published.moduleGeneration = s_module.moduleGeneration;
	published.frameGeneration = s_module.frameReceipt.frameGeneration;
	published.host = s_module.hostReceipt;
	published.surface = s_module.surfaceBorrow;
	if ( !RalMetal_PresentAcquire( s_module.presentation, &s_module.coreReceipt,
			&s_module.presentationReceipt, acquireGeneration,
			&published.drawable )
			|| !RalMetal_PresentClearAndSubmit( s_module.presentation,
				&s_module.coreReceipt, &s_module.presentationReceipt,
				&published.drawable, clearColor, presentGeneration,
				&published.presentation ) ) {
		if ( Ral_FrameShellCancel( &s_module.frameShell, &s_module.frameReceipt,
				&canceled ) ) s_module.frameReceipt = canceled;
		s_module.frameOpen = qfalse; MarkFailed(); return;
	}
	if ( !Ral_FrameShellComplete( &s_module.frameShell, &s_module.frameReceipt,
			&completed ) ) {
		s_module.frameOpen = qfalse; MarkFailed(); return;
	}
	published.frame = completed; published.ready = qtrue;
	s_module.frameReceipt = completed; s_module.frameOpen = qfalse;
	if ( !ModuleReceiptValid( &published ) ) { MarkFailed(); return; }
	s_module.published = published;
}

static void Shutdown( refShutdownCode_t code ) {
	ralFrameShellReceipt_t canceled, shutdown;
	ralPresentationHostCloseMode_t closeMode;
	if ( !s_module.loaded ) return;
	if ( s_module.frameOpen && Ral_FrameShellCancel( &s_module.frameShell,
			&s_module.frameReceipt, &canceled ) ) {
		s_module.frameReceipt = canceled; s_module.frameOpen = qfalse;
	}
	s_module.registered = qfalse;
	if ( code == REF_LEVEL_ONLY ) return;
	if ( s_module.core && !Ral_FrameShellShutdown( &s_module.frameShell,
			&s_module.frameReceipt, &shutdown ) ) MarkFailed();
	RalMetal_PresentDestroy( s_module.presentation );
	s_module.presentation = NULL;
	closeMode = ( code == REF_KEEP_WINDOW )
		? RAL_PRESENTATION_HOST_KEEP_OWNER
		: RAL_PRESENTATION_HOST_DESTROY_OWNER;
	if ( Ral_PresentationHostReceiptValid( &s_module.hostReceipt )
			&& !s_module.imports.PresentationHost.close(
				s_module.imports.PresentationHost.context,
				&s_module.hostReceipt, closeMode ) ) MarkFailed();
	RalMetal_CoreDestroy( s_module.core ); s_module.core = NULL;
	memset( &s_module.published, 0, sizeof( s_module.published ) );
	memset( &s_module.hostReceipt, 0, sizeof( s_module.hostReceipt ) );
	memset( &s_module.surfaceBorrow, 0, sizeof( s_module.surfaceBorrow ) );
	memset( &s_module.presentationReceipt, 0,
		sizeof( s_module.presentationReceipt ) );
	memset( &s_module.coreReceipt, 0, sizeof( s_module.coreReceipt ) );
	s_module.loaded = qfalse;
}

static int NoFragments( int numPoints, const vec3_t *points, const vec3_t projection,
		int maxPoints, vec3_t pointBuffer, int maxFragments,
		markFragment_t *fragmentBuffer ) {
	(void)numPoints; (void)points; (void)projection; (void)maxPoints;
	(void)pointBuffer; (void)maxFragments; (void)fragmentBuffer; return 0;
}
static int NoTag( orientation_t *tag, qhandle_t model, int startFrame, int endFrame,
		float fraction, const char *name ) {
	(void)tag; (void)model; (void)startFrame; (void)endFrame;
	(void)fraction; (void)name; return 0;
}
static void NoBounds( qhandle_t model, vec3_t mins, vec3_t maxs ) {
	(void)model; if ( mins ) memset( mins, 0, sizeof( vec3_t ) );
	if ( maxs ) memset( maxs, 0, sizeof( vec3_t ) );
}
static void NoFont( const char *name, int size, fontInfo_t *font ) {
	(void)name; (void)size; if ( font ) memset( font, 0, sizeof( *font ) );
}
static void NoRemap( const char *oldName, const char *newName, const char *offset ) {
	(void)oldName; (void)newName; (void)offset;
}
static qboolean NoToken( char *buffer, int size ) {
	if ( buffer && size > 0 ) buffer[0] = '\0'; return qfalse;
}
static qboolean NoPvs( const vec3_t a, const vec3_t b ) { (void)a; (void)b; return qfalse; }
static void NoVideo( int height, int width, byte *capture, byte *encode,
		qboolean motionJpeg ) {
	(void)height; (void)width; (void)capture; (void)encode; (void)motionJpeg;
}
static qboolean CanMinimize( void ) { return qtrue; }
static const glconfig_t *GetConfig( void ) {
	return s_module.core ? &s_module.config : NULL;
}
static qboolean GetMemoryBudget( uint64_t *deviceUsed, uint64_t *deviceBudget,
		uint64_t *hostUsed, uint64_t *hostBudget, int *pressure ) {
	if ( deviceUsed ) *deviceUsed = 0u; if ( deviceBudget ) *deviceBudget = 0u;
	if ( hostUsed ) *hostUsed = 0u; if ( hostBudget ) *hostBudget = 0u;
	if ( pressure ) *pressure = 0; return qfalse;
}
#if FEAT_FOG_SYSTEM
static void NoFog( refFogType_t *type, vec3_t color, float *depth, float *density ) {
	if ( type ) *type = REF_FT_NONE; if ( color ) memset( color, 0, sizeof( vec3_t ) );
	if ( depth ) *depth = 0.0f; if ( density ) *density = 0.0f;
}
static void NoViewFog( const vec3_t origin, refFogType_t *type, vec3_t color,
		float *depth, float *density, qboolean *useColor ) {
	(void)origin; NoFog( type, color, depth, density );
	if ( useColor ) *useColor = qfalse;
}
#endif
#if FEAT_HALO
static void NoHalo( const vec3_t origin, float red, float green, float blue,
		float scale, int id, qboolean visible ) {
	(void)origin; (void)red; (void)green; (void)blue;
	(void)scale; (void)id; (void)visible;
}
#endif
#if FEAT_IQM
static int NoIqmAnimations( qhandle_t model, iqmAnimInfo_t *animations, int count ) {
	(void)model; (void)animations; (void)count; return 0;
}
#endif
static int NoMdlAnimations( qhandle_t model, mdlAnimRange_t *animations, int count ) {
	(void)model; (void)animations; (void)count; return 0;
}
static void NoLightstyle( int style, const char *pattern ) {
	(void)style; (void)pattern;
}
static qboolean NoGpuProfile( refGpuProfileSample_t *sample ) {
	if ( sample ) memset( sample, 0, sizeof( *sample ) ); return qfalse;
}
static void PresentationChanged( const refPresentationChange_t *change ) {
	/* EndFrame already refreshes the presentation-host receipt transactionally. */
	(void)change;
}

static void FillExports( refexport_t *exports ) {
	memset( exports, 0, sizeof( *exports ) );
	exports->Shutdown = Shutdown; exports->BeginRegistration = BeginRegistration;
	exports->RegisterModel = RegisterHandle; exports->RegisterSkin = RegisterHandle;
	exports->RegisterShader = RegisterHandle; exports->RegisterShaderNoMip = RegisterHandle;
	exports->RegisterShaderLightMap = RegisterLightMap; exports->RegisterMSDFShader = RegisterMsdf;
	exports->RegisterPrimitiveShader = RegisterHandle; exports->PinShaderImages = NoopHandle;
	exports->LoadWorld = NoopWorld; exports->SetWorldVisData = NoopBytes;
	exports->EndRegistration = EndRegistration; exports->ClearScene = NoopVoid;
	exports->AddRefEntityToScene = NoopEntity; exports->AddPolyToScene = NoopPoly;
	exports->LightForPoint = NoLight; exports->AddLightToScene = NoopLight;
	exports->AddAdditiveLightToScene = NoopLight; exports->AddLinearLightToScene = NoopLinearLight;
	exports->AddRibbonToScene = NoopRibbon; exports->AddBeamToScene = NoopBeam;
	exports->AddSpriteToScene = NoopSprite; exports->EmitParticles = NoopEmitter;
	exports->AddDecalToScene = NoopDecal; exports->RegisterParticleClass = NoopParticleClass;
	exports->SetAtmosphere = NoopAtmosphere; exports->SetAtmosphereHeightgrid = NoopHeightgrid;
	exports->AddLensSourceToScene = NoopLens; exports->GetLensVisibility = NoLensVisibility;
	exports->RenderScene = NoopScene; exports->SetColor = NoopColor;
	exports->SetMSDFOutline = NoopMsdfOutline; exports->SetMSDFShadow = NoopMsdfShadow;
	exports->SetClipRegion = NoopColor; exports->DrawStretchPic = NoopPic;
	exports->DrawMenuBackdrop = NoopBackdrop; exports->DrawStretchPicOverlay = NoopPic;
	exports->DrawRotatedPic = NoopRotatedPic; exports->DrawLine = NoopLine;
	exports->DrawStretchRaw = NoopRaw; exports->UploadCinematic = NoopUpload;
	exports->BeginFrame = BeginFrame; exports->EndFrame = EndFrame;
	exports->MarkFragments = NoFragments; exports->LerpTag = NoTag;
	exports->ModelBounds = NoBounds; exports->RegisterFont = NoFont;
	exports->RemapShader = NoRemap; exports->GetEntityToken = NoToken;
	exports->inPVS = NoPvs; exports->TakeVideoFrame = NoVideo;
	exports->ThrottleBackend = NoopVoid; exports->FinishBloom = NoopVoid;
	exports->SetColorMappings = NoopVoid; exports->CanMinimize = CanMinimize;
	exports->GetConfig = GetConfig; exports->GetMemoryBudget = GetMemoryBudget;
	exports->VertexLighting = NoopBool; exports->SyncRender = NoopVoid;
#if FEAT_FOG_SYSTEM
	exports->GetGlobalFog = NoFog; exports->GetViewFog = NoViewFog;
#endif
#if FEAT_HALO
	exports->AddHaloToScene = NoHalo;
#endif
#if FEAT_IQM
	exports->GetIQMAnimations = NoIqmAnimations;
#endif
	exports->GetMDLAnimations = NoMdlAnimations;
	exports->SetLightstylePattern = NoLightstyle;
	exports->AddRailRibbonToScene = NoopRailRibbon;
	exports->GetGpuProfileSample = NoGpuProfile;
	exports->AddRefEntityToSceneTemporal = NoopEntityTemporal;
	exports->PresentationChanged = PresentationChanged;
}

WIRED_METAL_MODULE_EXPORT refexport_t *QDECL GetRefAPI( int apiVersion,
		refimport_t *imports ) {
	if ( !imports || apiVersion != REF_API_VERSION || s_module.loaded
			|| s_moduleCounter == UINT64_MAX - 1u
			|| !Ral_PresentationHostImportsValid(
				&imports->PresentationHost ) ) return NULL;
	memset( &s_module, 0, sizeof( s_module ) );
	s_module.moduleGeneration = ++s_moduleCounter;
	s_module.nextGeneration = s_module.moduleGeneration;
	s_module.imports = *imports; FillExports( &s_module.exports );
	s_module.loaded = qtrue;
	return &s_module.exports;
}
