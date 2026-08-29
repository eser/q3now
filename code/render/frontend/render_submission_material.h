// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_SUBMISSION_MATERIAL_H
#define WIRED_RENDER_FRONTEND_SUBMISSION_MATERIAL_H

#include "q_shared.h"
#include "../ral/core/ral_lighting.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_SUBMISSION_MAX_MATERIALS 1024u
#define RENDER_SUBMISSION_MAX_MATERIAL_BYTES (128u * 1024u * 1024u)
#define RENDER_SUBMISSION_MAX_IMAGE_DIMENSION 16384u
#define RENDER_MATERIAL_LIGHTING_SCHEMA_VERSION 2u
#define RENDER_MATERIAL_LIGHTING_Q16_ONE 65536
#define RENDER_MATERIAL_MAX_STAGES 8u

/* Authored Quake shader order. Numeric values remain valid between buckets. */
#define RENDER_MATERIAL_SORT_PORTAL       1.0f
#define RENDER_MATERIAL_SORT_ENVIRONMENT  2.0f
#define RENDER_MATERIAL_SORT_OPAQUE       3.0f
#define RENDER_MATERIAL_SORT_DECAL        4.0f
#define RENDER_MATERIAL_SORT_SEE_THROUGH  5.0f
#define RENDER_MATERIAL_SORT_BANNER       6.0f
#define RENDER_MATERIAL_SORT_FOG          7.0f
#define RENDER_MATERIAL_SORT_UNDERWATER   8.0f
#define RENDER_MATERIAL_SORT_BLEND        9.0f
#define RENDER_MATERIAL_SORT_ADDITIVE    10.0f
#define RENDER_MATERIAL_SORT_NEAREST     16.0f

typedef enum {
	RENDER_MATERIAL_TCGEN_TEXTURE = 0,
	RENDER_MATERIAL_TCGEN_LIGHTMAP,
	RENDER_MATERIAL_TCGEN_ENVIRONMENT
} renderMaterialTcGen_t;

typedef struct {
	qhandle_t material;
	uint32_t imageSource;
	uint32_t tcGen;
	uint32_t sourceBlend;
	uint32_t destinationBlend;
	uint32_t alphaTest;
	float alphaCutoff;
	float scaleScroll[4];
	float rotateDegrees;
	float turbulence[4];
	float stretch[4];
	qboolean hasTurbulence;
	qboolean hasStretch;
} renderMaterialStageSnapshot_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t sourceGeneration;
	uint64_t provenanceHash;
	int32_t diffuseReflectanceQ16[3];
	int32_t emissionRadianceQ16[3];
	ralLightMobility_t emissiveMobility;
	int32_t emissiveInfluenceRangeQ16;
	uint32_t emissiveShadowPriority;
	uint32_t emissiveRequestedProxyCount;
	qboolean participatesInStaticBake;
	qboolean emissiveExplicitProxyAuthority;
	qboolean emissiveInjectsAtmosphere;
	qboolean ready;
} renderMaterialLighting_t;

typedef enum {
	RENDER_ALPHA_OPAQUE = 0,
	RENDER_ALPHA_MASK,
	RENDER_ALPHA_BLEND,
	/* Legacy shader `blendFunc add` / GL_ONE GL_ONE.  It is kept explicit so
	 * backend adapters do not turn black additive texels into opaque geometry. */
	RENDER_ALPHA_ADDITIVE,
	/* Q3 `GL_SRC_ALPHA GL_ONE`: alpha-shaped additive energy. */
	RENDER_ALPHA_ALPHA_ADDITIVE
} renderAlphaMode_t;

typedef enum {
	/* Quake shader default: front-facing geometry is visible. */
	RENDER_CULL_BACK = 0,
	RENDER_CULL_FRONT,
	RENDER_CULL_NONE
} renderCullMode_t;

typedef struct {
	qhandle_t handle;
	uint64_t generation;
	uint64_t digest;
	uint32_t width;
	uint32_t height;
	uint32_t rowBytes;
	uint32_t byteCount;
	const byte *rgba8;
	qboolean clampToEdge;
	qboolean msdf;
	qboolean srgb;
	qboolean sky;
	qboolean skyBox;
	qboolean noDraw;
	float skyCloudHeight;
	float skyScaleScroll[4];
	qhandle_t skySecondaryMaterial;
	renderAlphaMode_t skySecondaryAlphaMode;
	float skySecondaryScaleScroll[4];
	renderAlphaMode_t alphaMode;
	renderCullMode_t cullMode;
	float alphaCutoff;
	qboolean depthWrite;
	float sort;
	renderMaterialStageSnapshot_t stages[RENDER_MATERIAL_MAX_STAGES];
	uint32_t stageCount;
	renderMaterialLighting_t lighting;
	qboolean ready;
} renderMaterialSnapshot_t;

typedef struct {
	renderMaterialSnapshot_t snapshot;
	char name[MAX_QPATH];
} renderMaterialRecord_t;

typedef struct renderSubmissionState_s renderSubmissionState_t;

qboolean RenderSubmission_MaterialSnapshot(
	const renderSubmissionState_t *state, qhandle_t handle,
	renderMaterialSnapshot_t *outSnapshot );
qhandle_t RenderSubmission_MaterialHandle( const renderSubmissionState_t *state,
	const char *name );

#ifdef __cplusplus
}
#endif

#endif
