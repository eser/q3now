// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RENDER_MATERIAL_PROJECT_COOK_H
#define WIRED_RENDER_MATERIAL_PROJECT_COOK_H

#include "render_material_script.h"
#include "../ral/core/ral_material_package.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_MATERIAL_PROJECT_COOK_SCHEMA_VERSION 2u

typedef enum {
	RENDER_MATERIAL_PROJECT_COOK_OK = 0,
	RENDER_MATERIAL_PROJECT_COOK_INVALID_ARGUMENT,
	RENDER_MATERIAL_PROJECT_COOK_OVER_CAPACITY,
	RENDER_MATERIAL_PROJECT_COOK_SOURCE_REJECTED,
	RENDER_MATERIAL_PROJECT_COOK_DEPENDENCY_REJECTED,
	RENDER_MATERIAL_PROJECT_COOK_PRODUCT_REJECTED,
	RENDER_MATERIAL_PROJECT_COOK_MANIFEST_REJECTED,
	RENDER_MATERIAL_PROJECT_COOK_IO_REJECTED
} renderMaterialProjectCookFailure_t;

typedef struct {
	renderMaterialProjectCookFailure_t reason;
	uint32_t shaderIndex;
	uint32_t dependencyIndex;
	ralMaterialSourceDiagnostic_t source;
	ralMaterialCookDiagnostic_t cook;
} renderMaterialProjectCookDiagnostic_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t generation;
	uint32_t mapShaderCount;
	uint32_t cookedMaterialCount;
	uint32_t materialVariantCount;
	uint32_t legacyStageCount;
	uint32_t legacyVariantCount;
	uint32_t fallbackStageCount;
	uint32_t proxyProgramCount;
	uint64_t manifestArtifactHash;
	uint64_t corpusHash;
	uint64_t projectReferenceArtifactHash;
	uint32_t mapChecksum;
	qboolean ready;
} renderMaterialProjectCookReceipt_t;

/* Explicit offline command path. It streams one source/cook receipt at a time,
 * publishes immutable content-addressed products, and publishes the package
 * manifest only after the complete map corpus succeeds. */
qboolean RenderMaterialProjectCook_Execute(
	const renderMaterialScriptCatalog_t *catalog, const mapFile_t *map,
	const refimport_t *imports, const char *derivedRoot, uint64_t generation,
	renderMaterialProjectCookReceipt_t *outReceipt,
	renderMaterialProjectCookDiagnostic_t *outDiagnostic );

qboolean RenderMaterialProjectCook_ReceiptValid(
	const renderMaterialProjectCookReceipt_t *receipt );

/* Emit the bounded corpus/variant accounting produced by an explicit project
 * cook.  This is deliberately command-time telemetry: adapters call it only
 * after a valid receipt is published, never from registration or frame paths. */
qboolean RenderMaterialProjectCook_LogTelemetry(
	const renderMaterialProjectCookReceipt_t *receipt,
	const refimport_t *imports );

#ifdef __cplusplus
}
#endif

#endif
