// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_submission.h"

#include <strings.h>

qboolean RenderSubmission_MaterialSnapshot(
		const renderSubmissionState_t *state, qhandle_t handle,
		renderMaterialSnapshot_t *outSnapshot ) {
	uint32_t first, count;
	if ( !state || !state->initialized || !outSnapshot || handle <= 0 ) return qfalse;
	/* Material handles are monotonic because records append at registration. */
	first = 0u; count = state->materialCount;
	while ( count ) {
		uint32_t step = count / 2u;
		uint32_t index = first + step;
		qhandle_t candidate = state->materials[index].snapshot.handle;
		if ( candidate < handle ) {
			first = index + 1u; count -= step + 1u;
		} else if ( candidate > handle ) {
			count = step;
		} else {
			*outSnapshot = state->materials[index].snapshot;
			return qtrue;
		}
	}
	return qfalse;
}

qhandle_t RenderSubmission_MaterialHandle( const renderSubmissionState_t *state,
		const char *name ) {
	uint32_t i;
	if ( !state || !state->initialized || !name || !name[0] ) return 0;
	for ( i = 0u; i < state->materialCount; ++i ) {
		if ( !strcasecmp( state->materials[i].name, name ) )
			return state->materials[i].snapshot.handle;
	}
	return 0;
}
