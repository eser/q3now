// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_OPENGL_LIGHTING_H
#define WIRED_RAL_OPENGL_LIGHTING_H

#include "ral_opengl_core.h"
#include "ral_lighting_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_OPENGL_LIGHTING_SCHEMA_VERSION 1u

typedef struct ralOpenGlLighting_s ralOpenGlLighting_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t coreGeneration;
	ralLightingRuntimePlan_t plan;
	uint32_t textureNames[RAL_LIGHTING_RUNTIME_MAX_PLANES];
	uint64_t uploadHash;
	qboolean ready;
} ralOpenGlLightingReceipt_t;

qboolean RalOpenGl_LightingUpload( ralOpenGlCore_t *core,
	const ralOpenGlCoreReceipt_t *coreReceipt, const void *artifactBytes,
	uint64_t artifactByteLength, const ralLightingRuntimePlan_t *plan,
	ralOpenGlLighting_t **outLighting, ralOpenGlLightingReceipt_t *outReceipt );
qboolean RalOpenGl_LightingReceiptExact( const ralOpenGlLightingReceipt_t *a,
	const ralOpenGlLightingReceipt_t *b );
qboolean RalOpenGl_LightingDestroy( ralOpenGlCore_t *core,
	const ralOpenGlCoreReceipt_t *coreReceipt, ralOpenGlLighting_t *lighting,
	const ralOpenGlLightingReceipt_t *authority );

#ifdef __cplusplus
}
#endif

#endif
