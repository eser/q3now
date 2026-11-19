// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#ifndef WIRED_VK_SKY_MATERIAL_H
#define WIRED_VK_SKY_MATERIAL_H

#include "../../../../frontend/render_material_stage_gpu.h"

// Adapter-local borrowed resources for the shared GPU material program. The
// capture scheduler supplies its frozen cohort clock, not wall-clock time.
// No texture creation, decoding, ownership transfer or display transform here.
typedef struct {
	qboolean ready;
	qboolean animated;
	uint64_t revision;
	renderSkyMaterialGpuParams_t params;
	struct ralTextureView_s *stageViews[RENDER_MATERIAL_MAX_STAGES];
	struct ralSampler_s *stageSamplers[RENDER_MATERIAL_MAX_STAGES];
} ralFrontendSkyMaterial_t;

// Output-atomic on failure. No active sky publishes ready=false for pre-world
// startup; a missing authored image/sampler rejects rather than using white.
qboolean R_FrontendSkyMaterialPacket( int captureTimeMs,
		ralFrontendSkyMaterial_t *outPacket );

// Read the submitted world's shader clock, never the mutable 2D/backend clock.
qboolean R_FrontendSkyCaptureTime( int *outTimeMs, int *outSceneTimeMs );

#endif
