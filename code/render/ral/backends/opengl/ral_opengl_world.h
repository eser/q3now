// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_OPENGL_WORLD_H
#define WIRED_RAL_OPENGL_WORLD_H

#include "ral_opengl_frontend.h"
#include "ral_opengl_lighting.h"
#include "ral_atmosphere.h"
#include "ral_display_visibility.h"
#include "ral_sky_radiance.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_OPENGL_WORLD_SCHEMA_VERSION 13u
/* Effect-mask gba value that selects the camera motion (no object offset): half-float max, far outside any per-frame offset. */
#define RAL_OPENGL_WORLD_CAMERA_MOTION 65504.0f

typedef struct {
	uint64_t generation;
	uint32_t programName;
	uint32_t skyProgramName;
	uint32_t outputWidth;
	uint32_t outputHeight;
	/* Adapter-local scene-complete boundary; postprocess runs before every UI
	 * primitive and must restore the product draw bindings it replaces. */
	qboolean (*sceneComplete)( void *context );
	void *sceneCompleteContext;
	const uint32_t *materialTextureNames;
	uint32_t materialTextureCount;
	uint32_t atmosphereBufferName;
	uint32_t physicalSkyTextureName;
	uint32_t currentSkyIrradianceTextureName;
	uint32_t previousSkyIrradianceTextureName;
	uint32_t currentSkyRadianceTextureName;
	uint32_t previousSkyRadianceTextureName;
	ralSkyRadianceConsumerReceipt_t skyRadianceConsumer;
	ralAtmospherePlanReceipt_t atmosphere;
	uint32_t weatherProgramName;
	uint32_t weatherUniformBufferName;
	uint32_t weatherParticleBufferName;
	uint32_t weatherParticleCount;
	const ralOpenGlLightingReceipt_t *directionalLighting;
	ralDisplayVisibilityPlan_t displayVisibility;
	float aerialHaze; /* r_atmosphereAerialHaze weight, 0..1 */
	float lightmapBoost; /* r_lightmapBoost, linear world-lightmap overbright (1..24) */
	/* TAA: the sub-pixel jitter (NDC) folded into the world view axes, and colour attachment 1 (the RGBA16F effect mask) being
	 * bound: r = history trust (cleared to 1, reduced by the draws that mark it), gba = the object motion offset (the surface's
	 * previous world position minus its current one) or RAL_OPENGL_WORLD_CAMERA_MOTION where the camera motion applies. Scene
	 * programs write r only; the object motion draw writes gba only. */
	float jitterNdc[2];
	qboolean coverageActive;
	/* Entity motion (with coverageActive): the open entity pose cache transaction of this TAA frame (render_temporal_entity.h)
	 * and the program that draws the recorded entities' motion offsets depth-tested against the finished opaque scene. */
	qboolean entityMotionActive;
	int entityMotionWorld;
	uint64_t entityMotionFrame;
	uint32_t objectMotionProgramName;
	const renderDistantWorldSubmission_t *distantWorld;
	const renderEffectDecalPool_t *effectDecalPool;
	renderEffectDecalPoolReceipt_t effectDecalPoolReceipt;
	const renderEffectBeamPool_t *effectBeamPool;
	renderEffectBeamPoolReceipt_t effectBeamPoolReceipt;
} ralOpenGlWorldLowerInfo_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t coreGeneration;
	uint64_t generation;
	uint64_t frontendFrameDigest;
	uint64_t loweringDigest;
	uint32_t uploadedMaterialCount;
	uint32_t uploadedWorldVertexCount;
	uint32_t uploadedWorldIndexCount;
	uint32_t worldDrawCount;
	uint32_t uploadedDistantWorldVertexCount;
	uint32_t uploadedDistantWorldIndexCount;
	uint32_t distantWorldCandidateCount;
	uint32_t distantWorldDrawCount;
	uint32_t legacyLightmapDrawCount;
	uint32_t directionalStaticDrawCount;
	uint64_t surfaceLightingBindingDigest;
	uint32_t polygonCount;
	uint32_t lightCount;
	uint32_t effectIndexCount;
	uint32_t effectDrawCount;
	uint32_t effectSpriteDrawCount;
	uint32_t effectDecalDrawCount;
	uint32_t effectRibbonDrawCount;
	uint32_t effectBeamDrawCount;
	uint32_t effectBeamStageDrawCount;
	uint32_t modelEntityCount;
	uint32_t primitiveEntityCount;
	uint32_t temporalEntityCount;
	/* Entity motion: poses recorded into the pose cache, of them with a committed previous pose, and motion offset draws. */
	uint32_t entityMotionRecorded;
	uint32_t entityMotionPrevious;
	uint32_t entityMotionDraws;
	uint32_t localIrradianceEntityCount;
	uint32_t localIrradianceDrawCount;
	uint32_t entityIndexCount;
	uint32_t entityDrawCount;
	uint32_t uiPrimitiveCount;
	uint32_t texturedUiPrimitiveCount;
	uint32_t msdfUiPrimitiveCount;
	uint32_t uiDrawCount;
	uint32_t weatherDrawCount;
	uint32_t weatherInstanceCount;
	uint32_t nativeDrawCount;
	uint32_t unresolvedCount;
	uint32_t fallbackCount;
	uint32_t fatalCount;
	qboolean ready;
} ralOpenGlWorldReceipt_t;

qboolean RalOpenGl_WorldLower( ralOpenGlCore_t *core,
	const ralOpenGlCoreReceipt_t *coreReceipt,
	const renderSubmissionState_t *frontend,
	const renderSubmissionReceipt_t *submission,
	const ralOpenGlFrontendPlanReceipt_t *plan,
	const ralOpenGlWorldLowerInfo_t *info,
	ralOpenGlWorldReceipt_t *outReceipt );
qboolean RalOpenGl_WorldReceiptExact( const ralOpenGlWorldReceipt_t *a,
	const ralOpenGlWorldReceipt_t *b );
/* Uses the very same world projection parameters; output is bottom-left UV
 * and standard GL device depth, not a CPU visibility/occlusion decision. */
qboolean RalOpenGl_WorldProjectLens( const renderWorldSnapshot_t *view,
	const float origin[3], uint32_t outputHeight, float projected[3], float *radiusScale );

#ifdef __cplusplus
}
#endif

#endif
