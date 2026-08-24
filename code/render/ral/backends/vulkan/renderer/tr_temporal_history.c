// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_temporal_history.h"

#include <string.h>

#define TEMPORAL_HISTORY_USAGE ((ralTextureUsage_t)( \
	RAL_TEXTURE_USAGE_SAMPLED | RAL_TEXTURE_USAGE_STORAGE | \
	RAL_TEXTURE_USAGE_TRANSFER_SRC | RAL_TEXTURE_USAGE_TRANSFER_DST ))

static qboolean AliasesLiveTexture( const temporalHistoryResources_t *live,
		const ralTexture_t *texture ) {
	unsigned i;
	if ( !live || !texture ) return qfalse;
	for ( i = 0; i < 2; ++i )
		if ( texture == live->color[i] || texture == live->depth[i] ) return qtrue;
	return qfalse;
}

static qboolean AliasesLiveView( const temporalHistoryResources_t *live,
		const ralTextureView_t *view ) {
	unsigned i;
	if ( !live || !view ) return qfalse;
	for ( i = 0; i < 2; ++i )
		if ( view == live->colorView[i] || view == live->depthView[i] ) return qtrue;
	return qfalse;
}

static void DestroyHandles( temporalHistoryResources_t *history ) {
	for ( unsigned i = 0; i < 2; ++i ) {
		if ( history->colorView[i] ) {
			qboolean duplicate = qfalse;
			for ( unsigned j = 0; j < i; ++j ) duplicate |= history->colorView[i] == history->colorView[j]
				|| history->colorView[i] == history->depthView[j];
			if ( !duplicate ) Ral_DestroyTextureView( history->colorView[i] );
		}
		if ( history->depthView[i] ) {
			qboolean duplicate = history->depthView[i] == history->colorView[i];
			for ( unsigned j = 0; j < i; ++j ) duplicate |= history->depthView[i] == history->colorView[j]
				|| history->depthView[i] == history->depthView[j];
			if ( !duplicate ) Ral_DestroyTextureView( history->depthView[i] );
		}
	}
	for ( unsigned i = 0; i < 2; ++i ) {
		if ( history->color[i] ) {
			qboolean duplicate = qfalse;
			for ( unsigned j = 0; j < i; ++j ) duplicate |= history->color[i] == history->color[j]
				|| history->color[i] == history->depth[j];
			if ( !duplicate ) Ral_DestroyTexture( history->color[i] );
		}
		if ( history->depth[i] ) {
			qboolean duplicate = history->depth[i] == history->color[i];
			for ( unsigned j = 0; j < i; ++j ) duplicate |= history->depth[i] == history->color[j]
				|| history->depth[i] == history->depth[j];
			if ( !duplicate ) Ral_DestroyTexture( history->depth[i] );
		}
	}
}

static void DestroyCandidate( temporalHistoryResources_t *candidate,
		const temporalHistoryResources_t *live ) {
	for ( unsigned i = 0; i < 2; ++i ) {
		if ( AliasesLiveView( live, candidate->colorView[i] ) ) candidate->colorView[i] = NULL;
		if ( AliasesLiveView( live, candidate->depthView[i] ) ) candidate->depthView[i] = NULL;
		if ( AliasesLiveTexture( live, candidate->color[i] ) ) candidate->color[i] = NULL;
		if ( AliasesLiveTexture( live, candidate->depth[i] ) ) candidate->depth[i] = NULL;
	}
	DestroyHandles( candidate );
}

static qboolean CandidateHandlesDistinct( const temporalHistoryResources_t *candidate ) {
	const void *textures[4] = { candidate->color[0], candidate->depth[0],
		candidate->color[1], candidate->depth[1] };
	const void *views[4] = { candidate->colorView[0], candidate->depthView[0],
		candidate->colorView[1], candidate->depthView[1] };
	for ( unsigned i = 0; i < 4; ++i ) {
		if ( !textures[i] || !views[i] ) return qfalse;
		for ( unsigned j = i + 1; j < 4; ++j )
			if ( textures[i] == textures[j] || views[i] == views[j] ) return qfalse;
	}
	return qtrue;
}

static qboolean CommittedSourceValid(
		const temporalHistoryResources_t *history,
		const temporalHistoryCommittedReceipt_t *committed ) {
	const temporalHistoryFeedbackSource_t *source;
	const void *identities[16];
	uint32_t i, j;
	if ( !history || !committed ) return qfalse;
	source = &committed->source;
	if ( source->backend != history->backend || !source->sourceSceneColor
			|| !source->sourcePostprocessGroup || !source->sourceHistogramGroup
			|| !source->sourceColor || !source->sourceColorView
			|| !source->postprocessGroup || !source->histogramGroup
			|| !source->batchToken
			|| source->frameId != committed->frameId || !source->contentSerial
			|| !source->frameCount || source->frameCount > 4
			|| source->commandSlot >= source->frameCount
			|| source->worldIndex != committed->worldIndex
			|| source->width != committed->width
			|| source->height != committed->height
			|| source->topologyEpoch != committed->topologyEpoch
			|| source->planGeneration != committed->planGeneration
			|| !source->sceneColorAttachmentGeneration
			|| !source->targetAllocationGeneration
			|| !source->storeOwnerAllocationGeneration
			|| source->sceneFormat != RAL_FORMAT_R16G16B16A16_SFLOAT
			|| ( source->producer == TEMPORAL_HISTORY_WRITE_CURRENT_SEED
				&& source->resolveOwnerAllocationGeneration )
			|| ( source->producer == TEMPORAL_HISTORY_WRITE_RESOLVED_FEEDBACK
				&& !source->resolveOwnerAllocationGeneration )
			|| ( source->producer != TEMPORAL_HISTORY_WRITE_CURRENT_SEED
				&& source->producer !=
					TEMPORAL_HISTORY_WRITE_RESOLVED_FEEDBACK ) ) return qfalse;
	identities[0] = history->backend;
	identities[1] = source->sourceSceneColor;
	identities[2] = source->sourcePostprocessGroup;
	identities[3] = source->sourceHistogramGroup;
	identities[4] = source->sourceColor;
	identities[5] = source->sourceColorView;
	identities[6] = source->postprocessGroup;
	identities[7] = source->histogramGroup;
	identities[8] = history->color[0]; identities[9] = history->colorView[0];
	identities[10] = history->depth[0]; identities[11] = history->depthView[0];
	identities[12] = history->color[1]; identities[13] = history->colorView[1];
	identities[14] = history->depth[1]; identities[15] = history->depthView[1];
	for ( i = 0; i < 16; ++i ) {
		if ( !identities[i] ) return qfalse;
		for ( j = i + 1; j < 16; ++j )
			if ( identities[i] == identities[j] ) return qfalse;
	}
	return qtrue;
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
	if ( history->ready && history->backend == backend
			&& history->width == width && history->height == height
			&& history->topologyEpoch == topologyEpoch ) return qtrue;
	{
		const ralCaps_t *caps = Ral_GetCaps( backend );
		if ( !caps || !caps->maxTextureDimension2D
				|| width > caps->maxTextureDimension2D
				|| height > caps->maxTextureDimension2D
				|| !Ral_TextureFormatSupports( backend,
					RAL_FORMAT_R16G16B16A16_SFLOAT, TEMPORAL_HISTORY_USAGE )
				|| !Ral_TextureFormatSupports( backend,
					RAL_FORMAT_R32_SFLOAT, TEMPORAL_HISTORY_USAGE ) ) return qfalse;
	}
	if ( history->allocationGeneration == UINT32_MAX ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.backend = backend;
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
		if ( AliasesLiveTexture( history, candidate.color[i] ) ) goto fail;
		tci.format = RAL_FORMAT_R32_SFLOAT;
		tci.debugName = i ? "wired-temporal-depth-1" : "wired-temporal-depth-0";
		candidate.depth[i] = Ral_CreateTexture( backend, &tci );
		if ( !candidate.depth[i] || candidate.depth[i] == candidate.color[i]
				|| AliasesLiveTexture( history, candidate.depth[i] ) ) goto fail;
		memset( &vci, 0, sizeof( vci ) );
		vci.viewType = RAL_TEXTURE_2D;
		vci.format = RAL_FORMAT_UNDEFINED;
		vci.texture = candidate.color[i];
		candidate.colorView[i] = Ral_CreateTextureView( backend, &vci );
		if ( !candidate.colorView[i] || AliasesLiveView( history, candidate.colorView[i] ) ) goto fail;
		vci.texture = candidate.depth[i];
		candidate.depthView[i] = Ral_CreateTextureView( backend, &vci );
		if ( !candidate.depthView[i]
				|| candidate.depthView[i] == candidate.colorView[i]
				|| AliasesLiveView( history, candidate.depthView[i] ) ) goto fail;
	}
	if ( !CandidateHandlesDistinct( &candidate ) ) goto fail;
	candidate.ready = qtrue;
	DestroyHandles( history );
	*history = candidate;
	return qtrue;
fail:
	DestroyCandidate( &candidate, history );
	return qfalse;
}

qboolean R_TemporalHistoryBuildFrameView(
		const temporalHistoryResources_t *history,
		const ralTemporalFramePlan_t *plan,
		const temporalHistoryCommittedReceipt_t *committed,
		int worldIndex, temporalHistoryFrameView_t *outView ) {
	temporalHistoryFrameView_t view;
	uint32_t readIndex;
	if ( !history || !history->ready || !history->backend || !plan || !outView
			|| !plan->enabled || !plan->frameId || !plan->generation
			|| plan->historyReadIndex > 1 || plan->historyWriteIndex > 1 ) return qfalse;
	if ( plan->historyValid ) {
		if ( plan->historyReadIndex == plan->historyWriteIndex || !committed
				|| !committed->valid || committed->worldIndex != worldIndex
				|| !CommittedSourceValid( history, committed )
				|| committed->frameId == UINT64_MAX
				|| committed->frameId + 1u != plan->frameId
				|| committed->planGeneration != plan->generation
				|| committed->allocationGeneration != history->allocationGeneration
				|| committed->historyIndex != plan->historyReadIndex
				|| committed->width != history->width || committed->height != history->height
				|| committed->topologyEpoch != history->topologyEpoch ) return qfalse;
		readIndex = plan->historyReadIndex;
		if ( committed->color != history->color[readIndex]
				|| committed->colorView != history->colorView[readIndex]
				|| committed->depth != history->depth[readIndex]
				|| committed->depthView != history->depthView[readIndex] ) return qfalse;
	} else {
		// Bind the non-written slot as a descriptor-safe dummy; the shader's
		// historyValid=false branch is required not to load it.
		readIndex = plan->historyWriteIndex ^ 1u;
	}
	if ( !history->color[readIndex] || !history->colorView[readIndex]
			|| !history->depth[readIndex] || !history->depthView[readIndex]
			|| !history->color[plan->historyWriteIndex]
			|| !history->colorView[plan->historyWriteIndex]
			|| !history->depth[plan->historyWriteIndex]
			|| !history->depthView[plan->historyWriteIndex] ) return qfalse;
	memset( &view, 0, sizeof( view ) );
	if ( plan->historyValid ) view.committed = *committed;
	view.historyValid = plan->historyValid ? qtrue : qfalse;
	view.readIndex = readIndex;
	view.writeIndex = plan->historyWriteIndex;
	view.readColor = history->color[readIndex];
	view.readColorView = history->colorView[readIndex];
	view.readDepth = history->depth[readIndex];
	view.readDepthView = history->depthView[readIndex];
	view.writeColor = history->color[plan->historyWriteIndex];
	view.writeColorView = history->colorView[plan->historyWriteIndex];
	view.writeDepth = history->depth[plan->historyWriteIndex];
	view.writeDepthView = history->depthView[plan->historyWriteIndex];
	*outView = view;
	return qtrue;
}

qboolean R_TemporalHistoryBuildPendingWrite(
		const temporalHistoryResources_t *history,
		const ralTemporalFramePlan_t *plan, int worldIndex,
		const temporalHistoryFeedbackSource_t *source,
		temporalHistoryPendingWriteReceipt_t *outPending ) {
	temporalHistoryPendingWriteReceipt_t pending;
	uint32_t writeIndex;
	const void *identities[16];
	uint32_t i, j;
	if ( !history || !history->ready || !history->backend || !plan
			|| !plan->enabled || !plan->frameId || !plan->generation
			|| plan->historyReadIndex > 1 || plan->historyWriteIndex > 1
			|| worldIndex < 0 || !source || !outPending
			|| source->backend != history->backend || !source->sourceSceneColor
			|| !source->sourcePostprocessGroup || !source->sourceHistogramGroup
			|| !source->sourceColor || !source->sourceColorView
			|| !source->postprocessGroup || !source->histogramGroup
			|| !source->batchToken
			|| source->frameId != plan->frameId || !source->contentSerial
			|| !source->frameCount || source->frameCount > 4
			|| source->commandSlot >= source->frameCount
			|| source->worldIndex != worldIndex
			|| source->width != history->width
			|| source->height != history->height
			|| source->topologyEpoch != history->topologyEpoch
			|| source->planGeneration != plan->generation
			|| !source->sceneColorAttachmentGeneration
			|| !source->targetAllocationGeneration
			|| !source->storeOwnerAllocationGeneration
			|| source->sceneFormat != RAL_FORMAT_R16G16B16A16_SFLOAT ) return qfalse;
	if ( plan->historyValid ) {
		if ( plan->historyReadIndex == plan->historyWriteIndex
				|| source->producer != TEMPORAL_HISTORY_WRITE_RESOLVED_FEEDBACK
				|| !source->resolveOwnerAllocationGeneration ) return qfalse;
	} else if ( source->producer != TEMPORAL_HISTORY_WRITE_CURRENT_SEED
			|| source->resolveOwnerAllocationGeneration ) return qfalse;
	writeIndex = plan->historyWriteIndex;
	if ( !history->width || !history->height || !history->topologyEpoch
			|| !history->allocationGeneration || !history->color[writeIndex]
			|| !history->colorView[writeIndex] || !history->depth[writeIndex]
			|| !history->depthView[writeIndex] ) return qfalse;
	identities[0] = history->backend;
	identities[1] = source->sourceSceneColor;
	identities[2] = source->sourcePostprocessGroup;
	identities[3] = source->sourceHistogramGroup;
	identities[4] = source->sourceColor;
	identities[5] = source->sourceColorView;
	identities[6] = source->postprocessGroup;
	identities[7] = source->histogramGroup;
	identities[8] = history->color[0];
	identities[9] = history->colorView[0];
	identities[10] = history->depth[0];
	identities[11] = history->depthView[0];
	identities[12] = history->color[1];
	identities[13] = history->colorView[1];
	identities[14] = history->depth[1];
	identities[15] = history->depthView[1];
	for ( i = 0; i < 16; ++i ) {
		if ( !identities[i] ) return qfalse;
		for ( j = i + 1; j < 16; ++j )
			if ( identities[i] == identities[j] ) return qfalse;
	}
	memset( &pending, 0, sizeof( pending ) );
	pending.write.frameId = plan->frameId;
	pending.write.worldIndex = worldIndex;
	pending.write.planGeneration = plan->generation;
	pending.write.allocationGeneration = history->allocationGeneration;
	pending.write.historyIndex = writeIndex;
	pending.write.width = history->width;
	pending.write.height = history->height;
	pending.write.topologyEpoch = history->topologyEpoch;
	pending.write.color = history->color[writeIndex];
	pending.write.colorView = history->colorView[writeIndex];
	pending.write.depth = history->depth[writeIndex];
	pending.write.depthView = history->depthView[writeIndex];
	pending.write.source = *source;
	pending.write.valid = qtrue;
	pending.valid = qtrue;
	*outPending = pending;
	return qtrue;
}

static qboolean FeedbackSourceEqualExact(
		const temporalHistoryFeedbackSource_t *a,
		const temporalHistoryFeedbackSource_t *b ) {
	if ( !a || !b ) return qfalse;
	return a->backend == b->backend
		&& a->sourceSceneColor == b->sourceSceneColor
		&& a->sourcePostprocessGroup == b->sourcePostprocessGroup
		&& a->sourceHistogramGroup == b->sourceHistogramGroup
		&& a->sourceColor == b->sourceColor
		&& a->sourceColorView == b->sourceColorView
		&& a->postprocessGroup == b->postprocessGroup
		&& a->histogramGroup == b->histogramGroup
		&& a->batchToken == b->batchToken
		&& a->frameId == b->frameId
		&& a->contentSerial == b->contentSerial
		&& a->commandSlot == b->commandSlot
		&& a->frameCount == b->frameCount
		&& a->worldIndex == b->worldIndex
		&& a->width == b->width && a->height == b->height
		&& a->topologyEpoch == b->topologyEpoch
		&& a->planGeneration == b->planGeneration
		&& a->sceneColorAttachmentGeneration ==
			b->sceneColorAttachmentGeneration
		&& a->targetAllocationGeneration == b->targetAllocationGeneration
		&& a->resolveOwnerAllocationGeneration ==
			b->resolveOwnerAllocationGeneration
		&& a->storeOwnerAllocationGeneration ==
			b->storeOwnerAllocationGeneration
		&& a->sceneFormat == b->sceneFormat
		&& a->producer == b->producer ? qtrue : qfalse;
}

qboolean R_TemporalHistoryPendingWriteEqualExact(
		const temporalHistoryPendingWriteReceipt_t *a,
		const temporalHistoryPendingWriteReceipt_t *b ) {
	if ( !a || !b ) return qfalse;
	return a->write.frameId == b->write.frameId
		&& a->write.worldIndex == b->write.worldIndex
		&& a->write.planGeneration == b->write.planGeneration
		&& a->write.allocationGeneration == b->write.allocationGeneration
		&& a->write.historyIndex == b->write.historyIndex
		&& a->write.width == b->write.width
		&& a->write.height == b->write.height
		&& a->write.topologyEpoch == b->write.topologyEpoch
		&& a->write.color == b->write.color
		&& a->write.colorView == b->write.colorView
		&& a->write.depth == b->write.depth
		&& a->write.depthView == b->write.depthView
		&& FeedbackSourceEqualExact(
			&a->write.source, &b->write.source )
		&& a->write.valid == b->write.valid
		&& a->valid == b->valid ? qtrue : qfalse;
}
