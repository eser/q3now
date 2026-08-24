// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_SHADER_COHORT_H
#define WIRED_VK_TEMPORAL_SHADER_COHORT_H

#include "../../../core/ral_pipeline.h"

typedef enum {
	VK_TEMPORAL_SHADER_PRESERVE = 1,
	VK_TEMPORAL_SHADER_WRITE = 2,
	VK_TEMPORAL_SHADER_INVALIDATE = 3,
	VK_TEMPORAL_SHADER_IQM_INVALIDATE = 4
} vkTemporalShaderRecipeKind_t;

typedef struct {
	const unsigned char *bytes;
	uint32_t size;
} vkTemporalShaderBlob_t;

typedef struct {
	vkTemporalShaderRecipeKind_t kind;
	qboolean alphaTested;
	qboolean depthOnly;
	qboolean blended;
	qboolean special;
	qboolean dynamicDiscard;
	qboolean fog;
	ralFormat_t sceneFormat;
	ralColorBlendAttachment_t sceneBlend;
	const void *genericSetLayouts[4];
	const void *iqmSetLayouts[2];
	vkTemporalShaderBlob_t ordinaryVertex;
	vkTemporalShaderBlob_t ordinaryFragment;
	vkTemporalShaderBlob_t temporalVertex;
	vkTemporalShaderBlob_t temporalWriteFragment;
	vkTemporalShaderBlob_t temporalInvalidateFragment;
	vkTemporalShaderBlob_t iqmVertex;
	vkTemporalShaderBlob_t iqmInvalidateFragment;
} vkTemporalShaderRecipeInput_t;

typedef struct {
	const void *setLayouts[4];
	uint32_t numSetLayouts;
	uint32_t pushOffset;
	uint32_t pushSize;
	uint32_t pushStages;
	uint32_t numPushRanges;
	ralFormat_t colorFormats[3];
	ralColorBlendAttachment_t colorBlends[3];
	uint32_t numColorAttachments;
	vkTemporalShaderBlob_t vertex;
	vkTemporalShaderBlob_t fragment;
	qboolean directSpirvOverride;
	vkTemporalShaderRecipeKind_t kind;
} vkTemporalShaderRecipe_t;

// Definition-only exact recipe authoring. It creates no layout, module or
// pipeline and has no production caller. Failure leaves `out` byte-identical.
qboolean VK_TemporalShaderRecipeBuild(
	const vkTemporalShaderRecipeInput_t *input,
	vkTemporalShaderRecipe_t *out );

#endif
