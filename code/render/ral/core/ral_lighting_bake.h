// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#ifndef WIRED_RAL_LIGHTING_BAKE_H
#define WIRED_RAL_LIGHTING_BAKE_H
#include "ral_lighting.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RAL_LIGHTING_BAKE_SCHEMA_VERSION 1u
#define RAL_LIGHTING_BAKE_RECEIPT_SCHEMA_VERSION 1u
#define RAL_LIGHTING_BAKE_MAX_PATCHES 4096u
#define RAL_LIGHTING_BAKE_MAX_LINKS 65536u
#define RAL_LIGHTING_BAKE_MAX_BOUNCES 16u
#define RAL_LIGHTING_BAKE_MAX_ENERGY_Q16 1073741823
#define RAL_LIGHTING_PATCH_SCHEMA_VERSION 1u
#define RAL_LIGHTING_PATCH_GRAPH_SCHEMA_VERSION 1u
#define RAL_LIGHTING_PATCH_GRAPH_RECEIPT_SCHEMA_VERSION 1u
#define RAL_LIGHTING_PATCH_MAX_DIRTY_REGIONS 256u
typedef struct {
	uint64_t patchId;
	uint64_t sourceGeneration;
	uint64_t provenanceHash;
	uint64_t regionId;
	ralLightVec3Q16_t centroid;
	ralLightVec3Q16_t normal;
	int32_t areaQ16;
	int32_t diffuseReflectanceQ16[3];
	int32_t emissionRadianceQ16[3];
} ralLightingBakePatch_t;
typedef struct {
	uint32_t receiverPatch;
	uint32_t emitterPatch;
	uint32_t formFactorQ16;
} ralLightingBakeLink_t;
typedef struct {
	uint32_t schemaVersion;
	uint64_t bakeGeneration;
	uint64_t staticIndirectKey;
	uint64_t staticBakeHash;
	uint64_t producerVersion;
	uint64_t settingsHash;
	const ralLightingBakePatch_t *patches;
	uint32_t patchCount;
	const ralLightingBakeLink_t *links;
	uint32_t linkCount;
	uint32_t bounceCount;
	int32_t energyClampQ16;
} ralLightingBakeRequest_t;
typedef struct {
	uint32_t schemaVersion;
	uint64_t bakeGeneration;
	uint64_t staticIndirectKey;
	uint64_t producerVersion;
	uint64_t radianceHash;
	uint64_t directionHash;
	uint32_t patchCount;
	uint32_t linkCount;
	uint32_t completedBounces;
	uint32_t clampedChannelCount;
	qboolean ready;
} ralLightingBakeReceipt_t;
typedef struct { int64_t x, y, z; } ralLightingBakeDirectionAccum_t;
typedef struct {
	uint32_t schemaVersion;
	uint64_t triangleId;
	uint64_t surfaceId;
	uint64_t sourceGeneration;
	uint64_t provenanceHash;
	uint64_t regionId;
	ralLightVec3Q16_t vertices[3];
	int32_t diffuseReflectanceQ16[3];
	int32_t emissionRadianceQ16[3];
} ralLightingPatchTriangle_t;
typedef struct {
	uint32_t receiverTriangle;
	uint32_t emitterTriangle;
	uint32_t visibilityQ16;
} ralLightingPatchVisibility_t;
typedef struct {
	uint32_t schemaVersion;
	uint64_t graphGeneration;
	const ralLightingPatchTriangle_t *triangles;
	uint32_t triangleCount;
	const ralLightingPatchVisibility_t *visibility;
	uint32_t visibilityCount;
	uint32_t minimumFormFactorQ16;
	uint32_t dirtyRegionCount;
	uint64_t dirtyRegionIds[RAL_LIGHTING_PATCH_MAX_DIRTY_REGIONS];
} ralLightingPatchGraphRequest_t;
typedef struct {
	uint32_t schemaVersion;
	uint64_t graphGeneration;
	uint64_t geometryHash;
	uint64_t materialHash;
	uint64_t emissiveHash;
	uint64_t graphHash;
	uint32_t patchCount;
	uint32_t linkCount;
	uint32_t dirtyPatchCount;
	qboolean ready;
} ralLightingPatchGraphReceipt_t;
qboolean Ral_LightingBakeRun( const ralLightingBakeRequest_t *request,
	ralLightVec3Q16_t *scratchIncoming, uint32_t scratchIncomingCapacity,
	ralLightVec3Q16_t *scratchOutgoing, uint32_t scratchOutgoingCapacity,
	ralLightingBakeDirectionAccum_t *scratchDirection, uint32_t scratchDirectionCapacity,
	ralLightVec3Q16_t *outIndirectRadiance, uint32_t outputCapacity,
	ralLightVec3Q16_t *outDominantDirection, uint32_t directionCapacity,
	ralLightingBakeReceipt_t *outReceipt );
qboolean Ral_LightingBakeReceiptValid( const ralLightingBakeReceipt_t *receipt );
qboolean Ral_LightingPatchGraphBuild( const ralLightingPatchGraphRequest_t *request,
	ralLightingBakePatch_t *outPatches, uint32_t patchCapacity,
	ralLightingBakeLink_t *outLinks, uint32_t linkCapacity,
	uint8_t *outDirtyPatches, uint32_t dirtyCapacity,
	ralLightingPatchGraphReceipt_t *outReceipt );
qboolean Ral_LightingPatchGraphReceiptValid(
	const ralLightingPatchGraphReceipt_t *receipt );
#ifdef __cplusplus
}
#endif
#endif
