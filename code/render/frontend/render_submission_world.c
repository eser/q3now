// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_submission.h"

#include <stdio.h>

qboolean RenderSubmission_WorldSnapshot( const renderSubmissionState_t *state,
		renderWorldSnapshot_t *outSnapshot ) {
	if ( !state || !outSnapshot || !state->initialized
			|| ( !state->frameOpen && !state->frameSealed )
			|| !state->worldSnapshot.ready || !state->sceneRendered ) return qfalse;
	*outSnapshot = state->worldSnapshot;
	return qtrue;
}

qboolean RenderSubmission_ViewSnapshot( const renderSubmissionState_t *state,
		renderWorldSnapshot_t *outSnapshot ) {
	if ( !state || !outSnapshot || !state->initialized
			|| ( !state->frameOpen && !state->frameSealed )
			|| !state->sceneRendered ) return qfalse;
	*outSnapshot = state->worldSnapshot;
	return qtrue;
}

qboolean RenderSubmission_LightmapMaterialName( char outName[MAX_QPATH],
		uint32_t checksum, int lightmapIndex ) {
	int written;
	if ( !outName || lightmapIndex < 0 ) return qfalse;
	written = snprintf( outName, MAX_QPATH, "*lightmap/%08x/%d",
		checksum, lightmapIndex );
	return written > 0 && written < MAX_QPATH ? qtrue : qfalse;
}
