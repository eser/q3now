// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_SUBMISSION_WORLD_H
#define WIRED_RENDER_FRONTEND_SUBMISSION_WORLD_H

#include "q_shared.h"
#include "render_submission_material.h"

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

qboolean RenderSubmission_WorldSnapshot( const renderSubmissionState_t *state,
	renderWorldSnapshot_t *outSnapshot );
qboolean RenderSubmission_ViewSnapshot( const renderSubmissionState_t *state,
	renderWorldSnapshot_t *outSnapshot );
qboolean RenderSubmission_LightmapMaterialName( char outName[MAX_QPATH],
	uint32_t checksum, int lightmapIndex );

#ifdef __cplusplus
}
#endif

#endif
