// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_module.h"
#include "maps/map_format_registry.h"
#include "render_image_decode.h"
#include "render_material_script.h"
#include "tr_public.h"
#include "tr_screenshot.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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
	renderSubmissionState_t frontend;
	renderMaterialScriptCatalog_t materialScripts;
	const mapFile_t *loadedWorld;
	glconfig_t config;
	uint64_t moduleGeneration;
	uint64_t nextGeneration;
	uint64_t lastLoggedWorldDigest;
	qboolean loggedCompleteContentReceipt;
	char screenshotName[MAX_OSPATH];
	int initLogChannel;
	qboolean loaded;
	qboolean registered;
	qboolean frameOpen;
	qboolean registrationFramePublished;
	qboolean screenshotCommandsRegistered;
	qboolean screenshotPending;
	qboolean screenshotSilent;
	qboolean loggedInvalidEntityDrop;
} wiredMetalModuleState_t;

#ifdef __cplusplus
#define WIRED_METAL_MODULE_EXPORT extern "C" Q_EXPORT
#else
#define WIRED_METAL_MODULE_EXPORT Q_EXPORT
#endif

static wiredMetalModuleState_t s_module;
static uint64_t s_moduleCounter;
refimport_t ri;

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
		&& RenderSubmission_ReceiptExact( &receipt->frontend,
			&receipt->frontend )
		&& receipt->frontend.ownerGeneration == receipt->moduleGeneration
		&& receipt->frontend.frameGeneration == receipt->frameGeneration
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
		&& RenderSubmission_ReceiptExact( &a->frontend, &b->frontend )
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


static void MarkFailed( const char *reason ) {
	if ( !s_module.exports.initFailed ) {
		if ( s_module.imports.LogCh && s_module.initLogChannel >= 0 ) {
			s_module.imports.LogCh( s_module.initLogChannel, SEV_WARN,
				"Wired native Metal RAL: lifecycle failure (%s)\n", reason );
		} else if ( s_module.imports.Log ) {
			s_module.imports.Log( SEV_WARN,
				"Wired native Metal RAL: lifecycle failure (%s)\n", reason );
		}
	}
	s_module.exports.initFailed = qtrue;
}

qboolean RB_ScheduleScreenshot( int typeMask, const char *fileName,
		qboolean silent ) {
	if ( typeMask != SCREENSHOT_PNG || !fileName || !fileName[0]
			|| s_module.screenshotPending || !s_module.presentation
			|| strlen( fileName ) >= sizeof( s_module.screenshotName )
			|| !RalMetal_PresentRequestCapture( s_module.presentation ) ) return qfalse;
	(void)snprintf( s_module.screenshotName, sizeof( s_module.screenshotName ),
		"%s", fileName );
	s_module.screenshotSilent = silent; s_module.screenshotPending = qtrue;
	return qtrue;
}

void R_LevelShot( void ) {
	if ( s_module.imports.LogCh && s_module.initLogChannel >= 0 )
		s_module.imports.LogCh( s_module.initLogChannel, SEV_WARN,
			"Wired native Metal RAL: levelshot capture is not supported\n" );
}

static qboolean CompleteScreenshot( void ) {
	const byte *rgb;
	byte *scaled = NULL;
	uint32_t width, height, outputWidth, outputHeight;
	if ( !s_module.screenshotPending ) return qtrue;
	rgb = RalMetal_PresentCaptureRgb( s_module.presentation, &width, &height );
	outputWidth = s_module.hostReceipt.logicalWidth;
	outputHeight = s_module.hostReceipt.logicalHeight;
	if ( rgb && outputWidth && outputHeight
			&& ( outputWidth != width || outputHeight != height ) ) {
		uint64_t bytes = (uint64_t)outputWidth * outputHeight * 3u;
		if ( outputWidth <= width && outputHeight <= height && bytes <= INT_MAX ) {
			scaled = (byte *)malloc( (size_t)bytes );
			if ( scaled ) {
				for ( uint32_t y = 0u; y < outputHeight; ++y ) {
					uint32_t sourceY = (uint32_t)( ( (uint64_t)y * height
						+ outputHeight / 2u ) / outputHeight );
					if ( sourceY >= height ) sourceY = height - 1u;
					for ( uint32_t x = 0u; x < outputWidth; ++x ) {
						uint32_t sourceX = (uint32_t)( ( (uint64_t)x * width
							+ outputWidth / 2u ) / outputWidth );
						if ( sourceX >= width ) sourceX = width - 1u;
						memcpy( scaled + ( (size_t)y * outputWidth + x ) * 3u,
							rgb + ( (size_t)sourceY * width + sourceX ) * 3u, 3u );
					}
				}
				rgb = scaled;
			}
		}
	}
	if ( !rgb || !outputWidth || !outputHeight
			|| outputWidth > INT_MAX || outputHeight > INT_MAX
			|| ( ( outputWidth != width || outputHeight != height ) && !scaled )
			|| !R_SavePNG( s_module.screenshotName, rgb,
				(int)outputWidth, (int)outputHeight ) ) {
		free( scaled );
		s_module.screenshotPending = qfalse;
		if ( s_module.imports.LogCh && s_module.initLogChannel >= 0 )
			s_module.imports.LogCh( s_module.initLogChannel, SEV_WARN,
				"Wired native Metal RAL: screenshot capture failed\n" );
		return qfalse;
	}
	free( scaled );
	if ( !s_module.screenshotSilent ) R_ScreenshotPrintSaved( s_module.screenshotName );
	s_module.screenshotPending = qfalse; s_module.screenshotName[0] = '\0';
	return qtrue;
}

static qhandle_t RegisterAsset( renderAssetKind_t kind, const char *name ) {
	return RenderSubmission_RegisterAsset( &s_module.frontend, kind, name );
}
static qhandle_t RegisterMaterialImageSource( renderAssetKind_t kind,
		const char *name, const char *imageName, qboolean clampToEdge ) {
	static const byte white[4] = { 255u, 255u, 255u, 255u };
	renderMaterialScriptEntry_t scripted;
	byte *pixels = NULL;
	uint32_t width = 0u, height = 0u;
	char resolved[MAX_QPATH];
	qhandle_t handle;
	memset( &scripted, 0, sizeof( scripted ) );
	if ( name && RenderMaterialScript_Lookup( &s_module.materialScripts,
			name, &scripted ) ) {
		imageName = scripted.imageName;
		clampToEdge = scripted.clampToEdge;
	}
	if ( name && ( !strcmp( name, "*white" ) || !strcmp( name, "white" ) ) )
		return RenderSubmission_RegisterMaterialImage( &s_module.frontend,
			kind, name, qtrue, white, 1u, 1u );
	if ( imageName && imageName[0] != '*'
			&& RenderImage_DecodeRgba8( imageName, &pixels, &width, &height, resolved ) ) {
		handle = RenderSubmission_RegisterMaterialImage( &s_module.frontend,
			kind, name, clampToEdge, pixels, width, height );
		ri.Free( pixels );
		if ( handle && scripted.name[0]
				&& !RenderSubmission_SetMaterialRasterPolicy( &s_module.frontend,
					handle, scripted.alphaMode, scripted.alphaCutoff,
					scripted.depthWrite ) ) return 0;
		return handle;
	}
	handle = RenderSubmission_RegisterMaterialImage( &s_module.frontend,
		kind, name, clampToEdge, NULL, 0u, 0u );
	if ( handle && scripted.name[0]
			&& !RenderSubmission_SetMaterialRasterPolicy( &s_module.frontend,
				handle, scripted.alphaMode, scripted.alphaCutoff,
				scripted.depthWrite ) ) return 0;
	return handle;
}
static qhandle_t RegisterMaterialImage( renderAssetKind_t kind, const char *name,
		qboolean clampToEdge ) {
	return RegisterMaterialImageSource( kind, name, name, clampToEdge );
}
static qhandle_t RegisterModel( const char *name ) {
	void *bytes = NULL;
	renderModelSnapshot_t model;
	qhandle_t handle;
	int byteCount;
	if ( !name || !name[0] ) return 0;
	if ( name[0] == '*' && s_module.loadedWorld ) {
		char *end = NULL;
		long index = strtol( name + 1, &end, 10 );
		if ( end != name + 1 && *end == '\0' && index > 0
				&& index < s_module.loadedWorld->numSubModels ) {
			const dmodel_t *model = &s_module.loadedWorld->subModels[index];
			return RenderSubmission_RegisterInlineModel( &s_module.frontend, name,
				(uint32_t)model->firstSurface, (uint32_t)model->numSurfaces );
		}
	}
	if ( !ri.FS_ReadFile || !ri.FS_FreeFile )
		return RegisterAsset( RENDER_ASSET_MODEL, name );
	byteCount = ri.FS_ReadFile( name, &bytes );
	if ( byteCount <= 0 || !bytes || (uint64_t)byteCount > UINT32_MAX ) {
		if ( bytes ) ri.FS_FreeFile( bytes );
		return 0;
	}
	handle = RenderSubmission_RegisterModelData( &s_module.frontend, name,
		bytes, (uint32_t)byteCount );
	ri.FS_FreeFile( bytes );
	if ( !handle ) {
		return 0;
	}
	if ( !RenderSubmission_ModelSnapshot( &s_module.frontend, handle, &model ) ) return 0;
	for ( uint32_t batch = 0u; batch < model.batchCount; ++batch ) {
		qhandle_t material = RegisterMaterialImage( RENDER_ASSET_MATERIAL,
			model.batches[batch].materialName, qfalse );
		if ( !material || !RenderSubmission_SetModelBatchMaterial(
				&s_module.frontend, handle, batch, material ) ) return 0;
	}
	return handle;
}
static qhandle_t RegisterSkin( const char *name ) {
	return RegisterAsset( RENDER_ASSET_SKIN, name );
}
static qhandle_t RegisterMaterial( const char *name ) {
	return RegisterMaterialImage( RENDER_ASSET_MATERIAL, name, qfalse );
}
static qhandle_t RegisterMaterialNoMip( const char *name ) {
	return RegisterMaterialImage( RENDER_ASSET_MATERIAL, name, qtrue );
}
static qhandle_t RegisterPrimitiveMaterial( const char *name ) {
	return RegisterMaterialImage( RENDER_ASSET_PRIMITIVE_MATERIAL, name, qfalse );
}
static qhandle_t RegisterLightMap( const char *name, int lightmap ) {
	(void)lightmap; return RegisterMaterialImage( RENDER_ASSET_MATERIAL,
		name, qtrue );
}
static qhandle_t RegisterMsdf( const char *name, float range, int width, int height ) {
	char imageName[MAX_QPATH];
	size_t length;
	(void)range; (void)width; (void)height;
	if ( !name || ( length = strlen( name ) ) < 6u
			|| strcmp( name + length - 6u, "_atlas" ) )
		return RegisterMaterialImage( RENDER_ASSET_MSDF, name, qtrue );
	if ( length - 6u + 4u >= sizeof( imageName ) ) return 0;
	memcpy( imageName, name, length - 6u );
	memcpy( imageName + length - 6u, ".png", 5u );
	return RegisterMaterialImageSource( RENDER_ASSET_MSDF, name, imageName, qtrue );
}
static qboolean PrepareWorldMaterials( const mapFile_t *bsp ) {
	byte *rgba = NULL;
	uint32_t side = 0u;
	uint64_t pixels;
	if ( !bsp || bsp->numShaders < 0 || bsp->numLightmapPages < 0
			|| bsp->lightmapPageSize < 0
			|| ( bsp->numShaders && !bsp->shaders ) ) return qfalse;
	for ( int i = 0; i < bsp->numShaders; ++i ) {
		if ( !RegisterMaterialImage( RENDER_ASSET_MATERIAL,
				bsp->shaders[i].shader, qfalse ) ) return qfalse;
	}
	if ( bsp->numLightmapPages == 0 ) return qtrue;
	if ( !bsp->lightmapData || bsp->lightmapPageSize <= 0
			|| ( bsp->lightmapPageSize % 3 ) != 0 ) return qfalse;
	pixels = (uint32_t)bsp->lightmapPageSize / 3u;
	if ( !pixels || pixels > RENDER_SUBMISSION_MAX_MATERIAL_BYTES / 4u )
		return qfalse;
	for ( side = 1u; (uint64_t)side * side < pixels; ++side ) {
		if ( side >= RENDER_SUBMISSION_MAX_IMAGE_DIMENSION ) return qfalse;
	}
	if ( (uint64_t)side * side != pixels
			|| side > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION ) return qfalse;
	rgba = (byte *)ri.Malloc( (int)( pixels * 4u ) );
	if ( !rgba ) return qfalse;
	for ( int page = 0; page < bsp->numLightmapPages; ++page ) {
		char name[MAX_QPATH];
		const byte *rgb = bsp->lightmapData
			+ (size_t)page * (size_t)bsp->lightmapPageSize;
		for ( uint64_t pixel = 0u; pixel < pixels; ++pixel ) {
			rgba[pixel * 4u + 0u] = rgb[pixel * 3u + 0u];
			rgba[pixel * 4u + 1u] = rgb[pixel * 3u + 1u];
			rgba[pixel * 4u + 2u] = rgb[pixel * 3u + 2u];
			rgba[pixel * 4u + 3u] = 255u;
		}
		if ( !RenderSubmission_LightmapMaterialName( name,
				(uint32_t)bsp->checksum, page )
				|| !RenderSubmission_RegisterMaterialImage( &s_module.frontend,
					RENDER_ASSET_LIGHTMAP, name, qtrue, rgba, side, side ) ) {
			ri.Free( rgba ); return qfalse;
		}
	}
	ri.Free( rgba );
	return qtrue;
}
static void NoopHandle( qhandle_t handle ) { (void)handle; }
static void NoopVoid( void ) {}
static void NoopBool( qboolean value ) { (void)value; }
static void SubmitWorld( const mapFile_t *bsp, int worldIndex ) {
	if ( !PrepareWorldMaterials( bsp )
			|| !RenderSubmission_LoadWorld( &s_module.frontend, bsp, worldIndex ) )
		MarkFailed( "frontend-world" );
	else s_module.loadedWorld = bsp;
}
static void NoopBytes( const byte *bytes ) { (void)bytes; }
static void SubmitClearScene( void ) {
	if ( !RenderSubmission_ClearScene( &s_module.frontend ) )
		MarkFailed( "frontend-clear-scene" );
}

static qboolean EntityIngressValid( const refEntity_t *entity,
		const refEntityMotion_t *motion ) {
	if ( !entity ) {
		MarkFailed( "frontend-entity-null" ); return qfalse;
	}
	if ( (unsigned)entity->reType >= RT_MAX_REF_ENTITY_TYPE ) {
		MarkFailed( "frontend-entity-type" ); return qfalse;
	}
	if ( ( motion && !RefEntityMotion_IsValid( motion ) )
			|| isnan( entity->origin[0] ) || isnan( entity->origin[1] )
			|| isnan( entity->origin[2] ) ) {
		/* Public renderer ABI semantics: malformed per-content submissions are
		 * dropped, not promoted into a backend lifecycle failure. Vulkan follows
		 * the same rule for NaN origins and invalid temporal payloads. */
		if ( !s_module.loggedInvalidEntityDrop && s_module.imports.LogCh
				&& s_module.initLogChannel >= 0 ) {
			s_module.imports.LogCh( s_module.initLogChannel, SEV_WARN,
				"Wired native Metal RAL: dropped invalid refEntity submission\n" );
			s_module.loggedInvalidEntityDrop = qtrue;
		}
		return qfalse;
	}
	return qtrue;
}

static void SubmitEntity( const refEntity_t *entity, qboolean shaderTime ) {
	(void)shaderTime;
	if ( !s_module.frameOpen ) return;
	if ( !EntityIngressValid( entity, NULL ) ) return;
	if ( !RenderSubmission_AddEntity( &s_module.frontend, entity, NULL ) )
		MarkFailed( "frontend-entity" );
}
static void SubmitEntityTemporal( const refEntity_t *entity,
		const refEntityMotion_t *motion ) {
	if ( !s_module.frameOpen ) return;
	if ( !EntityIngressValid( entity, motion ) ) return;
	if ( !RenderSubmission_AddEntity( &s_module.frontend, entity, motion ) )
		MarkFailed( "frontend-temporal-entity" );
}
static void SubmitPoly( qhandle_t shader, int vertices,
		const polyVert_t *data, int count ) {
	if ( !s_module.frameOpen ) return;
	if ( !RenderSubmission_AddPoly( &s_module.frontend, shader, vertices,
			data, count ) ) MarkFailed( "frontend-poly" );
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
	if ( !s_module.frameOpen ) return;
	if ( !RenderSubmission_AddLight( &s_module.frontend, origin, NULL,
			intensity, red, green, blue ) ) MarkFailed( "frontend-light" );
}
static void NoopLinearLight( const vec3_t start, const vec3_t end, float intensity,
		float red, float green, float blue ) {
	if ( !s_module.frameOpen ) return;
	if ( !RenderSubmission_AddLight( &s_module.frontend, start, end,
			intensity, red, green, blue ) ) MarkFailed( "frontend-linear-light" );
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
static void SubmitScene( const refdef_t *refdef, int worldIndex ) {
	if ( !s_module.frameOpen ) return;
	if ( !RenderSubmission_RenderScene( &s_module.frontend, refdef, worldIndex ) )
		MarkFailed( "frontend-scene" );
}
static void SubmitColor( const float *color ) {
	if ( !RenderSubmission_SetColor( &s_module.frontend, color ) )
		MarkFailed( "frontend-color" );
}
static void NoopClip( const float *region ) { (void)region; }
static void NoopMsdfOutline( float width, const float *color,
		float glowWidth, const float *glowColor ) {
	(void)width; (void)color; (void)glowWidth; (void)glowColor;
}
static void NoopMsdfShadow( float x, float y, const float *color ) {
	(void)x; (void)y; (void)color;
}
static void NoopPic( float x, float y, float width, float height,
		float s1, float t1, float s2, float t2, qhandle_t shader ) {
	if ( !s_module.frameOpen ) return;
	if ( !RenderSubmission_AddUiQuad( &s_module.frontend, x, y, width, height,
			s1, t1, s2, t2, 0.0f, shader ) ) MarkFailed( "frontend-ui-quad" );
}
static void NoopBackdrop( float x, float y, float width, float height,
		float time, float mouseX, float mouseY, float transition ) {
	if ( !s_module.frameOpen ) return;
	if ( !RenderSubmission_AddUiQuad( &s_module.frontend, x, y, width, height,
			time, mouseX, mouseY, transition, 0.0f, INT_MAX ) )
		MarkFailed( "frontend-ui-backdrop" );
}
static void NoopRotatedPic( float x, float y, float width, float height,
		float s1, float t1, float s2, float t2, float angle, qhandle_t shader ) {
	if ( !s_module.frameOpen ) return;
	if ( !RenderSubmission_AddUiQuad( &s_module.frontend, x, y, width, height,
			s1, t1, s2, t2, angle, shader ) ) MarkFailed( "frontend-ui-rotated" );
}
static void NoopLine( float x1, float y1, float x2, float y2,
		float width, qhandle_t shader ) {
	if ( !s_module.frameOpen ) return;
	if ( !RenderSubmission_AddUiLine( &s_module.frontend, x1, y1, x2, y2,
			width, shader ) ) MarkFailed( "frontend-ui-line" );
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
	int swapInterval = s_module.imports.Cvar_VariableIntegerValue
		? s_module.imports.Cvar_VariableIntegerValue( "r_swapInterval" ) : 1;
	memset( createInfo, 0, sizeof( *createInfo ) );
	if ( !Ral_PresentationPolicyRequestFromLegacySwapInterval( swapInterval,
			2u, &request ) ) return qfalse;
	if ( !Ral_ResolvePresentationPolicy( &request, policy ) ) return qfalse;
	format->format = RAL_FORMAT_B8G8R8A8_SRGB;
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
	qboolean initializedNow;
	if ( !config || !s_module.loaded || s_module.frameOpen ) {
		MarkFailed( "begin-registration-state" ); return;
	}
	initializedNow = s_module.core ? qfalse : qtrue;
	if ( initializedNow && !InitializeOwners() ) {
		MarkFailed( "owner-initialization" ); return;
	}
	if ( !s_module.frontend.initialized
			&& !RenderSubmission_Init( &s_module.frontend,
				s_module.moduleGeneration ) ) {
		MarkFailed( "frontend-initialization" ); return;
	}
	if ( !s_module.materialScripts.ready
			&& !RenderMaterialScript_Load( &s_module.materialScripts, &ri ) ) {
		MarkFailed( "material-script-catalog" ); return;
	}
	s_module.registered = qtrue;
	s_module.registrationFramePublished = qfalse;
	if ( !s_module.screenshotCommandsRegistered && ri.Cmd_AddCommand
			&& ri.Cmd_RemoveCommand ) {
		R_ScreenshotRegisterCommands();
		s_module.screenshotCommandsRegistered = qtrue;
	}
	*config = s_module.config;
	if ( initializedNow && s_module.imports.LogCh && s_module.initLogChannel >= 0 ) {
		s_module.imports.LogCh( s_module.initLogChannel, SEV_INFO,
			"Wired native Metal RAL: initialized logical=%ux%u pixels=%ux%u\n",
			s_module.hostReceipt.logicalWidth, s_module.hostReceipt.logicalHeight,
			s_module.hostReceipt.pixelWidth, s_module.hostReceipt.pixelHeight );
	}
	if ( s_module.imports.LogCh && s_module.initLogChannel >= 0 ) {
		s_module.imports.LogCh( s_module.initLogChannel, SEV_INFO,
			"Wired native Metal RAL: registration ready logical=%ux%u pixels=%ux%u swapInterval=%d presentMode=%d displaySync=%d\n",
			s_module.hostReceipt.logicalWidth, s_module.hostReceipt.logicalHeight,
			s_module.hostReceipt.pixelWidth, s_module.hostReceipt.pixelHeight,
			s_module.imports.Cvar_VariableIntegerValue
				? s_module.imports.Cvar_VariableIntegerValue( "r_swapInterval" ) : 1,
			(int)s_module.presentationReceipt.selected.presentMode,
			s_module.presentationReceipt.displaySyncEnabled ? 1 : 0 );
	}
}

static void EndRegistration( void ) {
	if ( !s_module.registered || s_module.frameOpen ) {
		MarkFailed( "end-registration-state" );
	}
}

static void BeginFrame( stereoFrame_t stereoFrame ) {
	ralFrameShellReceipt_t recording;
	uint64_t generation = NextGeneration();
	(void)stereoFrame;
	if ( !generation || !s_module.registered || s_module.frameOpen
			|| !Ral_FrameShellBegin( &s_module.frameShell, &s_module.frameReceipt,
				generation, &recording )
			|| !RenderSubmission_BeginFrame( &s_module.frontend, generation ) ) {
		MarkFailed( "begin-frame-state" ); return;
	}
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
	uint64_t contentDigest;
	qboolean completeContent, worldChanged, logContent;
	if ( frontEndMsec ) *frontEndMsec = 0;
	if ( backEndMsec ) *backEndMsec = 0;
	if ( !s_module.frameOpen || !acquireGeneration || !presentGeneration ) {
		MarkFailed( "end-frame-state" ); return;
	}
	contentDigest = RenderSubmission_FrameDigest( &s_module.frontend );
	if ( !contentDigest ) {
		RenderSubmission_CancelFrame( &s_module.frontend );
		MarkFailed( "frontend-frame-digest" ); return;
	}
	clearColor[0] = (float)( ( contentDigest >> 0u ) & 0xffu ) / 1020.0f + 0.05f;
	clearColor[1] = (float)( ( contentDigest >> 8u ) & 0xffu ) / 1020.0f + 0.05f;
	clearColor[2] = (float)( ( contentDigest >> 16u ) & 0xffu ) / 1020.0f + 0.05f;
	if ( !RefreshPresentation() ) {
		if ( Ral_FrameShellCancel( &s_module.frameShell, &s_module.frameReceipt,
				&canceled ) ) s_module.frameReceipt = canceled;
		RenderSubmission_CancelFrame( &s_module.frontend );
		s_module.frameOpen = qfalse; MarkFailed( "presentation-refresh" ); return;
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
			&published.drawable ) ) {
		/* CAMetalLayer may transiently publish no drawable while Cocoa applies a
		 * resize/visibility transaction. Cancel this frame without poisoning the
		 * renderer; the next engine frame refreshes the host and retries. */
		if ( Ral_FrameShellCancel( &s_module.frameShell, &s_module.frameReceipt,
				&canceled ) ) s_module.frameReceipt = canceled;
		RenderSubmission_CancelFrame( &s_module.frontend );
		s_module.frameOpen = qfalse;
		return;
	}
	if ( !RalMetal_PresentClearAndSubmit( s_module.presentation,
				&s_module.coreReceipt, &s_module.presentationReceipt,
				&published.drawable, &s_module.frontend, clearColor, presentGeneration,
				&published.presentation ) ) {
		if ( Ral_FrameShellCancel( &s_module.frameShell, &s_module.frameReceipt,
				&canceled ) ) s_module.frameReceipt = canceled;
		RenderSubmission_CancelFrame( &s_module.frontend );
		s_module.frameOpen = qfalse; MarkFailed( "present-submit" ); return;
	}
	(void)CompleteScreenshot();
	if ( !Ral_FrameShellComplete( &s_module.frameShell, &s_module.frameReceipt,
			&completed ) ) {
		RenderSubmission_CancelFrame( &s_module.frontend );
		s_module.frameOpen = qfalse; MarkFailed( "frame-complete" ); return;
	}
	if ( !RenderSubmission_EndFrame( &s_module.frontend,
			completed.frameGeneration, &published.frontend ) ) {
		s_module.frameOpen = qfalse; MarkFailed( "frontend-frame-complete" ); return;
	}
	published.frame = completed; published.ready = qtrue;
	s_module.frameReceipt = completed; s_module.frameOpen = qfalse;
	if ( !ModuleReceiptValid( &published ) ) {
		MarkFailed( "published-receipt" ); return;
	}
	s_module.published = published;
	completeContent = ( published.frontend.entityCount > 0u
		&& published.frontend.uiPrimitiveCount > 0u
		&& published.presentation.loweredWorldBatchCount > 0u
		&& published.presentation.patchWorldBatchCount > 0u
		&& published.presentation.lightmappedWorldBatchCount > 0u
		&& published.presentation.modelEntityCount > 0u
		&& published.presentation.loweredUiPrimitiveCount > 0u
		&& published.presentation.texturedUiPrimitiveCount > 0u
		&& published.presentation.msdfUiPrimitiveCount > 0u
		&& published.presentation.unresolvedEntityCount == 0u ) ? qtrue : qfalse;
	worldChanged = ( published.frontend.worldDigest
		!= s_module.lastLoggedWorldDigest ) ? qtrue : qfalse;
	if ( worldChanged ) s_module.loggedCompleteContentReceipt = qfalse;
	logContent = ( worldChanged || ( completeContent
		&& !s_module.loggedCompleteContentReceipt ) ) ? qtrue : qfalse;
	if ( published.frontend.worldLoaded == qtrue
			&& published.frontend.sceneRendered == qtrue
			&& logContent
			&& s_module.imports.LogCh && s_module.initLogChannel >= 0 ) {
		s_module.imports.LogCh( s_module.initLogChannel, SEV_INFO,
			"Wired native Metal RAL: content receipt asset=%016llx world=%016llx frame=%016llx readback=%016llx bytes=%u readbackXY=%u,%u surfaces=%u vertices=%u indices=%u assets=%u materials=%u resolvedMaterials=%u materialBytes=%u models=%u modelBytes=%u entities=%u temporalEntities=%u ui=%u loweredWorld=%u worldBatches=%u texturedWorld=%u lightmappedWorld=%u patchWorld=%u maskedWorld=%u blendedWorld=%u depthWriteWorld=%u loweredEntity=%u entityBatches=%u modelEntities=%u primitiveEntities=%u unresolvedEntities=%u loweredUi=%u texturedUi=%u msdfUi=%u\n",
			(unsigned long long)published.frontend.assetDigest,
			(unsigned long long)published.frontend.worldDigest,
			(unsigned long long)published.frontend.frameDigest,
			(unsigned long long)published.presentation.readbackDigest,
			published.presentation.readbackByteCount,
			published.presentation.readbackX,
			published.presentation.readbackY,
			published.frontend.worldSurfaceCount,
			published.frontend.worldVertexCount,
			published.frontend.worldIndexCount,
			published.frontend.registeredAssetCount,
			published.frontend.registeredMaterialCount,
			published.frontend.resolvedMaterialCount,
			published.frontend.materialBytes,
			published.frontend.registeredModelCount,
			published.frontend.modelBytes,
			published.frontend.entityCount,
			published.frontend.temporalEntityCount,
			published.frontend.uiPrimitiveCount,
			published.presentation.loweredWorldIndexCount,
			published.presentation.loweredWorldBatchCount,
			published.presentation.texturedWorldBatchCount,
			published.presentation.lightmappedWorldBatchCount,
			published.presentation.patchWorldBatchCount,
			published.presentation.maskedWorldBatchCount,
			published.presentation.blendedWorldBatchCount,
			published.presentation.depthWriteWorldBatchCount,
			published.presentation.loweredEntityIndexCount,
			published.presentation.loweredEntityBatchCount,
			published.presentation.modelEntityCount,
			published.presentation.primitiveEntityCount,
			published.presentation.unresolvedEntityCount,
			published.presentation.loweredUiPrimitiveCount,
			published.presentation.texturedUiPrimitiveCount,
			published.presentation.msdfUiPrimitiveCount );
		s_module.lastLoggedWorldDigest = published.frontend.worldDigest;
		if ( completeContent ) s_module.loggedCompleteContentReceipt = qtrue;
	}
	if ( !s_module.registrationFramePublished && s_module.imports.LogCh
			&& s_module.initLogChannel >= 0 ) {
		s_module.imports.LogCh( s_module.initLogChannel, SEV_INFO,
			"Wired native Metal RAL: registration frame presented generation=%llu owner=%llu\n",
			(unsigned long long)published.frameGeneration,
			(unsigned long long)published.moduleGeneration );
	}
	s_module.registrationFramePublished = qtrue;
}

static void Shutdown( refShutdownCode_t code ) {
	ralFrameShellReceipt_t canceled, shutdown;
	ralPresentationHostCloseMode_t closeMode;
	if ( !s_module.loaded ) return;
	if ( s_module.frameOpen && Ral_FrameShellCancel( &s_module.frameShell,
			&s_module.frameReceipt, &canceled ) ) {
		s_module.frameReceipt = canceled; s_module.frameOpen = qfalse;
	}
	RenderSubmission_CancelFrame( &s_module.frontend );
	/* Loading UI frames remain legal between level teardown and the next
	 * BeginRegistration.  Keep the live registration and presentation owners
	 * across REF_LEVEL_ONLY, matching the engine's map-transition contract. */
	if ( code == REF_LEVEL_ONLY ) { s_module.loadedWorld = NULL; return; }
	s_module.registered = qfalse;
	if ( s_module.screenshotCommandsRegistered ) {
		R_ScreenshotUnregisterCommands();
		s_module.screenshotCommandsRegistered = qfalse;
	}
	s_module.screenshotPending = qfalse; s_module.screenshotName[0] = '\0';
	if ( s_module.core && !Ral_FrameShellShutdown( &s_module.frameShell,
			&s_module.frameReceipt, &shutdown ) ) MarkFailed( "frame-shutdown" );
	RalMetal_PresentDestroy( s_module.presentation );
	s_module.presentation = NULL;
	closeMode = ( code == REF_KEEP_WINDOW )
		? RAL_PRESENTATION_HOST_KEEP_OWNER
		: RAL_PRESENTATION_HOST_DESTROY_OWNER;
	if ( Ral_PresentationHostReceiptValid( &s_module.hostReceipt )
			&& !s_module.imports.PresentationHost.close(
				s_module.imports.PresentationHost.context,
				&s_module.hostReceipt, closeMode ) ) MarkFailed( "host-close" );
	RalMetal_CoreDestroy( s_module.core ); s_module.core = NULL;
	RenderSubmission_Reset( &s_module.frontend );
	memset( &s_module.published, 0, sizeof( s_module.published ) );
	memset( &s_module.hostReceipt, 0, sizeof( s_module.hostReceipt ) );
	memset( &s_module.surfaceBorrow, 0, sizeof( s_module.surfaceBorrow ) );
	memset( &s_module.presentationReceipt, 0,
		sizeof( s_module.presentationReceipt ) );
	memset( &s_module.coreReceipt, 0, sizeof( s_module.coreReceipt ) );
	if ( s_module.imports.LogCh && s_module.initLogChannel >= 0 ) {
		s_module.imports.LogCh( s_module.initLogChannel, SEV_INFO,
			"Wired native Metal RAL: shutdown complete generation=%llu\n",
			(unsigned long long)s_module.moduleGeneration );
	}
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
	exports->RegisterModel = RegisterModel; exports->RegisterSkin = RegisterSkin;
	exports->RegisterShader = RegisterMaterial; exports->RegisterShaderNoMip = RegisterMaterialNoMip;
	exports->RegisterShaderLightMap = RegisterLightMap; exports->RegisterMSDFShader = RegisterMsdf;
	exports->RegisterPrimitiveShader = RegisterPrimitiveMaterial; exports->PinShaderImages = NoopHandle;
	exports->LoadWorld = SubmitWorld; exports->SetWorldVisData = NoopBytes;
	exports->EndRegistration = EndRegistration; exports->ClearScene = SubmitClearScene;
	exports->AddRefEntityToScene = SubmitEntity; exports->AddPolyToScene = SubmitPoly;
	exports->LightForPoint = NoLight; exports->AddLightToScene = NoopLight;
	exports->AddAdditiveLightToScene = NoopLight; exports->AddLinearLightToScene = NoopLinearLight;
	exports->AddRibbonToScene = NoopRibbon; exports->AddBeamToScene = NoopBeam;
	exports->AddSpriteToScene = NoopSprite; exports->EmitParticles = NoopEmitter;
	exports->AddDecalToScene = NoopDecal; exports->RegisterParticleClass = NoopParticleClass;
	exports->SetAtmosphere = NoopAtmosphere; exports->SetAtmosphereHeightgrid = NoopHeightgrid;
	exports->AddLensSourceToScene = NoopLens; exports->GetLensVisibility = NoLensVisibility;
	exports->RenderScene = SubmitScene; exports->SetColor = SubmitColor;
	exports->SetMSDFOutline = NoopMsdfOutline; exports->SetMSDFShadow = NoopMsdfShadow;
	exports->SetClipRegion = NoopClip; exports->DrawStretchPic = NoopPic;
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
	exports->AddRefEntityToSceneTemporal = SubmitEntityTemporal;
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
	s_module.initLogChannel = -1;
	s_module.imports = *imports;
	ri = *imports;
	if ( imports->GetLogChannel && imports->LogCh ) {
		s_module.initLogChannel = imports->GetLogChannel( "renderer.init" );
	}
	FillExports( &s_module.exports );
	s_module.loaded = qtrue;
	return &s_module.exports;
}
