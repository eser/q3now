// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_submission.h"

#include <string.h>
#include <strings.h>

qboolean RenderSubmission_MaterialIndex(
		const renderSubmissionState_t *state, qhandle_t handle,
		uint32_t *outIndex ) {
	uint32_t first, count;
	if ( !state || !state->initialized || !outIndex || handle <= 0 ) return qfalse;
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
			*outIndex = index;
			return qtrue;
		}
	}
	return qfalse;
}

qboolean RenderSubmission_MaterialSnapshot(
		const renderSubmissionState_t *state, qhandle_t handle,
		renderMaterialSnapshot_t *outSnapshot ) {
	uint32_t index;
	if ( !outSnapshot || !RenderSubmission_MaterialIndex( state, handle, &index ) )
		return qfalse;
	*outSnapshot = state->materials[index].snapshot;
	return qtrue;
}

const renderMaterialSnapshot_t *RenderSubmission_MaterialSnapshotView(
		const renderSubmissionState_t *state, qhandle_t handle ) {
	uint32_t index;
	if ( !RenderSubmission_MaterialIndex( state, handle, &index ) ) return NULL;
	return &state->materials[index].snapshot;
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

qboolean RenderSubmission_TextureSnapshot(
		const renderSubmissionState_t *state, qhandle_t handle,
		renderTextureSnapshot_t *outSnapshot ) {
	uint32_t first, count, materialIndex;
	if ( !state || !state->initialized || !outSnapshot || handle <= 0 )
		return qfalse;
	if ( RenderSubmission_MaterialIndex( state, handle, &materialIndex ) ) {
		const renderMaterialSnapshot_t *material =
			&state->materials[materialIndex].snapshot;
		memset( outSnapshot, 0, sizeof( *outSnapshot ) );
		outSnapshot->handle = material->handle;
		outSnapshot->generation = material->generation;
		outSnapshot->digest = material->digest;
		outSnapshot->textureCacheKey = material->textureCacheKey;
		outSnapshot->textureCacheKeyHash = material->textureCacheKeyHash;
		outSnapshot->textureCacheKeyReady = material->textureCacheKeyReady;
		outSnapshot->width = material->width;
		outSnapshot->height = material->height;
		outSnapshot->rowBytes = material->rowBytes;
		outSnapshot->byteCount = material->byteCount;
		outSnapshot->rgba8 = material->rgba8;
		outSnapshot->clampToEdge = material->clampToEdge;
		outSnapshot->msdf = material->msdf;
		outSnapshot->srgb = material->srgb;
		outSnapshot->ready = material->ready;
		return qtrue;
	}
	/* Both catalogs append monotonically allocated handles, so stage texture
	 * lookup remains bounded and logarithmic without exposing their names as
	 * semantic materials. */
	first = 0u; count = state->stageTextureCount;
	while ( count ) {
		const uint32_t step = count / 2u;
		const uint32_t index = first + step;
		const qhandle_t candidate = state->stageTextures[index].snapshot.handle;
		if ( candidate < handle ) {
			first = index + 1u; count -= step + 1u;
		} else if ( candidate > handle ) {
			count = step;
		} else {
			*outSnapshot = state->stageTextures[index].snapshot;
			return qtrue;
		}
	}
	return qfalse;
}
