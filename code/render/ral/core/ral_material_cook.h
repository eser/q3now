// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_MATERIAL_COOK_H
#define WIRED_RAL_MATERIAL_COOK_H

#include "ral_legacy_material.h"
#include "ral_material.h"
#include "ral_material_proxy.h"
#include "ral_material_source.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_MATERIAL_COOK_SCHEMA_VERSION 4u
#define RAL_MATERIAL_COOK_MAX_BINDINGS RAL_MATERIAL_SOURCE_MAX_DEPENDENCIES

typedef enum {
	RAL_MATERIAL_COOK_OK = 0,
	RAL_MATERIAL_COOK_INVALID_SOURCE,
	RAL_MATERIAL_COOK_OVER_CAPACITY,
	RAL_MATERIAL_COOK_MISSING_BINDING,
	RAL_MATERIAL_COOK_DUPLICATE_BINDING,
	RAL_MATERIAL_COOK_INVALID_BINDING,
	RAL_MATERIAL_COOK_STALE_BINDING,
	RAL_MATERIAL_COOK_MATERIAL_REJECTED,
	RAL_MATERIAL_COOK_LEGACY_REJECTED,
	RAL_MATERIAL_COOK_PROXY_REJECTED,
	RAL_MATERIAL_COOK_ALLOCATION_REJECTED
} ralMaterialCookFailure_t;

typedef struct {
	uint32_t dependencyIndex;
	ralMaterialDependencyRole_t role;
	uint64_t sourceId;
	uint64_t fsGeneration;
	uint64_t creationOptionsHash;
	uint64_t artifactHash;
	ralTextureCacheKey_t texture;
} ralMaterialCookBinding_t;

typedef struct {
	ralMaterialCookFailure_t reason;
	uint32_t itemIndex;
	ralMaterialSourceSpan_t span;
} ralMaterialCookDiagnostic_t;

typedef struct {
	uint32_t schemaVersion;
	ralMaterialSourceReceipt_t source;
	uint32_t bindingCount;
	ralMaterialCookBinding_t bindings[RAL_MATERIAL_COOK_MAX_BINDINGS];
	ralMaterialReceipt_t material;
	uint32_t legacyStageCount;
	ralLegacyMaterialReceipt_t legacyStages[RAL_MATERIAL_SOURCE_MAX_STAGES];
	qboolean hasProxy;
	ralMaterialProxyProgramReceipt_t proxy;
	uint64_t artifactHash;
	qboolean ready;
} ralMaterialCookReceipt_t;

qboolean Ral_MaterialCook( const ralMaterialSourceReceipt_t *source,
	const ralMaterialCookBinding_t *bindings, uint32_t bindingCount,
	ralMaterialCookReceipt_t *outReceipt,
	ralMaterialCookDiagnostic_t *outDiagnostic );
qboolean Ral_MaterialCookReceiptValid(
	const ralMaterialCookReceipt_t *receipt );
qboolean Ral_MaterialCookReceiptExact(
	const ralMaterialCookReceipt_t *a,
	const ralMaterialCookReceipt_t *b );

#ifdef __cplusplus
}
#endif
#endif
