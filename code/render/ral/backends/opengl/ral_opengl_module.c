// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_opengl_module.h"
#include "ral_opengl_lighting.h"
#include "maps/map_format_registry.h"
#include "render_image_decode.h"
#include "render_lighting_project_cook.h"
#include "render_lighting_sidecar.h"
#include "render_material_script.h"
#include "tr_public.h"
#include "tr_screenshot.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	refimport_t					  imports;
	refexport_t					  exports;
	ralOpenGlCore_t				 *core;
	ralOpenGlCoreReceipt_t		  coreReceipt;
	ralOpenGlProduct_t			 *product;
	renderSubmissionState_t		  frontend;
	renderMaterialScriptCatalog_t materialScripts;
	renderLightingSidecarReceipt_t lightingSidecar;
	renderIrradianceSidecarReceipt_t irradianceSidecar;
	ralOpenGlLighting_t			 *directionalLighting;
	ralOpenGlLightingReceipt_t	  directionalLightingReceipt;
	ralOpenGlModuleFrameReceipt_t published;
	glconfig_t					  config;
	const mapFile_t				 *loadedWorld;
	const mapFile_t				 *loadedWorlds[MAX_RENDER_WORLDS];
	int                               activeWorldIndex;
	uint64_t					  moduleGeneration;
	uint64_t					  nextGeneration;
	uint64_t					  currentFrameGeneration;
	uint64_t					  presentedFrames;
	uint64_t					  lastLoggedWorldDigest;
	qboolean					  loggedCompleteContentReceipt;
	qhandle_t					  defaultMaterial;
	qboolean					  loaded;
	qboolean					  registered;
	qboolean					  frameOpen;
	qboolean					  glInitialized;
	qboolean					  screenshotCommandsRegistered;
	qboolean					  screenshotPending;
	qboolean					  screenshotSilent;
	qboolean					  loggedInvalidEntityOrigin;
	char						  screenshotName[MAX_OSPATH];
	int							  logChannel;
	int							  receiptLogChannel;
	cvar_t						 *brightness;
	cvar_t						  brightnessFallback;
} wiredOpenGlModuleState_t;

static wiredOpenGlModuleState_t s_module;
static uint64_t					s_moduleCounter;
refimport_t						ri;

static uint64_t NextGeneration( void )
{
	if ( s_module.nextGeneration >= UINT64_MAX - 1u )
		return 0u;
	return ++s_module.nextGeneration;
}

static qboolean ReceiptValid( const ralOpenGlModuleFrameReceipt_t *receipt )
{
	return receipt && receipt->schemaVersion == RAL_OPENGL_MODULE_SCHEMA_VERSION &&
		   receipt->backendType == RAL_BACKEND_OPENGL && receipt->moduleGeneration && receipt->frameGeneration &&
		   RalOpenGl_CoreReceiptExact( &receipt->core, &receipt->core ) &&
		   RenderSubmission_ReceiptExact( &receipt->frontend, &receipt->frontend ) &&
		   RalOpenGl_ProductFrameReceiptExact( &receipt->product, &receipt->product ) &&
		   Ral_AtmospherePlanReceiptExact( &receipt->atmosphere, &receipt->atmosphere ) &&
		   receipt->frontend.ownerGeneration == receipt->moduleGeneration &&
		   receipt->frontend.frameGeneration == receipt->frameGeneration &&
		   receipt->product.productGeneration == receipt->moduleGeneration &&
		   receipt->product.frameGeneration == receipt->frameGeneration &&
		   receipt->atmosphere.frameGeneration == receipt->frameGeneration &&
		   !memcmp( &receipt->atmosphere, &receipt->product.atmosphere, sizeof( receipt->atmosphere ) ) &&
		   receipt->product.plan.frontendFrameDigest == receipt->frontend.frameDigest &&
		   receipt->product.unresolvedCount == 0u && receipt->product.fallbackCount == 0u &&
		   receipt->product.fatalCount == 0u && receipt->ready == qtrue;
}

Q_EXPORT qboolean RalOpenGl_ModuleFrameReceiptExact( const ralOpenGlModuleFrameReceipt_t *a,
													 const ralOpenGlModuleFrameReceipt_t *b )
{
	return ReceiptValid( a ) && ReceiptValid( b ) && !memcmp( a, b, sizeof( *a ) );
}

Q_EXPORT qboolean WiredOpenGl_GetFrameReceipt( ralOpenGlModuleFrameReceipt_t *outReceipt )
{
	if ( !outReceipt || !ReceiptValid( &s_module.published ) )
		return qfalse;
	*outReceipt = s_module.published;
	return qtrue;
}

static void LogFailure( const char *reason )
{
	if ( !s_module.exports.initFailed && s_module.imports.LogCh && s_module.logChannel >= 0 )
		s_module.imports.LogCh( s_module.logChannel, SEV_WARN, "Wired native OpenGL RAL: lifecycle failure (%s)\n",
								reason );
	s_module.exports.initFailed = qtrue;
}

qboolean RB_ScheduleScreenshot( int typeMask, const char *fileName, qboolean silent )
{
	if ( typeMask != SCREENSHOT_PNG || !fileName || !fileName[0] || s_module.screenshotPending || !s_module.product ||
		 strlen( fileName ) >= sizeof( s_module.screenshotName ) )
		return qfalse;
	(void)snprintf( s_module.screenshotName, sizeof( s_module.screenshotName ), "%s", fileName );
	s_module.screenshotSilent  = silent;
	s_module.screenshotPending = qtrue;
	return qtrue;
}

void R_LevelShot( void )
{
	if ( s_module.imports.LogCh && s_module.logChannel >= 0 )
		s_module.imports.LogCh( s_module.logChannel, SEV_WARN,
								"Wired native OpenGL RAL: levelshot capture is not supported\n" );
}

static qboolean CompleteScreenshot( void )
{
	byte	*pixels;
	uint64_t bytes;
	if ( !s_module.screenshotPending )
		return qtrue;
	bytes = (uint64_t)s_module.config.vidWidth * (uint64_t)s_module.config.vidHeight * 3u;
	if ( s_module.config.vidWidth <= 0 || s_module.config.vidHeight <= 0 || bytes > UINT32_MAX )
		return qfalse;
	pixels = (byte *)malloc( (size_t)bytes );
	if ( !pixels )
		return qfalse;
	if ( !RalOpenGl_ProductReadbackRgb( s_module.product, pixels, (uint32_t)bytes ) ||
		 !R_SavePNG( s_module.screenshotName, pixels, s_module.config.vidWidth, s_module.config.vidHeight ) ) {
		free( pixels );
		return qfalse;
	}
	free( pixels );
	if ( !s_module.screenshotSilent )
		R_ScreenshotPrintSaved( s_module.screenshotName );
	s_module.screenshotPending = qfalse;
	s_module.screenshotName[0] = '\0';
	return qtrue;
}

static ralOpenGlProc_t ResolveGl( const char *name, void *userData )
{
	wiredOpenGlModuleState_t *module = (wiredOpenGlModuleState_t *)userData;
	ralOpenGlProc_t			  proc;
	if ( !module || !module->imports.GL_GetProcAddress )
		return NULL;
	proc = (ralOpenGlProc_t)module->imports.GL_GetProcAddress( name );
	if ( !proc && module->imports.LogCh && module->logChannel >= 0 )
		module->imports.LogCh( module->logChannel, SEV_WARN,
							   "Wired native OpenGL RAL: required entry point unavailable (%s)\n",
							   name ? name : "<null>" );
	return proc;
}

static qhandle_t RegisterImageSource( renderAssetKind_t kind, const char *name, const char *imageName, qboolean clamp )
{
	static const byte			white[4] = { 255u, 255u, 255u, 255u };
	renderMaterialScriptEntry_t scripted;
	byte					   *pixels = NULL;
	uint32_t					width = 0u, height = 0u;
	char						resolved[MAX_QPATH];
	qhandle_t					handle;
	memset( &scripted, 0, sizeof( scripted ) );
	if ( name && RenderMaterialScript_Lookup( &s_module.materialScripts, name, &scripted ) ) {
		imageName = scripted.imageName;
		clamp	  = scripted.clampToEdge;
	}
	/* BSP catalogs use the exact `noshader` sentinel for geometry without an
	 * authored material. It is a defined neutral material, not a missing-asset
	 * fallback; every other unresolved authored name remains a hard failure. */
	if ( name && ( name[0] == '*' || !strcmp( name, "noshader" ) ) )
		return RenderSubmission_RegisterMaterialImage( &s_module.frontend, kind, name, qtrue, white, 1u, 1u );
	if ( imageName && RenderImage_DecodeRgba8( imageName, &pixels, &width, &height, resolved ) ) {
		const char *identityName = scripted.name[0] ? name : resolved;
		handle = RenderSubmission_RegisterMaterialImage( &s_module.frontend, kind,
			identityName, clamp, pixels, width, height );
		ri.Free( pixels );
		if ( handle && scripted.name[0] &&
			 !RenderSubmission_SetMaterialRasterPolicy( &s_module.frontend, handle, scripted.alphaMode,
													scripted.alphaCutoff, scripted.depthWrite ) )
			return 0;
		if ( handle && scripted.hasLighting &&
			 !RenderMaterialScript_ApplyLighting( &s_module.materialScripts, name,
				 &s_module.frontend, handle ) ) return 0;
		return handle;
	}
	return 0;
}

static qhandle_t RegisterImage( renderAssetKind_t kind, const char *name, qboolean clamp )
{
	return RegisterImageSource( kind, name, name, clamp );
}

static qhandle_t RegisterModel( const char *name )
{
	void				 *bytes = NULL;
	renderModelSnapshot_t model;
	qhandle_t			  handle;
	int					  byteCount;
	char				  canonical[MAX_QPATH];
	if ( !name || !name[0] )
		return 0;
	if ( name[0] == '*' && s_module.loadedWorld ) {
		char *end	= NULL;
		long  index = strtol( name + 1, &end, 10 );
		if ( end != name + 1 && *end == '\0' && index > 0 && index < s_module.loadedWorld->numSubModels ) {
			const dmodel_t *source = &s_module.loadedWorld->subModels[index];
			return RenderSubmission_RegisterInlineModel( &s_module.frontend, name, (uint32_t)source->firstSurface,
														 (uint32_t)source->numSurfaces );
		}
	}
	if ( s_module.imports.FS_ResolveResource
			&& s_module.imports.FS_ResolveResource( name, canonical,
				sizeof( canonical ), NULL, NULL, NULL ) ) name = canonical;
	if ( !s_module.imports.FS_ReadFile || !s_module.imports.FS_FreeFile )
		return 0;
	byteCount = s_module.imports.FS_ReadFile( name, &bytes );
	if ( byteCount <= 0 || !bytes ) {
		if ( bytes )
			s_module.imports.FS_FreeFile( bytes );
		return 0;
	}
	handle = RenderSubmission_RegisterModelData( &s_module.frontend, name, bytes, (uint32_t)byteCount );
	s_module.imports.FS_FreeFile( bytes );
	if ( !handle || !RenderSubmission_ModelSnapshot( &s_module.frontend, handle, &model ) )
		return 0;
	for ( uint32_t i = 0u; i < model.batchCount; ++i ) {
		qhandle_t material = RegisterImage( RENDER_ASSET_MATERIAL, model.batches[i].materialName, qfalse );
		if ( !material || !RenderSubmission_SetModelBatchMaterial( &s_module.frontend, handle, i, material ) )
			return 0;
	}
	return handle;
}

static qhandle_t RegisterSkin( const char *name )
{
	return RenderSubmission_RegisterAsset( &s_module.frontend, RENDER_ASSET_SKIN, name );
}
static qhandle_t RegisterShader( const char *name )
{
	return RegisterImage( RENDER_ASSET_MATERIAL, name, qfalse );
}
static qhandle_t RegisterShaderNoMip( const char *name )
{
	return RegisterImage( RENDER_ASSET_MATERIAL, name, qtrue );
}
static qhandle_t RegisterLightMap( const char *name, int lightmap )
{
	(void)lightmap;
	return RegisterImage( RENDER_ASSET_LIGHTMAP, name, qtrue );
}
static qhandle_t RegisterMsdf( const char *name, float range, int width, int height )
{
	char   imageName[MAX_QPATH];
	size_t length;
	(void)range;
	(void)width;
	(void)height;
	if ( !name || ( length = strlen( name ) ) < 6u || strcmp( name + length - 6u, "_atlas" ) )
		return RegisterImage( RENDER_ASSET_MSDF, name, qtrue );
	if ( length - 6u + 4u >= sizeof( imageName ) )
		return 0;
	memcpy( imageName, name, length - 6u );
	memcpy( imageName + length - 6u, ".png", 5u );
	return RegisterImageSource( RENDER_ASSET_MSDF, name, imageName, qtrue );
}
static qhandle_t RegisterPrimitive( const char *name )
{
	return RegisterImage( RENDER_ASSET_PRIMITIVE_MATERIAL, name, qfalse );
}

static qboolean PrepareWorldMaterials( const mapFile_t *bsp )
{
	byte	*rgba = NULL;
	uint32_t side = 0u;
	uint64_t pixels;
	if ( !bsp || bsp->numSurfaces < 0 || bsp->numShaders < 0 || bsp->numLightmapPages < 0 ||
		 bsp->lightmapPageSize < 0 || ( bsp->numSurfaces && !bsp->surfaces ) || ( bsp->numShaders && !bsp->shaders ) ) {
		if ( s_module.imports.LogCh && s_module.logChannel >= 0 )
			s_module.imports.LogCh( s_module.logChannel, SEV_WARN,
									"Wired native OpenGL RAL: invalid world material catalog\n" );
		return qfalse;
	}
	for ( int i = 0; i < bsp->numSurfaces; ++i ) {
		const dsurface_t *surface = &bsp->surfaces[i];
		const dshader_t	 *shader;
		if ( surface->surfaceType == MST_BAD || surface->surfaceType == MST_FLARE )
			continue;
		if ( surface->shaderNum < 0 || surface->shaderNum >= bsp->numShaders ) {
			if ( s_module.imports.LogCh && s_module.logChannel >= 0 )
				s_module.imports.LogCh(
					s_module.logChannel, SEV_WARN,
					"Wired native OpenGL RAL: world surface shader index invalid surface=%d shader=%d\n", i,
					surface->shaderNum );
			return qfalse;
		}
		shader = &bsp->shaders[surface->shaderNum];
		if ( shader->surfaceFlags & ( SURF_NODRAW | SURF_SKIP ) )
			continue;
		if ( !RenderSubmission_MaterialHandle( &s_module.frontend, shader->shader ) &&
			 !RegisterShader( shader->shader ) ) {
			if ( s_module.imports.LogCh && s_module.logChannel >= 0 )
				s_module.imports.LogCh(
					s_module.logChannel, SEV_WARN,
					"Wired native OpenGL RAL: world material registration failed surface=%d shader=%d name=%s\n", i,
					surface->shaderNum, shader->shader );
			return qfalse;
		}
	}
	if ( bsp->numLightmapPages == 0 )
		return qtrue;
	if ( !bsp->lightmapData || bsp->lightmapPageSize <= 0 || ( bsp->lightmapPageSize % 3 ) != 0 )
		return qfalse;
	pixels = (uint32_t)bsp->lightmapPageSize / 3u;
	if ( !pixels || pixels > RENDER_SUBMISSION_MAX_MATERIAL_BYTES / 4u )
		return qfalse;
	for ( side = 1u; (uint64_t)side * side < pixels; ++side )
		if ( side >= RENDER_SUBMISSION_MAX_IMAGE_DIMENSION )
			return qfalse;
	if ( (uint64_t)side * side != pixels || side > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION )
		return qfalse;
	rgba = (byte *)ri.Malloc( (int)( pixels * 4u ) );
	if ( !rgba )
		return qfalse;
	for ( int i = 0; i < bsp->numLightmapPages; ++i ) {
		char		name[MAX_QPATH];
		const byte *rgb = bsp->lightmapData + (size_t)i * (size_t)bsp->lightmapPageSize;
		for ( uint64_t pixel = 0u; pixel < pixels; ++pixel ) {
			rgba[pixel * 4u + 0u] = rgb[pixel * 3u + 0u];
			rgba[pixel * 4u + 1u] = rgb[pixel * 3u + 1u];
			rgba[pixel * 4u + 2u] = rgb[pixel * 3u + 2u];
			rgba[pixel * 4u + 3u] = 255u;
		}
		if ( !RenderSubmission_LightmapMaterialName( name, (uint32_t)bsp->checksum, i ) ||
			 !RenderSubmission_RegisterMaterialImage( &s_module.frontend, RENDER_ASSET_LIGHTMAP, name, qtrue, rgba,
													  side, side ) ) {
			if ( s_module.imports.LogCh && s_module.logChannel >= 0 )
				s_module.imports.LogCh( s_module.logChannel, SEV_WARN,
										"Wired native OpenGL RAL: world lightmap registration failed page=%d side=%u\n",
										i, side );
			ri.Free( rgba );
			return qfalse;
		}
	}
	ri.Free( rgba );
	return qtrue;
}

static void DestroyDirectionalLighting( void ) {
	if ( s_module.product )
		(void)RalOpenGl_ProductSetDirectionalLighting( s_module.product, NULL );
	if ( s_module.directionalLighting && s_module.core )
		(void)RalOpenGl_LightingDestroy( s_module.core, &s_module.coreReceipt,
			s_module.directionalLighting, &s_module.directionalLightingReceipt );
	s_module.directionalLighting = NULL;
	memset( &s_module.directionalLightingReceipt, 0,
		sizeof( s_module.directionalLightingReceipt ) );
}

static qboolean UploadDirectionalLighting( void ) {
	const renderDirectionalLightingRecord_t *record;
	ralLightingRuntimePlan_t plan;
	ralStaticLightingCapabilities_t capabilities = { qtrue, qtrue, qtrue, qtrue };
	uint64_t digest;
	DestroyDirectionalLighting();
	if ( s_module.lightingSidecar.status ==
			RENDER_LIGHTING_SIDECAR_MISSING_COMPATIBILITY ) return qtrue;
	if ( !RenderSubmission_DirectionalLightingSnapshot( &s_module.frontend,
			&record, &digest ) || !record || !digest
			|| !Ral_LightingRuntimePlanBuild( RAL_BACKEND_OPENGL,
				NextGeneration(), record->ownedArtifactBytes,
				record->artifactByteLength, &record->artifact, &capabilities,
				&plan ) ) return qfalse;
	if ( !RalOpenGl_LightingUpload( s_module.core, &s_module.coreReceipt,
		record->ownedArtifactBytes, record->artifactByteLength, &plan,
		&s_module.directionalLighting, &s_module.directionalLightingReceipt ) )
		return qfalse;
	if ( !RalOpenGl_ProductSetDirectionalLighting( s_module.product,
			&s_module.directionalLightingReceipt ) ) {
		DestroyDirectionalLighting();
		return qfalse;
	}
	return qtrue;
}

static void LoadWorld( const mapFile_t *bsp, int worldIndex )
{
	if ( !PrepareWorldMaterials( bsp ) )
		LogFailure( "frontend-world-materials" );
	else if ( !RenderSubmission_LoadWorld( &s_module.frontend, bsp, worldIndex ) )
		LogFailure( "frontend-world-geometry" );
	else if ( !RenderLightingSidecar_LoadDirectional( &s_module.frontend,
			&s_module.imports, bsp->name, &s_module.lightingSidecar ) )
		LogFailure( "frontend-directional-lighting-sidecar" );
	else if ( !RenderLightingSidecar_LoadIrradiance( &s_module.frontend,
			&s_module.imports, bsp->name, &s_module.irradianceSidecar ) )
		LogFailure( "frontend-irradiance-lighting-sidecar" );
	else if ( !UploadDirectionalLighting() )
		LogFailure( "native-directional-lighting-upload" );
	else {
		s_module.loadedWorlds[worldIndex] = bsp;
		s_module.activeWorldIndex = worldIndex;
		s_module.loadedWorld = bsp;
	}
}

static qboolean SelectWorld( int worldIndex )
{
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS
			|| !s_module.loadedWorlds[worldIndex]
			|| !RenderSubmission_SelectWorld( &s_module.frontend, worldIndex ) ) return qfalse;
	s_module.activeWorldIndex = worldIndex;
	s_module.loadedWorld = s_module.loadedWorlds[worldIndex];
	return qtrue;
}

static qboolean UnloadWorld( int worldIndex )
{
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS
			|| !RenderSubmission_UnloadWorld( &s_module.frontend, worldIndex ) ) return qfalse;
	s_module.loadedWorlds[worldIndex] = NULL;
	if ( s_module.activeWorldIndex == worldIndex ) {
		s_module.loadedWorld = NULL;
		for ( int i = 0; i < MAX_RENDER_WORLDS; ++i )
			if ( s_module.loadedWorlds[i] ) { (void)SelectWorld( i ); break; }
	}
	return qtrue;
}

static int ResidentWorldCount( void )
{
	return RenderSubmission_ResidentWorldCount( &s_module.frontend );
}

static qboolean CookLightingProject( const char *derivedRoot )
{
	renderLightingProjectCookRequest_t request;
	renderLightingProjectCookReceipt_t receipt;
	char worldStem[RENDER_LIGHTING_PROJECT_WORLD_STEM_CAPACITY];
	return s_module.loadedWorld &&
		RenderLightingProjectCook_DefaultRequest( s_module.loadedWorld,
			derivedRoot, &request, worldStem ) &&
		RenderLightingProjectCook_Execute( &s_module.frontend,
			s_module.loadedWorld, &s_module.imports, &request, &receipt ) &&
		RenderLightingProjectCook_ReceiptValid( &receipt );
}

static qboolean InitializeOwners( void )
{
	ralOpenGlCoreCreateInfo_t info;
	uint64_t				  coreGeneration = NextGeneration();
	if ( !coreGeneration || !s_module.imports.GLimp_InitOpenGL46 || !s_module.imports.GLimp_Shutdown ||
		 !s_module.imports.GLimp_EndFrame || !s_module.imports.GL_GetProcAddress )
		return qfalse;
	memset( &s_module.config, 0, sizeof( s_module.config ) );
	s_module.imports.GLimp_InitOpenGL46( &s_module.config );
	s_module.glInitialized = qtrue;
	memset( &info, 0, sizeof( info ) );
	info.generation		 = coreGeneration;
	info.contextIdentity = (uintptr_t)&s_module;
	info.resolveProc	 = ResolveGl;
	info.resolveUserData = &s_module;
	if ( !RalOpenGl_CoreCreate( &info, &s_module.core, &s_module.coreReceipt ) ) {
		LogFailure( "core-create" );
		goto fail;
	}
	if ( !RalOpenGl_ProductCreate( s_module.core, &s_module.coreReceipt, s_module.moduleGeneration,
								   &s_module.product ) ) {
		LogFailure( "product-create" );
		goto fail;
	}
	if ( !RalOpenGl_ProductSetOutputExtent( s_module.product, (uint32_t)s_module.config.vidWidth,
											(uint32_t)s_module.config.vidHeight ) ) {
		LogFailure( "product-output-extent" );
		goto fail;
	}
	(void)snprintf( s_module.config.renderer_string, sizeof( s_module.config.renderer_string ),
					"Wired native OpenGL 4.6 RAL" );
	(void)snprintf( s_module.config.vendor_string, sizeof( s_module.config.vendor_string ), "%s",
					s_module.coreReceipt.caps.vendorName );
	(void)snprintf( s_module.config.version_string, sizeof( s_module.config.version_string ), "OpenGL %u.%u Core",
					s_module.coreReceipt.versionMajor, s_module.coreReceipt.versionMinor );
	s_module.config.maxTextureSize	= (int)s_module.coreReceipt.caps.maxTextureDimension2D;
	s_module.config.numTextureUnits = (int)s_module.coreReceipt.caps.maxSampledTexturesPerShaderStage;
	return qtrue;
fail:
	RalOpenGl_ProductDestroy( s_module.product );
	s_module.product = NULL;
	RalOpenGl_CoreDestroy( s_module.core );
	s_module.core = NULL;
	s_module.imports.GLimp_Shutdown( qtrue );
	s_module.glInitialized = qfalse;
	return qfalse;
}

static void BeginRegistration( glconfig_t *config )
{
	if ( !config || !s_module.loaded || s_module.frameOpen ) {
		LogFailure( "begin-registration-state" );
		return;
	}
	if ( !s_module.core && !InitializeOwners() ) {
		LogFailure( "owner-initialization" );
		return;
	}
	if ( !s_module.brightness ) {
		if ( !ri.Cvar_Get || !ri.Cvar_CheckRange ) {
			memset( &s_module.brightnessFallback, 0,
				sizeof( s_module.brightnessFallback ) );
			s_module.brightnessFallback.value = 1.0f;
			s_module.brightnessFallback.integer = 1;
			s_module.brightness = &s_module.brightnessFallback;
		} else {
			s_module.brightness = ri.Cvar_Get( "r_brightness", "1",
				CVAR_ARCHIVE | CVAR_NODEFAULT );
			if ( !s_module.brightness ) {
				LogFailure( "display-visibility-cvar" );
				return;
			}
			ri.Cvar_CheckRange( s_module.brightness, "0", "32", CV_FLOAT );
			if ( ri.Cvar_SetDescription ) ri.Cvar_SetDescription( s_module.brightness,
				"Continuous display visibility scalar; 1.0 is authored identity, fractional values are preserved." );
			if ( ri.Cvar_SetGroup ) ri.Cvar_SetGroup( s_module.brightness, CVG_RENDERER );
		}
	}
	if ( !s_module.frontend.initialized && !RenderSubmission_Init( &s_module.frontend, s_module.moduleGeneration ) ) {
		LogFailure( "frontend-initialization" );
		return;
	}
	if ( !s_module.materialScripts.ready && !RenderMaterialScript_Load( &s_module.materialScripts, &ri ) ) {
		LogFailure( "material-script-catalog" );
		return;
	}
	if ( !s_module.defaultMaterial )
		s_module.defaultMaterial = RegisterImage( RENDER_ASSET_MATERIAL, "*white", qtrue );
	if ( !s_module.defaultMaterial ) {
		LogFailure( "default-material" );
		return;
	}
	if ( !s_module.screenshotCommandsRegistered && ri.Cmd_AddCommand && ri.Cmd_RemoveCommand ) {
		R_ScreenshotRegisterCommands();
		s_module.screenshotCommandsRegistered = qtrue;
	}
	s_module.registered = qtrue;
	*config				= s_module.config;
	if ( s_module.imports.LogCh && s_module.receiptLogChannel >= 0 )
		s_module.imports.LogCh( s_module.receiptLogChannel, SEV_INFO,
								"Wired native OpenGL RAL: registration ready logical=%dx%d version=%u.%u\n",
								s_module.config.vidWidth, s_module.config.vidHeight, s_module.coreReceipt.versionMajor,
								s_module.coreReceipt.versionMinor );
}

static void EndRegistration( void )
{
	if ( !s_module.registered || s_module.frameOpen )
		LogFailure( "end-registration-state" );
}

static void BeginFrame( stereoFrame_t stereo )
{
	uint64_t generation = NextGeneration();
	(void)stereo;
	if ( !generation || !s_module.registered || s_module.frameOpen ||
		 !RenderSubmission_BeginFrame( &s_module.frontend, generation ) ) {
		LogFailure( "begin-frame-state" );
		return;
	}
	s_module.currentFrameGeneration = generation;
	s_module.frameOpen				= qtrue;
}

static void EndFrame( int *frontEndMsec, int *backEndMsec )
{
	ralOpenGlModuleFrameReceipt_t  receipt;
	ralOpenGlFrontendPlanReceipt_t planProbe;
	ralDisplayVisibilityPlan_t visibility;
	qboolean					   completeContent, worldChanged, logContent;
	if ( frontEndMsec )
		*frontEndMsec = 0;
	if ( backEndMsec )
		*backEndMsec = 0;
	if ( !s_module.frameOpen ) {
		LogFailure( "end-frame-state" );
		return;
	}
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion	 = RAL_OPENGL_MODULE_SCHEMA_VERSION;
	receipt.backendType		 = RAL_BACKEND_OPENGL;
	receipt.moduleGeneration = s_module.moduleGeneration;
	receipt.frameGeneration	 = s_module.currentFrameGeneration;
	if ( !RenderSubmission_EndFrame( &s_module.frontend, receipt.frameGeneration, &receipt.frontend ) ) {
		s_module.frameOpen				= qfalse;
		s_module.currentFrameGeneration = 0u;
		LogFailure( "frontend-end-frame" );
		return;
	}
	memset( &planProbe, 0, sizeof( planProbe ) );
	if ( !RalOpenGl_FrontendPlanBuild( &s_module.coreReceipt, &s_module.frontend, &receipt.frontend,
									   receipt.frameGeneration, &planProbe ) ) {
		s_module.frameOpen				= qfalse;
		s_module.currentFrameGeneration = 0u;
		LogFailure( "frontend-plan" );
		return;
	}
	if ( !s_module.brightness
			|| !Ral_DisplayVisibilityPlanBuild( s_module.brightness->value, &visibility )
			|| !RalOpenGl_ProductSetDisplayVisibility( s_module.product, &visibility ) ) {
		s_module.frameOpen = qfalse;
		s_module.currentFrameGeneration = 0u;
		LogFailure( "display-visibility" );
		return;
	}
	if ( !RalOpenGl_ProductRender( s_module.product, &s_module.frontend, &receipt.frontend, receipt.frameGeneration,
								   &receipt.product ) ) {
		s_module.frameOpen				= qfalse;
		s_module.currentFrameGeneration = 0u;
		LogFailure( "product-render" );
		return;
	}
	receipt.atmosphere = receipt.product.atmosphere;
	if ( !CompleteScreenshot() ) {
		s_module.frameOpen				= qfalse;
		s_module.currentFrameGeneration = 0u;
		LogFailure( "screenshot-capture" );
		return;
	}
	s_module.imports.GLimp_EndFrame();
	receipt.presentedFrames			= ++s_module.presentedFrames;
	receipt.core					= s_module.coreReceipt;
	receipt.ready					= qtrue;
	s_module.frameOpen				= qfalse;
	s_module.currentFrameGeneration = 0u;
	if ( !ReceiptValid( &receipt ) ) {
		LogFailure( "published-receipt" );
		return;
	}
	s_module.published = receipt;
	completeContent	   = receipt.product.plan.loweredWorldBatchCount > 0u &&
					  receipt.product.plan.loweredModelEntityCount > 0u &&
					  receipt.product.plan.loweredUiPrimitiveCount > 0u;
	worldChanged = receipt.frontend.worldDigest != s_module.lastLoggedWorldDigest;
	if ( worldChanged )
		s_module.loggedCompleteContentReceipt = qfalse;
	logContent = worldChanged || ( completeContent && !s_module.loggedCompleteContentReceipt );
	if ( receipt.frontend.worldLoaded && receipt.frontend.sceneRendered && logContent && s_module.imports.LogCh &&
		 s_module.receiptLogChannel >= 0 ) {
		s_module.imports.LogCh(
			s_module.receiptLogChannel, SEV_INFO,
			"Wired native OpenGL RAL: content receipt map=%s asset=%016llx world=%016llx frame=%016llx surfaces=%u "
			"vertices=%u indices=%u assets=%u materials=%u resolvedMaterials=%u materialBytes=%u models=%u "
			"modelBytes=%u entities=%u temporalEntities=%u polygons=%u lights=%u ui=%u loweredWorld=%u worldBatches=%u "
			"loweredEntity=%u entityBatches=%u modelEntities=%u primitiveEntities=%u loweredPolygons=%u "
			"loweredLights=%u effectBatches=%u loweredUi=%u texturedUi=%u msdfUi=%u nativeDraws=%u unresolved=%u "
			"fallback=%u fatal=%u\n",
			s_module.loadedWorld ? s_module.loadedWorld->name : "<unknown>",
			(unsigned long long)receipt.frontend.assetDigest, (unsigned long long)receipt.frontend.worldDigest,
			(unsigned long long)receipt.frontend.frameDigest, receipt.frontend.worldSurfaceCount,
			receipt.frontend.worldVertexCount, receipt.frontend.worldIndexCount, receipt.frontend.registeredAssetCount,
			receipt.frontend.registeredMaterialCount, receipt.frontend.resolvedMaterialCount,
			receipt.frontend.materialBytes, receipt.frontend.registeredModelCount, receipt.frontend.modelBytes,
			receipt.frontend.entityCount, receipt.frontend.temporalEntityCount, receipt.frontend.polygonCount,
			receipt.frontend.lightCount, receipt.frontend.uiPrimitiveCount, receipt.product.plan.loweredWorldIndexCount,
			receipt.product.plan.loweredWorldBatchCount, receipt.product.plan.loweredEntityIndexCount,
			receipt.product.plan.loweredEntityBatchCount, receipt.product.plan.loweredModelEntityCount,
			receipt.product.plan.loweredPrimitiveEntityCount, receipt.product.plan.loweredPolygonCount,
			receipt.product.plan.loweredLightCount, receipt.product.plan.loweredEffectBatchCount,
			receipt.product.plan.loweredUiPrimitiveCount, receipt.product.plan.texturedUiPrimitiveCount,
			receipt.product.plan.msdfUiPrimitiveCount, receipt.product.native.nativeDrawCount,
			receipt.product.unresolvedCount, receipt.product.fallbackCount, receipt.product.fatalCount );
		s_module.lastLoggedWorldDigest = receipt.frontend.worldDigest;
		if ( completeContent )
			s_module.loggedCompleteContentReceipt = qtrue;
	}
}

static void Shutdown( refShutdownCode_t code )
{
	if ( !s_module.loaded )
		return;
	if ( s_module.frameOpen )
		RenderSubmission_CancelFrame( &s_module.frontend );
	s_module.frameOpen				= qfalse;
	s_module.currentFrameGeneration = 0u;
	s_module.loadedWorld			= NULL;
	DestroyDirectionalLighting();
	if ( code == REF_LEVEL_ONLY ) {
		if ( !RenderSubmission_ResetEffectRegistries( &s_module.frontend ) )
			LogFailure( "level-effect-registry-reset" );
		return;
	}
	if ( s_module.screenshotCommandsRegistered && ri.Cmd_RemoveCommand ) {
		R_ScreenshotUnregisterCommands();
		s_module.screenshotCommandsRegistered = qfalse;
	}
	s_module.screenshotPending = qfalse;
	s_module.screenshotName[0] = '\0';
	RalOpenGl_ProductDestroy( s_module.product );
	s_module.product = NULL;
	RalOpenGl_CoreDestroy( s_module.core );
	s_module.core = NULL;
	RenderSubmission_Reset( &s_module.frontend );
	if ( s_module.glInitialized && s_module.imports.GLimp_Shutdown )
		s_module.imports.GLimp_Shutdown( code == REF_UNLOAD_DLL ? qtrue : qfalse );
	s_module.glInitialized = qfalse;
	if ( s_module.imports.LogCh && s_module.logChannel >= 0 )
		s_module.imports.LogCh( s_module.logChannel, SEV_INFO, "Wired native OpenGL RAL: shutdown complete code=%d\n",
								(int)code );
	memset( &s_module.published, 0, sizeof( s_module.published ) );
	s_module.loaded		= qfalse;
	s_module.registered = qfalse;
}

static void ClearScene( void )
{
	if ( s_module.frameOpen && !RenderSubmission_ClearScene( &s_module.frontend ) )
		LogFailure( "clear-scene" );
}
static qboolean PrepareEntity( const refEntity_t *entity, refEntity_t *outSubmitted )
{
	if ( !s_module.frameOpen || !entity || !outSubmitted )
		return qfalse;
	if ( (unsigned)entity->reType >= RT_MAX_REF_ENTITY_TYPE ) {
		if ( s_module.imports.Terminate )
			s_module.imports.Terminate( TERM_CLIENT_DROP, "OpenGL RAL AddEntity: bad reType %d", entity->reType );
		return qfalse;
	}
	if ( s_module.frontend.entityCount >= RENDER_SUBMISSION_MAX_ENTITIES ) {
		if ( s_module.imports.LogCh && s_module.logChannel >= 0 )
			s_module.imports.LogCh( s_module.logChannel, SEV_DEBUG,
									"Wired native OpenGL RAL: dropping entity at capacity\n" );
		return qfalse;
	}
	if ( isnan( entity->origin[0] ) || isnan( entity->origin[1] ) || isnan( entity->origin[2] ) ) {
		if ( !s_module.loggedInvalidEntityOrigin && s_module.imports.LogCh && s_module.logChannel >= 0 ) {
			s_module.loggedInvalidEntityOrigin = qtrue;
			s_module.imports.LogCh( s_module.logChannel, SEV_WARN,
									"Wired native OpenGL RAL: dropping entity with NaN origin\n" );
		}
		return qfalse;
	}
	*outSubmitted = *entity;
	if ( outSubmitted->reType == RT_MODEL && outSubmitted->hModel == 0 && outSubmitted->customShader == 0 )
		outSubmitted->customShader = s_module.defaultMaterial;
	return qtrue;
}
static void AddEntity( const refEntity_t *entity, qboolean shaderTime )
{
	refEntity_t submitted;
	const cmSkin_t *skin = NULL;
	(void)shaderTime;
	if ( !PrepareEntity( entity, &submitted ) )
		return;
	if ( submitted.characterSkin && s_module.imports.GetCharacterSkin )
		skin = s_module.imports.GetCharacterSkin( submitted.characterSkin );
	if ( !RenderSubmission_AddEntitySkinned( &s_module.frontend, &submitted, NULL, skin ) )
		LogFailure( "entity" );
}
static void AddEntityTemporal( const refEntity_t *entity, const refEntityMotion_t *motion )
{
	refEntity_t submitted;
	const cmSkin_t *skin = NULL;
	if ( !s_module.frameOpen || !entity || !RefEntityMotion_IsValid( motion ) )
		return;
	if ( !PrepareEntity( entity, &submitted ) )
		return;
	if ( submitted.characterSkin && s_module.imports.GetCharacterSkin )
		skin = s_module.imports.GetCharacterSkin( submitted.characterSkin );
	if ( !RenderSubmission_AddEntitySkinned( &s_module.frontend, &submitted, motion, skin ) )
		LogFailure( "temporal-entity" );
}
static void AddPoly( qhandle_t shader, int vertices, const polyVert_t *data, int count )
{
	if ( s_module.frameOpen && !RenderSubmission_AddPoly( &s_module.frontend, shader, vertices, data, count ) )
		LogFailure( "poly" );
}
static void AddLight( const vec3_t origin, float intensity, float red, float green, float blue )
{
	if ( s_module.frameOpen &&
		 !RenderSubmission_AddLight( &s_module.frontend, origin, NULL, intensity, red, green, blue ) )
		LogFailure( "light" );
}
static void AddLinearLight( const vec3_t start, const vec3_t end, float intensity, float red, float green, float blue )
{
	if ( s_module.frameOpen &&
		 !RenderSubmission_AddLight( &s_module.frontend, start, end, intensity, red, green, blue ) )
		LogFailure( "linear-light" );
}
static void RenderScene( const refdef_t *view, int worldIndex )
{
	if ( s_module.frameOpen && !RenderSubmission_RenderScene( &s_module.frontend, view, worldIndex ) )
		LogFailure( "scene" );
}
static void SetColor( const float *color )
{
	if ( !RenderSubmission_SetColor( &s_module.frontend, color ) )
		LogFailure( "color" );
}
static void SetUiTransform( const refUiTransform_t *transform )
{
	if ( !RenderSubmission_SetUiTransform( &s_module.frontend, transform ) )
		LogFailure( "ui-transform" );
}
static void DrawPic( float x, float y, float width, float height, float s1, float t1, float s2, float t2,
					 qhandle_t shader )
{
	if ( s_module.frameOpen &&
		 !RenderSubmission_AddUiQuad( &s_module.frontend, x, y, width, height, s1, t1, s2, t2, 0.0f, shader ) )
		LogFailure( "ui" );
}
static void DrawRotatedPic( float x, float y, float width, float height, float s1, float t1, float s2, float t2,
							float angle, qhandle_t shader )
{
	if ( s_module.frameOpen &&
		 !RenderSubmission_AddUiQuad( &s_module.frontend, x, y, width, height, s1, t1, s2, t2, angle, shader ) )
		LogFailure( "ui-rotated" );
}
static void DrawLine( float x1, float y1, float x2, float y2, float width, qhandle_t shader )
{
	if ( s_module.frameOpen && !RenderSubmission_AddUiLine( &s_module.frontend, x1, y1, x2, y2, width, shader ) )
		LogFailure( "ui-line" );
}
static void DrawBackdrop( float x, float y, float width, float height, float time, float mouseX, float mouseY,
						  float transition )
{
	DrawPic( x, y, width, height, time, mouseX, mouseY, transition, s_module.defaultMaterial );
}

static void NoopVoid( void )
{
}
static void NoopBool( qboolean value )
{
	(void)value;
}
static void NoopHandle( qhandle_t value )
{
	(void)value;
}
static void NoopBytes( const byte *value )
{
	(void)value;
}
static int NoLightForPoint( vec3_t point, vec3_t ambient, vec3_t directed, vec3_t direction )
{
	(void)point;
	if ( ambient )
		memset( ambient, 0, sizeof( vec3_t ) );
	if ( directed )
		memset( directed, 0, sizeof( vec3_t ) );
	if ( direction )
		memset( direction, 0, sizeof( vec3_t ) );
	return 0;
}
static void AddEffectRibbon( const ribbonDesc_t *v )
{
	if ( !RenderSubmission_AddEffectRibbon( &s_module.frontend, v ) )
		LogFailure( "effect-ribbon" );
}
static void NoRail( const railRibbonDesc_t *v )
{
	(void)v;
}
static void NoBeam( const beamDesc_t *v )
{
	(void)v;
}
static void AddEffectSprite( const spriteDesc_t *v )
{
	if ( !RenderSubmission_AddEffectSprite( &s_module.frontend, v ) )
		LogFailure( "effect-sprite" );
}
static void AddEffectEmitter( const emitterDesc_t *v )
{
	if ( !RenderSubmission_AddEffectEmitter( &s_module.frontend, v ) )
		LogFailure( "effect-emitter" );
}
static void AddEffectDecal( const decalDesc_t *v )
{
	if ( !RenderSubmission_AddEffectDecal( &s_module.frontend, v ) )
		LogFailure( "effect-decal" );
}
static void RegisterParticleClass( particleClassHandle_t h, const particleClass_t *v )
{
	if ( !RenderSubmission_RegisterParticleClass( &s_module.frontend, h, v ) )
		LogFailure( "particle-class" );
}
static void SetAtmosphere( const atmosphericDesc_t *v )
{
	if ( !RenderSubmission_SetAtmosphere( &s_module.frontend, v ) )
		LogFailure( "atmosphere-state" );
}
static void AddAtmosphereEmitter( const atmosphereEmitter_t *v )
{
	if ( !RenderSubmission_AddAtmosphereEmitter( &s_module.frontend, v ) )
		LogFailure( "atmosphere-emitter" );
}
static void RegisterAtmosphereEffectProfile( uint32_t handle, const atmosphereEffectProfile_t *v )
{
	if ( !RenderSubmission_RegisterAtmosphereEffectProfile( &s_module.frontend, handle, v ) )
		LogFailure( "atmosphere-effect-profile" );
}
static void AddAtmosphereSurfaceEvent( const atmosphereSurfaceEvent_t *v )
{
	if ( !RenderSubmission_AddAtmosphereSurfaceEvent( &s_module.frontend, v ) )
		LogFailure( "atmosphere-surface-event" );
}
static void AddAtmosphereMediaVolume( const atmosphereMediaVolume_t *v )
{
	if ( !RenderSubmission_AddAtmosphereMediaVolume( &s_module.frontend, v ) )
		LogFailure( "atmosphere-media-volume" );
}
static void NoHeightgrid( const float *v, int c )
{
	(void)v;
	(void)c;
}
static void NoLens( const lensSourceDesc_t *v )
{
	(void)v;
}
static qboolean NoLensVisibility( int id, float *v )
{
	(void)id;
	if ( v )
		*v = 0;
	return qfalse;
}
static void NoClip( const float *v )
{
	(void)v;
}
static void NoOutline( float w, const float *c, float g, const float *gc )
{
	(void)w;
	(void)c;
	(void)g;
	(void)gc;
}
static void NoShadow( float x, float y, const float *c )
{
	(void)x;
	(void)y;
	(void)c;
}
static void NoRaw( int x, int y, int w, int h, int c, int r, byte *d, int n, qboolean dirty )
{
	(void)x;
	(void)y;
	(void)w;
	(void)h;
	(void)c;
	(void)r;
	(void)d;
	(void)n;
	(void)dirty;
}
static void NoUpload( int w, int h, int c, int r, byte *d, int n, qboolean dirty )
{
	(void)w;
	(void)h;
	(void)c;
	(void)r;
	(void)d;
	(void)n;
	(void)dirty;
}
static int NoFragments( int n, const vec3_t *p, const vec3_t pr, int mp, vec3_t pb, int mf, markFragment_t *f )
{
	(void)n;
	(void)p;
	(void)pr;
	(void)mp;
	(void)pb;
	(void)mf;
	(void)f;
	return 0;
}
static int SubmitTag( orientation_t *t, qhandle_t m, int s, int e, float f, const char *n )
{
	return RenderSubmission_LerpTag( &s_module.frontend, t, m, s, e, f, n );
}
static void NoBounds( qhandle_t m, vec3_t mins, vec3_t maxs )
{
	(void)m;
	if ( mins )
		memset( mins, 0, sizeof( vec3_t ) );
	if ( maxs )
		memset( maxs, 0, sizeof( vec3_t ) );
}
static void NoFont( const char *n, int s, fontInfo_t *f )
{
	(void)n;
	(void)s;
	if ( f )
		memset( f, 0, sizeof( *f ) );
}
static void NoRemap( const char *a, const char *b, const char *c )
{
	(void)a;
	(void)b;
	(void)c;
}
static qboolean NoToken( char *b, int s )
{
	if ( b && s > 0 )
		b[0] = '\0';
	return qfalse;
}
static qboolean NoPvs( const vec3_t a, const vec3_t b )
{
	(void)a;
	(void)b;
	return qfalse;
}
static void NoVideo( int h, int w, byte *c, byte *e, qboolean m )
{
	(void)h;
	(void)w;
	(void)c;
	(void)e;
	(void)m;
}
static qboolean CanMinimize( void )
{
	return qtrue;
}
static const glconfig_t *GetConfig( void )
{
	return s_module.core ? &s_module.config : NULL;
}
static qboolean GetMemoryBudget( uint64_t *d, uint64_t *db, uint64_t *h, uint64_t *hb, int *p )
{
	if ( d )
		*d = 0;
	if ( db )
		*db = 0;
	if ( h )
		*h = 0;
	if ( hb )
		*hb = 0;
	if ( p )
		*p = 0;
	return qfalse;
}
static int NoMdlAnimations( qhandle_t m, mdlAnimRange_t *a, int c )
{
	(void)m;
	(void)a;
	(void)c;
	return 0;
}
static void NoLightstyle( int s, const char *p )
{
	(void)s;
	(void)p;
}
static qboolean NoGpuProfile( refGpuProfileSample_t *s )
{
	if ( s )
		memset( s, 0, sizeof( *s ) );
	return qfalse;
}
static void PresentationChanged( const refPresentationChange_t *change )
{
	if ( !change || !s_module.product )
		return;
	if ( change->logicalWidth > 0 && change->logicalHeight > 0 &&
		 !RalOpenGl_ProductSetOutputExtent( s_module.product, (uint32_t)change->logicalWidth,
											(uint32_t)change->logicalHeight ) )
		LogFailure( "presentation-extent" );
}

static void FillExports( refexport_t *e )
{
	memset( e, 0, sizeof( *e ) );
	e->Shutdown						   = Shutdown;
	e->BeginRegistration			   = BeginRegistration;
	e->RegisterModel				   = RegisterModel;
	e->RegisterSkin					   = RegisterSkin;
	e->RegisterShader				   = RegisterShader;
	e->RegisterShaderNoMip			   = RegisterShaderNoMip;
	e->RegisterShaderLightMap		   = RegisterLightMap;
	e->RegisterMSDFShader			   = RegisterMsdf;
	e->RegisterPrimitiveShader		   = RegisterPrimitive;
	e->PinShaderImages				   = NoopHandle;
	e->LoadWorld					   = LoadWorld;
	e->SelectWorld                  = SelectWorld;
	e->UnloadWorld                  = UnloadWorld;
	e->ResidentWorldCount           = ResidentWorldCount;
	e->SetWorldVisData				   = NoopBytes;
	e->EndRegistration				   = EndRegistration;
	e->ClearScene					   = ClearScene;
	e->AddRefEntityToScene			   = AddEntity;
	e->AddPolyToScene				   = AddPoly;
	e->LightForPoint				   = NoLightForPoint;
	e->AddLightToScene				   = AddLight;
	e->AddAdditiveLightToScene		   = AddLight;
	e->AddLinearLightToScene		   = AddLinearLight;
	e->AddRibbonToScene				   = AddEffectRibbon;
	e->AddRailRibbonToScene			   = NoRail;
	e->AddBeamToScene				   = NoBeam;
	e->AddSpriteToScene				   = AddEffectSprite;
	e->EmitParticles				   = AddEffectEmitter;
	e->AddDecalToScene				   = AddEffectDecal;
	e->RegisterParticleClass		   = RegisterParticleClass;
	e->SetAtmosphere				   = SetAtmosphere;
	e->SetAtmosphereHeightgrid		   = NoHeightgrid;
	e->AddLensSourceToScene			   = NoLens;
	e->GetLensVisibility			   = NoLensVisibility;
	e->RenderScene					   = RenderScene;
	e->SetColor						   = SetColor;
	e->SetMSDFOutline				   = NoOutline;
	e->SetMSDFShadow				   = NoShadow;
	e->SetClipRegion				   = NoClip;
	e->SetUiTransform				   = SetUiTransform;
	e->DrawStretchPic				   = DrawPic;
	e->DrawMenuBackdrop				   = DrawBackdrop;
	e->DrawStretchPicOverlay		   = DrawPic;
	e->DrawRotatedPic				   = DrawRotatedPic;
	e->DrawLine						   = DrawLine;
	e->DrawStretchRaw				   = NoRaw;
	e->UploadCinematic				   = NoUpload;
	e->BeginFrame					   = BeginFrame;
	e->EndFrame						   = EndFrame;
	e->MarkFragments				   = NoFragments;
	e->LerpTag						   = SubmitTag;
	e->ModelBounds					   = NoBounds;
	e->RegisterFont					   = NoFont;
	e->RemapShader					   = NoRemap;
	e->GetEntityToken				   = NoToken;
	e->inPVS						   = NoPvs;
	e->TakeVideoFrame				   = NoVideo;
	e->ThrottleBackend				   = NoopVoid;
	e->FinishBloom					   = NoopVoid;
	e->SetColorMappings				   = NoopVoid;
	e->CanMinimize					   = CanMinimize;
	e->GetConfig					   = GetConfig;
	e->GetMemoryBudget				   = GetMemoryBudget;
	e->VertexLighting				   = NoopBool;
	e->SyncRender					   = NoopVoid;
	e->GetMDLAnimations				   = NoMdlAnimations;
	e->SetLightstylePattern			   = NoLightstyle;
	e->GetGpuProfileSample			   = NoGpuProfile;
	e->AddRefEntityToSceneTemporal	   = AddEntityTemporal;
	e->PresentationChanged			   = PresentationChanged;
	e->AddAtmosphereEmitter			   = AddAtmosphereEmitter;
	e->RegisterAtmosphereEffectProfile = RegisterAtmosphereEffectProfile;
	e->AddAtmosphereSurfaceEvent	   = AddAtmosphereSurfaceEvent;
	e->AddAtmosphereMediaVolume		   = AddAtmosphereMediaVolume;
	e->CookLightingProject			   = CookLightingProject;
}

Q_EXPORT refexport_t *QDECL GetRefAPI( int apiVersion, refimport_t *imports )
{
	if ( !imports || apiVersion != REF_API_VERSION || s_module.loaded || s_moduleCounter >= UINT64_MAX - 1u ||
		 !imports->GLimp_InitOpenGL46 || !imports->GLimp_Shutdown || !imports->GLimp_EndFrame ||
		 !imports->GL_GetProcAddress )
		return NULL;
	memset( &s_module, 0, sizeof( s_module ) );
	s_module.imports		   = *imports;
	s_module.moduleGeneration  = ++s_moduleCounter;
	ri						   = *imports;
	s_module.nextGeneration	   = s_module.moduleGeneration;
	s_module.logChannel		   = -1;
	s_module.receiptLogChannel = -1;
	if ( imports->GetLogChannel && imports->LogCh ) {
		s_module.logChannel		   = imports->GetLogChannel( "renderer.init" );
		s_module.receiptLogChannel = imports->GetLogChannel( "renderer.receipt" );
	}
	FillExports( &s_module.exports );
	s_module.loaded = qtrue;
	return &s_module.exports;
}
