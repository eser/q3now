// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_SUBMISSION_WORLD_H
#define WIRED_RENDER_FRONTEND_SUBMISSION_WORLD_H

#include "q_shared.h"
#include "tr_types.h"
#include "render_submission_material.h"
#include "../ral/core/ral_atmosphere.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_SUBMISSION_MAX_WORLD_VERTICES ( 1024u * 1024u )
#define RENDER_SUBMISSION_MAX_WORLD_INDICES ( 3u * 1024u * 1024u )
#define RENDER_SUBMISSION_MAX_WORLD_BATCHES 65536u
#define RENDER_SUBMISSION_PATCH_SUBDIVISIONS 4u

typedef enum {
	RENDER_WORLD_SURFACE_PLANAR = 1,
	RENDER_WORLD_SURFACE_PATCH,
	RENDER_WORLD_SURFACE_TRIANGLES
} renderWorldSurfaceType_t;

typedef struct {
	float position[3];
	float texCoord[2];
	float lightmapCoord[2];
	uint8_t color[4];
	float normal[3];
} renderWorldVertex_t;

typedef struct {
	uint32_t sourceSurfaceIndex;
	uint32_t firstIndex;
	uint32_t indexCount;
	int32_t shaderIndex;
	int32_t lightmapIndex;
	qhandle_t baseMaterial;
	qhandle_t lightmapMaterial;
	renderWorldSurfaceType_t surfaceType;
	renderAlphaMode_t alphaMode;
	renderCullMode_t cullMode;
	float alphaCutoff;
	qboolean depthWrite;
	float sort;
	qboolean sky;
	qboolean noDraw;
	qboolean visible;
	/* Conservative world-space bounds, computed once from the source surface.
	 * Quadratic patches remain inside their control-point convex hull. */
	float boundsCenter[3];
	float boundsRadius;
} renderWorldBatch_t;

typedef struct {
	const renderWorldVertex_t *vertices;
	const uint32_t *indices;
	const renderWorldBatch_t *batches;
	uint32_t vertexCount;
	uint32_t indexCount;
	uint32_t batchCount;
	uint32_t patchBatchCount;
	uint32_t patchTriangleCount;
	float viewOrigin[3];
	float viewAxis[3][3];
	float fovX;
	float fovY;
	int32_t viewportX;
	int32_t viewportY;
	uint32_t viewportWidth;
	uint32_t viewportHeight;
	/* Painter-order seam for RDF_NOWORLDMODEL UI subscenes.  Native RAL
	 * products lower entity geometry and UI primitives separately, so retain
	 * the number of UI primitives that preceded RenderScene; the backend can
	 * composite the preview between the UI prefix and suffix. */
	uint32_t uiPrimitiveInsertionIndex;
	uint32_t rdflags;
	int32_t timeMs;
	qboolean ready;
} renderWorldSnapshot_t;

typedef struct renderSubmissionState_s renderSubmissionState_t;

typedef struct {
	uint32_t projectedCoverageQ16;
	uint32_t lodLevelCount;
	qboolean visible;
} renderDistantWorldDrawMetric_t;

#define RENDER_DISTANT_WORLD_SUBMISSION_SCHEMA_VERSION 1u

/* Borrowed frontend geometry plus a pointer-free, bounded RAL selection
 * receipt. Backends may upload/cache this world through their ordinary world
 * path, but this contract itself owns no native resource or gameplay state. */
typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t frameGeneration;
	uint64_t contentIdentity;
	renderWorldSnapshot_t world;
	ralSkyDistantDrawPlanReceipt_t drawPlan;
	qboolean ready;
} renderDistantWorldSubmission_t;

qboolean RenderSubmission_WorldSnapshot( const renderSubmissionState_t *state,
	renderWorldSnapshot_t *outSnapshot );
/* Registration-time geometry, independent of view/frame publication. The
 * exact resident lifecycle generation prevents stale catalog rebinding after
 * unload/reload. Returned pointers are borrowed until that world is unloaded;
 * this does not publish a renderable frame or switch the active world. */
qboolean RenderSubmission_ResidentWorldSnapshot(
	const renderSubmissionState_t *state, int worldIndex,
	uint64_t lifecycleGeneration, renderWorldSnapshot_t *outSnapshot );
qboolean RenderSubmission_ViewSnapshot( const renderSubmissionState_t *state,
		renderWorldSnapshot_t *outSnapshot );
qboolean RenderSubmission_ViewCameraCut( const renderSubmissionState_t *state );
/* Builds an app-neutral distant-scene plan from an already resident secondary
 * world without selecting it or creating resources. Candidate scratch remains
 * caller-owned so this per-frame bridge performs no allocation. */
qboolean RenderSubmission_DistantWorldDrawPlan(
	const renderSubmissionState_t *state, int worldIndex,
	const ralSkyEnvironmentPlanReceipt_t *skyEnvironment,
	const renderDistantWorldDrawMetric_t *metrics, uint32_t metricCount,
	uint32_t maxVisibleDraws, ralSkyDistantDrawCandidate_t *candidateScratch,
	uint32_t candidateCapacity, ralSkyDistantDrawPlanReceipt_t *outReceipt );
qboolean RenderSubmission_DistantWorldSubmissionBuild(
	const renderSubmissionState_t *state, int worldIndex,
	const refdef_t *mainView,
	const ralSkyDistantDrawPlanReceipt_t *drawPlan,
	renderDistantWorldSubmission_t *outSubmission );
/* Atomically derives the paired render-only world for the active app from the
 * sealed (or still-open) main view.  All scratch remains caller-owned and the
 * active gameplay world is never selected or mutated. */
qboolean RenderSubmission_DistantWorldSubmissionForActiveView(
	renderSubmissionState_t *state,
	const ralSkyEnvironmentPlanReceipt_t *skyEnvironment,
	uint32_t lodLevelCount, uint32_t maxVisibleDraws,
	renderDistantWorldDrawMetric_t *metricScratch, uint32_t metricCapacity,
	ralSkyDistantDrawCandidate_t *candidateScratch,
	uint32_t candidateCapacity,
	renderDistantWorldSubmission_t *outSubmission );
qboolean RenderSubmission_DistantWorldSubmissionExact(
	const renderDistantWorldSubmission_t *a,
	const renderDistantWorldSubmission_t *b );
qboolean RenderSubmission_UpdateWorldVisibilityForView(
	renderSubmissionState_t *state, int worldIndex, const refdef_t *view );
qboolean RenderSubmission_DistantWorldBuildMetrics(
	renderSubmissionState_t *state, int worldIndex,
	const ralSkyEnvironmentPlanReceipt_t *skyEnvironment,
	const refdef_t *mainView, uint32_t lodLevelCount,
	renderDistantWorldDrawMetric_t *outMetrics, uint32_t metricCapacity,
	uint32_t *outMetricCount );
qboolean RenderSubmission_WorldContentIdentity(
	const renderSubmissionState_t *state, int worldIndex, uint64_t *outIdentity );
qboolean RenderSubmission_WorldLifecycleGeneration(
	const renderSubmissionState_t *state, int worldIndex,
	uint64_t *outGeneration );
qboolean RenderSubmission_LightmapMaterialName( char outName[MAX_QPATH],
	uint32_t checksum, int lightmapIndex );

#ifdef __cplusplus
}
#endif

#endif
