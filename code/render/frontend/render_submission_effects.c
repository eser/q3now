// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_submission.h"

qboolean RenderSubmission_EffectSnapshots(
		const renderSubmissionState_t *state,
		const renderPolyCommand_t **outPolygons,
		uint32_t *outPolygonCommandCount,
		const polyVert_t **outVertices, uint32_t *outVertexCount,
		const renderLightCommand_t **outLights, uint32_t *outLightCount ) {
	if ( !state || !state->initialized
			|| ( !state->frameOpen && !state->frameSealed )
			|| !outPolygons || !outPolygonCommandCount
			|| !outVertices || !outVertexCount || !outLights || !outLightCount )
		return qfalse;
	*outPolygons = state->polyCommands;
	*outPolygonCommandCount = state->polyCommandCount;
	*outVertices = state->polyVertices;
	*outVertexCount = state->polyVertexCount;
	*outLights = state->lights;
	*outLightCount = state->lightCount;
	return qtrue;
}
