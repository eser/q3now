// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_MATERIAL_RELOAD_H
#define WIRED_RAL_MATERIAL_RELOAD_H

#include "ral_material_proxy.h"
#include "ral_material_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_MATERIAL_RELOAD_SCHEMA_VERSION 1u
#define RAL_MATERIAL_RELOAD_RECEIPT_SCHEMA_VERSION 1u

typedef struct {
	ralBackendType_t backendType;
	uintptr_t textureIdentity;
	uint64_t resourceGeneration;
} ralMaterialRetirementItem_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t transactionGeneration;
	ralMaterialRuntimePlan_t activePlan;
	qboolean hasProxy;
	ralMaterialProxyProgramReceipt_t proxy;
	uint32_t retirementCount;
	ralMaterialRetirementItem_t retirements[RAL_MATERIAL_MAX_TEXTURES];
	qboolean ready;
} ralMaterialReloadReceipt_t;

typedef struct {
	const ralMaterialDescription_t *description;
	const ralTextureResourceReceipt_t *resources;
	uint32_t resourceCount;
	uint64_t runtimeGeneration;
	const ralMaterialProxyProgram_t *proxyProgram;
} ralMaterialReloadCandidate_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t transactionGeneration;
	ralMaterialRuntimePlan_t fallbackPlan;
	ralMaterialRuntimePlan_t livePlan;
	qboolean usingFallback;
	qboolean hasProxy;
	ralMaterialProxyProgramReceipt_t proxy;
	qboolean retirementPending;
	ralMaterialReloadReceipt_t pendingRetirement;
} ralMaterialReloadOwner_t;

qboolean Ral_MaterialReloadOwnerInit( const ralMaterialRuntimePlan_t *fallbackPlan,
	uint64_t transactionGeneration, ralMaterialReloadOwner_t *outOwner );
qboolean Ral_MaterialReloadOwnerValid( const ralMaterialReloadOwner_t *owner );
qboolean Ral_MaterialReloadTry( ralMaterialReloadOwner_t *owner,
	const ralMaterialReloadCandidate_t *candidate, uint64_t transactionGeneration,
	ralMaterialReloadReceipt_t *outReceipt );
qboolean Ral_MaterialReloadAcknowledge( ralMaterialReloadOwner_t *owner,
	const ralMaterialReloadReceipt_t *receipt );
qboolean Ral_MaterialReloadReceiptValid( const ralMaterialReloadReceipt_t *receipt );
qboolean Ral_MaterialReloadReceiptExact( const ralMaterialReloadReceipt_t *a,
	const ralMaterialReloadReceipt_t *b );

#ifdef __cplusplus
}
#endif
#endif
