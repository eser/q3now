// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_SUBMISSION_MODEL_H
#define WIRED_RENDER_FRONTEND_SUBMISSION_MODEL_H

#include "q_shared.h"
#include "tr_types.h"
#include "../ral/core/ral_irradiance_runtime.h"
#include "../ral/core/ral_lighting_composition.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_SUBMISSION_MAX_MODELS 512u
#define RENDER_SUBMISSION_MAX_MODEL_VERTICES 131072u
#define RENDER_SUBMISSION_MAX_MODEL_INDICES 786432u
#define RENDER_SUBMISSION_MAX_MODEL_FRAMES 1024u
#define RENDER_SUBMISSION_MAX_MODEL_BATCHES 64u
#define RENDER_SUBMISSION_MAX_MODEL_BYTES (128u * 1024u * 1024u)
#define RENDER_SUBMISSION_MAX_ENTITIES 4096u
#define RENDER_SUBMISSION_MAX_CHARACTER_SKINS 64u

typedef enum {
	RENDER_MODEL_MD3 = 1,
	RENDER_MODEL_IQM = 2,
	RENDER_MODEL_INLINE_BSP = 3
} renderModelFormat_t;

typedef struct {
	uint32_t firstVertex;
	uint32_t vertexCount;
	uint32_t firstIndex;
	uint32_t indexCount;
	qhandle_t material;
	char surfaceName[CM_SURFACE_NAME_LEN];
	char materialName[MAX_QPATH];
} renderModelBatch_t;

typedef struct {
	char name[MAX_QPATH];
	vec3_t origin;
	vec3_t axis[3];
} renderModelTag_t;

typedef struct {
	qhandle_t handle;
	uint64_t generation;
	uint64_t digest;
	renderModelFormat_t format;
	uint32_t frameCount;
	uint32_t vertexCount;
	uint32_t indexCount;
	uint32_t batchCount;
	uint32_t tagCount;
	const float *positions;
	const float *normals;
	const float *texCoords;
	const uint32_t *indices;
	const renderModelBatch_t *batches;
	/* Frame-major MD3 tags: frame * tagCount + tag index. */
	const renderModelTag_t *tags;
	qboolean ready;
} renderModelSnapshot_t;

typedef struct {
	qhandle_t handle;
	cmSkin_t skin;
} renderCharacterSkinSnapshot_t;

typedef struct {
	renderModelSnapshot_t snapshot;
	char name[MAX_QPATH];
} renderModelRecord_t;

typedef struct {
	refEntity_t entity;
	refEntityMotion_t motion;
	ralIrradianceEntitySampleReceipt_t localIrradiance;
	ralLightingCompositionReceipt_t lightingComposition;
	qboolean hasTemporal;
	qboolean hasLocalIrradiance;
} renderEntityCommand_t;

typedef struct renderSubmissionState_s renderSubmissionState_t;

qhandle_t RenderSubmission_RegisterModelData( renderSubmissionState_t *state,
	const char *name, const void *bytes, uint32_t byteCount );
qhandle_t RenderSubmission_RegisterInlineModel( renderSubmissionState_t *state,
	const char *name, uint32_t firstSurface, uint32_t surfaceCount );
qboolean RenderSubmission_SetModelBatchMaterial( renderSubmissionState_t *state,
	qhandle_t model, uint32_t batchIndex, qhandle_t material );
qboolean RenderSubmission_ModelSnapshot( const renderSubmissionState_t *state,
	qhandle_t handle, renderModelSnapshot_t *outSnapshot );
int RenderSubmission_LerpTag( const renderSubmissionState_t *state,
	orientation_t *tag, qhandle_t model, int startFrame, int endFrame,
	float fraction, const char *name );
const renderEntityCommand_t *RenderSubmission_EntityCommands(
	const renderSubmissionState_t *state, uint32_t *outCount );
qhandle_t RenderSubmission_EntityBatchMaterial(
	const renderSubmissionState_t *state, const renderEntityCommand_t *command,
	const renderModelBatch_t *batch );
qboolean RenderSubmission_AttachEntityIrradiance( renderSubmissionState_t *state,
	uint32_t entityIndex, const ralIrradianceEntitySampleReceipt_t *receipt );

#ifdef __cplusplus
}
#endif

#endif
