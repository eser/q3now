// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_temporal_motion_targets.h"

#include <string.h>

#define TEMPORAL_MOTION_TARGET_USAGE ((ralTextureUsage_t)( \
	RAL_TEXTURE_USAGE_COLOR_ATTACHMENT | RAL_TEXTURE_USAGE_SAMPLED | \
	RAL_TEXTURE_USAGE_TRANSFER_SRC ))

static void DestroyHandles( temporalMotionTargets_t *targets ) {
	if ( targets->validityView && targets->validityView != targets->velocityView ) {
		Ral_DestroyTextureView( targets->validityView );
	}
	if ( targets->velocityView ) Ral_DestroyTextureView( targets->velocityView );
	if ( targets->validity && targets->validity != targets->velocity ) {
		Ral_DestroyTexture( targets->validity );
	}
	if ( targets->velocity ) Ral_DestroyTexture( targets->velocity );
}

static qboolean AliasesLiveTexture( const temporalMotionTargets_t *live,
		const ralTexture_t *texture ) {
	return live && texture && ( texture == live->velocity
		|| texture == live->validity );
}

static qboolean AliasesLiveView( const temporalMotionTargets_t *live,
		const ralTextureView_t *view ) {
	return live && view && ( view == live->velocityView
		|| view == live->validityView );
}

static void DestroyCandidate( temporalMotionTargets_t *candidate,
		const temporalMotionTargets_t *live ) {
	if ( candidate->validityView && candidate->validityView != candidate->velocityView
			&& !AliasesLiveView( live, candidate->validityView ) )
		Ral_DestroyTextureView( candidate->validityView );
	if ( candidate->velocityView
			&& !AliasesLiveView( live, candidate->velocityView ) )
		Ral_DestroyTextureView( candidate->velocityView );
	if ( candidate->validity && candidate->validity != candidate->velocity
			&& !AliasesLiveTexture( live, candidate->validity ) )
		Ral_DestroyTexture( candidate->validity );
	if ( candidate->velocity
			&& !AliasesLiveTexture( live, candidate->velocity ) )
		Ral_DestroyTexture( candidate->velocity );
}

void R_TemporalMotionTargetsInit( temporalMotionTargets_t *targets ) {
	if ( targets ) memset( targets, 0, sizeof( *targets ) );
}

void R_TemporalMotionTargetsRelease( temporalMotionTargets_t *targets ) {
	uint32_t generation;
	if ( !targets ) return;
	generation = targets->allocationGeneration;
	DestroyHandles( targets );
	memset( targets, 0, sizeof( *targets ) );
	targets->allocationGeneration = generation;
}

qboolean R_TemporalMotionTargetsEnsure( temporalMotionTargets_t *targets,
		ralBackend_t *backend, uint32_t width, uint32_t height,
		uint32_t topologyEpoch ) {
	temporalMotionTargets_t candidate;
	ralTextureCreateInfo_t tci;
	ralTextureViewCreateInfo_t vci;
	const ralCaps_t *caps;

	if ( !targets || !backend || !width || !height || !topologyEpoch ) return qfalse;
	if ( targets->ready && targets->backend == backend
			&& targets->width == width && targets->height == height
			&& targets->topologyEpoch == topologyEpoch ) return qtrue;
	caps = Ral_GetCaps( backend );
	if ( !caps || !caps->independentBlend || caps->maxColorAttachments < 3 ) return qfalse;
	if ( !caps->maxTextureDimension2D || width > caps->maxTextureDimension2D
			|| height > caps->maxTextureDimension2D ) return qfalse;
	if ( !Ral_TextureFormatSupports( backend, RAL_FORMAT_R16G16_SFLOAT,
			TEMPORAL_MOTION_TARGET_USAGE )
		|| !Ral_TextureFormatSupports( backend, RAL_FORMAT_R8_UNORM,
			TEMPORAL_MOTION_TARGET_USAGE ) ) return qfalse;
	if ( targets->allocationGeneration == UINT32_MAX ) return qfalse;

	memset( &candidate, 0, sizeof( candidate ) );
	candidate.backend = backend;
	candidate.width = width;
	candidate.height = height;
	candidate.topologyEpoch = topologyEpoch;
	candidate.allocationGeneration = targets->allocationGeneration + 1u;

	memset( &tci, 0, sizeof( tci ) );
	tci.type = RAL_TEXTURE_2D;
	tci.width = width;
	tci.height = height;
	tci.depthOrArrayLayers = 1;
	tci.mipLevels = 1;
	tci.sampleCount = 1;
	tci.usage = TEMPORAL_MOTION_TARGET_USAGE;
	tci.memory = RAL_MEMORY_DEVICE_LOCAL;
	tci.format = RAL_FORMAT_R16G16_SFLOAT;
	tci.debugName = "wired-temporal-motion-velocity";
	candidate.velocity = Ral_CreateTexture( backend, &tci );
	if ( !candidate.velocity ) goto fail;
	if ( AliasesLiveTexture( targets, candidate.velocity ) ) goto fail;
	memset( &vci, 0, sizeof( vci ) );
	vci.texture = candidate.velocity;
	vci.viewType = RAL_TEXTURE_2D;
	vci.format = RAL_FORMAT_UNDEFINED;
	candidate.velocityView = Ral_CreateTextureView( backend, &vci );
	if ( !candidate.velocityView ) goto fail;
	if ( AliasesLiveView( targets, candidate.velocityView ) ) goto fail;

	tci.format = RAL_FORMAT_R8_UNORM;
	tci.debugName = "wired-temporal-motion-validity";
	candidate.validity = Ral_CreateTexture( backend, &tci );
	if ( !candidate.validity ) goto fail;
	if ( candidate.validity == candidate.velocity
			|| AliasesLiveTexture( targets, candidate.validity ) ) goto fail;
	vci.texture = candidate.validity;
	candidate.validityView = Ral_CreateTextureView( backend, &vci );
	if ( !candidate.validityView ) goto fail;
	if ( candidate.validityView == candidate.velocityView
			|| AliasesLiveView( targets, candidate.validityView ) ) goto fail;

	candidate.ready = qtrue;
	DestroyHandles( targets );
	*targets = candidate;
	return qtrue;

fail:
	DestroyCandidate( &candidate, targets );
	return qfalse;
}
