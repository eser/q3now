// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_module.h"
#include "ral_metal_lighting.h"
#include "ral_particle_contract.h"
#include "maps/map_format_registry.h"
#include "maps/meta.h"
#include "render_image_decode.h"
#include "render_sky_atlas.h"
#include "render_lighting_project_cook.h"
#include "render_material_project_cook.h"
#include "render_world_material_usage.h"
#include "render_material_runtime_owner.h"
#include "render_lighting_sidecar.h"
#include "render_material_script.h"
#include "render_texture_sampling.h"
#include "render_pbr_control.h"
#include "render_dynamic_light_control.h"
#include "tr_public.h"
#include "tr_screenshot.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
	ralMetalLighting_t *resource;
	ralMetalLightingReceipt_t receipt;
} wiredMetalWorldLighting_t;

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
	renderMaterialRuntimeOwner_t materialRuntimeOwner;
	renderLightingSidecarReceipt_t lightingSidecar;
	renderIrradianceSidecarReceipt_t irradianceSidecar;
	wiredMetalWorldLighting_t worldLighting[MAX_RENDER_WORLDS];
	const mapFile_t *loadedWorld;
	const mapFile_t *loadedWorlds[MAX_RENDER_WORLDS];
	const char *entityParsePoint;
	const char *entityParsePoints[MAX_RENDER_WORLDS];
	int activeWorldIndex;
	glconfig_t config;
	uint64_t moduleGeneration;
	uint64_t nextGeneration;
	uint64_t lastLoggedWorldDigest;
	qboolean loggedCompleteContentReceipt;
	qboolean loggedIrradianceReceipt;
	char screenshotName[MAX_OSPATH];
	int initLogChannel;
	int timingLogChannel;
	qboolean loaded;
	qboolean registered;
	qboolean resetRegistrationAssets;
	qboolean frameOpen;
	qboolean registrationFramePublished;
	qboolean screenshotCommandsRegistered;
	qboolean screenshotPending;
	qboolean screenshotSilent;
	qboolean loggedInvalidEntityDrop;
	qboolean loggedInvalidUiDrop;
	uint32_t unresolvedMaterialLogCount;
    cvar_t *gpuSpeeds;
	cvar_t *speeds;
    uint32_t timingFrames;
	uint64_t timingAcquireUs, timingSubmitUs;
	ralMetalSubmitTiming_t timingPhases;
    ralMetalGpuTiming_t timingBaseline;
	cvar_t *brightness;
	cvar_t *pbr;
	cvar_t pbrDefault;
	cvar_t *dynamicLight;
	cvar_t dynamicLightDefault;
	cvar_t *textureMode;
	cvar_t *anisotropic;
	cvar_t *maxAnisotropy;
	qboolean loggedInvalidSampling;
	qboolean samplingPublished;
	ralSamplerCreateInfo_t activeSampling;
	cvar_t brightnessFallback;
	cvar_t *lightmapBoost;
	cvar_t lightmapBoostFallback;
	cvar_t *aerialHaze;
	cvar_t aerialHazeFallback;
	cvar_t *skyPhysicalBlend;
	cvar_t skyPhysicalBlendFallback;
	cvar_t *lens;
	cvar_t lensFallback;
	cvar_t *shaderTimeOverride;
	cvar_t shaderTimeOverrideFallback;
	cvar_t *toneMap;
	cvar_t *bloomCvars[5];
	cvar_t bloomFallback[5];
	cvar_t *aa, *smaa;                   /* r_aa (0 off, 1 TAA, 2 SMAA) and the r_smaa preset */
	cvar_t aaFallback, smaaFallback;
	cvar_t toneMapFallback;
	cvar_t *toneMapExposure;
	cvar_t toneMapExposureFallback;
	cvar_t *autoExposure[8];
	cvar_t autoExposureDefaults[8];
	cvar_t *lottesContrast;
	cvar_t lottesContrastFallback;
	cvar_t *lottesShoulder;
	cvar_t lottesShoulderFallback;
	cvar_t *lottesMidIn;
	cvar_t lottesMidInFallback;
	cvar_t *lottesMidOut;
	cvar_t lottesMidOutFallback;
	cvar_t *lottesHdrMax;
	cvar_t lottesHdrMaxFallback;
	cvar_t *ambientScale;
	cvar_t ambientScaleFallback;
	cvar_t *directedScale;
	cvar_t directedScaleFallback;
} wiredMetalModuleState_t;

#ifdef __cplusplus
#define WIRED_METAL_MODULE_EXPORT extern "C" Q_EXPORT
#else
#define WIRED_METAL_MODULE_EXPORT Q_EXPORT
#endif

static wiredMetalModuleState_t s_module;
static uint64_t s_moduleCounter;
refimport_t ri;

static qboolean ModuleEffectReceiptValid(
		const ralMetalModuleFrameReceipt_t *receipt ) {
	if ( !receipt ) return qfalse;
	/* Transient primitives must lower one-for-one. Persistent decals/beams may
	 * include older live entries, but a newly admitted entry may never vanish.
	 * Emitters are admitted only with a registered class; any admitted emitter
	 * therefore has to produce a compute request and a live-pool draw. */
	return ( receipt->presentation.loweredEffectSpriteCount
			== receipt->frontend.effectSpriteCount
		&& receipt->presentation.loweredEffectRibbonCount
			== receipt->frontend.effectRibbonCount
		&& receipt->presentation.effectEmitterDispatchCount
			== receipt->frontend.effectEmitterCount
		&& receipt->presentation.loweredEffectDecalCount
			>= receipt->frontend.effectDecalCount
		&& receipt->presentation.loweredEffectBeamCount
			>= receipt->frontend.effectBeamCount
		&& receipt->presentation.lensSourceCount
			== receipt->frontend.lensSourceCount
		&& receipt->frontend.lensSourceDroppedCount == 0u
		&& receipt->presentation.lensVisibilityDispatchCount <= 1u
		&& ( receipt->frontend.effectEmitterCount == 0u
			|| receipt->presentation.effectParticleDrawCount
				== RAL_PARTICLE_POOL_CAPACITY ) ) ? qtrue : qfalse;
}

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
		&& Ral_AtmospherePlanReceiptExact( &receipt->atmosphere,
			&receipt->atmosphere )
		&& Ral_AtmospherePlanReceiptExact( &receipt->atmosphere,
			&receipt->presentation.atmosphere )
		&& receipt->frontend.ownerGeneration == receipt->moduleGeneration
		&& receipt->frontend.frameGeneration == receipt->frameGeneration
		&& receipt->atmosphere.frameGeneration == receipt->frameGeneration
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
		&& ModuleEffectReceiptValid( receipt )
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
		&& Ral_AtmospherePlanReceiptExact( &a->atmosphere, &b->atmosphere )
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
	/* Screenshot/readback is a display-referred presentation artifact. Keep its
	 * native backing extent, matching the drawable and the other RAL backends;
	 * logical points are a UI layout domain, not an image-storage contract. */
	outputWidth = s_module.hostReceipt.pixelWidth;
	outputHeight = s_module.hostReceipt.pixelHeight;
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
			|| !R_SaveScreenshotPNG( s_module.screenshotName, rgb,
				(int)outputWidth, (int)outputHeight, "metal" ) ) {
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

static qboolean PublishDecodedMaterialIdentity( qhandle_t handle,
		renderAssetKind_t kind, const char *resolved, const byte *pixels,
		uint32_t width, uint32_t height, qboolean noMipMaps ) {
	char canonical[MAX_QPATH];
	uint64_t sourceId = 0u, byteSize = 0u;
	unsigned filesystemGeneration = 0u;
	ralTextureCacheKey_t key;
	renderTextureSnapshot_t snapshot;
	if ( handle <= 0 || !resolved || !resolved[0] || !pixels || !width || !height
			|| !RenderSubmission_TextureSnapshot( &s_module.frontend, handle,
				&snapshot ) || snapshot.ready != qtrue
			|| !snapshot.rgba8 || !snapshot.width || !snapshot.height
			|| !ri.FS_ResolveResource
			|| !ri.FS_ResolveResource( resolved, canonical, sizeof( canonical ),
				&sourceId, &byteSize, &filesystemGeneration )
			|| !sourceId || !byteSize || !filesystemGeneration ) return qfalse;
	/* Material names are interned. If another registration resolves the same
	 * name through a different physical image, the first complete snapshot and
	 * its provenance remain authoritative; never splice the later key into it. */
	if ( snapshot.textureCacheKeyReady )
		return Ral_TextureCacheKeyValid( &snapshot.textureCacheKey )
			&& snapshot.textureCacheKey.asset.width == snapshot.width
			&& snapshot.textureCacheKey.asset.height == snapshot.height;
	if ( !Ral_MaterialRuntimeTextureKeyBuild( sourceId,
			filesystemGeneration, snapshot.rgba8, snapshot.width, snapshot.height,
			kind == RENDER_ASSET_MSDF ? qfalse : qtrue, noMipMaps,
			RAL_TEXTURE_RESIDENCY_STREAMED, &key ) ) return qfalse;
	return RenderSubmission_SetTextureCacheKey( &s_module.frontend,
		handle, &key );
}

static qhandle_t RegisterScriptedTextureResource( renderAssetKind_t kind,
		const renderMaterialScriptEntry_t *scripted, const char *imageName,
		qboolean clampToEdge ) {
	byte *pixels = NULL;
	uint32_t width = 0u, height = 0u;
	char resolved[MAX_QPATH];
	qhandle_t handle;
	if ( !scripted || !imageName || !imageName[0]
			|| imageName[0] == '*'
			|| !RenderImage_DecodeRgba8( imageName, &pixels, &width, &height,
				resolved ) ) return 0;
	handle = RenderSubmission_RegisterStageTexture( &s_module.frontend, kind,
		imageName, clampToEdge, pixels, width, height );
	if ( handle && !PublishDecodedMaterialIdentity( handle, kind, resolved,
			pixels, width, height, scripted->noMipMaps ) ) handle = 0;
	ri.Free( pixels );
	return handle;
}

static qboolean RegisterSkyProgramResources( renderAssetKind_t kind,
		const renderMaterialScriptEntry_t *scripted,
		qhandle_t outerTextures[RAL_MATERIAL_SKY_FACE_COUNT],
		qhandle_t innerTextures[RAL_MATERIAL_SKY_FACE_COUNT] ) {
	static const char *suffixes[RAL_MATERIAL_SKY_FACE_COUNT] = {
		"rt", "bk", "lf", "ft", "up", "dn"
	};
	const char *prefixes[2];
	qhandle_t *textureSets[2] = { outerTextures, innerTextures };
	if ( !scripted || !scripted->sky || !outerTextures || !innerTextures )
		return qfalse;
	memset( outerTextures, 0,
		RAL_MATERIAL_SKY_FACE_COUNT * sizeof( outerTextures[0] ) );
	memset( innerTextures, 0,
		RAL_MATERIAL_SKY_FACE_COUNT * sizeof( innerTextures[0] ) );
	prefixes[0] = scripted->skyBoxPrefix;
	prefixes[1] = scripted->skyInnerBoxPrefix;
	for ( uint32_t box = 0u; box < 2u; ++box ) {
		if ( !prefixes[box][0] ) continue;
		for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face ) {
			char path[MAX_QPATH];
			const int length = snprintf( path, sizeof( path ), "%s_%s",
				prefixes[box], suffixes[face] );
			if ( length <= 0 || length >= (int)sizeof( path ) ) return qfalse;
			textureSets[box][face] = RegisterScriptedTextureResource( kind, scripted,
				path, qtrue );
			if ( textureSets[box][face] <= 0 ) return qfalse;
		}
	}
	for ( uint32_t stageIndex = 0u; stageIndex < scripted->stageCount;
			++stageIndex ) {
		const renderMaterialScriptStage_t *stage = &scripted->stages[stageIndex];
		if ( stage->imageSource != RENDER_MATERIAL_STAGE_IMAGE ) continue;
		if ( stage->imageName[0]
				&& !RegisterScriptedTextureResource( kind, scripted, stage->imageName,
					stage->clampToEdge ) ) return qfalse;
		for ( uint32_t frame = 0u; frame < stage->animationFrameCount; ++frame ) {
			const char *frameName = scripted->animationFrameNames[
				stage->animationFrameOffset + frame];
			if ( !RegisterScriptedTextureResource( kind, scripted, frameName,
					stage->clampToEdge ) ) return qfalse;
		}
	}
	return qtrue;
}

static void ScriptedProgramRasterPolicy(
		const renderMaterialScriptEntry_t *scripted,
		renderAlphaMode_t *alphaMode, float *alphaCutoff,
		qboolean *depthWrite ) {
	const renderMaterialScriptStage_t *first;
	if ( !scripted || !alphaMode || !alphaCutoff || !depthWrite ) return;
	*alphaMode = scripted->alphaMode;
	*alphaCutoff = scripted->alphaCutoff;
	*depthWrite = scripted->depthWrite;
	if ( scripted->sky || !scripted->stageCount ) return;
	first = &scripted->stages[0];
	if ( first->alphaTest != RAL_MATERIAL_ALPHA_TEST_NONE ) {
		*alphaMode = RENDER_ALPHA_MASK;
		*alphaCutoff = first->alphaCutoff;
		*depthWrite = qtrue;
	} else if ( first->sourceBlend == RENDER_MATERIAL_BLEND_ONE
			&& first->destinationBlend == RENDER_MATERIAL_BLEND_ZERO ) {
		/* Later stages are composited into this established coverage in the
		 * fragment program. They must not turn the entire surface into a
		 * framebuffer blend (the previous Metal regression). */
		*alphaMode = RENDER_ALPHA_OPAQUE;
		*depthWrite = qtrue;
	} else if ( first->sourceBlend == RENDER_MATERIAL_BLEND_ONE
			&& first->destinationBlend == RENDER_MATERIAL_BLEND_ONE ) {
		*alphaMode = RENDER_ALPHA_ADDITIVE;
		*depthWrite = first->depthWrite;
	} else if ( first->sourceBlend == RENDER_MATERIAL_BLEND_SRC_ALPHA
			&& first->destinationBlend == RENDER_MATERIAL_BLEND_ONE ) {
		*alphaMode = RENDER_ALPHA_ALPHA_ADDITIVE;
		*depthWrite = first->depthWrite;
	} else {
		*alphaMode = RENDER_ALPHA_BLEND;
		*depthWrite = first->depthWrite;
	}
}

typedef struct {
	renderAssetKind_t kind;
	const renderMaterialScriptEntry_t *scripted;
	qhandle_t owner;
} scriptedStageContext_t;

static qhandle_t ResolveScriptedStage( void *context,
		const renderMaterialScriptStage_t *stage, const char *imageName ) {
	const scriptedStageContext_t *source = (const scriptedStageContext_t *)context;
	/* Reuse the already decoded primary only for the same sampler usage. */
	if ( !source->scripted->skyBoxPrefix[0]
			&& stage->clampToEdge == source->scripted->clampToEdge
			&& !strcasecmp( imageName, source->scripted->imageName ) )
		return source->owner;
	const qhandle_t handle = RegisterScriptedTextureResource( source->kind,
		source->scripted, imageName, stage->clampToEdge );
	if ( !handle && ri.Log ) ri.Log( SEV_WARN,
		"material '%s': unresolved stage image '%s'\n",
		source->scripted->name, imageName );
	return handle;
}

static qboolean ApplyScriptedMaterial( renderAssetKind_t kind, const char *name,
		qhandle_t handle, const renderMaterialScriptEntry_t *scripted ) {
	renderAlphaMode_t alphaMode;
	float alphaCutoff;
	qboolean depthWrite;
	renderMaterialStageSnapshot_t stages[RENDER_MATERIAL_MAX_STAGES];
	qhandle_t skyOuterTextures[RAL_MATERIAL_SKY_FACE_COUNT] = { 0 };
	qhandle_t skyInnerTextures[RAL_MATERIAL_SKY_FACE_COUNT] = { 0 };
	if ( !handle || !scripted || !scripted->name[0] ) return handle > 0;
	ScriptedProgramRasterPolicy( scripted, &alphaMode, &alphaCutoff,
		&depthWrite );
	if ( !RenderSubmission_SetMaterialRasterPolicy( &s_module.frontend, handle,
			alphaMode, alphaCutoff, depthWrite )
			|| !RenderSubmission_SetMaterialSort( &s_module.frontend, handle,
				scripted->sort )
			|| !RenderSubmission_SetMaterialCullMode( &s_module.frontend, handle,
				scripted->cullMode )
			|| !RenderSubmission_SetMaterialSky( &s_module.frontend, handle,
				scripted->sky )
			|| !RenderSubmission_SetMaterialSkyBox( &s_module.frontend, handle,
				scripted->skyBoxPrefix[0] ? qtrue : qfalse )
			|| !RenderSubmission_SetMaterialSkyCloudHeight( &s_module.frontend,
				handle, scripted->skyCloudHeight )
			|| !RenderSubmission_SetMaterialNoDraw( &s_module.frontend, handle,
				scripted->noDraw ) ) return qfalse;
	if ( ( scripted->sky || scripted->hasTcTransform )
			&& !RenderSubmission_SetMaterialSkyProjection(
			&s_module.frontend, handle, scripted->skyScale,
			scripted->skyScroll ) ) return qfalse;
	/* Publish the program before its sky texture metadata: the common
	 * frontend derives stage-only sky colour from these registered operands. */
	if ( scripted->stageCount ) {
		scriptedStageContext_t context = { kind, scripted, handle };
		if ( !RenderMaterialScript_BuildStages( scripted, ResolveScriptedStage,
				&context, stages )
				|| !RenderSubmission_SetMaterialStages( &s_module.frontend, handle,
					stages, scripted->stageCount ) ) return qfalse;
	}
	/* The atlas is a presentation product, not the authored sky identity.
	 * Publish every outer/inner face and cloud animation frame separately so
	 * the shared cooked runtime can resolve the same immutable RAL receipts on
	 * every adapter. Missing authored inputs reject the sky; no neutral fallback
	 * is permitted at this production boundary. */
	if ( scripted->sky
			&& ( !RegisterSkyProgramResources( kind, scripted,
				skyOuterTextures, skyInnerTextures )
				|| !RenderSubmission_SetMaterialAuthoredSkySource(
					&s_module.frontend, handle, &scripted->skySource )
				|| !RenderSubmission_SetMaterialAuthoredSkyTextures(
					&s_module.frontend, handle, skyOuterTextures,
					skyInnerTextures ) ) ) return qfalse;
	if ( scripted->hasLighting
			&& !RenderMaterialScript_ApplyLighting( &s_module.materialScripts,
				name, &s_module.frontend, handle ) ) return qfalse;
	return qtrue;
}

static qhandle_t RegisterMaterialImageSourceInternal( renderAssetKind_t kind,
		const char *name, const char *imageName, qboolean clampToEdge,
		qboolean allowRemap ) {
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
	if ( scripted.skyBoxPrefix[0] ) {
		if ( !wired_sky_atlas_decode( &ri, scripted.skyBoxPrefix, &pixels,
				&width, &height ) ) return 0;
		handle = RenderSubmission_RegisterMaterialImage( &s_module.frontend,
			kind, name, qtrue, pixels, width, height );
		ri.Free( pixels );
		if ( !handle ) return 0;
		if ( !ApplyScriptedMaterial( kind, name, handle, &scripted ) ) return 0;
		return handle;
	}
	if ( imageName && imageName[0] != '*'
			&& RenderImage_DecodeRgba8( imageName, &pixels, &width, &height, resolved ) ) {
		/* Decoding may resolve an extension or a VFS alias, but the material
		 * identity remains the authored name used by BSP/model batches. */
		const char *identityName = name && name[0] ? name : resolved;
		handle = RenderSubmission_RegisterMaterialImage( &s_module.frontend,
			kind, identityName, clampToEdge, pixels, width, height );
		if ( handle && !PublishDecodedMaterialIdentity( handle, kind, resolved,
				pixels, width, height,
				scripted.name[0] ? scripted.noMipMaps : qfalse ) ) handle = 0;
		ri.Free( pixels );
		/* Keep valid geometry renderable when the bounded CPU material snapshot
		 * budget is exhausted. The unresolved image path already uses a neutral
		 * material; use the same deterministic policy for decoded images that do
		 * not fit rather than rejecting the owning model. */
		if ( !handle )
			handle = RenderSubmission_RegisterMaterialImage( &s_module.frontend,
				kind, name, qtrue, white, 1u, 1u );
		if ( !ApplyScriptedMaterial( kind, name, handle, &scripted ) ) return 0;
		return handle;
	}
	if ( allowRemap && name && s_module.imports.MetaRemap_Lookup ) {
		const char *remapped = s_module.imports.MetaRemap_Lookup(
			(int)REMAP_KIND_SHADER, name );
		if ( remapped && remapped[0] && strcasecmp( remapped, name ) )
			/* A VFS/meta remap changes the physical image source, never the
			 * frontend material identity. World batches retain the authored BSP
			 * shader name and must resolve that same handle after registration. */
			return RegisterMaterialImageSourceInternal( kind, name,
				remapped, clampToEdge, qfalse );
	}
	if ( imageName && imageName[0] != '*' && s_module.imports.LogCh
			&& s_module.initLogChannel >= 0
			&& s_module.unresolvedMaterialLogCount < 64u ) {
		s_module.imports.LogCh( s_module.initLogChannel, SEV_WARN,
			"Wired native Metal RAL: unresolved material name=%s image=%s scripted=%u sky=%u noDraw=%u\n",
			name ? name : "(null)", imageName, scripted.name[0] ? 1u : 0u,
			scripted.sky ? 1u : 0u, scripted.noDraw ? 1u : 0u );
		s_module.unresolvedMaterialLogCount++;
	}
	/* Match Vulkan's R_FindShader/FinishShader contract exactly: a missing,
	 * scriptless image produces a zero-pass default shader.  Register a stable
	 * material identity for receipts, then mark the owning batches invisible so
	 * this placeholder cannot occlude valid geometry behind it. */
	if ( !scripted.name[0] ) {
		handle = RenderSubmission_RegisterMaterialImage( &s_module.frontend,
			kind, name, clampToEdge, NULL, 0u, 0u );
		if ( !handle || !RenderSubmission_SetMaterialNoDraw(
				&s_module.frontend, handle, qtrue ) ) return 0;
		return handle;
	}
	handle = RenderSubmission_RegisterMaterialImage( &s_module.frontend,
		kind, name, clampToEdge, scripted.noDraw ? NULL : white,
		scripted.noDraw ? 0u : 1u, scripted.noDraw ? 0u : 1u );
	if ( !ApplyScriptedMaterial( kind, name, handle, &scripted ) ) return 0;
	return handle;
}
static qhandle_t RegisterMaterialImageSource( renderAssetKind_t kind,
		const char *name, const char *imageName, qboolean clampToEdge ) {
	return RegisterMaterialImageSourceInternal( kind, name, imageName,
		clampToEdge, qtrue );
}
static qhandle_t RegisterMaterialImage( renderAssetKind_t kind, const char *name,
		qboolean clampToEdge ) {
	return RegisterMaterialImageSource( kind, name, name, clampToEdge );
}
static void LogModelReject( const char *name, const char *stage ) {
	if ( s_module.imports.LogCh && s_module.initLogChannel >= 0 )
		s_module.imports.LogCh( s_module.initLogChannel, SEV_WARN,
			"Wired native Metal RAL: model rejected path=%s stage=%s\n",
			name ? name : "(null)", stage ? stage : "unknown" );
}
static qhandle_t RegisterModel( const char *name ) {
	void *bytes = NULL;
	renderModelSnapshot_t model;
	qhandle_t handle;
	int byteCount;
	char canonical[MAX_QPATH];
	if ( !name || !name[0] ) return 0;
	if ( name[0] == '*' ) return RenderSubmission_RegisterBspModel( &s_module.frontend, name );
	if ( ri.FS_ResolveResource && ri.FS_ResolveResource( name, canonical,
			sizeof( canonical ), NULL, NULL, NULL ) ) name = canonical;
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
		LogModelReject( name, "decode" );
		return 0;
	}
	if ( !RenderSubmission_ModelSnapshot( &s_module.frontend, handle, &model ) ) {
		LogModelReject( name, "snapshot" ); return 0;
	}
	for ( uint32_t batch = 0u; batch < model.batchCount; ++batch ) {
		qhandle_t material = RegisterMaterialImage( RENDER_ASSET_MATERIAL,
			model.batches[batch].materialName, qfalse );
		if ( !material || !RenderSubmission_SetModelBatchMaterial(
				&s_module.frontend, handle, batch, material ) ) {
			LogModelReject( name, "material" ); return 0;
		}
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
	byte used[RENDER_SUBMISSION_MAX_MATERIALS];
	byte *rgba = NULL;
	uint32_t side = 0u;
	uint64_t pixels;
	if ( !RenderWorldMaterialUsage_Build( bsp, used, sizeof( used ) )
			|| bsp->numLightmapPages < 0
			|| bsp->lightmapPageSize < 0
			|| ( bsp->numShaders && !bsp->shaders ) ) return qfalse;
	for ( int i = 0; i < bsp->numShaders; ++i ) {
		if ( !used[i] ) continue;
		if ( !RegisterMaterialImage( RENDER_ASSET_MATERIAL,
				bsp->shaders[i].shader, qfalse ) ) {
			if ( s_module.imports.LogCh && s_module.initLogChannel >= 0 )
				s_module.imports.LogCh( s_module.initLogChannel, SEV_WARN,
					"Wired native Metal RAL: world material rejected map=%s shader=%s materials=%u stages=%u\n",
					bsp->name, bsp->shaders[i].shader,
					s_module.frontend.materialCount, s_module.frontend.stageTextureCount );
			return qfalse;
		}
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
			if ( s_module.imports.LogCh && s_module.initLogChannel >= 0 )
				s_module.imports.LogCh( s_module.initLogChannel, SEV_WARN,
					"Wired native Metal RAL: lightmap rejected map=%s page=%d/%d side=%u materials=%u bytes=%u limit=%u\n",
					bsp->name, page, bsp->numLightmapPages, side,
					s_module.frontend.materialCount, s_module.frontend.materialBytes,
					RENDER_SUBMISSION_MAX_MATERIAL_BYTES );
			ri.Free( rgba ); return qfalse;
		}
	}
	ri.Free( rgba );
	return qtrue;
}
static void DestroyDirectionalLighting( int worldIndex ) {
	wiredMetalWorldLighting_t *slot;
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS ) return;
	slot = &s_module.worldLighting[worldIndex];
	if ( s_module.presentation && s_module.activeWorldIndex == worldIndex )
		(void)RalMetal_PresentSetDirectionalLighting( s_module.presentation, NULL );
	if ( slot->resource && s_module.core )
		(void)RalMetal_LightingDestroy( s_module.core, &s_module.coreReceipt,
			slot->resource, &slot->receipt );
	memset( slot, 0, sizeof( *slot ) );
}
static qboolean UploadDirectionalLighting( int worldIndex ) {
	const renderDirectionalLightingRecord_t *record;
	ralLightingRuntimePlan_t plan;
	ralStaticLightingCapabilities_t capabilities = { qtrue, qtrue, qtrue, qtrue };
	wiredMetalWorldLighting_t *slot;
	uint64_t digest;
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS ) return qfalse;
	slot = &s_module.worldLighting[worldIndex];
	DestroyDirectionalLighting( worldIndex );
	if ( s_module.lightingSidecar.status ==
			RENDER_LIGHTING_SIDECAR_MISSING_COMPATIBILITY )
		return RalMetal_PresentSetDirectionalLighting( s_module.presentation, NULL );
	if ( !RenderSubmission_DirectionalLightingSnapshot( &s_module.frontend,
			&record, &digest ) || !record || !digest
			|| !Ral_LightingRuntimePlanBuild( RAL_BACKEND_METAL,
				NextGeneration(), record->ownedArtifactBytes,
				record->artifactByteLength, &record->artifact, &capabilities,
				&plan ) ) return qfalse;
	if ( !RalMetal_LightingUpload( s_module.core, &s_module.coreReceipt,
		record->ownedArtifactBytes, record->artifactByteLength, &plan,
		&slot->resource, &slot->receipt )
			|| !RalMetal_PresentSetDirectionalLighting( s_module.presentation,
				&slot->receipt ) ) {
		DestroyDirectionalLighting( worldIndex );
		return qfalse;
	}
	return qtrue;
}
static qboolean PublishMaterialRuntimeWorld( const mapFile_t *bsp,
		int worldIndex ) {
#if defined( NDEBUG )
	renderMaterialPackageDiagnostic_t diagnostic;
	uint64_t generation;
	if ( s_module.materialRuntimeOwner.ready != qtrue
			&& !RenderMaterialRuntimeOwner_Init(
				&s_module.materialRuntimeOwner, &s_module.imports ) ) return qfalse;
	if ( !RenderSubmission_WorldLifecycleGeneration( &s_module.frontend,
			worldIndex, &generation ) ) return qfalse;
	memset( &diagnostic, 0, sizeof( diagnostic ) );
	if ( !RenderMaterialRuntimeOwner_PublishWorld(
			&s_module.materialRuntimeOwner, bsp, worldIndex, generation,
			RalMetal_PresentResolveMaterialRuntimeResource,
			s_module.presentation, &diagnostic ) ) {
		if ( s_module.imports.LogCh && s_module.initLogChannel >= 0 )
			s_module.imports.LogCh( s_module.initLogChannel, SEV_WARN,
				"Wired native Metal RAL: cooked material catalog rejected world=%d reason=%u item=%u runtime=%u key=%llu\n",
				worldIndex, (unsigned)diagnostic.reason,
				(unsigned)diagnostic.itemIndex,
				(unsigned)diagnostic.runtime.reason,
				(unsigned long long)diagnostic.runtime.cacheKeyHash );
		return qfalse;
	}
	if ( !RalMetal_PresentSetMaterialRuntimeCatalog( s_module.presentation,
			RenderMaterialRuntimeOwner_World(
				&s_module.materialRuntimeOwner, worldIndex ),
			&s_module.frontend ) ) return qfalse;
#else
	(void)bsp; (void)worldIndex;
#endif
	return qtrue;
}
static void NoopHandle( qhandle_t handle ) { (void)handle; }
static void NoopVoid( void ) {}
static void NoopBool( qboolean value ) { (void)value; }
static void SubmitWorld( const mapFile_t *bsp, int worldIndex ) {
	const char *failure = NULL;
	if ( !PrepareWorldMaterials( bsp ) ) failure = "world-materials";
	else if ( !RenderSubmission_LoadWorld( &s_module.frontend, bsp, worldIndex ) )
		failure = "world-geometry";
	else if ( !RalMetal_PresentPrepareMaterialResources(
			s_module.presentation, &s_module.frontend ) ) failure = "world-material-resources";
	else if ( !PublishMaterialRuntimeWorld( bsp, worldIndex ) ) failure = "world-material-catalog";
	else if ( !RenderLightingSidecar_LoadDirectional( &s_module.frontend,
			&s_module.imports, bsp->name, &s_module.lightingSidecar ) )
		failure = "world-directional-sidecar";
	else if ( !RenderLightingSidecar_LoadIrradiance( &s_module.frontend,
			&s_module.imports, bsp->name, &s_module.irradianceSidecar ) )
		failure = "world-irradiance-sidecar";
	else if ( !UploadDirectionalLighting( worldIndex ) ) failure = "world-directional-upload";
	if ( failure ) MarkFailed( failure );
	else {
		s_module.loadedWorlds[worldIndex] = bsp;
		s_module.entityParsePoints[worldIndex] = bsp ? bsp->entityString : NULL;
		s_module.activeWorldIndex = worldIndex;
		s_module.loadedWorld = bsp;
		s_module.entityParsePoint = bsp ? bsp->entityString : NULL;
	}
}
static qboolean SelectWorld( int worldIndex ) {
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS
			|| !s_module.loadedWorlds[worldIndex]
			|| !RenderSubmission_SelectWorld( &s_module.frontend, worldIndex ) ) return qfalse;
	if ( !RalMetal_PresentSetDirectionalLighting( s_module.presentation,
			s_module.worldLighting[worldIndex].resource
				? &s_module.worldLighting[worldIndex].receipt : NULL ) ) return qfalse;
#if defined( NDEBUG )
	if ( !RalMetal_PresentSetMaterialRuntimeCatalog( s_module.presentation,
			RenderMaterialRuntimeOwner_World(
				&s_module.materialRuntimeOwner, worldIndex ),
			&s_module.frontend ) ) return qfalse;
#endif
	s_module.activeWorldIndex = worldIndex;
	s_module.loadedWorld = s_module.loadedWorlds[worldIndex];
	s_module.entityParsePoint = s_module.entityParsePoints[worldIndex];
	return qtrue;
}
static qboolean UnloadWorld( int worldIndex ) {
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS ) return qfalse;
	if ( s_module.activeWorldIndex == worldIndex
			&& !RalMetal_PresentSetMaterialRuntimeCatalog(
				s_module.presentation, NULL, &s_module.frontend ) ) return qfalse;
	if ( s_module.activeWorldIndex == worldIndex
			&& !RalMetal_PresentUnloadSkyRadianceResources(
				s_module.presentation ) ) return qfalse;
	if ( s_module.presentation
			&& !RalMetal_PresentUnloadWorld( s_module.presentation, worldIndex ) ) return qfalse;
	DestroyDirectionalLighting( worldIndex );
	if ( !RenderSubmission_UnloadWorld( &s_module.frontend, worldIndex ) ) return qfalse;
	RenderMaterialRuntimeOwner_UnloadWorld(
		&s_module.materialRuntimeOwner, worldIndex );
	s_module.loadedWorlds[worldIndex] = NULL;
	s_module.entityParsePoints[worldIndex] = NULL;
	if ( s_module.activeWorldIndex == worldIndex ) {
		s_module.loadedWorld = NULL; s_module.entityParsePoint = NULL;
		for ( int i = 0; i < MAX_RENDER_WORLDS; ++i )
			if ( s_module.loadedWorlds[i] ) { (void)SelectWorld( i ); break; }
	}
	return qtrue;
}
static int ResidentWorldCount( void ) {
	return RenderSubmission_ResidentWorldCount( &s_module.frontend );
}
static qboolean WorldContentIdentity( int worldIndex, uint64_t *outIdentity ) {
	return RenderSubmission_WorldContentIdentity( &s_module.frontend,
		worldIndex, outIdentity );
}
static qboolean CookLightingProject( const char *derivedRoot ) {
	renderLightingProjectCookRequest_t request;
	renderLightingProjectCookReceipt_t receipt;
	char worldStem[RENDER_LIGHTING_PROJECT_WORLD_STEM_CAPACITY];
	return s_module.loadedWorld
		&& RenderLightingProjectCook_DefaultRequest( s_module.loadedWorld,
			derivedRoot, &request, worldStem )
		&& RenderLightingProjectCook_Execute( &s_module.frontend,
			s_module.loadedWorld, &s_module.imports, &request, &receipt )
		&& RenderLightingProjectCook_ReceiptValid( &receipt );
}

static qboolean CookMaterialProject( const char *derivedRoot ) {
	renderMaterialProjectCookReceipt_t receipt;
	renderMaterialProjectCookDiagnostic_t diagnostic;
	uint64_t generation;
	qboolean cooked;
	if ( !s_module.loadedWorld ) return qfalse;
	generation = (uint32_t)s_module.loadedWorld->checksum;
	if ( !generation ) generation = 1u;
	cooked = RenderMaterialProjectCook_Execute( &s_module.materialScripts,
		s_module.loadedWorld, &s_module.imports, derivedRoot, generation,
		&receipt, &diagnostic );
	if ( !cooked ) {
		const char *shaderName = diagnostic.shaderIndex <
			(uint32_t)s_module.loadedWorld->numShaders
			? s_module.loadedWorld->shaders[diagnostic.shaderIndex].shader : "<none>";
		s_module.imports.Log( SEV_ERROR,
			"material project cook rejected: reason=%u shader=%u '%s' dependency=%u source=%u cook=%u\n",
			(unsigned)diagnostic.reason, diagnostic.shaderIndex, shaderName,
			diagnostic.dependencyIndex, (unsigned)diagnostic.source.reason,
			(unsigned)diagnostic.cook.reason );
		return qfalse;
	}
	return RenderMaterialProjectCook_LogTelemetry( &receipt,
		&s_module.imports );
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

static void AttachEntityLighting( uint32_t entityIndex ) {
	const ralLightVec3Q16_t normal = { 0, 0, RAL_LIGHT_Q16_ONE };
	const int32_t fallback[3] = { RAL_LIGHT_Q16_ONE,
		RAL_LIGHT_Q16_ONE, RAL_LIGHT_Q16_ONE };
	if ( entityIndex >= s_module.frontend.entityCount
			|| ( s_module.frontend.entities[entityIndex].entity.renderfx
				& RF_THIRD_PERSON ) ) return;
	if ( s_module.frontend.irradianceVolumeCount
			&& RenderSubmission_AttachConfiguredEntityIrradiance(
				&s_module.frontend, entityIndex, &normal, fallback ) ) return;
	(void)RenderSubmission_AttachLegacyLightGridEntityIrradiance(
		&s_module.frontend, entityIndex, s_module.ambientScale->value,
		s_module.directedScale->value );
}

static void SubmitEntity( const refEntity_t *entity, qboolean shaderTime ) {
	const cmSkin_t *skin = NULL;
	uint32_t entityIndex;
	(void)shaderTime;
	if ( !s_module.frameOpen ) return;
	if ( !EntityIngressValid( entity, NULL ) ) return;
	if ( entity->characterSkin && s_module.imports.GetCharacterSkin )
		skin = s_module.imports.GetCharacterSkin( entity->characterSkin );
	entityIndex = s_module.frontend.entityCount;
	if ( !RenderSubmission_AddEntitySkinned( &s_module.frontend, entity, NULL, skin ) )
		MarkFailed( "frontend-entity" );
	else AttachEntityLighting( entityIndex );
}
static void SubmitEntityTemporal( const refEntity_t *entity,
		const refEntityMotion_t *motion ) {
	const cmSkin_t *skin = NULL;
	uint32_t entityIndex;
	if ( !s_module.frameOpen ) return;
	if ( !EntityIngressValid( entity, motion ) ) return;
	if ( entity->characterSkin && s_module.imports.GetCharacterSkin )
		skin = s_module.imports.GetCharacterSkin( entity->characterSkin );
	entityIndex = s_module.frontend.entityCount;
	if ( !RenderSubmission_AddEntitySkinned( &s_module.frontend, entity, motion, skin ) )
		MarkFailed( "frontend-temporal-entity" );
	else AttachEntityLighting( entityIndex );
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
static void SubmitLight( const vec3_t origin, float intensity,
		float red, float green, float blue ) {
	if ( !s_module.frameOpen ) return;
	if ( s_module.dynamicLight && !s_module.dynamicLight->integer ) return;	// r_dynamiclight 0: a successful no-op, not a submission failure
	if ( !RenderSubmission_AddLight( &s_module.frontend, origin, NULL,
			intensity, red, green, blue ) ) MarkFailed( "frontend-light" );
}
static void SubmitLinearLight( const vec3_t start, const vec3_t end, float intensity,
		float red, float green, float blue ) {
	if ( !s_module.frameOpen ) return;
	if ( s_module.dynamicLight && !s_module.dynamicLight->integer ) return;
	if ( !RenderSubmission_AddLight( &s_module.frontend, start, end,
			intensity, red, green, blue ) ) MarkFailed( "frontend-linear-light" );
}
static void SubmitRibbon( const ribbonDesc_t *desc ) {
	if ( !RenderSubmission_AddEffectRibbon( &s_module.frontend, desc ) )
		MarkFailed( "frontend-effect-ribbon" );
}
static void NoopRailRibbon( const railRibbonDesc_t *desc ) { (void)desc; }
static void SubmitBeam( const beamDesc_t *desc ) {
	if ( !RenderSubmission_AddEffectBeam( &s_module.frontend, desc ) )
		MarkFailed( "frontend-effect-beam" );
}
static void SubmitSprite( const spriteDesc_t *desc ) {
	if ( !RenderSubmission_AddEffectSprite( &s_module.frontend, desc ) )
		MarkFailed( "frontend-effect-sprite" );
}
static void SubmitEmitter( const emitterDesc_t *desc ) {
	if ( !RenderSubmission_AddEffectEmitter( &s_module.frontend, desc ) )
		MarkFailed( "frontend-effect-emitter" );
}
static void SubmitDecal( const decalDesc_t *desc ) {
	if ( !RenderSubmission_AddEffectDecal( &s_module.frontend, desc ) )
		MarkFailed( "frontend-effect-decal" );
}
static void SubmitParticleClass( particleClassHandle_t handle,
		const particleClass_t *particleClass ) {
	if ( !RenderSubmission_RegisterParticleClass(
			&s_module.frontend, handle, particleClass ) )
		MarkFailed( "frontend-particle-class" );
}
static void SubmitAtmosphere( const atmosphericDesc_t *desc ) {
	if ( !RenderSubmission_SetAtmosphere( &s_module.frontend, desc ) )
		MarkFailed( "frontend-atmosphere-state" );
}
static void SubmitAtmosphereEmitter( const atmosphereEmitter_t *emitter ) {
	if ( !RenderSubmission_AddAtmosphereEmitter( &s_module.frontend, emitter ) )
		MarkFailed( "frontend-atmosphere-emitter" );
}
static void SubmitAtmosphereEffectProfile( uint32_t handle,
		const atmosphereEffectProfile_t *profile ) {
	if ( !RenderSubmission_RegisterAtmosphereEffectProfile(
			&s_module.frontend, handle, profile ) )
		MarkFailed( "frontend-atmosphere-effect-profile" );
}
static void SubmitAtmosphereSurfaceEvent(
		const atmosphereSurfaceEvent_t *event ) {
	if ( !RenderSubmission_AddAtmosphereSurfaceEvent(
			&s_module.frontend, event ) )
		MarkFailed( "frontend-atmosphere-surface-event" );
}
static void SubmitAtmosphereMediaVolume(
		const atmosphereMediaVolume_t *volume ) {
	if ( !RenderSubmission_AddAtmosphereMediaVolume(
			&s_module.frontend, volume ) )
		MarkFailed( "frontend-atmosphere-media-volume" );
}
static void SubmitHeightgrid( const float *grid, int count ) {
	if ( !RenderSubmission_SetAtmosphereHeightgrid( &s_module.frontend, grid, count ) )
		MarkFailed( "frontend-atmosphere-heightgrid" );
}
static void SubmitLens( const lensSourceDesc_t *desc ) {
	if ( s_module.lens && !s_module.lens->integer ) return;
	if ( !RenderSubmission_AddLensSource( &s_module.frontend, desc ) )
		MarkFailed( "frontend-lens-source" );
}
static qboolean LensVisibility( int id, float *visibility ) {
	/* Fail closed with r_lens 0 (Vulkan RE_GetLensVisibility contract). */
	if ( s_module.lens && !s_module.lens->integer ) {
		if ( visibility ) *visibility = 0.0f;
		return qfalse;
	}
	return s_module.presentation
		? RalMetal_PresentLensVisibility( s_module.presentation, id, visibility )
		: qfalse;
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
static void SubmitUiTransform( const refUiTransform_t *transform ) {
	if ( !RenderSubmission_SetUiTransform( &s_module.frontend, transform ) )
		MarkFailed( "frontend-ui-transform" );
}
static void NoopClip( const float *region ) { (void)region; }
static void NoopMsdfOutline( float width, const float *color,
		float glowWidth, const float *glowColor ) {
	(void)width; (void)color; (void)glowWidth; (void)glowColor;
}
static void NoopMsdfShadow( float x, float y, const float *color ) {
	(void)x; (void)y; (void)color;
}
static void DropInvalidUiSubmission( void ) {
	/* UI is authored content.  An invalid material or a frame that exhausts the
	 * portable primitive budget is dropped just like a malformed refEntity; it
	 * must not poison the renderer lifecycle or suppress the gameplay frame. */
	if ( !s_module.loggedInvalidUiDrop && s_module.imports.LogCh
			&& s_module.initLogChannel >= 0 ) {
		s_module.imports.LogCh( s_module.initLogChannel, SEV_WARN,
			"Wired native Metal RAL: dropped invalid/budget-exhausted UI submission\n" );
		s_module.loggedInvalidUiDrop = qtrue;
	}
}
static void NoopPic( float x, float y, float width, float height,
		float s1, float t1, float s2, float t2, qhandle_t shader ) {
	if ( !s_module.frameOpen ) return;
	if ( !RenderSubmission_AddUiQuad( &s_module.frontend, x, y, width, height,
			s1, t1, s2, t2, 0.0f, shader ) ) DropInvalidUiSubmission();
}
static void NoopBackdrop( float x, float y, float width, float height,
		float time, float mouseX, float mouseY, float transition ) {
	if ( !s_module.frameOpen ) return;
	if ( !RenderSubmission_AddUiQuad( &s_module.frontend, x, y, width, height,
			time, mouseX, mouseY, transition, 0.0f, INT_MAX ) )
		DropInvalidUiSubmission();
}
static void NoopRotatedPic( float x, float y, float width, float height,
		float s1, float t1, float s2, float t2, float angle, qhandle_t shader ) {
	if ( !s_module.frameOpen ) return;
	if ( !RenderSubmission_AddUiQuad( &s_module.frontend, x, y, width, height,
			s1, t1, s2, t2, angle, shader ) ) DropInvalidUiSubmission();
}
static void NoopLine( float x1, float y1, float x2, float y2,
		float width, qhandle_t shader ) {
	if ( !s_module.frameOpen ) return;
	if ( !RenderSubmission_AddUiLine( &s_module.frontend, x1, y1, x2, y2,
			width, shader ) ) DropInvalidUiSubmission();
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
	request.sceneFormat = RAL_FORMAT_R16G16B16A16_SFLOAT;
	request.requestHdrOutput = hdr;
	request.selectedOutput.format = presentation->selected.format;
	request.selectedOutput.colorSpace = presentation->selected.colorSpace;
	request.toneMapOperator = (ralToneMapOperator_t)( s_module.toneMap
		? s_module.toneMap->integer : 3 );
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

static cvar_t *RegisterMetalRenderCvar( const char *name,
		const char *defaultValue, const char *minimum, const char *maximum,
		qboolean integral, const char *description, cvar_t *fallback ) {
	cvar_t *value;
	if ( !ri.Cvar_Get || !ri.Cvar_CheckRange ) {
		memset( fallback, 0, sizeof( *fallback ) );
		fallback->value = (float)atof( defaultValue );
		fallback->integer = atoi( defaultValue );
		return fallback;
	}
	value = ri.Cvar_Get( name, defaultValue, CVAR_ARCHIVE | CVAR_NODEFAULT );
	if ( !value ) return NULL;
	ri.Cvar_CheckRange( value, minimum, maximum,
		integral ? CV_INTEGER : CV_FLOAT );
	if ( ri.Cvar_SetDescription ) ri.Cvar_SetDescription( value, description );
	if ( ri.Cvar_SetGroup ) ri.Cvar_SetGroup( value, CVG_RENDERER );
	return value;
}

static void BeginRegistration( glconfig_t *config ) {
	if ( !s_module.pbr )
		s_module.pbr = wired_render_pbr_register( &ri, &s_module.pbrDefault );
	if ( !s_module.pbr ) { MarkFailed( "pbr-cvar" ); return; }
	if ( !s_module.dynamicLight )
		s_module.dynamicLight = wired_render_dynamic_light_register( &ri, &s_module.dynamicLightDefault );
	if ( !s_module.dynamicLight ) { MarkFailed( "dynamiclight-cvar" ); return; }
	qboolean initializedNow;
	if ( !config || !s_module.loaded || s_module.frameOpen ) {
		MarkFailed( "begin-registration-state" ); return;
	}
	initializedNow = s_module.core ? qfalse : qtrue;
	if ( ri.Cvar_Get && !s_module.textureMode ) {
		s_module.textureMode = ri.Cvar_Get( "r_textureMode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE );
		s_module.anisotropic = ri.Cvar_Get( "r_ext_texture_filter_anisotropic", "1", CVAR_ARCHIVE );
		s_module.maxAnisotropy = ri.Cvar_Get( "r_ext_max_anisotropy", "8", CVAR_ARCHIVE );
		if ( ri.Cvar_CheckRange ) {
			if ( s_module.anisotropic ) ri.Cvar_CheckRange( s_module.anisotropic, "0", "1", CV_INTEGER );
			if ( s_module.maxAnisotropy ) ri.Cvar_CheckRange( s_module.maxAnisotropy, "1", "16", CV_INTEGER );
		}
	}
	s_module.timingFrames = 0u;
	if ( !s_module.gpuSpeeds && ri.Cvar_Get ) {
		s_module.gpuSpeeds = ri.Cvar_Get( "r_gpuSpeeds", "0", 0 );
	}
	if ( !s_module.speeds && ri.Cvar_Get ) {
		s_module.speeds = ri.Cvar_Get( "r_speeds", "0", 0 );
	}
	if ( initializedNow && !InitializeOwners() ) {
		MarkFailed( "owner-initialization" ); return;
	}
	if ( !s_module.brightness ) {
		if ( !ri.Cvar_Get || !ri.Cvar_CheckRange ) {
			memset( &s_module.brightnessFallback, 0,
				sizeof( s_module.brightnessFallback ) );
			s_module.brightnessFallback.value = 1.4f;
			s_module.brightnessFallback.integer = 1;
			s_module.brightness = &s_module.brightnessFallback;
		} else {
			s_module.brightness = ri.Cvar_Get( "r_brightness", "1.4",
				CVAR_ARCHIVE | CVAR_NODEFAULT );
			if ( !s_module.brightness ) {
				MarkFailed( "display-visibility-cvar" ); return;
			}
			ri.Cvar_CheckRange( s_module.brightness, "0", "32", CV_FLOAT );
			if ( ri.Cvar_SetDescription ) ri.Cvar_SetDescription( s_module.brightness,
				"Continuous display visibility scalar; default 1.4, 1.0 is authored identity, fractional values are preserved." );
			if ( ri.Cvar_SetGroup ) ri.Cvar_SetGroup( s_module.brightness, CVG_RENDERER );
		}
	}
	if ( !s_module.lightmapBoost ) {
		if ( !ri.Cvar_Get || !ri.Cvar_CheckRange ) {
			memset( &s_module.lightmapBoostFallback, 0,
				sizeof( s_module.lightmapBoostFallback ) );
			s_module.lightmapBoostFallback.value = 4.6f;
			s_module.lightmapBoostFallback.integer = 4;
			s_module.lightmapBoost = &s_module.lightmapBoostFallback;
		} else {
			s_module.lightmapBoost = ri.Cvar_Get( "r_lightmapBoost", "4.6",
				CVAR_ARCHIVE | CVAR_NODEFAULT );
			if ( !s_module.lightmapBoost ) {
				MarkFailed( "lightmap-boost-cvar" ); return;
			}
			ri.Cvar_CheckRange( s_module.lightmapBoost, "1", "24", CV_FLOAT );
			if ( ri.Cvar_SetDescription ) ri.Cvar_SetDescription(
				s_module.lightmapBoost,
				"World-lightmap overbright applied after sRGB-to-linear decode." );
			if ( ri.Cvar_SetGroup ) ri.Cvar_SetGroup( s_module.lightmapBoost,
				CVG_RENDERER );
		}
	}
	if ( !s_module.aerialHaze ) {
		if ( !ri.Cvar_Get || !ri.Cvar_CheckRange ) {
			memset( &s_module.aerialHazeFallback, 0,
				sizeof( s_module.aerialHazeFallback ) );
			s_module.aerialHazeFallback.value = 1.0f;
			s_module.aerialHazeFallback.integer = 1;
			s_module.aerialHaze = &s_module.aerialHazeFallback;
		} else {
			s_module.aerialHaze = ri.Cvar_Get( "r_atmosphereAerialHaze", "1",
				CVAR_ARCHIVE | CVAR_NODEFAULT );
			if ( !s_module.aerialHaze ) {
				MarkFailed( "aerial-haze-cvar" ); return;
			}
			ri.Cvar_CheckRange( s_module.aerialHaze, "0", "1", CV_FLOAT );
			if ( ri.Cvar_SetDescription ) ri.Cvar_SetDescription(
				s_module.aerialHaze,
				"Aerial-perspective tint of analytic weather media by sky radiance (0 = flat media colour)." );
			if ( ri.Cvar_SetGroup ) ri.Cvar_SetGroup( s_module.aerialHaze,
				CVG_RENDERER );
		}
	}
	/* r_lens gates the lens occlusion oracle exactly like Vulkan (default off);
	 * a host without cvars (contract tests) keeps the oracle enabled. */
	if ( !s_module.lens ) {
		if ( !ri.Cvar_Get || !ri.Cvar_CheckRange ) {
			memset( &s_module.lensFallback, 0, sizeof( s_module.lensFallback ) );
			s_module.lensFallback.value = 1.0f; s_module.lensFallback.integer = 1;
			s_module.lens = &s_module.lensFallback;
		} else {
			s_module.lens = ri.Cvar_Get( "r_lens", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
			if ( !s_module.lens ) { MarkFailed( "lens-cvar" ); return; }
			ri.Cvar_CheckRange( s_module.lens, "0", "1", CV_INTEGER );
			if ( ri.Cvar_SetDescription ) ri.Cvar_SetDescription( s_module.lens, "Depth-sampled lens occlusion oracle for flares (replaces the per-flare dot-probe with a smooth N-tap visibility). Requires r_fbo 1 + r_flares 1." );
			if ( ri.Cvar_SetGroup ) ri.Cvar_SetGroup( s_module.lens, CVG_RENDERER );
		}
	}
	if ( !s_module.skyPhysicalBlend ) {
		if ( !ri.Cvar_Get || !ri.Cvar_CheckRange ) {
			memset( &s_module.skyPhysicalBlendFallback, 0,
				sizeof( s_module.skyPhysicalBlendFallback ) );
			s_module.skyPhysicalBlendFallback.value = 0.6f;
			s_module.skyPhysicalBlendFallback.integer = 0;
			s_module.skyPhysicalBlend = &s_module.skyPhysicalBlendFallback;
		} else {
			s_module.skyPhysicalBlend = ri.Cvar_Get( "r_skyPhysicalBlend", "0.6",
				CVAR_ARCHIVE | CVAR_NODEFAULT );
			if ( !s_module.skyPhysicalBlend ) {
				MarkFailed( "sky-physical-blend-cvar" ); return;
			}
			ri.Cvar_CheckRange( s_module.skyPhysicalBlend, "0", "1", CV_FLOAT );
			if ( ri.Cvar_SetDescription ) ri.Cvar_SetDescription(
				s_module.skyPhysicalBlend,
				"0..1: how strongly the physical sky relights the authored sky in physical mode (1 = full relight, 0 = authored)." );
			if ( ri.Cvar_SetGroup ) ri.Cvar_SetGroup( s_module.skyPhysicalBlend,
				CVG_RENDERER );
		}
	}
	if ( !s_module.shaderTimeOverride ) {
		if ( !ri.Cvar_Get || !ri.Cvar_CheckRange ) {
			memset( &s_module.shaderTimeOverrideFallback, 0,
				sizeof( s_module.shaderTimeOverrideFallback ) );
			s_module.shaderTimeOverride = &s_module.shaderTimeOverrideFallback;
		} else {
			s_module.shaderTimeOverride = ri.Cvar_Get( "r_pinShaderTime", "0",
				CVAR_CHEAT | CVAR_NODEFAULT );
			if ( !s_module.shaderTimeOverride ) {
				MarkFailed( "shader-time-cvar" ); return;
			}
			ri.Cvar_CheckRange( s_module.shaderTimeOverride, "0", "86400", CV_FLOAT );
			if ( ri.Cvar_SetDescription ) ri.Cvar_SetDescription(
				s_module.shaderTimeOverride,
				"Pins material and sky shader time for deterministic visual captures; 0 uses scene time." );
			if ( ri.Cvar_SetGroup ) ri.Cvar_SetGroup( s_module.shaderTimeOverride,
				CVG_RENDERER );
		}
	}
	if ( !s_module.toneMap ) {
		static const struct { const char *name, *value, *min, *max, *description; } bloom[] = {
			{ "r_bloom", "1", "0", "1", "Enable linear HDR bloom before tonemap and UI." },
			{ "r_bloomPasses", "2", "1", "4", "Number of bloom pyramid levels (1-4)." },
			{ "r_bloomThresholdMode", "0", "0", "2", "Bloom extraction metric: channel, average, Rec.709 luma." },
			{ "r_bloomThreshold", "0.32", "0", "65504", "Bloom threshold in linear scene radiance." },
			{ "r_bloomIntensity", "0.5", "0", "65504", "Final bloom blend factor." }
		};
		for ( unsigned i = 0; i < ARRAY_LEN( bloom ); ++i ) {
			s_module.bloomCvars[i] = RegisterMetalRenderCvar( bloom[i].name,
				bloom[i].value, bloom[i].min, bloom[i].max, i < 3u ? qtrue : qfalse,
				bloom[i].description, &s_module.bloomFallback[i] );
			if ( !s_module.bloomCvars[i] ) { MarkFailed( "bloom-cvars" ); return; }
		}
		s_module.aa = RegisterMetalRenderCvar( "r_aa", "1", "0", "2", qtrue,
			"Anti-aliasing: 0 off, 1 TAA (default; camera-motion reprojection on this renderer, moving entities rely on the depth test and clip), 2 SMAA.",
			&s_module.aaFallback );
		s_module.smaa = RegisterMetalRenderCvar( "r_smaa", "0", "0", "4", qtrue,
			"SMAA quality preset 1 low .. 4 ultra; 0 uses preset 3 (high) when r_aa selects SMAA.", &s_module.smaaFallback );
		if ( !s_module.aa || !s_module.smaa ) { MarkFailed( "aa-cvars" ); return; }
		if ( ri.Cvar_Get && ri.Cvar_Set ) { ri.Cvar_Get( "r_aaSupported", "1", CVAR_TEMP ); ri.Cvar_Set( "r_aaSupported", "1" ); }
		s_module.toneMap = RegisterMetalRenderCvar( "r_tonemap", "3", "0", "4",
			qtrue, "HDR tone mapping operator: 0 identity, 1 PBR Neutral, 2 AgX, 3 Lottes, 4 Reinhard.",
			&s_module.toneMapFallback );
		s_module.toneMapExposure = RegisterMetalRenderCvar( "r_tonemapExposure",
			"1", "0.1", "8", qfalse,
			"Linear pre-tonemap exposure multiplier.",
			&s_module.toneMapExposureFallback );
		s_module.lottesContrast = RegisterMetalRenderCvar( "r_lottes_contrast",
			"1.6", "0.5", "3", qfalse, "Lottes tonemap contrast.",
			&s_module.lottesContrastFallback );
		s_module.lottesShoulder = RegisterMetalRenderCvar( "r_lottes_shoulder",
			"0.977", "0.5", "1", qfalse, "Lottes highlight shoulder.",
			&s_module.lottesShoulderFallback );
		s_module.lottesMidIn = RegisterMetalRenderCvar( "r_lottes_mid_in",
			"0.18", "0", "1", qfalse, "Lottes input midpoint.",
			&s_module.lottesMidInFallback );
		s_module.lottesMidOut = RegisterMetalRenderCvar( "r_lottes_mid_out",
			"0.267", "0", "1", qfalse, "Lottes output midpoint.",
			&s_module.lottesMidOutFallback );
		s_module.lottesHdrMax = RegisterMetalRenderCvar( "r_lottes_hdr_max",
			"8", "1", "64", qfalse, "Lottes HDR white point.",
			&s_module.lottesHdrMaxFallback );
		if ( !s_module.toneMap || !s_module.toneMapExposure
				|| !s_module.lottesContrast || !s_module.lottesShoulder
				|| !s_module.lottesMidIn || !s_module.lottesMidOut
				|| !s_module.lottesHdrMax ) {
			MarkFailed( "tonemap-cvars" ); return;
		}
	}
	if ( !s_module.autoExposure[0] ) {
		static const struct {
			const char *name, *value, *min, *max;
		} controls[] = {
			{ "r_hdrAutoExposure", "1", "0", "1" },
			{ "r_hdrExposureKey", "0.0123", "0.01", "1" },
			{ "r_hdrExposurePctLow", "0.5", "0", "1" },
			{ "r_hdrExposurePctHigh", "0.2", "0", "1" },
			{ "r_hdrAdaptionRateUp", "1.5", "0", "100" },
			{ "r_hdrAdaptionRateDown", "0.5", "0", "100" },
			{ "r_hdrExposureMin", "0.25", "0.01", "1" },
			{ "r_hdrExposureMax", "8", "1", "64" }
		};
		for ( unsigned i = 0; i < ARRAY_LEN( controls ); ++i ) {
			s_module.autoExposure[i] = RegisterMetalRenderCvar( controls[i].name,
				controls[i].value, controls[i].min, controls[i].max,
				i == 0 ? qtrue : qfalse, "GPU histogram exposure adaptation.",
				&s_module.autoExposureDefaults[i] );
			if ( !s_module.autoExposure[i] ) { MarkFailed( "exposure-cvars" ); return; }
			/* Tuning is per-frame data, not a pipeline rebuild request. */
			if ( i && ri.Cvar_SetGroup )
				ri.Cvar_SetGroup( s_module.autoExposure[i], CVG_NONE );
		}
	}
	if ( !s_module.ambientScale ) {
		s_module.ambientScale = RegisterMetalRenderCvar( "r_ambientScale",
			"0.6", "0", "4", qfalse,
			"Light-grid ambient scaling on entity models.",
			&s_module.ambientScaleFallback );
		s_module.directedScale = RegisterMetalRenderCvar( "r_directedScale",
			"1", "0", "4", qfalse,
			"Light-grid directional scaling on entity models.",
			&s_module.directedScaleFallback );
		if ( !s_module.ambientScale || !s_module.directedScale ) {
			MarkFailed( "entity-light-grid-cvars" ); return;
		}
	}
	if ( s_module.resetRegistrationAssets ) {
		if ( !RalMetal_PresentResetRegistrationAssets( s_module.presentation,
				&s_module.frontend ) ) {
			MarkFailed( "registration-assets-reset" ); return;
		}
		s_module.resetRegistrationAssets = qfalse;
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
	if ( !BuildColorOutputReceipt( &s_module.presentationReceipt,
			&s_module.colorOutputReceipt ) ) {
		MarkFailed( "color-output-contract" ); return;
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
	s_module.frontend.pbrEnabled = s_module.pbr && s_module.pbr->integer != 0 ? qtrue : qfalse;
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
		s_module.timingFrames = 0u;
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
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	uint64_t contentDigest;
	qboolean completeContent, worldChanged, logContent;
	ralDisplayVisibilityPlan_t visibility;
    const qboolean timing = s_module.gpuSpeeds && s_module.gpuSpeeds->integer
        && ri.Microseconds && ri.LogCh && s_module.timingLogChannel >= 0
        ? qtrue : qfalse;
    int64_t submitStart = 0;
    ralMetalSubmitTiming_t submitTiming = {};
    if ( !timing ) s_module.timingFrames = 0u;
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
	ralExposureSettings_t exposure = {};
	ralBloomSettings_t bloom = {};
	qboolean bloomReady = qtrue;
	int smaaQuality = 0;
	qboolean taaEnabled = qfalse;
	if ( s_module.aa && s_module.smaa ) {
		/* r_aa 0 leaves r_smaa as a developer override; 1 (TAA) and 2 (SMAA) both run SMAA here: the Metal renderer has no jittered
		 * velocity-carrying scene pass or history yet (TASK-257), so a TAA request degrades to SMAA instead of to no anti-aliasing. */
		const int mode = s_module.aa->integer, preset = s_module.smaa->integer;
		smaaQuality = mode == 0 ? preset : ( mode == 2 ? ( preset ? preset : 3 ) : 0 );
		taaEnabled = mode == 1 ? qtrue : qfalse;
	}
	for ( unsigned i = 0; i < ARRAY_LEN( s_module.bloomCvars ); ++i )
		if ( !s_module.bloomCvars[i] ) bloomReady = qfalse;
	if ( bloomReady ) {
		bloom.enabled = s_module.bloomCvars[0]->integer;
		bloom.levels = s_module.bloomCvars[1]->integer;
		bloom.thresholdMode = s_module.bloomCvars[2]->integer;
		bloom.threshold = s_module.bloomCvars[3]->value;
		bloom.intensity = s_module.bloomCvars[4]->value;
	}
	qboolean exposureReady = qtrue;
	for ( unsigned i = 0; i < ARRAY_LEN( s_module.autoExposure ); ++i )
		if ( !s_module.autoExposure[i] ) exposureReady = qfalse;
	if ( exposureReady ) {
		exposure.enabled = s_module.autoExposure[0]->integer;
		exposure.key = s_module.autoExposure[1]->value;
		exposure.pctLow = s_module.autoExposure[2]->value;
		exposure.pctHigh = s_module.autoExposure[3]->value;
		exposure.rateUp = s_module.autoExposure[4]->value;
		exposure.rateDown = s_module.autoExposure[5]->value;
		exposure.minExp = s_module.autoExposure[6]->value;
		exposure.maxExp = s_module.autoExposure[7]->value;
	}
	ralSamplerCreateInfo_t sampling;
	const qboolean samplingReady = wired_texture_sampling_policy(
		s_module.textureMode ? s_module.textureMode->string : "GL_LINEAR_MIPMAP_LINEAR",
		s_module.anisotropic && s_module.anisotropic->integer ? qtrue : qfalse,
		s_module.maxAnisotropy ? s_module.maxAnisotropy->value : 1.0f, &sampling );
	/* Like GL_TextureMode, invalid user input leaves the active policy intact;
	 * it must not turn a typo into renderer shutdown. */
	if ( !samplingReady && !s_module.loggedInvalidSampling && ri.Log )
		ri.Log( SEV_WARN, "invalid texture sampling settings; keeping active samplers\n" );
	s_module.loggedInvalidSampling = !samplingReady;
	const qboolean samplingApplied = !samplingReady
		|| wired_metal_present_set_sampling( s_module.presentation, &sampling );
	if ( samplingReady && samplingApplied && ( !s_module.samplingPublished
			|| memcmp( &sampling, &s_module.activeSampling, sizeof( sampling ) ) ) ) {
		s_module.activeSampling = sampling;
		s_module.samplingPublished = qtrue;
		if ( ri.LogCh && s_module.initLogChannel >= 0 ) ri.LogCh( s_module.initLogChannel, SEV_INFO,
			"Wired metal material sampler: min=%d mag=%d mip=%d aniso=%g maxLod=%g\n",
			sampling.minFilter, sampling.magFilter, sampling.mipmapMode,
			fminf( 16.0f, floorf( sampling.maxAnisotropy ) ), sampling.maxLod );
	}
	if ( !samplingApplied
			|| !bloomReady || !RalMetal_PresentSetBloom( s_module.presentation, &bloom )
			|| !RalMetal_PresentSetSmaaQuality( s_module.presentation, smaaQuality )
			|| !RalMetal_PresentSetTaa( s_module.presentation, taaEnabled )
			|| !exposureReady || !RalMetal_PresentSetExposure( s_module.presentation, &exposure )
			|| !s_module.brightness
			|| !Ral_DisplayVisibilityPlanBuild( s_module.brightness->value, &visibility )
			|| !RalMetal_PresentSetDisplayVisibility( s_module.presentation,
				&visibility )
			|| !s_module.lightmapBoost
			|| !RalMetal_PresentSetLightmapBoost( s_module.presentation,
				s_module.lightmapBoost->value )
			|| !s_module.aerialHaze
			|| !RalMetal_PresentSetAerialHaze( s_module.presentation,
				s_module.aerialHaze->value )
			|| !s_module.skyPhysicalBlend
			|| !Ral_SkySurfaceSetPhysicalBlendScale( s_module.skyPhysicalBlend->value )
			|| !s_module.shaderTimeOverride
			|| !RalMetal_PresentSetShaderTimeOverride( s_module.presentation,
				s_module.shaderTimeOverride->value )
			|| !s_module.toneMap || !s_module.toneMapExposure
			|| !s_module.lottesContrast || !s_module.lottesShoulder
			|| !s_module.lottesMidIn || !s_module.lottesMidOut
			|| !s_module.lottesHdrMax
			|| !RalMetal_PresentSetToneMap( s_module.presentation,
				(uint32_t)s_module.toneMap->integer,
				s_module.toneMapExposure->value,
				s_module.lottesContrast->value,
				s_module.lottesShoulder->value,
				s_module.lottesMidIn->value,
				s_module.lottesMidOut->value,
				s_module.lottesHdrMax->value ) ) {
		if ( Ral_FrameShellCancel( &s_module.frameShell, &s_module.frameReceipt,
				&canceled ) ) s_module.frameReceipt = canceled;
		RenderSubmission_CancelFrame( &s_module.frontend );
		s_module.frameOpen = qfalse; MarkFailed( "display-visibility" ); return;
	}
    if ( timing ) {
        if ( !s_module.timingFrames ) {
            s_module.timingAcquireUs = s_module.timingSubmitUs = 0u;
            memset( &s_module.timingPhases, 0, sizeof( s_module.timingPhases ) );
            (void)RalMetal_PresentGpuTiming( s_module.presentation,
                &s_module.timingBaseline );
        }
        submitStart = ri.Microseconds();
    }
	const ralMetalSubmitResult_t submitResult = RalMetal_PresentRenderAndSubmit(
		s_module.presentation, &s_module.coreReceipt, &s_module.presentationReceipt,
		&s_module.frontend, clearColor, acquireGeneration, presentGeneration,
		&published.drawable, &published.presentation, timing ? &submitTiming : NULL );
	if ( submitResult != RAL_METAL_SUBMIT_PRESENTED ) {
		if ( Ral_FrameShellCancel( &s_module.frameShell, &s_module.frameReceipt,
				&canceled ) ) s_module.frameReceipt = canceled;
		RenderSubmission_CancelFrame( &s_module.frontend );
		s_module.frameOpen = qfalse;
		/* A temporarily unavailable drawable is a retryable host event, not a
		 * reason to switch renderer or publish an incomplete frame receipt. */
		if ( submitResult != RAL_METAL_SUBMIT_DRAWABLE_UNAVAILABLE )
			MarkFailed( "present-submit" );
		return;
	}
    if ( timing ) {
        const int64_t end = ri.Microseconds();
        const uint64_t acquireMicroseconds = submitTiming.acquireNs / 1000u;
        if ( end >= submitStart && (uint64_t)( end - submitStart ) >= acquireMicroseconds ) {
            s_module.timingAcquireUs += acquireMicroseconds;
            s_module.timingSubmitUs += (uint64_t)( end - submitStart ) - acquireMicroseconds;
#define WIRED_METAL_ACCUM_PHASE(field) s_module.timingPhases.field += submitTiming.field
            WIRED_METAL_ACCUM_PHASE(prepareNs);
            WIRED_METAL_ACCUM_PHASE(encodeBeforeAcquireNs);
            WIRED_METAL_ACCUM_PHASE(acquireNs);
            WIRED_METAL_ACCUM_PHASE(encodeAfterAcquireNs);
            WIRED_METAL_ACCUM_PHASE(submitNs);
            WIRED_METAL_ACCUM_PHASE(retireNs);
            WIRED_METAL_ACCUM_PHASE(totalNs);
#undef WIRED_METAL_ACCUM_PHASE
            if ( ++s_module.timingFrames == 200u ) {
                ralMetalGpuTiming_t gpu = {};
                if ( RalMetal_PresentGpuTiming( s_module.presentation, &gpu ) ) {
                    ri.LogCh( s_module.timingLogChannel, SEV_INFO, "metal perf v1: frames=200 acquire_total_us=%llu submit_total_us=%llu gpu_samples=%llu gpu_total_ns=%llu gpu_unavailable=%llu\n",
                        (unsigned long long)s_module.timingAcquireUs,
                        (unsigned long long)s_module.timingSubmitUs,
                        (unsigned long long)( gpu.sampleCount - s_module.timingBaseline.sampleCount ),
                        (unsigned long long)( gpu.durationNanoseconds - s_module.timingBaseline.durationNanoseconds ),
                        (unsigned long long)( gpu.unavailableCount - s_module.timingBaseline.unavailableCount ) );
                    ri.LogCh( s_module.timingLogChannel, SEV_INFO,
                        "metal gpu timeline v1: frames=200 samples=%llu commit_to_start_ns=%llu execution_ns=%llu unavailable=%llu\n",
                        (unsigned long long)( gpu.timelineSampleCount - s_module.timingBaseline.timelineSampleCount ),
                        (unsigned long long)( gpu.commitToStartNanoseconds - s_module.timingBaseline.commitToStartNanoseconds ),
                        (unsigned long long)( gpu.timelineDurationNanoseconds - s_module.timingBaseline.timelineDurationNanoseconds ),
                        (unsigned long long)( gpu.timelineUnavailableCount - s_module.timingBaseline.timelineUnavailableCount ) );
                }
                ralMetalGpuTiming_t splitGpu = {};
                if ( RalMetal_PresentGpuTiming( s_module.presentation, &splitGpu ) ) {
                    ri.LogCh( s_module.timingLogChannel, SEV_INFO,
                        "metal acquire scene v1: frames=200 samples=%llu unavailable=%llu acquire_ns=%llu before_scene_ns=%llu during_scene_ns=%llu after_scene_ns=%llu\n",
                        (unsigned long long)( splitGpu.acquireSampleCount - s_module.timingBaseline.acquireSampleCount ),
                        (unsigned long long)( splitGpu.acquireUnavailableCount - s_module.timingBaseline.acquireUnavailableCount ),
                        (unsigned long long)( splitGpu.acquireDurationNanoseconds - s_module.timingBaseline.acquireDurationNanoseconds ),
                        (unsigned long long)( splitGpu.acquireBeforeSceneNanoseconds - s_module.timingBaseline.acquireBeforeSceneNanoseconds ),
                        (unsigned long long)( splitGpu.acquireDuringSceneNanoseconds - s_module.timingBaseline.acquireDuringSceneNanoseconds ),
                        (unsigned long long)( splitGpu.acquireAfterSceneNanoseconds - s_module.timingBaseline.acquireAfterSceneNanoseconds ) );
                    ri.LogCh( s_module.timingLogChannel, SEV_INFO,
                        "metal gpu split v1: frames=200 samples=%llu scene_ns=%llu output_ns=%llu overlap_ns=%llu gap_ns=%llu\n",
                        (unsigned long long)( splitGpu.splitSampleCount - s_module.timingBaseline.splitSampleCount ),
                        (unsigned long long)( splitGpu.sceneNanoseconds - s_module.timingBaseline.sceneNanoseconds ),
                        (unsigned long long)( splitGpu.outputNanoseconds - s_module.timingBaseline.outputNanoseconds ),
                        (unsigned long long)( splitGpu.overlapNanoseconds - s_module.timingBaseline.overlapNanoseconds ),
                        (unsigned long long)( splitGpu.gapNanoseconds - s_module.timingBaseline.gapNanoseconds ) );
                }
                const ralMetalSubmitTiming_t p = s_module.timingPhases;
                ri.LogCh( s_module.timingLogChannel, SEV_INFO,
                    "metal cpu phases v1: frames=200 prepare_ns=%llu encode_pre_ns=%llu acquire_ns=%llu encode_post_ns=%llu submit_ns=%llu retire_ns=%llu total_ns=%llu\n",
                    (unsigned long long)p.prepareNs, (unsigned long long)p.encodeBeforeAcquireNs,
                    (unsigned long long)p.acquireNs, (unsigned long long)p.encodeAfterAcquireNs,
                    (unsigned long long)p.submitNs, (unsigned long long)p.retireNs,
                    (unsigned long long)p.totalNs );
                s_module.timingFrames = 0u;
            }
        } else s_module.timingFrames = 0u;
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
	published.atmosphere = published.presentation.atmosphere;
	published.frame = completed; published.ready = qtrue;
	s_module.frameReceipt = completed; s_module.frameOpen = qfalse;
	if ( !ModuleReceiptValid( &published ) ) {
		MarkFailed( "published-receipt" ); return;
	}
	s_module.published = published;
	/* Submission evidence, not a synchronous live-particle readback or GPU
	 * completion claim. Keep diagnostics opt-in and use the validated receipt. */
	if ( s_module.speeds && s_module.speeds->integer == 7
			&& ri.LogCh && s_module.timingLogChannel >= 0 ) {
		const ralMetalPresentReceipt_t *p = &published.presentation;
		ri.LogCh( s_module.timingLogChannel, SEV_INFO,
			"metal effects submission v1: emitters=%u admitted=%u pool_instances=%u weather_particles=%u weather_computes=%u weather_draws=%u\n",
			p->effectEmitterDispatchCount, p->effectParticle.admittedParticleCount,
			p->effectParticleDrawCount, p->weather.activeParticleCount,
			p->weatherDispatchCount, p->weatherDrawCount );
		ri.LogCh( s_module.timingLogChannel, SEV_INFO,
			"metal weather collision v1: heightgrid=%u\n", (unsigned)p->weather.heightgridActive );
	}
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
	if ( worldChanged ) {
		s_module.loggedCompleteContentReceipt = qfalse;
		s_module.loggedIrradianceReceipt = qfalse;
	}
	logContent = ( worldChanged || ( completeContent
		&& !s_module.loggedCompleteContentReceipt ) ) ? qtrue : qfalse;
	{
		/* One line per 1000 TAA frames: the gameplay check reads the resolved / bootstrap ratio from it. */
		static uint32_t lastLoggedTaaFrames;
		uint32_t taaResolved = 0u, taaBootstrap = 0u;
		RalMetal_PresentTaaFrameCounts( s_module.presentation, &taaResolved, &taaBootstrap );
		if ( taaResolved + taaBootstrap >= lastLoggedTaaFrames + 1000u && s_module.imports.LogCh && s_module.initLogChannel >= 0 ) {
			lastLoggedTaaFrames = taaResolved + taaBootstrap;
			s_module.imports.LogCh( s_module.initLogChannel, SEV_INFO,
				"Wired native Metal RAL: taa frames resolved=%u bootstrap=%u\n", taaResolved, taaBootstrap );
		}
	}
	if ( published.frontend.worldLoaded == qtrue
			&& published.frontend.sceneRendered == qtrue
			&& logContent
			&& s_module.imports.LogCh && s_module.initLogChannel >= 0 ) {
		s_module.imports.LogCh( s_module.initLogChannel, SEV_INFO,
			"Wired native Metal RAL: scene format=%s extent=%ux%u\n",
			s_module.colorOutputReceipt.sceneFormat == RAL_FORMAT_R16G16B16A16_SFLOAT
				? "R16G16B16A16_SFLOAT" : "UNEXPECTED",
			published.presentation.width, published.presentation.height );
		s_module.imports.LogCh( s_module.initLogChannel, SEV_INFO,
			"Wired native Metal RAL: content receipt asset=%016llx world=%016llx frame=%016llx readback=%016llx bytes=%u readbackXY=%u,%u surfaces=%u vertices=%u indices=%u assets=%u materials=%u resolvedMaterials=%u materialBytes=%u models=%u modelBytes=%u entities=%u temporalEntities=%u localIrradianceEntities=%u irradianceVolumes=%u ui=%u loweredWorld=%u worldBatches=%u texturedWorld=%u lightmappedWorld=%u patchWorld=%u maskedWorld=%u blendedWorld=%u depthWriteWorld=%u loweredEntity=%u entityBatches=%u modelEntities=%u primitiveEntities=%u unresolvedEntities=%u loweredUi=%u texturedUi=%u msdfUi=%u\n",
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
			published.frontend.localIrradianceEntityCount,
			published.frontend.irradianceVolumeCount,
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
	if ( published.frontend.irradianceVolumeCount > 0u
			&& published.frontend.localIrradianceEntityCount > 0u
			&& !s_module.loggedIrradianceReceipt
			&& s_module.imports.LogCh && s_module.initLogChannel >= 0 ) {
		s_module.imports.LogCh( s_module.initLogChannel, SEV_INFO,
			"Wired native Metal RAL: irradiance receipt volumes=%u entities=%u draws=%u\n",
			published.frontend.irradianceVolumeCount,
			published.frontend.localIrradianceEntityCount,
			published.presentation.localIrradianceDrawCount );
		s_module.loggedIrradianceReceipt = qtrue;
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
	for ( int i = 0; i < MAX_RENDER_WORLDS; ++i )
		DestroyDirectionalLighting( i );
	(void)RalMetal_PresentSetMaterialRuntimeCatalog(
		s_module.presentation, NULL, &s_module.frontend );
	RenderMaterialRuntimeOwner_Destroy( &s_module.materialRuntimeOwner );
	/* Loading UI frames remain legal between level teardown and the next
	 * BeginRegistration.  Keep the live registration and presentation owners
	 * across REF_LEVEL_ONLY, matching the engine's map-transition contract. */
	if ( code == REF_LEVEL_ONLY ) {
		s_module.resetRegistrationAssets = qtrue;
		s_module.loadedWorld = NULL;
		s_module.entityParsePoint = NULL;
		if ( !RenderSubmission_ResetEffectRegistries( &s_module.frontend ) )
			MarkFailed( "level-effect-registry-reset" );
		return;
	}
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
static int SubmitTag( orientation_t *tag, qhandle_t model, int startFrame, int endFrame,
		float fraction, const char *name ) {
	return RenderSubmission_LerpTag( &s_module.frontend, tag, model,
		startFrame, endFrame, fraction, name );
}
static void ModelBounds( qhandle_t model, vec3_t mins, vec3_t maxs ) {
	RenderSubmission_ModelBounds( &s_module.frontend, model, mins, maxs );
}
static void NoFont( const char *name, int size, fontInfo_t *font ) {
	(void)name; (void)size; if ( font ) memset( font, 0, sizeof( *font ) );
}
static void NoRemap( const char *oldName, const char *newName, const char *offset ) {
	(void)oldName; (void)newName; (void)offset;
}
static qboolean GetEntityToken( char *buffer, int size ) {
	const char *cursor, *start;
	size_t length;
	if ( !buffer || size <= 0 ) return qfalse;
	buffer[0] = '\0';
	if ( !s_module.loadedWorld || !s_module.loadedWorld->entityString )
		return qfalse;
	cursor = s_module.entityParsePoint;
	if ( !cursor ) cursor = s_module.loadedWorld->entityString;
	while ( *cursor && (unsigned char)*cursor <= ' ' ) cursor++;
	if ( !*cursor ) {
		s_module.entityParsePoint = s_module.loadedWorld->entityString;
		return qfalse;
	}
	if ( *cursor == '{' || *cursor == '}' ) {
		buffer[0] = *cursor++;
		buffer[1] = '\0';
		s_module.entityParsePoint = cursor;
		return qtrue;
	}
	if ( *cursor == '"' ) {
		start = ++cursor;
		while ( *cursor && *cursor != '"' ) cursor++;
		length = (size_t)( cursor - start );
		if ( *cursor == '"' ) cursor++;
	} else {
		start = cursor;
		while ( *cursor && (unsigned char)*cursor > ' '
				&& *cursor != '{' && *cursor != '}' ) cursor++;
		length = (size_t)( cursor - start );
	}
	if ( length >= (size_t)size ) length = (size_t)size - 1u;
	memcpy( buffer, start, length );
	buffer[length] = '\0';
	s_module.entityParsePoint = cursor;
	return qtrue;
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
static void NoFog( refFogType_t *type, vec3_t color, float *depth, float *density ) {
	if ( type ) *type = REF_FT_NONE; if ( color ) memset( color, 0, sizeof( vec3_t ) );
	if ( depth ) *depth = 0.0f; if ( density ) *density = 0.0f;
}
static void NoViewFog( const vec3_t origin, refFogType_t *type, vec3_t color,
		float *depth, float *density, qboolean *useColor ) {
	(void)origin; NoFog( type, color, depth, density );
	if ( useColor ) *useColor = qfalse;
}
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
	exports->SelectWorld = SelectWorld; exports->UnloadWorld = UnloadWorld;
	exports->ResidentWorldCount = ResidentWorldCount;
	exports->WorldContentIdentity = WorldContentIdentity;
	exports->EndRegistration = EndRegistration; exports->ClearScene = SubmitClearScene;
	exports->AddRefEntityToScene = SubmitEntity; exports->AddPolyToScene = SubmitPoly;
	exports->LightForPoint = NoLight; exports->AddLightToScene = SubmitLight;
	exports->AddAdditiveLightToScene = SubmitLight;
	exports->AddLinearLightToScene = SubmitLinearLight;
	exports->AddRibbonToScene = SubmitRibbon; exports->AddBeamToScene = SubmitBeam;
	exports->AddSpriteToScene = SubmitSprite; exports->EmitParticles = SubmitEmitter;
	exports->AddDecalToScene = SubmitDecal; exports->RegisterParticleClass = SubmitParticleClass;
	exports->SetAtmosphere = SubmitAtmosphere; exports->SetAtmosphereHeightgrid = SubmitHeightgrid;
	exports->AddLensSourceToScene = SubmitLens; exports->GetLensVisibility = LensVisibility;
	exports->RenderScene = SubmitScene; exports->SetColor = SubmitColor;
	exports->SetMSDFOutline = NoopMsdfOutline; exports->SetMSDFShadow = NoopMsdfShadow;
	exports->SetClipRegion = NoopClip; exports->SetUiTransform = SubmitUiTransform;
	exports->DrawStretchPic = NoopPic;
	exports->DrawMenuBackdrop = NoopBackdrop; exports->DrawStretchPicOverlay = NoopPic;
	exports->DrawRotatedPic = NoopRotatedPic; exports->DrawLine = NoopLine;
	exports->DrawStretchRaw = NoopRaw; exports->UploadCinematic = NoopUpload;
	exports->BeginFrame = BeginFrame; exports->EndFrame = EndFrame;
	exports->MarkFragments = NoFragments; exports->LerpTag = SubmitTag;
	exports->ModelBounds = ModelBounds; exports->RegisterFont = NoFont;
	exports->RemapShader = NoRemap; exports->GetEntityToken = GetEntityToken;
	exports->inPVS = NoPvs; exports->TakeVideoFrame = NoVideo;
	/* Deferred submissions run bloom in PresentEndFrame, after scene effects and
	 * before tonemap/UI. The legacy immediate FinishBloom hook must not run it twice. */
	exports->ThrottleBackend = NoopVoid; exports->FinishBloom = NoopVoid;
	exports->SetColorMappings = NoopVoid; exports->CanMinimize = CanMinimize;
	exports->GetConfig = GetConfig; exports->GetMemoryBudget = GetMemoryBudget;
	exports->VertexLighting = NoopBool; exports->SyncRender = NoopVoid;
	exports->GetGlobalFog = NoFog; exports->GetViewFog = NoViewFog;
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
	exports->AddAtmosphereEmitter = SubmitAtmosphereEmitter;
	exports->RegisterAtmosphereEffectProfile = SubmitAtmosphereEffectProfile;
	exports->AddAtmosphereSurfaceEvent = SubmitAtmosphereSurfaceEvent;
	exports->AddAtmosphereMediaVolume = SubmitAtmosphereMediaVolume;
	exports->CookLightingProject = CookLightingProject;
	exports->CookMaterialProject = CookMaterialProject;
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
	s_module.timingLogChannel = -1;
	s_module.imports = *imports;
	ri = *imports;
	if ( imports->GetLogChannel && imports->LogCh ) {
		s_module.initLogChannel = imports->GetLogChannel( "renderer.init" );
		s_module.timingLogChannel = imports->GetLogChannel( "renderer.timing" );
	}
	FillExports( &s_module.exports );
	s_module.loaded = qtrue;
	return &s_module.exports;
}
