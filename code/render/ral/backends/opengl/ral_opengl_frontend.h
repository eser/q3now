// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_OPENGL_FRONTEND_H
#define WIRED_RAL_OPENGL_FRONTEND_H

#include "ral_opengl_core.h"
#include "render_submission.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_OPENGL_FRONTEND_SCHEMA_VERSION 1u

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t coreGeneration;
	uint64_t planGeneration;
	uint64_t frontendOwnerGeneration;
	uint64_t frontendFrameGeneration;
	uint64_t frontendFrameDigest;
	uint64_t loweringDigest;
	uint32_t loweredMaterialCount;
	uint32_t loweredWorldVertexCount;
	uint32_t loweredWorldIndexCount;
	uint32_t loweredWorldBatchCount;
	uint32_t texturedWorldBatchCount;
	uint32_t lightmappedWorldBatchCount;
	uint32_t patchWorldBatchCount;
	uint32_t loweredModelEntityCount;
	uint32_t loweredPrimitiveEntityCount;
	uint32_t loweredTemporalEntityCount;
	uint32_t loweredLocalIrradianceEntityCount;
	uint32_t loweredEntityIndexCount;
	uint32_t loweredEntityBatchCount;
	uint32_t loweredUiPrimitiveCount;
	uint32_t texturedUiPrimitiveCount;
	uint32_t msdfUiPrimitiveCount;
	uint32_t submittedPolygonCount;
	uint32_t submittedLightCount;
	uint32_t loweredPolygonCount;
	uint32_t loweredLightCount;
	uint32_t loweredEffectIndexCount;
	uint32_t loweredEffectBatchCount;
	uint32_t unresolvedCount;
	uint32_t fallbackCount;
	uint32_t fatalCount;
	qboolean ready;
} ralOpenGlFrontendPlanReceipt_t;

qboolean RalOpenGl_FrontendPlanBuild(
	const ralOpenGlCoreReceipt_t *coreReceipt,
	const renderSubmissionState_t *frontend,
	const renderSubmissionReceipt_t *submission, uint64_t planGeneration,
	ralOpenGlFrontendPlanReceipt_t *outReceipt );
qboolean RalOpenGl_FrontendPlanReceiptExact(
	const ralOpenGlFrontendPlanReceipt_t *a,
	const ralOpenGlFrontendPlanReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
