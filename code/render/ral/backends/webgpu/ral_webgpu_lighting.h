// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_WEBGPU_LIGHTING_H
#define WIRED_RAL_WEBGPU_LIGHTING_H

#include "ral_webgpu_resource.h"
#include "../../core/ral_lighting_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_WEBGPU_LIGHTING_SCHEMA_VERSION 1u

typedef struct ralWebGpuLighting_s ralWebGpuLighting_t;

typedef struct {
	uint32_t schemaVersion;
	ralLightingRuntimePlan_t plan;
	ralWebGpuResourceReceipt_t resources[RAL_LIGHTING_RUNTIME_MAX_PLANES];
	ralWebGpuWriteReceipt_t writes[RAL_LIGHTING_RUNTIME_MAX_PLANES];
	qboolean ready;
} ralWebGpuLightingReceipt_t;

qboolean RalWebGpu_LightingUpload( ralWebGpuResourceLayer_t *resources,
	const void *artifactBytes, uint64_t artifactByteLength,
	const ralLightingRuntimePlan_t *plan, ralWebGpuLighting_t **outLighting,
	ralWebGpuLightingReceipt_t *outReceipt );
qboolean RalWebGpu_LightingReceiptExact( const ralWebGpuLightingReceipt_t *a,
	const ralWebGpuLightingReceipt_t *b );
qboolean RalWebGpu_LightingDestroy( ralWebGpuResourceLayer_t *resources,
	ralWebGpuLighting_t *lighting, const ralWebGpuLightingReceipt_t *authority );

#ifdef __cplusplus
}
#endif
#endif
