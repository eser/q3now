// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_TR_TEMPORAL_MOTION_TARGETS_H
#define WIRED_TR_TEMPORAL_MOTION_TARGETS_H

#include "../../../core/ral_backend.h"
#include "../../../core/ral_resource.h"

typedef struct {
	ralBackend_t *backend;
	ralTexture_t *velocity;
	ralTextureView_t *velocityView;
	ralTexture_t *validity;
	ralTextureView_t *validityView;
	uint32_t width, height, topologyEpoch, allocationGeneration;
	qboolean ready;
} temporalMotionTargets_t;

void R_TemporalMotionTargetsInit( temporalMotionTargets_t *targets );
qboolean R_TemporalMotionTargetsEnsure( temporalMotionTargets_t *targets,
	ralBackend_t *backend, uint32_t width, uint32_t height,
	uint32_t topologyEpoch );
void R_TemporalMotionTargetsRelease( temporalMotionTargets_t *targets );

#endif
