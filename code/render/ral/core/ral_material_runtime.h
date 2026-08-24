// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_MATERIAL_RUNTIME_H
#define WIRED_RAL_MATERIAL_RUNTIME_H

#include "ral_material.h"
#include "ral_resource.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_MATERIAL_RUNTIME_SCHEMA_VERSION 1u
#define RAL_MATERIAL_RUNTIME_COHORT_SCHEMA_VERSION 1u

typedef struct {
	uint32_t bindingSlot;
	ralMaterialTextureRole_t role;
	uint64_t cacheKeyHash;
	ralTextureResourceReceipt_t resource;
} ralMaterialRuntimeBinding_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t runtimeGeneration;
	ralMaterialReceipt_t material;
	uint32_t bindingCount;
	ralMaterialRuntimeBinding_t bindings[RAL_MATERIAL_MAX_TEXTURES];
	qboolean ready;
} ralMaterialRuntimePlan_t;

typedef enum {
	RAL_MATERIAL_RUNTIME_READY = 1,
	RAL_MATERIAL_RUNTIME_INVALIDATED
} ralMaterialRuntimeState_t;

typedef enum {
	RAL_MATERIAL_DEPENDENCY_ARTIFACT = 1,
	RAL_MATERIAL_DEPENDENCY_TEXTURE
} ralMaterialDependencyKind_t;

typedef struct {
	ralMaterialDependencyKind_t kind;
	uint64_t dependencyGeneration;
	uint64_t artifactHash;
	uintptr_t textureIdentity;
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
qboolean Ral_MaterialRuntimePlanValid( const ralMaterialRuntimePlan_t *plan );
qboolean Ral_MaterialRuntimePlanExact( const ralMaterialRuntimePlan_t *a,
	const ralMaterialRuntimePlan_t *b );
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
