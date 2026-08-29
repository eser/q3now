// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_VULKAN_LIGHTING_H
#define WIRED_RAL_VULKAN_LIGHTING_H

#include "../../core/ral.h"
#include "../../core/ral_lighting_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_VULKAN_LIGHTING_SCHEMA_VERSION 1u

typedef struct ralVulkanLighting_s ralVulkanLighting_t;

typedef struct {
	uint32_t schemaVersion;
	ralLightingRuntimePlan_t plan;
	ralTextureResourceReceipt_t resources[RAL_LIGHTING_RUNTIME_MAX_PLANES];
	uint64_t uploadHash;
	qboolean ready;
} ralVulkanLightingReceipt_t;

qboolean RalVulkan_LightingUpload( ralBackend_t *backend,
	const void *artifactBytes, uint64_t artifactByteLength,
	const ralLightingRuntimePlan_t *plan, ralVulkanLighting_t **outLighting,
	ralVulkanLightingReceipt_t *outReceipt );
qboolean RalVulkan_LightingReceiptExact( const ralVulkanLightingReceipt_t *a,
	const ralVulkanLightingReceipt_t *b );
qboolean RalVulkan_LightingDestroy( ralBackend_t *backend,
	ralVulkanLighting_t *lighting, const ralVulkanLightingReceipt_t *authority );

#ifdef __cplusplus
}
#endif

#endif
