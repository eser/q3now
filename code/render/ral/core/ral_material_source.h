// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_MATERIAL_SOURCE_H
#define WIRED_RAL_MATERIAL_SOURCE_H

#include "../../../qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION 1u
#define RAL_MATERIAL_SOURCE_RECEIPT_SCHEMA_VERSION 1u
#define RAL_MATERIAL_SOURCE_MAX_DEPENDENCIES 32u
#define RAL_MATERIAL_SOURCE_MAX_STAGES 8u
#define RAL_MATERIAL_SOURCE_MAX_TCMODS 8u
#define RAL_MATERIAL_SOURCE_MAX_DEFORMS 8u

typedef enum {
	RAL_MATERIAL_PROVENANCE_Q3_SHADER = 1,
	RAL_MATERIAL_PROVENANCE_WIRED_LUA
} ralMaterialProvenance_t;

typedef enum {
	RAL_MATERIAL_DEPENDENCY_TEXTURE = 1,
	RAL_MATERIAL_DEPENDENCY_RENDER_PROGRAM,
	RAL_MATERIAL_DEPENDENCY_LIGHTING,
	RAL_MATERIAL_DEPENDENCY_RESIDENCY
} ralMaterialDependencyRole_t;

typedef enum {
	RAL_MATERIAL_MAP_IMAGE = 1,
	RAL_MATERIAL_MAP_LIGHTMAP,
	RAL_MATERIAL_MAP_WHITE
} ralMaterialMapKind_t;

typedef enum {
	RAL_MATERIAL_TCGEN_TEXTURE = 0,
	RAL_MATERIAL_TCGEN_LIGHTMAP,
	RAL_MATERIAL_TCGEN_ENVIRONMENT,
	RAL_MATERIAL_TCGEN_VECTOR
} ralMaterialSourceTcGen_t;

typedef enum {
	RAL_MATERIAL_TCMOD_SCALE = 1,
	RAL_MATERIAL_TCMOD_SCROLL,
	RAL_MATERIAL_TCMOD_ROTATE,
	RAL_MATERIAL_TCMOD_TURBULENCE,
	RAL_MATERIAL_TCMOD_STRETCH,
	RAL_MATERIAL_TCMOD_TRANSFORM
} ralMaterialTcModType_t;

typedef enum {
	RAL_MATERIAL_DEFORM_WAVE = 1,
	RAL_MATERIAL_DEFORM_NORMAL,
	RAL_MATERIAL_DEFORM_BULGE,
	RAL_MATERIAL_DEFORM_MOVE,
	RAL_MATERIAL_DEFORM_AUTOSPRITE,
	RAL_MATERIAL_DEFORM_AUTOSPRITE2
} ralMaterialDeformType_t;

typedef enum {
	RAL_MATERIAL_SOURCE_OK = 0,
	RAL_MATERIAL_SOURCE_INVALID_SCHEMA,
	RAL_MATERIAL_SOURCE_INVALID_PROVENANCE,
	RAL_MATERIAL_SOURCE_INVALID_SEMANTIC_NAME,
	RAL_MATERIAL_SOURCE_OVER_CAPACITY,
	RAL_MATERIAL_SOURCE_INVALID_DEPENDENCY,
	RAL_MATERIAL_SOURCE_DUPLICATE_DEPENDENCY,
	RAL_MATERIAL_SOURCE_INVALID_STAGE,
	RAL_MATERIAL_SOURCE_INVALID_TCMOD,
	RAL_MATERIAL_SOURCE_INVALID_DEFORM,
	RAL_MATERIAL_SOURCE_NONFINITE_VALUE
} ralMaterialSourceFailure_t;

typedef struct {
	uint64_t sourceId;
	uint32_t line;
	uint32_t column;
} ralMaterialSourceSpan_t;

typedef struct {
	ralMaterialDependencyRole_t role;
	ralMaterialSourceSpan_t span;
	uint64_t sourceId;
	uint64_t fsGeneration;
	uint64_t size;
	uint64_t creationOptionsHash;
	char canonicalPath[MAX_QPATH];
} ralMaterialSourceDependency_t;

typedef struct {
	ralMaterialTcModType_t type;
	ralMaterialSourceSpan_t span;
	float parameters[6];
} ralMaterialSourceTcMod_t;

typedef struct {
	ralMaterialSourceSpan_t span;
	ralMaterialMapKind_t mapKind;
	uint32_t dependencyIndex;
	ralMaterialSourceTcGen_t tcGen;
	uint32_t sourceBlend;
	uint32_t destinationBlend;
	uint32_t alphaTest;
	float alphaCutoff;
	qboolean depthWrite;
	uint32_t tcModCount;
	ralMaterialSourceTcMod_t tcMods[RAL_MATERIAL_SOURCE_MAX_TCMODS];
} ralMaterialSourceStage_t;

typedef struct {
	ralMaterialDeformType_t type;
	ralMaterialSourceSpan_t span;
	float parameters[8];
} ralMaterialSourceDeform_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t generation;
	ralMaterialProvenance_t provenance;
	ralMaterialSourceSpan_t declaration;
	char semanticName[MAX_QPATH];
	uint32_t surfaceFlags;
	uint32_t cullMode;
	float sort;
	qboolean sky;
	qboolean noDraw;
	qboolean polygonOffset;
	float skyCloudHeight;
	float fogColor[3];
	float fogDepth;
	float diffuseReflectance[3];
	float emissiveRadiance[3];
	float emissiveRange;
	uint32_t lightingFlags;
	uint32_t dependencyCount;
	ralMaterialSourceDependency_t dependencies[RAL_MATERIAL_SOURCE_MAX_DEPENDENCIES];
	uint32_t stageCount;
	ralMaterialSourceStage_t stages[RAL_MATERIAL_SOURCE_MAX_STAGES];
	uint32_t deformCount;
	ralMaterialSourceDeform_t deforms[RAL_MATERIAL_SOURCE_MAX_DEFORMS];
} ralMaterialSourceIr_t;

typedef struct {
	ralMaterialSourceFailure_t reason;
	uint32_t itemIndex;
	ralMaterialSourceSpan_t span;
} ralMaterialSourceDiagnostic_t;

typedef struct {
	uint32_t schemaVersion;
	ralMaterialSourceIr_t source;
	uint64_t semanticHash;
	uint64_t dependencyHash;
	uint64_t artifactHash;
	qboolean ready;
} ralMaterialSourceReceipt_t;

qboolean Ral_MaterialSourceCompile( const ralMaterialSourceIr_t *source,
	ralMaterialSourceReceipt_t *outReceipt,
	ralMaterialSourceDiagnostic_t *outDiagnostic );
qboolean Ral_MaterialSourceReceiptValid(
	const ralMaterialSourceReceipt_t *receipt );
qboolean Ral_MaterialSourceReceiptExact(
	const ralMaterialSourceReceipt_t *a,
	const ralMaterialSourceReceipt_t *b );

#ifdef __cplusplus
}
#endif
#endif
