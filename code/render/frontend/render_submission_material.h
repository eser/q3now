// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_SUBMISSION_MATERIAL_H
#define WIRED_RENDER_FRONTEND_SUBMISSION_MATERIAL_H

#include "q_shared.h"
#include "../ral/core/ral_lighting.h"
#include "../ral/core/ral_material_source.h"
#include "../ral/core/ral_material_runtime.h"
#include "../ral/core/ral_material_stage_depth.h"
#include "../ral/core/ral_material_stage_blend.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_SUBMISSION_MAX_MATERIALS 1024u
#define RENDER_SUBMISSION_MAX_MATERIAL_BYTES (128u * 1024u * 1024u)
#define RENDER_SUBMISSION_MAX_STAGE_TEXTURES 2048u
#define RENDER_SUBMISSION_MAX_STAGE_TEXTURE_BYTES (128u * 1024u * 1024u)
#define RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES \
	( RENDER_SUBMISSION_MAX_MATERIALS + RENDER_SUBMISSION_MAX_STAGE_TEXTURES )
#define RENDER_SUBMISSION_MAX_IMAGE_DIMENSION 16384u
#define RENDER_MATERIAL_LIGHTING_SCHEMA_VERSION 2u
#define RENDER_MATERIAL_FACTORS_SCHEMA_VERSION 1u
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
	RENDER_MATERIAL_TCGEN_ENVIRONMENT,
	RENDER_MATERIAL_TCGEN_VECTOR
} renderMaterialTcGen_t;

#define RENDER_MATERIAL_MAX_ANIMATION_FRAMES \
	RAL_MATERIAL_SOURCE_MAX_ANIMATION_FRAMES

/* Runtime operands only; source spans remain in the immutable cook receipt. */
typedef struct {
	uint32_t type;
	float parameters[6];
} renderMaterialTcMod_t;

typedef struct {
	qhandle_t material;
	qhandle_t animationMaterials[RENDER_MATERIAL_MAX_ANIMATION_FRAMES];
	uint32_t animationFrameCount;
	float animationFrequency;
	qboolean animationLooping;
	uint32_t imageSource;
	uint32_t tcGen;
	float tcGenVectors[2][3];
	uint32_t sourceBlend;
	uint32_t destinationBlend;
	uint32_t alphaTest;
	float alphaCutoff;
	/* Per-stage depth semantics survive publication; batch state is not a substitute. */
	qboolean depthWrite;
	uint32_t depthFunc;
	uint32_t rgbGen;
	uint32_t alphaGen;
	float constantColor[4];
	ralMaterialSourceWave_t rgbWave;
	ralMaterialSourceWave_t alphaWave;
	float portalRange;
	float scaleScroll[4];
	float rotateDegrees;
	float turbulence[4];
	float stretch[4];
	qboolean hasTurbulence;
	qboolean hasStretch;
	uint32_t tcModCount;
	renderMaterialTcMod_t tcMods[RAL_MATERIAL_SOURCE_MAX_TCMODS];
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

/* One backend-neutral, world-scoped publication of the immutable factors from
 * a cooked material runtime plan.  The cohort is replaced atomically: a
 * backend may never retain factors from one world while drawing another. */
typedef struct {
	qhandle_t material;
	ralMaterialFactors_t factors;
} renderMaterialFactorPublicationEntry_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t runtimeGeneration;
	ralMaterialFactors_t factors;
	qboolean ready;
} renderMaterialFactorsSnapshot_t;

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
	/* Canonical content identity produced from the same texture-asset request
	 * used by the offline material cooker.  Native GPU ownership remains in the
	 * backend adapter; the complete key (not merely its digest) is the
	 * handle-free, exact format/extent/mip join between a cooked material binding
	 * and that adapter's resident resource receipt. */
	ralTextureCacheKey_t textureCacheKey;
	uint64_t textureCacheKeyHash;
	qboolean textureCacheKeyReady;
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
	ralMaterialSourceSky_t authoredSkySource;
	/* Frontend texture handles in canonical Q3 rt/bk/lf/ft/up/dn order.
	 * These identify already-resident resources without exposing a backend
	 * image/view handle through the shared submission contract. */
	qhandle_t authoredSkyOuterTextures[RAL_MATERIAL_SKY_FACE_COUNT];
	qhandle_t authoredSkyInnerTextures[RAL_MATERIAL_SKY_FACE_COUNT];
	float authoredSkyAverage[3];
	qboolean authoredSkySourceReady;
	qboolean authoredSkyTexturesReady;
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
	ralMaterialStageDepthPlan_t stageDepth;
	qboolean stageBlendAdditive;
	renderMaterialLighting_t lighting;
	renderMaterialFactorsSnapshot_t factors;
	qboolean ready;
} renderMaterialSnapshot_t;

typedef struct {
	renderMaterialSnapshot_t snapshot;
	char name[MAX_QPATH];
} renderMaterialRecord_t;

/* A shader stage image is a texture resource, not another semantic material.
 * Keeping it out of the material catalog prevents animMap frames from
 * exhausting material ownership and preserves authored material handles for
 * late-loaded pickups, UI and WiredFX effects. */
typedef struct {
	qhandle_t handle;
	uint64_t generation;
	uint64_t digest;
	ralTextureCacheKey_t textureCacheKey;
	uint64_t textureCacheKeyHash;
	qboolean textureCacheKeyReady;
	uint32_t width;
	uint32_t height;
	uint32_t rowBytes;
	uint32_t byteCount;
	const byte *rgba8;
	qboolean clampToEdge;
	qboolean msdf;
	qboolean srgb;
	qboolean ready;
} renderTextureSnapshot_t;

/* Material-factor/wave/raster revisions are not texture revisions. Reuse an
 * already resident image only when the authoritative cooked content key and
 * exact native shape/format/mips still match. Sampler state is separate. */
static ID_INLINE qboolean RenderSubmission_TextureResourceMatches(
		const renderTextureSnapshot_t *texture,
		const ralMaterialRuntimeResource_t *resident ) {
	ralMaterialRuntimeResource_t validated;
	if ( !texture || !resident || !texture->ready || !texture->rgba8
			|| texture->textureCacheKeyReady != qtrue
			|| !texture->textureCacheKeyHash
			|| texture->textureCacheKeyHash != texture->textureCacheKey.keyHash
			|| texture->textureCacheKeyHash != resident->cacheKeyHash
			|| texture->width != resident->resource.width
			|| texture->height != resident->resource.height
			|| resident->resource.format != ( texture->srgb
				? RAL_FORMAT_R8G8B8A8_SRGB : RAL_FORMAT_R8G8B8A8_UNORM ) ) return qfalse;
	return Ral_MaterialRuntimeResourceBuild( &texture->textureCacheKey,
		&resident->resource, &validated );
}

typedef struct {
	renderTextureSnapshot_t snapshot;
	char name[MAX_QPATH];
} renderTextureRecord_t;

typedef struct renderSubmissionState_s renderSubmissionState_t;

/* Current registration catalog index, not a persistent handle or GPU owner.
 * Records append with monotonic handles; failure leaves outIndex untouched. */
qboolean RenderSubmission_MaterialIndex(
	const renderSubmissionState_t *state, qhandle_t handle, uint32_t *outIndex );
qboolean RenderSubmission_MaterialSnapshot(
	const renderSubmissionState_t *state, qhandle_t handle,
	renderMaterialSnapshot_t *outSnapshot );
/* Borrowed, read-only view of the catalog record; null when the handle is
 * unknown. Valid until the next registration changes the catalog. Per-frame
 * readers use this instead of RenderSubmission_MaterialSnapshot, which copies
 * the whole record. */
const renderMaterialSnapshot_t *RenderSubmission_MaterialSnapshotView(
	const renderSubmissionState_t *state, qhandle_t handle );
qhandle_t RenderSubmission_MaterialHandle( const renderSubmissionState_t *state,
	const char *name );
qboolean RenderSubmission_TextureSnapshot(
	const renderSubmissionState_t *state, qhandle_t handle,
	renderTextureSnapshot_t *outSnapshot );
qboolean RenderSubmission_SetMaterialTextureCacheKey(
	renderSubmissionState_t *state, qhandle_t handle,
	const ralTextureCacheKey_t *textureCacheKey );
qboolean RenderSubmission_PublishMaterialFactors(
	renderSubmissionState_t *state, uint64_t runtimeGeneration,
	const renderMaterialFactorPublicationEntry_t *entries,
	uint32_t entryCount );

#ifdef __cplusplus
}
#endif

#endif
