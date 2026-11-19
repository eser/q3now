// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_MATERIAL_SCRIPT_H
#define WIRED_RENDER_FRONTEND_MATERIAL_SCRIPT_H

#include "render_submission_material.h"
#include "render_lighting_metadata.h"
#include "../ral/core/ral_material_source.h"
#include "tr_public.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_MATERIAL_SCRIPT_MAX_ENTRIES 4096u
#define RENDER_MATERIAL_SCRIPT_MAX_STAGES 8u
#define RENDER_MATERIAL_SCRIPT_MAX_ANIMATION_FRAMES \
	RAL_MATERIAL_SOURCE_MAX_ANIMATION_FRAMES
#define RENDER_MATERIAL_SCRIPT_MAX_ANIMATION_PATHS \
	RAL_MATERIAL_SOURCE_MAX_DEPENDENCIES

typedef enum {
	RENDER_MATERIAL_STAGE_IMAGE = 0,
	RENDER_MATERIAL_STAGE_LIGHTMAP,
	RENDER_MATERIAL_STAGE_WHITE
} renderMaterialStageImageSource_t;

typedef enum {
	RENDER_MATERIAL_BLEND_ZERO = 0,
	RENDER_MATERIAL_BLEND_ONE,
	RENDER_MATERIAL_BLEND_SRC_COLOR,
	RENDER_MATERIAL_BLEND_ONE_MINUS_SRC_COLOR,
	RENDER_MATERIAL_BLEND_DST_COLOR,
	RENDER_MATERIAL_BLEND_ONE_MINUS_DST_COLOR,
	RENDER_MATERIAL_BLEND_SRC_ALPHA,
	RENDER_MATERIAL_BLEND_ONE_MINUS_SRC_ALPHA,
	RENDER_MATERIAL_BLEND_DST_ALPHA,
	RENDER_MATERIAL_BLEND_ONE_MINUS_DST_ALPHA,
	RENDER_MATERIAL_BLEND_SRC_ALPHA_SATURATE
} renderMaterialBlendFactor_t;

typedef struct {
	char imageName[MAX_QPATH];
	uint32_t animationFrameOffset;
	uint32_t animationFrameCount;
	float animationFrequency;
	qboolean animationLooping;
	uint32_t sourceLine;
	uint32_t sourceColumn;
	renderMaterialStageImageSource_t imageSource;
	renderMaterialTcGen_t tcGen;
	float tcGenVectors[2][3];
	renderMaterialBlendFactor_t sourceBlend;
	renderMaterialBlendFactor_t destinationBlend;
	qboolean clampToEdge;
	ralMaterialSourceAlphaTest_t alphaTest;
	float alphaCutoff;
	qboolean depthWrite;
	ralMaterialDepthFunc_t depthFunc;
	ralMaterialColorGen_t rgbGen;
	ralMaterialAlphaGen_t alphaGen;
	float constantColor[4];
	ralMaterialSourceWave_t rgbWave;
	ralMaterialSourceWave_t alphaWave;
	float portalRange;
	float scale[2];
	float scroll[2];
	float rotateDegrees;
	float turbulence[4];
	float stretch[4];
	qboolean hasTurbulence;
	qboolean hasStretch;
	qboolean hasTransform;
	float transform[6];
	uint32_t sourceTcModCount;
	ralMaterialSourceTcMod_t sourceTcMods[RAL_MATERIAL_SOURCE_MAX_TCMODS];
} renderMaterialScriptStage_t;

typedef struct {
	char name[MAX_QPATH];
	char declarationPath[MAX_QPATH];
	uint64_t declarationSourceId;
	uint64_t declarationSize;
	unsigned declarationGeneration;
	uint32_t declarationLine;
	uint32_t declarationColumn;
	char imageName[MAX_QPATH];
	char skyBoxPrefix[MAX_QPATH];
	char skyInnerBoxPrefix[MAX_QPATH];
	ralMaterialSourceSky_t skySource;
	char secondaryImageName[MAX_QPATH];
	renderAlphaMode_t alphaMode;
	renderAlphaMode_t secondaryAlphaMode;
	renderCullMode_t cullMode;
	float alphaCutoff;
	qboolean depthWrite;
	float sort;
	qboolean sortExplicit;
	uint32_t surfaceFlags;
	uint32_t contentFlags;
	qboolean clearSolid;
	qboolean clampToEdge;
	qboolean secondaryClampToEdge;
	qboolean sky;
	qboolean noDraw;
	qboolean polygonOffset;
	qboolean noPicMip;
	qboolean noMipMaps;
	float skyCloudHeight;
	float fogColor[3];
	float fogDepth;
	ralMaterialFogType_t fogType;
	float fogDensity;
	float fogFarClip;
	float skyScale[2];
	float skyScroll[2];
	qboolean hasTcTransform;
	float secondarySkyScale[2];
	float secondarySkyScroll[2];
	renderEmissiveMaterialAuthoring_t lighting;
	qboolean hasLighting;
	char animationFrameNames[RENDER_MATERIAL_SCRIPT_MAX_ANIMATION_PATHS][MAX_QPATH];
	uint32_t animationPathCount;
	renderMaterialScriptStage_t stages[RENDER_MATERIAL_SCRIPT_MAX_STAGES];
	uint32_t stageCount;
	ralMaterialSourceDeform_t deforms[RAL_MATERIAL_SOURCE_MAX_DEFORMS];
	uint32_t deformCount;
} renderMaterialScriptEntry_t;

typedef struct {
	renderMaterialScriptEntry_t entries[RENDER_MATERIAL_SCRIPT_MAX_ENTRIES];
	uint32_t count;
	qboolean ready;
} renderMaterialScriptCatalog_t;

qboolean RenderMaterialScript_Load( renderMaterialScriptCatalog_t *catalog,
	const refimport_t *imports );
/* Load/cook boundary only. The resolver retains resources in the existing
 * submission owner; failure leaves the output program unchanged. */
typedef qhandle_t (*renderMaterialStageResolver_t)( void *context,
	const renderMaterialScriptStage_t *stage, const char *imageName );
qboolean RenderMaterialScript_BuildStages(
	const renderMaterialScriptEntry_t *entry,
	renderMaterialStageResolver_t resolve, void *context,
	renderMaterialStageSnapshot_t outStages[RENDER_MATERIAL_MAX_STAGES] );
qboolean RenderMaterialScript_Lookup(
	const renderMaterialScriptCatalog_t *catalog, const char *name,
	renderMaterialScriptEntry_t *outEntry );
qboolean RenderMaterialScript_CompileSource(
	const renderMaterialScriptEntry_t *entry, const refimport_t *imports,
	uint64_t materialGeneration, ralMaterialSourceReceipt_t *outReceipt,
	ralMaterialSourceDiagnostic_t *outDiagnostic );
qboolean RenderMaterialScript_ApplyLighting(
	const renderMaterialScriptCatalog_t *catalog, const char *name,
	renderSubmissionState_t *submission, qhandle_t material );

#ifdef __cplusplus
}
#endif

#endif
