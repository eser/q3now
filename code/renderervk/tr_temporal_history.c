// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_temporal_history.h"

#include <string.h>

static void DestroyHandles( temporalHistoryResources_t *history ) {
	for ( unsigned i = 0; i < 2; ++i ) {
		if ( history->colorView[i] ) Ral_DestroyTextureView( history->colorView[i] );
		if ( history->depthView[i] ) Ral_DestroyTextureView( history->depthView[i] );
		if ( history->color[i] ) Ral_DestroyTexture( history->color[i] );
		if ( history->depth[i] ) Ral_DestroyTexture( history->depth[i] );
	}
}

void R_TemporalHistoryInit( temporalHistoryResources_t *history ) {
	if ( history ) memset( history, 0, sizeof( *history ) );
}

void R_TemporalHistoryRelease( temporalHistoryResources_t *history ) {
	uint32_t generation;
	if ( !history ) return;
	generation = history->allocationGeneration;
	DestroyHandles( history );
	memset( history, 0, sizeof( *history ) );
	history->allocationGeneration = generation;
}

qboolean R_TemporalHistoryEnsure( temporalHistoryResources_t *history,
		ralBackend_t *backend, uint32_t width, uint32_t height,
		uint32_t topologyEpoch ) {
	temporalHistoryResources_t candidate;
	ralTextureCreateInfo_t tci;
	ralTextureViewCreateInfo_t vci;

	if ( !history || !backend || !width || !height || !topologyEpoch ) return qfalse;
	if ( history->ready && history->width == width && history->height == height
			&& history->topologyEpoch == topologyEpoch ) return qtrue;
	if ( history->allocationGeneration == UINT32_MAX ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.width = width;
	candidate.height = height;
	candidate.topologyEpoch = topologyEpoch;
	candidate.allocationGeneration = history->allocationGeneration + 1u;
	memset( &tci, 0, sizeof( tci ) );
	tci.type = RAL_TEXTURE_2D;
	tci.width = width;
	tci.height = height;
	tci.depthOrArrayLayers = 1;
	tci.mipLevels = 1;
	tci.sampleCount = 1;
	tci.usage = RAL_TEXTURE_USAGE_SAMPLED | RAL_TEXTURE_USAGE_STORAGE
		| RAL_TEXTURE_USAGE_TRANSFER_SRC | RAL_TEXTURE_USAGE_TRANSFER_DST;
	tci.memory = RAL_MEMORY_DEVICE_LOCAL;
	for ( unsigned i = 0; i < 2; ++i ) {
		tci.format = RAL_FORMAT_R16G16B16A16_SFLOAT;
		tci.debugName = i ? "wired-temporal-color-1" : "wired-temporal-color-0";
		candidate.color[i] = Ral_CreateTexture( backend, &tci );
		if ( !candidate.color[i] ) goto fail;
		tci.format = RAL_FORMAT_R32_SFLOAT;
		tci.debugName = i ? "wired-temporal-depth-1" : "wired-temporal-depth-0";
		candidate.depth[i] = Ral_CreateTexture( backend, &tci );
		if ( !candidate.depth[i] ) goto fail;
		memset( &vci, 0, sizeof( vci ) );
		vci.viewType = RAL_TEXTURE_2D;
		vci.format = RAL_FORMAT_UNDEFINED;
		vci.texture = candidate.color[i];
		candidate.colorView[i] = Ral_CreateTextureView( backend, &vci );
		if ( !candidate.colorView[i] ) goto fail;
		vci.texture = candidate.depth[i];
		candidate.depthView[i] = Ral_CreateTextureView( backend, &vci );
		if ( !candidate.depthView[i] ) goto fail;
	}
	candidate.ready = qtrue;
	DestroyHandles( history );
	*history = candidate;
	return qtrue;
fail:
	DestroyHandles( &candidate );
	return qfalse;
}
