// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_MATERIAL_PROXY_H
#define WIRED_RAL_MATERIAL_PROXY_H

#include "ral_material.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_MATERIAL_PROXY_LEGACY_SCHEMA_VERSION 1u
#define RAL_MATERIAL_PROXY_SCHEMA_VERSION 2u
#define RAL_MATERIAL_PROXY_RECEIPT_SCHEMA_VERSION 2u
#define RAL_MATERIAL_PROXY_EVALUATION_SCHEMA_VERSION 2u
#define RAL_MATERIAL_PROXY_MAX_INSTRUCTIONS 32u
#define RAL_MATERIAL_PROXY_BASE_VALUE_COUNT 12u
#define RAL_MATERIAL_PROXY_VALUE_COUNT 256u
#define RAL_MATERIAL_PROXY_Q16_ONE 65536
#define RAL_MATERIAL_PROXY_EXPRESSION_MAX_INSTRUCTIONS 512u
#define RAL_MATERIAL_PROXY_EXPRESSION_MAX_REGISTERS 576u
#define RAL_MATERIAL_PROXY_EXPRESSION_MAX_CONSTANTS 128u
#define RAL_MATERIAL_PROXY_EXPRESSION_MAX_FIELDS 128u
#define RAL_MATERIAL_PROXY_EXPRESSION_MAX_TABLES 8u
#define RAL_MATERIAL_PROXY_EXPRESSION_TABLE_SAMPLES 1025u
#define RAL_MATERIAL_PROXY_FIELD_NAME 32u
#define RAL_MATERIAL_PROXY_INLINE_MAX_BYTES \
	( RAL_MATERIAL_PROXY_VALUE_COUNT * 4u )
#define RAL_MATERIAL_PROXY_REGISTER_TIME 0u
#define RAL_MATERIAL_PROXY_REGISTER_SCALAR 1u
#define RAL_MATERIAL_PROXY_REGISTER_BASE_VALUES 2u

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

/*
 * Schema-v2 expression instructions deliberately retain the compact 8-byte
 * three-address shape used by the cook artifact.  Values are four-lane Q16
 * registers; scalar results are broadcast to every lane.  Constants and
 * lookup tables live in the pointer-free program receipt, never in runtime
 * heap state.
 */
typedef enum {
	RAL_MATERIAL_EXPRESSION_UNMASKED_MOVE_CONSTANT = 0,
	RAL_MATERIAL_EXPRESSION_UNMASKED_MOVE,
	RAL_MATERIAL_EXPRESSION_MOVE_CONSTANT,
	RAL_MATERIAL_EXPRESSION_MOVE,
	RAL_MATERIAL_EXPRESSION_SWIZZLE,
	RAL_MATERIAL_EXPRESSION_ADD,
	RAL_MATERIAL_EXPRESSION_SUBTRACT,
	RAL_MATERIAL_EXPRESSION_MULTIPLY,
	RAL_MATERIAL_EXPRESSION_DOT3,
	RAL_MATERIAL_EXPRESSION_DOT4,
	RAL_MATERIAL_EXPRESSION_DIVIDE,
	RAL_MATERIAL_EXPRESSION_MOD,
	RAL_MATERIAL_EXPRESSION_GT,
	RAL_MATERIAL_EXPRESSION_GE,
	RAL_MATERIAL_EXPRESSION_LT,
	RAL_MATERIAL_EXPRESSION_LE,
	RAL_MATERIAL_EXPRESSION_EQ,
	RAL_MATERIAL_EXPRESSION_NE,
	RAL_MATERIAL_EXPRESSION_AND,
	RAL_MATERIAL_EXPRESSION_OR,
	RAL_MATERIAL_EXPRESSION_TABLE,
	RAL_MATERIAL_EXPRESSION_OPCODE_COUNT
} ralMaterialExpressionOpcode_t;

typedef struct {
	uint16_t opcode;
	uint16_t destination;
	uint16_t a;
	uint16_t b;
} ralMaterialExpressionInstruction_t;

typedef struct {
	int32_t lane[4];
} ralMaterialExpressionValue_t;

typedef struct {
	uint16_t sampleCount;
	uint16_t flags;
	int32_t samplesQ16[RAL_MATERIAL_PROXY_EXPRESSION_TABLE_SAMPLES];
} ralMaterialExpressionTable_t;

typedef enum {
	RAL_MATERIAL_EXPRESSION_TABLE_WRAP = 1u << 0,
	RAL_MATERIAL_EXPRESSION_TABLE_SNAP = 1u << 1
} ralMaterialExpressionTableFlag_t;

typedef struct {
	char name[RAL_MATERIAL_PROXY_FIELD_NAME];
	uint16_t registerIndex;
	uint16_t inlineByteOffset;
	uint8_t componentCount;
	uint8_t outputValueOffset;
	uint16_t reserved;
} ralMaterialExpressionField_t;

typedef char ralMaterialExpressionInstructionMustRemainEightBytes[
	( sizeof( ralMaterialExpressionInstruction_t ) == 8u ) ? 1 : -1];

typedef struct {
	uint32_t schemaVersion;
	uint64_t materialArtifactHash;
	uint32_t instructionCount;
	ralMaterialProxyInstruction_t instructions[RAL_MATERIAL_PROXY_MAX_INSTRUCTIONS];
	uint32_t expressionRegisterCount;
	uint32_t expressionConstantCount;
	uint32_t expressionInstructionCount;
	uint32_t expressionFieldCount;
	uint32_t expressionTableCount;
	ralMaterialExpressionValue_t expressionConstants[
		RAL_MATERIAL_PROXY_EXPRESSION_MAX_CONSTANTS];
	ralMaterialExpressionInstruction_t expressionInstructions[
		RAL_MATERIAL_PROXY_EXPRESSION_MAX_INSTRUCTIONS];
	ralMaterialExpressionField_t expressionFields[
		RAL_MATERIAL_PROXY_EXPRESSION_MAX_FIELDS];
	ralMaterialExpressionTable_t expressionTables[
		RAL_MATERIAL_PROXY_EXPRESSION_MAX_TABLES];
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
