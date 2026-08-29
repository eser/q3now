// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_LIGHTING_COOK_DRIVER_H
#define WIRED_RAL_LIGHTING_COOK_DRIVER_H

#include "ral_lighting_cook_pipeline.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_LIGHTING_COOK_DRIVER_SCHEMA_VERSION 1u
#define RAL_LIGHTING_COOK_DRIVER_RECEIPT_SCHEMA_VERSION 1u

typedef qboolean ( *ralLightingCookProduceBatchFn )(
	void *context, const ralLightingCookReceipt_t *batch,
	qboolean finalBatch, ralLightingCookArtifact_t *outArtifacts,
	uint32_t artifactCapacity, uint32_t *outArtifactCount );

typedef struct {
	uint32_t schemaVersion;
	ralLightingCookPipelineRequest_t pipeline;
	ralLightingCookProduceBatchFn produceBatch;
	void *producerContext;
	ralLightingCookArtifact_t *artifactStorage;
	uint32_t artifactCapacity;
} ralLightingCookDriverRequest_t;

typedef struct {
	uint32_t schemaVersion;
	ralLightingCookReceipt_t plan;
	ralLightingCookPipelineReceipt_t publication;
	uint32_t batchCount;
	uint32_t completedRegionCount;
	uint32_t producedArtifactCount;
	qboolean cancelled;
	qboolean ready;
} ralLightingCookDriverReceipt_t;

qboolean Ral_LightingCookDriverExecute(
	const ralLightingCookDriverRequest_t *request,
	ralLightingCookDriverReceipt_t *outReceipt );
qboolean Ral_LightingCookDriverReceiptValid(
	const ralLightingCookDriverReceipt_t *receipt );

#ifdef __cplusplus
}
#endif

#endif
