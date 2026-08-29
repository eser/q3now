// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_MATERIAL_SCRIPT_H
#define WIRED_RENDER_FRONTEND_MATERIAL_SCRIPT_H

#include "render_submission_material.h"
#include "render_lighting_metadata.h"
#include "tr_public.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_MATERIAL_SCRIPT_MAX_ENTRIES 4096u
#define RENDER_MATERIAL_SCRIPT_MAX_STAGES 8u

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
	renderMaterialStageImageSource_t imageSource;
	renderMaterialTcGen_t tcGen;
	renderMaterialBlendFactor_t sourceBlend;
	renderMaterialBlendFactor_t destinationBlend;
	qboolean clampToEdge;
	qboolean alphaTest;
	float alphaCutoff;
	float scale[2];
	float scroll[2];
	float rotateDegrees;
	float turbulence[4];
	float stretch[4];
	qboolean hasTurbulence;
	qboolean hasStretch;
} renderMaterialScriptStage_t;

typedef struct {
	char name[MAX_QPATH];
	char imageName[MAX_QPATH];
	char skyBoxPrefix[MAX_QPATH];
	char secondaryImageName[MAX_QPATH];
	renderAlphaMode_t alphaMode;
	renderAlphaMode_t secondaryAlphaMode;
	renderCullMode_t cullMode;
	float alphaCutoff;
	qboolean depthWrite;
	float sort;
	qboolean sortExplicit;
	qboolean clampToEdge;
	qboolean secondaryClampToEdge;
	qboolean sky;
	qboolean noDraw;
	float skyCloudHeight;
	float skyScale[2];
	float skyScroll[2];
	qboolean hasTcTransform;
	float secondarySkyScale[2];
	float secondarySkyScroll[2];
	renderEmissiveMaterialAuthoring_t lighting;
	qboolean hasLighting;
	renderMaterialScriptStage_t stages[RENDER_MATERIAL_SCRIPT_MAX_STAGES];
	uint32_t stageCount;
} renderMaterialScriptEntry_t;

typedef struct {
	renderMaterialScriptEntry_t entries[RENDER_MATERIAL_SCRIPT_MAX_ENTRIES];
	uint32_t count;
	qboolean ready;
} renderMaterialScriptCatalog_t;

qboolean RenderMaterialScript_Load( renderMaterialScriptCatalog_t *catalog,
	const refimport_t *imports );
qboolean RenderMaterialScript_Lookup(
	const renderMaterialScriptCatalog_t *catalog, const char *name,
	renderMaterialScriptEntry_t *outEntry );
qboolean RenderMaterialScript_ApplyLighting(
	const renderMaterialScriptCatalog_t *catalog, const char *name,
	renderSubmissionState_t *submission, qhandle_t material );

#ifdef __cplusplus
}
#endif

#endif
