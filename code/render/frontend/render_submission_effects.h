// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_SUBMISSION_EFFECTS_H
#define WIRED_RENDER_FRONTEND_SUBMISSION_EFFECTS_H

#include "tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_SUBMISSION_MAX_POLY_COMMANDS 4096u
#define RENDER_SUBMISSION_MAX_POLY_VERTICES 262144u
#define RENDER_SUBMISSION_MAX_LIGHTS 4096u

typedef struct {
	qhandle_t material;
	uint32_t firstVertex;
	uint32_t verticesPerPolygon;
	uint32_t polygonCount;
} renderPolyCommand_t;

typedef struct {
	float origin[3];
	float end[3];
	float intensity;
	float color[3];
	qboolean hasEnd;
} renderLightCommand_t;

typedef struct renderSubmissionState_s renderSubmissionState_t;

qboolean RenderSubmission_EffectSnapshots(
	const renderSubmissionState_t *state,
	const renderPolyCommand_t **outPolygons, uint32_t *outPolygonCommandCount,
	const polyVert_t **outVertices, uint32_t *outVertexCount,
	const renderLightCommand_t **outLights, uint32_t *outLightCount );

#ifdef __cplusplus
}
#endif

#endif
