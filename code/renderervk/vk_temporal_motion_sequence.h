// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_MOTION_SEQUENCE_H
#define WIRED_VK_TEMPORAL_MOTION_SEQUENCE_H

#include "tr_temporal_motion.h"
#include "vk_temporal_generic_pipeline_table.h"

typedef struct {
	uint64_t lane0;
	uint64_t lane1;
	uint32_t count;
} vkTemporalMotionDrawSequence_t;

// Appends one effective draw decision to an ordered, generation-bound digest.
// PRESERVE has no exact3 receipt; WRITE/INVALIDATE require the exact receipt
// that authorized the corresponding candidate pipeline set.
qboolean VK_TemporalMotionDrawSequenceAppend(
	vkTemporalMotionDrawSequence_t *sequence,
	uint32_t absoluteEntMatSlot, uint32_t pipelineSlot,
	temporalMotionOutcome_t outcome,
	const vkTemporalGenericPipelineReceipt_t *pipelineReceipt );
qboolean VK_TemporalMotionDrawSequenceEqual(
	const vkTemporalMotionDrawSequence_t *a,
	const vkTemporalMotionDrawSequence_t *b );

#endif
