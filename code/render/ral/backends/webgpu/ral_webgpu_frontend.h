// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_WEBGPU_FRONTEND_H
#define WIRED_RAL_WEBGPU_FRONTEND_H

#include "ral_webgpu_runtime.h"
#include "render_submission.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_WEBGPU_FRONTEND_SCHEMA_VERSION 1u

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t backendGeneration;
	uint64_t planGeneration;
	uint64_t frontendOwnerGeneration;
	uint64_t frontendFrameGeneration;
	uint64_t frontendFrameDigest;
	uint64_t loweringDigest;
	uint32_t materialCount;
	uint32_t worldVertexCount;
	uint32_t worldIndexCount;
	uint32_t worldBatchCount;
	uint32_t patchBatchCount;
	uint32_t lightmappedWorldBatchCount;
	uint32_t modelEntityCount;
	uint32_t primitiveEntityCount;
	uint32_t temporalEntityCount;
	uint32_t localIrradianceEntityCount;
	uint32_t entityIndexCount;
	uint32_t entityBatchCount;
	uint32_t polygonCount;
	uint32_t polygonBatchCount;
	uint32_t lightCount;
	uint32_t uiPrimitiveCount;
	uint32_t texturedUiPrimitiveCount;
	uint32_t msdfUiPrimitiveCount;
	uint32_t drawCount;
	uint32_t unresolvedCount;
	uint32_t fallbackCount;
	uint32_t fatalCount;
	qboolean ready;
} ralWebGpuFrontendPlanReceipt_t;

qboolean RalWebGpu_FrontendPlanBuild( ralWebGpuRuntime_t *runtime,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const renderSubmissionState_t *frontend,
	const renderSubmissionReceipt_t *submission, uint64_t planGeneration,
		ralWebGpuFrontendPlanReceipt_t *outReceipt );
uint32_t RalWebGpu_FrontendPlanLastFailureStage( void );
qboolean RalWebGpu_FrontendPlanReceiptExact(
	const ralWebGpuFrontendPlanReceipt_t *a,
	const ralWebGpuFrontendPlanReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
