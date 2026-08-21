// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_LEGACY_MATERIAL_H
#define WIRED_RAL_LEGACY_MATERIAL_H

#include "../../qcommon/q_shared.h"

typedef enum {
	RAL_LEGACY_COMBINE_MODULATE = 0,
	RAL_LEGACY_COMBINE_ADD_IDENTITY = 1,
	RAL_LEGACY_COMBINE_ADD_NON_IDENTITY = 2,
	RAL_LEGACY_COMBINE_MODULATE_ALPHA = 3,
	RAL_LEGACY_COMBINE_MODULATE_ONE_MINUS_ALPHA = 4,
	RAL_LEGACY_COMBINE_MIX_SOURCE_ALPHA = 5,
	RAL_LEGACY_COMBINE_MIX_ONE_MINUS_SOURCE_ALPHA = 6,
	RAL_LEGACY_COMBINE_DST_COLOR_SOURCE_ALPHA = 7
} ralLegacyCombine_t;

typedef enum {
	RAL_LEGACY_ALPHA_TEST_NONE = 0,
	RAL_LEGACY_ALPHA_TEST_KEEP_NOT_EQUAL = 1,
	RAL_LEGACY_ALPHA_TEST_KEEP_LESS = 2,
	RAL_LEGACY_ALPHA_TEST_KEEP_GREATER_EQUAL = 3
} ralLegacyAlphaTest_t;

typedef enum {
	RAL_LEGACY_MATERIAL_DIRECT = 0,
	RAL_LEGACY_MATERIAL_FALLBACK = 1
} ralLegacyMaterialOutcome_t;

typedef enum {
	RAL_LEGACY_FALLBACK_NONE = 0,
	RAL_LEGACY_FALLBACK_ENTITY_UNIFORM,
	RAL_LEGACY_FALLBACK_ALPHA_TEST,
	RAL_LEGACY_FALLBACK_DEPTH_FRAGMENT,
	RAL_LEGACY_FALLBACK_ALPHA_TO_COVERAGE,
	RAL_LEGACY_FALLBACK_ABSOLUTE_LIGHT,
	RAL_LEGACY_FALLBACK_DISCARD,
	RAL_LEGACY_FALLBACK_NORMAL_MAP,
	RAL_LEGACY_FALLBACK_IBL
} ralLegacyFallbackReason_t;

// Backend-neutral material facts that are known outside the shader constants.
// textureCount is the number of additional texture-coordinate axes (0..2).
typedef struct {
	uint32_t textureCount;
	qboolean shaderFog;
	qboolean environmentMapping;
	qboolean textureCoordinateAnimation;
	qboolean vertexDeform;
} ralLegacyMaterialFacts_t;

// Semantic form of the legacy generic shader specialization. Backends lower
// this contract; they do not reinterpret Vulkan specialization-word offsets.
typedef struct {
	qboolean entityStorageTransform;
	ralLegacyAlphaTest_t alphaTest;
	float alphaTestValue;
	float depthFragment;
	qboolean alphaToCoverage;
	uint32_t textureDomainMask;
	qboolean absoluteLight;
	ralLegacyCombine_t combine;
	uint32_t discardMode;
	float fixedColor;
	float fixedAlpha;
	uint32_t fogFactor;
	float depthFadeScale;
	uint32_t normalFormat;
	qboolean iblEnabled;
	uint32_t lightmapSlot;
} ralLegacyMaterialVariant_t;

typedef struct {
	ralLegacyMaterialFacts_t facts;
	ralLegacyMaterialVariant_t variant;
	ralLegacyMaterialOutcome_t outcome;
	ralLegacyFallbackReason_t fallbackReason;
	qboolean ready;
} ralLegacyMaterialReceipt_t;

// Invalid semantic input returns qfalse and leaves `out` unchanged. Every
// valid input returns qtrue with either DIRECT or a deterministic FALLBACK.
qboolean Ral_LegacyMaterialResolve( const ralLegacyMaterialFacts_t *facts,
	const ralLegacyMaterialVariant_t *variant,
	ralLegacyMaterialReceipt_t *out );
qboolean Ral_LegacyMaterialReceiptExact( const ralLegacyMaterialReceipt_t *a,
	const ralLegacyMaterialReceipt_t *b );

#endif
