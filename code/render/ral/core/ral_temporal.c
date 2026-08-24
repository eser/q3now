// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_temporal.h"

#include <string.h>

static float Ral_TemporalRadicalInverse( uint32_t value, uint32_t base ) {
	float inverseBase = 1.0f / (float)base;
	float fraction = inverseBase;
	float result = 0.0f;

	while ( value ) {
		result += (float)( value % base ) * fraction;
		value /= base;
		fraction *= inverseBase;
	}
	return result;
}

static void Ral_TemporalJitter( uint8_t phase, float outPixels[2] ) {
	uint32_t sample = (uint32_t)( phase % RAL_TEMPORAL_JITTER_PHASES ) + 1u;
	outPixels[0] = Ral_TemporalRadicalInverse( sample, 2u ) - 0.5f;
	outPixels[1] = Ral_TemporalRadicalInverse( sample, 3u ) - 0.5f;
}

void Ral_TemporalInit( ralTemporalState_t *state ) {
	if ( state ) {
		memset( state, 0, sizeof( *state ) );
	}
}

int Ral_TemporalBeginFrame( ralTemporalState_t *state,
		const ralTemporalFrameInput_t *input, ralTemporalFramePlan_t *outPlan ) {
	ralTemporalFramePlan_t plan;
	uint32_t resetMask = RAL_TEMPORAL_RESET_NONE;
	int resetHistory = 0;

	if ( !state || !input || !outPlan || state->pending || !input->frameId
			|| !input->width || !input->height
			|| ( state->lastCommittedFrame
				&& input->frameId <= state->lastCommittedFrame ) ) {
		return 0;
	}

	memset( &plan, 0, sizeof( plan ) );
	plan.frameId = input->frameId;
	plan.enabled = input->enabled ? 1u : 0u;

	if ( !plan.enabled ) {
		if ( state->active ) {
			resetMask |= RAL_TEMPORAL_RESET_DISABLED;
		}
		plan.generation = state->generation + ( resetMask ? 1u : 0u );
		plan.resetMask = resetMask;
	} else {
		if ( !state->active ) {
			resetMask |= RAL_TEMPORAL_RESET_ACTIVATED;
			resetHistory = 1;
		} else {
			if ( state->width != input->width || state->height != input->height ) {
				resetMask |= RAL_TEMPORAL_RESET_EXTENT;
			}
			if ( state->viewId != input->viewId ) {
				resetMask |= RAL_TEMPORAL_RESET_VIEW;
			}
			if ( state->topologyEpoch != input->topologyEpoch ) {
				resetMask |= RAL_TEMPORAL_RESET_TOPOLOGY;
			}
			if ( state->lastCommittedFrame
					&& input->frameId != state->lastCommittedFrame + 1u ) {
				resetMask |= RAL_TEMPORAL_RESET_FRAME_GAP;
			}
			resetHistory = resetMask != RAL_TEMPORAL_RESET_NONE;
		}
		if ( input->cameraCut ) {
			resetMask |= RAL_TEMPORAL_RESET_CAMERA_CUT;
			resetHistory = 1;
		}

		plan.generation = state->generation + ( resetHistory ? 1u : 0u );
		plan.resetMask = resetMask;
		plan.historyValid = (uint8_t)( state->historyValid && !resetHistory );
		plan.historyReadIndex = plan.historyValid ? state->lastWriteIndex : 0u;
		plan.historyWriteIndex = plan.historyValid
			? (uint8_t)( state->lastWriteIndex ^ 1u ) : 0u;
		plan.jitterPhase = resetHistory ? 0u : state->nextJitterPhase;
		Ral_TemporalJitter( plan.jitterPhase, plan.sceneJitterPixels );
		plan.sceneJitterUv[0] = plan.sceneJitterPixels[0] / (float)input->width;
		plan.sceneJitterUv[1] = plan.sceneJitterPixels[1] / (float)input->height;
		if ( plan.historyValid ) {
			plan.previousJitterPixels[0] = state->previousJitterPixels[0];
			plan.previousJitterPixels[1] = state->previousJitterPixels[1];
		}
	}
	if ( resetMask && state->generation == UINT32_MAX ) {
		return 0;
	}

	state->pending = 1u;
	state->pendingPlan = plan;
	state->pendingWidth = input->width;
	state->pendingHeight = input->height;
	state->pendingViewId = input->viewId;
	state->pendingTopologyEpoch = input->topologyEpoch;
	*outPlan = plan;
	return 1;
}

int Ral_TemporalCommitFrame( ralTemporalState_t *state, uint64_t frameId ) {
	const ralTemporalFramePlan_t *plan;

	if ( !state || !state->pending || state->pendingPlan.frameId != frameId ) {
		return 0;
	}

	plan = &state->pendingPlan;
	state->generation = plan->generation;
	state->lastCommittedFrame = frameId;
	state->width = state->pendingWidth;
	state->height = state->pendingHeight;
	state->viewId = state->pendingViewId;
	state->topologyEpoch = state->pendingTopologyEpoch;
	state->active = plan->enabled;
	if ( plan->enabled ) {
		state->historyValid = 1u;
		state->lastWriteIndex = plan->historyWriteIndex;
		state->nextJitterPhase = (uint8_t)(
			( plan->jitterPhase + 1u ) % RAL_TEMPORAL_JITTER_PHASES );
		state->previousJitterPixels[0] = plan->sceneJitterPixels[0];
		state->previousJitterPixels[1] = plan->sceneJitterPixels[1];
	} else {
		state->historyValid = 0u;
		state->lastWriteIndex = 0u;
		state->nextJitterPhase = 0u;
		state->previousJitterPixels[0] = 0.0f;
		state->previousJitterPixels[1] = 0.0f;
	}
	state->pending = 0u;
	memset( &state->pendingPlan, 0, sizeof( state->pendingPlan ) );
	return 1;
}

int Ral_TemporalCancelFrame( ralTemporalState_t *state, uint64_t frameId ) {
	if ( !state || !state->pending || state->pendingPlan.frameId != frameId ) {
		return 0;
	}
	state->pending = 0u;
	memset( &state->pendingPlan, 0, sizeof( state->pendingPlan ) );
	return 1;
}
