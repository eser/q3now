// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_SUBMISSION_H
#define WIRED_RENDER_FRONTEND_SUBMISSION_H

#include "q_shared.h"
#include "tr_public.h"
#include "render_submission_material.h"
#include "render_submission_model.h"
#include "render_submission_ui.h"
#include "render_submission_world.h"
#include "render_submission_effects.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_SUBMISSION_SCHEMA_VERSION 7u

typedef enum {
	RENDER_ASSET_MODEL = 1,
	RENDER_ASSET_SKIN,
	RENDER_ASSET_MATERIAL,
	RENDER_ASSET_MSDF,
	RENDER_ASSET_PRIMITIVE_MATERIAL,
	RENDER_ASSET_LIGHTMAP
} renderAssetKind_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t ownerGeneration;
	uint64_t frameGeneration;
	uint64_t assetDigest;
	uint64_t materialDigest;
	uint64_t modelDigest;
	uint64_t worldDigest;
	uint64_t sceneDigest;
	uint64_t uiDigest;
	uint64_t frameDigest;
	uint32_t worldSurfaceCount;
	uint32_t worldVertexCount;
	uint32_t worldIndexCount;
	uint32_t registeredAssetCount;
	uint32_t registeredMaterialCount;
	uint32_t resolvedMaterialCount;
	uint32_t materialBytes;
	uint32_t registeredModelCount;
	uint32_t modelBytes;
	uint32_t entityCount;
	uint32_t temporalEntityCount;
	uint32_t polygonCount;
	uint32_t lightCount;
	uint32_t uiPrimitiveCount;
	qboolean worldLoaded;
	qboolean sceneRendered;
	qboolean ready;
} renderSubmissionReceipt_t;

typedef struct renderSubmissionState_s {
	uint64_t ownerGeneration;
	uint64_t assetDigest;
	uint64_t materialDigest;
	uint64_t modelDigest;
	uint64_t worldDigest;
	uint64_t sceneDigest;
	uint64_t uiDigest;
	uint32_t worldSurfaceCount;
	uint32_t worldVertexCount;
	uint32_t worldIndexCount;
	uint32_t registeredAssetCount;
	uint32_t registeredMaterialCount;
	uint32_t resolvedMaterialCount;
	uint32_t materialBytes;
	uint32_t materialCount;
	uint32_t modelCount;
	uint32_t modelBytes;
	uint32_t entityCount;
	uint32_t temporalEntityCount;
	uint32_t polygonCount;
	uint32_t lightCount;
	uint32_t uiPrimitiveCount;
	uint32_t nextHandle;
	uint64_t nextMaterialGeneration;
	uint64_t nextModelGeneration;
	float color[4];
	renderMaterialRecord_t materials[RENDER_SUBMISSION_MAX_MATERIALS];
	renderModelRecord_t models[RENDER_SUBMISSION_MAX_MODELS];
	renderEntityCommand_t entities[RENDER_SUBMISSION_MAX_ENTITIES];
	renderUiPrimitive_t uiPrimitives[RENDER_SUBMISSION_MAX_UI_PRIMITIVES];
	renderPolyCommand_t *polyCommands;
	polyVert_t *polyVertices;
	renderLightCommand_t *lights;
	uint32_t polyCommandCount;
	uint32_t polyCommandCapacity;
	uint32_t polyVertexCount;
	uint32_t polyVertexCapacity;
	uint32_t lightCapacity;
	renderWorldSnapshot_t worldSnapshot;
	qboolean worldLoaded;
	qboolean sceneRendered;
	qboolean initialized;
	qboolean frameOpen;
	// EndFrame seals the retained native-free payload for backend consumption.
	// It remains readable until the next BeginFrame or CancelFrame.
	qboolean frameSealed;
} renderSubmissionState_t;

qboolean RenderSubmission_Init( renderSubmissionState_t *state,
	uint64_t ownerGeneration );
void RenderSubmission_Reset( renderSubmissionState_t *state );
qhandle_t RenderSubmission_RegisterAsset( renderSubmissionState_t *state,
	renderAssetKind_t kind, const char *name );
qhandle_t RenderSubmission_RegisterMaterialImage( renderSubmissionState_t *state,
	renderAssetKind_t kind, const char *name, qboolean clampToEdge,
	const byte *rgba8, uint32_t width, uint32_t height );
qboolean RenderSubmission_SetMaterialRasterPolicy( renderSubmissionState_t *state,
	qhandle_t handle, renderAlphaMode_t alphaMode, float alphaCutoff,
	qboolean depthWrite );
qboolean RenderSubmission_RecordAsset( renderSubmissionState_t *state,
	renderAssetKind_t kind, const char *name, qhandle_t handle );
qboolean RenderSubmission_LoadWorld( renderSubmissionState_t *state,
	const mapFile_t *bsp, int worldIndex );
qboolean RenderSubmission_BeginFrame( renderSubmissionState_t *state,
	uint64_t frameGeneration );
void RenderSubmission_CancelFrame( renderSubmissionState_t *state );
qboolean RenderSubmission_ClearScene( renderSubmissionState_t *state );
qboolean RenderSubmission_AddEntity( renderSubmissionState_t *state,
	const refEntity_t *entity, const refEntityMotion_t *motion );
qboolean RenderSubmission_AddPoly( renderSubmissionState_t *state,
	qhandle_t material, int verticesPerPoly, const polyVert_t *vertices,
	int polygonCount );
qboolean RenderSubmission_AddLight( renderSubmissionState_t *state,
	const vec3_t origin, const vec3_t end, float intensity,
	float red, float green, float blue );
qboolean RenderSubmission_RenderScene( renderSubmissionState_t *state,
	const refdef_t *view, int worldIndex );
qboolean RenderSubmission_SetColor( renderSubmissionState_t *state,
	const float *rgba );
qboolean RenderSubmission_AddUiQuad( renderSubmissionState_t *state,
	float x, float y, float width, float height, float s1, float t1,
	float s2, float t2, float rotation, qhandle_t material );
qboolean RenderSubmission_AddUiLine( renderSubmissionState_t *state,
	float x1, float y1, float x2, float y2, float width,
	qhandle_t material );
uint64_t RenderSubmission_FrameDigest( const renderSubmissionState_t *state );
qboolean RenderSubmission_EndFrame( renderSubmissionState_t *state,
	uint64_t frameGeneration, renderSubmissionReceipt_t *outReceipt );
qboolean RenderSubmission_ReceiptExact( const renderSubmissionReceipt_t *a,
	const renderSubmissionReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
