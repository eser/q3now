// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_MATERIAL_PROXY_H
#define WIRED_RAL_MATERIAL_PROXY_H

#include "ral_material.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_MATERIAL_PROXY_SCHEMA_VERSION 1u
#define RAL_MATERIAL_PROXY_RECEIPT_SCHEMA_VERSION 1u
#define RAL_MATERIAL_PROXY_EVALUATION_SCHEMA_VERSION 1u
#define RAL_MATERIAL_PROXY_MAX_INSTRUCTIONS 32u
#define RAL_MATERIAL_PROXY_VALUE_COUNT 12u
#define RAL_MATERIAL_PROXY_Q16_ONE 65536

typedef enum {
	RAL_MATERIAL_PROXY_BASE_COLOR_R = 0,
	RAL_MATERIAL_PROXY_BASE_COLOR_G,
	RAL_MATERIAL_PROXY_BASE_COLOR_B,
	RAL_MATERIAL_PROXY_BASE_COLOR_A,
	RAL_MATERIAL_PROXY_EMISSIVE_R,
	RAL_MATERIAL_PROXY_EMISSIVE_G,
	RAL_MATERIAL_PROXY_EMISSIVE_B,
	RAL_MATERIAL_PROXY_METALLIC,
	RAL_MATERIAL_PROXY_ROUGHNESS,
	RAL_MATERIAL_PROXY_NORMAL_SCALE,
	RAL_MATERIAL_PROXY_OCCLUSION_STRENGTH,
	RAL_MATERIAL_PROXY_ALPHA_CUTOFF
} ralMaterialProxyTarget_t;

typedef enum {
	RAL_MATERIAL_PROXY_SET = 1,
	RAL_MATERIAL_PROXY_ADD,
	RAL_MATERIAL_PROXY_MULTIPLY,
	RAL_MATERIAL_PROXY_LINEAR,
	RAL_MATERIAL_PROXY_SINE
} ralMaterialProxyOpcode_t;

typedef enum {
	RAL_MATERIAL_PROXY_INPUT_NONE = 0,
	RAL_MATERIAL_PROXY_INPUT_TIME,
	RAL_MATERIAL_PROXY_INPUT_SCALAR
} ralMaterialProxyInput_t;

typedef enum {
	RAL_MATERIAL_PROXY_DOMAIN_RUNTIME = 1,
	RAL_MATERIAL_PROXY_DOMAIN_PREVIEW
} ralMaterialProxyDomain_t;

typedef struct {
	ralMaterialProxyTarget_t target;
	ralMaterialProxyOpcode_t opcode;
	ralMaterialProxyInput_t input;
	int32_t aQ16;
	int32_t bQ16;
	int32_t cQ16;
} ralMaterialProxyInstruction_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t materialArtifactHash;
	uint32_t instructionCount;
	ralMaterialProxyInstruction_t instructions[RAL_MATERIAL_PROXY_MAX_INSTRUCTIONS];
} ralMaterialProxyProgram_t;

typedef struct {
	uint32_t schemaVersion;
	ralMaterialProxyProgram_t program;
	uint64_t programHash;
	qboolean ready;
} ralMaterialProxyProgramReceipt_t;

typedef struct {
	uint64_t evaluationGeneration;
	uint64_t timeMilliseconds;
	int32_t scalarQ16;
} ralMaterialProxyInputs_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t evaluationGeneration;
	uint64_t materialArtifactHash;
	uint64_t programHash;
	uint64_t timeMilliseconds;
	int32_t scalarQ16;
	uint32_t valueCount;
	int32_t valuesQ16[RAL_MATERIAL_PROXY_VALUE_COUNT];
	qboolean ready;
} ralMaterialProxyEvaluationReceipt_t;

qboolean Ral_MaterialProxyProgramBuild( const ralMaterialReceipt_t *material,
	const ralMaterialProxyProgram_t *program,
	ralMaterialProxyProgramReceipt_t *outReceipt );
qboolean Ral_MaterialProxyProgramReceiptValid(
	const ralMaterialProxyProgramReceipt_t *receipt );
qboolean Ral_MaterialProxyEvaluate( const ralMaterialReceipt_t *material,
	const ralMaterialProxyProgramReceipt_t *program,
	const ralMaterialProxyInputs_t *inputs, ralMaterialProxyDomain_t domain,
	uint32_t outputCapacity, ralMaterialProxyEvaluationReceipt_t *outReceipt );
qboolean Ral_MaterialProxyEvaluationReceiptExact(
	const ralMaterialProxyEvaluationReceipt_t *a,
	const ralMaterialProxyEvaluationReceipt_t *b );

#ifdef __cplusplus
}
#endif
#endif
