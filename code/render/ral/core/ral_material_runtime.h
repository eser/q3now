// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_MATERIAL_RUNTIME_H
#define WIRED_RAL_MATERIAL_RUNTIME_H

#include "ral_material_cook.h"
#include "ral_material.h"
#include "ral_resource.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_MATERIAL_RUNTIME_SCHEMA_VERSION 3u
#define RAL_MATERIAL_RUNTIME_COHORT_SCHEMA_VERSION 1u

typedef struct {
	uint32_t bindingSlot;
	ralMaterialTextureRole_t role;
	uint64_t cacheKeyHash;
	ralTextureResourceReceipt_t resource;
} ralMaterialRuntimeBinding_t;

/* Cook-time dependencies which do not necessarily become sampled texture
 * bindings still participate in the runtime cohort.  Keeping their canonical
 * identity in the portable plan lets render-program, lighting and residency
 * publication invalidate the same atomic material owner as texture changes. */
typedef struct {
	uint32_t dependencyIndex;
	ralMaterialDependencyRole_t role;
	uint64_t sourceId;
	uint64_t fsGeneration;
	uint64_t creationOptionsHash;
	uint64_t artifactHash;
	qboolean hasTextureResource;
	uint64_t cacheKeyHash;
	ralTextureResourceReceipt_t textureResource;
} ralMaterialRuntimeDependency_t;

/* Portable Q3 sky program retained by the runtime plan.  Faces continue to
 * reference dependency indices in Q3 rt/bk/lf/ft/up/dn order; adapters only
 * lower the attached RAL texture receipts to their native binding model. */
typedef struct {
	qboolean present;
	float cloudHeight;
	ralMaterialSourceSky_t authored;
	uint32_t stageCount;
	ralMaterialSourceStage_t stages[RAL_MATERIAL_SOURCE_MAX_STAGES];
} ralMaterialRuntimeSkySource_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t runtimeGeneration;
	ralMaterialReceipt_t material;
	uint32_t bindingCount;
	ralMaterialRuntimeBinding_t bindings[RAL_MATERIAL_MAX_TEXTURES];
	uint32_t dependencyCount;
	ralMaterialRuntimeDependency_t dependencies[RAL_MATERIAL_COOK_MAX_BINDINGS];
	ralMaterialRuntimeSkySource_t sky;
	qboolean ready;
} ralMaterialRuntimePlan_t;

typedef struct {
	uint64_t cacheKeyHash;
	ralTextureResourceReceipt_t resource;
} ralMaterialRuntimeResource_t;

qboolean Ral_MaterialRuntimeTextureKeyBuild(
	uint64_t sourceId, uint64_t filesystemGeneration,
	const unsigned char *rgba8, uint32_t width, uint32_t height,
	qboolean srgb, qboolean noMipMaps,
	ralTextureAssetResidency_t residency, ralTextureCacheKey_t *outKey );
qboolean Ral_MaterialRuntimeResourceBuild(
	const ralTextureCacheKey_t *key,
	const ralTextureResourceReceipt_t *resource,
	ralMaterialRuntimeResource_t *outResource );

typedef enum {
	RAL_MATERIAL_RUNTIME_BUILD_OK = 0,
	RAL_MATERIAL_RUNTIME_BUILD_INVALID_COOKED,
	RAL_MATERIAL_RUNTIME_BUILD_INVALID_RESOURCE_SET,
	RAL_MATERIAL_RUNTIME_BUILD_MISSING_RESOURCE,
	RAL_MATERIAL_RUNTIME_BUILD_DUPLICATE_RESOURCE,
	RAL_MATERIAL_RUNTIME_BUILD_PLAN_REJECTED
} ralMaterialRuntimeBuildFailure_t;

typedef struct {
	ralMaterialRuntimeBuildFailure_t reason;
	uint32_t itemIndex;
	uint64_t cacheKeyHash;
} ralMaterialRuntimeBuildDiagnostic_t;

typedef enum {
	RAL_MATERIAL_RUNTIME_READY = 1,
	RAL_MATERIAL_RUNTIME_INVALIDATED
} ralMaterialRuntimeState_t;

typedef enum {
	RAL_MATERIAL_RUNTIME_DEPENDENCY_ARTIFACT = 1,
	RAL_MATERIAL_RUNTIME_DEPENDENCY_TEXTURE,
	RAL_MATERIAL_RUNTIME_DEPENDENCY_SOURCE
} ralMaterialDependencyKind_t;

typedef struct {
	ralMaterialDependencyKind_t kind;
	uint64_t dependencyGeneration;
	uint64_t artifactHash;
	uintptr_t textureIdentity;
	ralMaterialDependencyRole_t role;
	uint64_t sourceId;
	uint64_t creationOptionsHash;
	uint64_t previousGeneration;
	uint64_t nextGeneration;
} ralMaterialDependencyEvent_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t cohortGeneration;
	uint64_t dependencyGeneration;
	ralMaterialRuntimeState_t state;
	ralMaterialRuntimePlan_t plan;
} ralMaterialRuntimeCohort_t;

qboolean Ral_MaterialRuntimePlanBuild( const ralMaterialReceipt_t *material,
	const ralTextureResourceReceipt_t *resources, uint32_t resourceCount,
	uint64_t runtimeGeneration, ralMaterialRuntimePlan_t *outPlan );
qboolean Ral_MaterialRuntimePlanBuildCooked(
	const ralMaterialCookReceipt_t *cooked,
	const ralMaterialRuntimeResource_t *resources, uint32_t resourceCount,
	uint64_t runtimeGeneration, ralMaterialRuntimePlan_t *outPlan,
	ralMaterialRuntimeBuildDiagnostic_t *outDiagnostic );
qboolean Ral_MaterialRuntimePlanValid( const ralMaterialRuntimePlan_t *plan );
qboolean Ral_MaterialRuntimePlanExact( const ralMaterialRuntimePlan_t *a,
	const ralMaterialRuntimePlan_t *b );
qboolean Ral_MaterialRuntimePlanResolveDependencyTexture(
	const ralMaterialRuntimePlan_t *plan, uint32_t dependencyIndex,
	ralTextureResourceReceipt_t *outResource );
qboolean Ral_MaterialRuntimeCohortInit( const ralMaterialRuntimePlan_t *plan,
	uint64_t cohortGeneration, uint64_t dependencyGeneration,
	ralMaterialRuntimeCohort_t *outCohort );
qboolean Ral_MaterialRuntimeCohortValid( const ralMaterialRuntimeCohort_t *cohort );
qboolean Ral_MaterialRuntimeInvalidate( ralMaterialRuntimeCohort_t *cohort,
	const ralMaterialDependencyEvent_t *event, qboolean *outInvalidated );

#ifdef __cplusplus
}
#endif
#endif
