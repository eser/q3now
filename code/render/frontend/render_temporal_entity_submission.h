// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_TEMPORAL_ENTITY_SUBMISSION_H
#define WIRED_RENDER_TEMPORAL_ENTITY_SUBMISSION_H

/* The entity-motion rules the RAL renderers share (Metal, WebGPU): which entity batch writes its object velocity into the TAA
 * effect mask, and the pose an entity records into the shared entity pose cache (render_temporal_entity.h). Header-only, so a
 * target links nothing beyond the submission frontend and the pose cache it already uses. */

#include "render_submission.h"
#include "render_temporal_entity.h"

#include <string.h>

/* The rule every renderer applies (R_TemporalMotionClassify) to an entity batch whose previous-frame vertex positions are known (a
 * rigid brush entity, or an MD3 / IQM model whose previous animation frame is replayed): opaque, depth-writing, unanimated texture.
 * Alpha-tested batches stay invalidated: the mask rasterizes their whole geometry, holes included. An environment-mapped stage is
 * shaded by the surface orientation, not carried by the geometry: when the entity turned since its previous pose (`rotated`), its
 * reflection does not follow the motion vector, so it is treated like an animated texture. */
static inline qboolean RenderTemporal_EntityBatchWritesMotion( const renderSubmissionState_t *frontend, qhandle_t material,
		qboolean rotated ) {
	renderMaterialSnapshot_t snapshot;
	temporalMotionDrawFacts_t facts;
	if ( !RenderSubmission_MaterialSnapshot( frontend, material, &snapshot ) || !snapshot.ready ) return qfalse;
	memset( &facts, 0, sizeof( facts ) );
	facts.geometry = TEMPORAL_MOTION_GEOMETRY_MOD_BRUSH_RIGID;
	facts.visible = qtrue;
	facts.alphaTested = snapshot.alphaMode == RENDER_ALPHA_MASK ? qtrue : qfalse;
	facts.additive = snapshot.alphaMode == RENDER_ALPHA_ADDITIVE ? qtrue : qfalse;
	facts.blended = snapshot.alphaMode != RENDER_ALPHA_OPAQUE && snapshot.alphaMode != RENDER_ALPHA_MASK ? qtrue : qfalse;
	/* Sorted after SS_OPAQUE but drawn opaque and depth-writing (an item frame whose opaque, environment-mapped base carries a
	 * blended detail stage) is opaque geometry, not a translucent effect. */
	facts.transparent = snapshot.sort > 3.0f
		&& ( snapshot.alphaMode != RENDER_ALPHA_OPAQUE || !snapshot.depthWrite ) ? qtrue : qfalse;   /* SS_OPAQUE */
	facts.decal = snapshot.sort == 4.0f ? qtrue : qfalse;        /* SS_DECAL */
	facts.opaque = !facts.blended && !facts.transparent ? qtrue : qfalse;
	facts.depthAuthoritative = snapshot.depthWrite;
	facts.sky = snapshot.sky;
	facts.effectMask = qtrue;
	for ( uint32_t stage = 0u; stage < snapshot.stageCount && stage < RENDER_MATERIAL_MAX_STAGES; ++stage )
		if ( snapshot.stages[stage].tcModCount || snapshot.stages[stage].animationFrameCount > 1u
				|| ( rotated && snapshot.stages[stage].tcGen == RENDER_MATERIAL_TCGEN_ENVIRONMENT ) ) facts.textureAnimated = qtrue;
	if ( facts.alphaTested ) return qfalse;
	return R_TemporalMotionClassify( &facts ) == TEMPORAL_MOTION_WRITE_VALID ? qtrue : qfalse;
}

/* True when the entity's orientation changed between its previous pose and its current one. */
static inline qboolean RenderTemporal_PoseRotated( const temporalEntityPose_t *previous, const temporalEntityPose_t *current ) {
	for ( uint32_t i = 0u; i < 9u; ++i ) {
		const float d = previous->axis[i] - current->axis[i];
		if ( d > 1.0e-5f || d < -1.0e-5f ) return qtrue;
	}
	return qfalse;
}

/* The pose an entity records: model identity (handle, data, topology, generation, digest), animation frame and transform. */
static inline void RenderTemporal_EntityPose( const refEntity_t *entity, const renderModelSnapshot_t *model,
		temporalEntityPose_t *pose ) {
	memset( pose, 0, sizeof( *pose ) );
	pose->hModel = entity->hModel;
	pose->modelToken = (uintptr_t)model->handle;
	pose->modelDataToken = (uintptr_t)model->positions;
	pose->modelType = (uint32_t)model->format;
	pose->modelTopology = model->vertexCount;
	pose->modelAllocationGeneration = (uint32_t)model->generation;
	pose->modelContentDigest = model->digest;
	pose->frame = entity->frame; pose->oldframe = entity->oldframe; pose->backlerp = entity->backlerp;
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		pose->origin[axis] = entity->origin[axis];
		for ( uint32_t component = 0u; component < 3u; ++component )
			pose->axis[axis * 3u + component] = entity->axis[axis][component];
	}
	pose->nonNormalizedAxes = entity->nonNormalizedAxes ? 1u : 0u;
}

#endif
