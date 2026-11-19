// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_MATERIAL_SOURCE_H
#define WIRED_RAL_MATERIAL_SOURCE_H

#include "ral_material.h"
#include "ral_material_proxy.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION 5u
#define RAL_MATERIAL_SOURCE_RECEIPT_SCHEMA_VERSION 4u
#define RAL_MATERIAL_SKY_FACE_COUNT 6u
#define RAL_MATERIAL_SOURCE_MAX_DEPENDENCIES 64u
#define RAL_MATERIAL_SOURCE_MAX_STAGES 8u
#define RAL_MATERIAL_SOURCE_MAX_TCMODS 8u
#define RAL_MATERIAL_SOURCE_MAX_DEFORMS 8u
#define RAL_MATERIAL_SOURCE_MAX_ANIMATION_FRAMES 24u

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
	RAL_MATERIAL_DEFORM_AUTOSPRITE2,
	RAL_MATERIAL_DEFORM_PROJECTION_SHADOW,
	RAL_MATERIAL_DEFORM_TEXT0,
	RAL_MATERIAL_DEFORM_TEXT1,
	RAL_MATERIAL_DEFORM_TEXT2,
	RAL_MATERIAL_DEFORM_TEXT3,
	RAL_MATERIAL_DEFORM_TEXT4,
	RAL_MATERIAL_DEFORM_TEXT5,
	RAL_MATERIAL_DEFORM_TEXT6,
	RAL_MATERIAL_DEFORM_TEXT7
} ralMaterialDeformType_t;

typedef enum {
	RAL_MATERIAL_WAVE_NONE = 0,
	RAL_MATERIAL_WAVE_SIN,
	RAL_MATERIAL_WAVE_SQUARE,
	RAL_MATERIAL_WAVE_TRIANGLE,
	RAL_MATERIAL_WAVE_SAWTOOTH,
	RAL_MATERIAL_WAVE_INVERSE_SAWTOOTH,
	RAL_MATERIAL_WAVE_NOISE
} ralMaterialWaveFunction_t;

typedef enum {
	RAL_MATERIAL_COLOR_BAD = 0,
	RAL_MATERIAL_COLOR_IDENTITY_LIGHTING,
	RAL_MATERIAL_COLOR_IDENTITY,
	RAL_MATERIAL_COLOR_ENTITY,
	RAL_MATERIAL_COLOR_ONE_MINUS_ENTITY,
	RAL_MATERIAL_COLOR_EXACT_VERTEX,
	RAL_MATERIAL_COLOR_VERTEX,
	RAL_MATERIAL_COLOR_ONE_MINUS_VERTEX,
	RAL_MATERIAL_COLOR_WAVE,
	RAL_MATERIAL_COLOR_LIGHTING_DIFFUSE,
	RAL_MATERIAL_COLOR_FOG,
	RAL_MATERIAL_COLOR_CONST
} ralMaterialColorGen_t;

typedef enum {
	RAL_MATERIAL_ALPHA_IDENTITY = 0,
	RAL_MATERIAL_ALPHA_SKIP,
	RAL_MATERIAL_ALPHA_ENTITY,
	RAL_MATERIAL_ALPHA_ONE_MINUS_ENTITY,
	RAL_MATERIAL_ALPHA_VERTEX,
	RAL_MATERIAL_ALPHA_ONE_MINUS_VERTEX,
	RAL_MATERIAL_ALPHA_LIGHTING_SPECULAR,
	RAL_MATERIAL_ALPHA_WAVE,
	RAL_MATERIAL_ALPHA_PORTAL,
	RAL_MATERIAL_ALPHA_CONST
} ralMaterialAlphaGen_t;

typedef enum {
	RAL_MATERIAL_DEPTH_LEQUAL = 0,
	RAL_MATERIAL_DEPTH_EQUAL
} ralMaterialDepthFunc_t;

typedef enum {
	RAL_MATERIAL_ALPHA_TEST_NONE = 0,
	RAL_MATERIAL_ALPHA_TEST_KEEP_NOT_EQUAL,
	RAL_MATERIAL_ALPHA_TEST_KEEP_LESS,
	RAL_MATERIAL_ALPHA_TEST_KEEP_GREATER_EQUAL
} ralMaterialSourceAlphaTest_t;

typedef enum {
	RAL_MATERIAL_FOG_NONE = 0,
	RAL_MATERIAL_FOG_LINEAR,
	RAL_MATERIAL_FOG_EXP,
	RAL_MATERIAL_FOG_EXP2
} ralMaterialFogType_t;

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
	RAL_MATERIAL_SOURCE_INVALID_PBR_TEXTURE,
	RAL_MATERIAL_SOURCE_DUPLICATE_PBR_TEXTURE,
	RAL_MATERIAL_SOURCE_INVALID_AUTHORED_PROXY,
	RAL_MATERIAL_SOURCE_NONFINITE_VALUE,
	RAL_MATERIAL_SOURCE_INVALID_SKY
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
	ralMaterialWaveFunction_t function;
	ralMaterialSourceSpan_t span;
	float base;
	float amplitude;
	float phase;
	float frequency;
} ralMaterialSourceWave_t;

typedef struct {
	ralMaterialSourceSpan_t span;
	ralMaterialMapKind_t mapKind;
	uint32_t dependencyIndex;
	ralMaterialSourceTcGen_t tcGen;
	float tcGenVectors[2][3];
	uint32_t sourceBlend;
	uint32_t destinationBlend;
	ralMaterialSourceAlphaTest_t alphaTest;
	float alphaCutoff;
	qboolean depthWrite;
	ralMaterialDepthFunc_t depthFunc;
	ralMaterialColorGen_t rgbGen;
	ralMaterialAlphaGen_t alphaGen;
	float constantColor[4];
	ralMaterialSourceWave_t rgbWave;
	ralMaterialSourceWave_t alphaWave;
	float portalRange;
	float animationFrequency;
	qboolean animationLooping;
	uint32_t animationFrameCount;
	uint32_t animationDependencyIndices[RAL_MATERIAL_SOURCE_MAX_ANIMATION_FRAMES];
	uint32_t tcModCount;
	ralMaterialSourceTcMod_t tcMods[RAL_MATERIAL_SOURCE_MAX_TCMODS];
} ralMaterialSourceStage_t;

typedef struct {
	ralMaterialDeformType_t type;
	ralMaterialSourceSpan_t span;
	float parameters[8];
} ralMaterialSourceDeform_t;

typedef struct {
	ralMaterialSourceSpan_t span;
	uint32_t dependencyIndex;
	ralMaterialTextureRole_t role;
	ralMaterialChannelConvention_t channels;
	uint32_t uvSet;
} ralMaterialSourcePbrTexture_t;

/* Q3 sky authoring, not an Earth-atmosphere preset. Face order is Q3
 * rt/bk/lf/ft/up/dn; the GPU adapter owns cube-face remapping. Cloud layers
 * remain ordered source.stages, including tcMods and wave/animation programs.
 * Sun intensity retains Q3 compiler units, not exposure or physical watts. */
typedef struct {
	ralMaterialSourceSpan_t parmsSpan;
	ralMaterialSourceSpan_t sunSpan;
	qboolean outerBox;
	qboolean innerBox;
	uint32_t outerDependencies[RAL_MATERIAL_SKY_FACE_COUNT];
	uint32_t innerDependencies[RAL_MATERIAL_SKY_FACE_COUNT];
	qboolean hasSun;
	qboolean sunExtended;
	float sunColor[3];
	float sunIntensity;
	float sunAzimuthDegrees;
	float sunElevationDegrees;
	/* sunExt sampling hints are bake metadata, not runtime cloud counts
	 * or the angular size of the visible sun disk. */
	float sunDevianceDegrees;
	uint32_t sunSamples;
} ralMaterialSourceSky_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t generation;
	ralMaterialProvenance_t provenance;
	ralMaterialSourceSpan_t declaration;
	char semanticName[MAX_QPATH];
	uint32_t surfaceFlags;
	uint32_t contentFlags;
	qboolean clearSolid;
	uint32_t cullMode;
	float sort;
	qboolean sky;
	qboolean noDraw;
	qboolean polygonOffset;
	qboolean noPicMip;
	qboolean noMipMaps;
	float skyCloudHeight;
	ralMaterialSourceSky_t skySource;
	float fogColor[3];
	float fogDepth;
	ralMaterialFogType_t fogType;
	float fogDensity;
	float fogFarClip;
	float diffuseReflectance[3];
	float emissiveRadiance[3];
	float emissiveRange;
	uint32_t lightingFlags;
	ralMaterialFactors_t pbrFactors;
	ralMaterialRenderPolicy_t pbrRenderPolicy;
	uint32_t pbrTextureCount;
	ralMaterialSourcePbrTexture_t pbrTextures[RAL_MATERIAL_MAX_TEXTURES];
	uint32_t dependencyCount;
	ralMaterialSourceDependency_t dependencies[RAL_MATERIAL_SOURCE_MAX_DEPENDENCIES];
	uint32_t stageCount;
	ralMaterialSourceStage_t stages[RAL_MATERIAL_SOURCE_MAX_STAGES];
	uint32_t deformCount;
	ralMaterialSourceDeform_t deforms[RAL_MATERIAL_SOURCE_MAX_DEFORMS];
	qboolean hasAuthoredProxy;
	ralMaterialProxyProgram_t authoredProxy;
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
