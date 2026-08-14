// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_TR_TEMPORAL_HISTORY_H
#define WIRED_TR_TEMPORAL_HISTORY_H

#include "../renderer/ral/ral_resource.h"
#include "../renderer/ral/ral_temporal.h"

typedef struct {
	ralTexture_t *color[2];
	ralTextureView_t *colorView[2];
	ralTexture_t *depth[2];
	ralTextureView_t *depthView[2];
	uint32_t width, height, topologyEpoch, allocationGeneration;
	qboolean ready;
} temporalHistoryResources_t;

void R_TemporalHistoryInit( temporalHistoryResources_t *history );
qboolean R_TemporalHistoryEnsure( temporalHistoryResources_t *history,
	ralBackend_t *backend, uint32_t width, uint32_t height,
	uint32_t topologyEpoch );
void R_TemporalHistoryRelease( temporalHistoryResources_t *history );

// Backend hand-off. The frame plan/resources remain owned by tr_temporal_input;
// callers receive borrowed pointers valid through the frame submit decision.
qboolean R_TemporalBackendGetPending( int worldIndex, uint64_t frameId,
	const ralTemporalFramePlan_t **plan, temporalHistoryResources_t **history );
void R_TemporalBackendMarkHistoryRecorded( int worldIndex, uint64_t frameId,
	qboolean previousSlotRead );

#endif
