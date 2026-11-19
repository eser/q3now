// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_MATERIAL_PROXY_LOWER_H
#define WIRED_RAL_MATERIAL_PROXY_LOWER_H

#include "ral_material_proxy.h"
#include "ral_material_source.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	RAL_MATERIAL_PROXY_LOWER_OK = 0,
	RAL_MATERIAL_PROXY_LOWER_INVALID_SOURCE,
	RAL_MATERIAL_PROXY_LOWER_INVALID_MATERIAL,
	RAL_MATERIAL_PROXY_LOWER_INVALID_PARAMETER,
	RAL_MATERIAL_PROXY_LOWER_OVER_CAPACITY,
	RAL_MATERIAL_PROXY_LOWER_PROGRAM_REJECTED
} ralMaterialProxyLowerFailure_t;

typedef struct {
	ralMaterialProxyLowerFailure_t reason;
	uint32_t stageIndex;
	uint32_t operationIndex;
	ralMaterialSourceSpan_t span;
} ralMaterialProxyLowerDiagnostic_t;

/*
 * Lowers Q3 waveform/tcMod/deform runtime parameters into the one canonical
 * proxy-v2 evaluator.  Static source facts remain in ralMaterialSourceReceipt;
 * named fields here are the compact per-frame inline values consumed by a
 * backend-neutral material binding.  A material with no runtime/static proxy
 * fields succeeds with outHasProgram=false and a zero receipt.
 */
qboolean Ral_MaterialProxyLowerSource(
	const ralMaterialSourceReceipt_t *source,
	const ralMaterialReceipt_t *material,
	qboolean *outHasProgram,
	ralMaterialProxyProgramReceipt_t *outReceipt,
	ralMaterialProxyLowerDiagnostic_t *outDiagnostic );

#ifdef __cplusplus
}
#endif
#endif
