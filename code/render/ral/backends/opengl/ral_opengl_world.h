// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_OPENGL_WORLD_H
#define WIRED_RAL_OPENGL_WORLD_H

#include "ral_opengl_frontend.h"
#include "ral_opengl_lighting.h"
#include "ral_atmosphere.h"
#include "ral_display_visibility.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_OPENGL_WORLD_SCHEMA_VERSION 4u

typedef struct {
	uint64_t generation;
	uint32_t programName;
	uint32_t outputWidth;
	uint32_t outputHeight;
	uint32_t atmosphereBufferName;
	ralAtmospherePlanReceipt_t atmosphere;
	uint32_t weatherProgramName;
	uint32_t weatherUniformBufferName;
	uint32_t weatherParticleBufferName;
	uint32_t weatherParticleCount;
	const ralOpenGlLightingReceipt_t *directionalLighting;
	ralDisplayVisibilityPlan_t displayVisibility;
} ralOpenGlWorldLowerInfo_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t coreGeneration;
	uint64_t generation;
	uint64_t frontendFrameDigest;
	uint64_t loweringDigest;
	uint32_t uploadedMaterialCount;
	uint32_t uploadedWorldVertexCount;
	uint32_t uploadedWorldIndexCount;
	uint32_t worldDrawCount;
	uint32_t legacyLightmapDrawCount;
	uint32_t directionalStaticDrawCount;
	uint64_t surfaceLightingBindingDigest;
	uint32_t polygonCount;
	uint32_t lightCount;
	uint32_t effectIndexCount;
	uint32_t effectDrawCount;
	uint32_t modelEntityCount;
	uint32_t primitiveEntityCount;
	uint32_t temporalEntityCount;
	uint32_t localIrradianceEntityCount;
	uint32_t localIrradianceDrawCount;
	uint32_t entityIndexCount;
	uint32_t entityDrawCount;
	uint32_t uiPrimitiveCount;
	uint32_t texturedUiPrimitiveCount;
	uint32_t msdfUiPrimitiveCount;
	uint32_t uiDrawCount;
	uint32_t weatherDrawCount;
	uint32_t weatherInstanceCount;
	uint32_t nativeDrawCount;
	uint32_t unresolvedCount;
	uint32_t fallbackCount;
	uint32_t fatalCount;
	qboolean ready;
} ralOpenGlWorldReceipt_t;

qboolean RalOpenGl_WorldLower( ralOpenGlCore_t *core,
	const ralOpenGlCoreReceipt_t *coreReceipt,
	const renderSubmissionState_t *frontend,
	const renderSubmissionReceipt_t *submission,
	const ralOpenGlFrontendPlanReceipt_t *plan,
	const ralOpenGlWorldLowerInfo_t *info,
	ralOpenGlWorldReceipt_t *outReceipt );
qboolean RalOpenGl_WorldReceiptExact( const ralOpenGlWorldReceipt_t *a,
	const ralOpenGlWorldReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
