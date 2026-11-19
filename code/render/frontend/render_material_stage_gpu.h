// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RENDER_MATERIAL_STAGE_GPU_H
#define WIRED_RENDER_MATERIAL_STAGE_GPU_H

#include "render_submission_material.h"
#include <string.h>
#include <math.h>

// Backend-neutral std140 stage operands. Native resource/texture selection is
// separate; this is the exact 448-byte MaterialStageParams GPU wire layout.
// Input is a validated submission snapshot (tcModCount <= MAX_TCMODS); target
// is part of an already-zeroed draw packet. Do not clear it again per stage.
typedef struct {
	uint32_t meta[4];
	uint32_t generators[4];
	float params[4];
	float scaleScroll[4];
	float turbulence[4];
	float stretch[4];
	float constantColor[4];
	float rgbWave[4];
	float alphaWave[4];
	float tcGenVectorS[4];
	float tcGenVectorT[4];
	uint32_t tcModMeta[4];
	struct {
		uint32_t type;
		float parameters[6];
		uint32_t reserved;
	} tcMods[RAL_MATERIAL_SOURCE_MAX_TCMODS];
} renderMaterialStageGpuParams_t;

static inline void wired_material_stage_pack_tcmods( const renderMaterialStageSnapshot_t *source,
		renderMaterialStageGpuParams_t *target ) {
	target->tcModMeta[0] = source->tcModCount;
	for ( uint32_t op = 0u; op < source->tcModCount; ++op ) {
		target->tcMods[op].type = source->tcMods[op].type;
		memcpy( target->tcMods[op].parameters, source->tcMods[op].parameters,
			sizeof( target->tcMods[op].parameters ) );
	}
}

static inline void wired_material_stage_pack( const renderMaterialStageSnapshot_t *stage,
		renderMaterialStageGpuParams_t *stageParams ) {
	wired_material_stage_pack_tcmods( stage, stageParams );
	stageParams->meta[0] = stage->sourceBlend;
	stageParams->meta[1] = stage->destinationBlend;
	stageParams->meta[2] = stage->imageSource | ( stage->tcGen << 8u );
	stageParams->meta[3] = stage->alphaTest;
	stageParams->generators[0] = stage->rgbGen;
	stageParams->generators[1] = stage->alphaGen;
	stageParams->generators[2] = (uint32_t)stage->rgbWave.function;
	stageParams->generators[3] = (uint32_t)stage->alphaWave.function;
	stageParams->params[0] = stage->alphaCutoff;
	stageParams->params[1] = stage->rotateDegrees;
	stageParams->params[2] = stage->hasTurbulence ? 1.0f : 0.0f;
	stageParams->params[3] = stage->hasStretch ? 1.0f : 0.0f;
	memcpy( stageParams->scaleScroll, stage->scaleScroll,
		sizeof( stageParams->scaleScroll ) );
	memcpy( stageParams->turbulence, stage->turbulence,
		sizeof( stageParams->turbulence ) );
	memcpy( stageParams->stretch, stage->stretch,
		sizeof( stageParams->stretch ) );
	memcpy( stageParams->constantColor, stage->constantColor,
		sizeof( stageParams->constantColor ) );
	stageParams->rgbWave[0] = stage->rgbWave.base;
	stageParams->rgbWave[1] = stage->rgbWave.amplitude;
	stageParams->rgbWave[2] = stage->rgbWave.phase;
	stageParams->rgbWave[3] = stage->rgbWave.frequency;
	stageParams->alphaWave[0] = stage->alphaWave.base;
	stageParams->alphaWave[1] = stage->alphaWave.amplitude;
	stageParams->alphaWave[2] = stage->alphaWave.phase;
	stageParams->alphaWave[3] = stage->alphaWave.frequency;
	memcpy( stageParams->tcGenVectorS, stage->tcGenVectors[0],
		3u * sizeof( float ) );
	stageParams->tcGenVectorS[3] = stage->portalRange;
	memcpy( stageParams->tcGenVectorT, stage->tcGenVectors[1],
		3u * sizeof( float ) );
}

typedef char wired_material_stage_gpu_layout[(sizeof(renderMaterialStageGpuParams_t) == 448u) ? 1 : -1];

typedef struct {
	uint32_t meta[4];
	float timeHeight[4];
	renderMaterialStageGpuParams_t stages[RENDER_MATERIAL_MAX_STAGES];
} renderSkyMaterialGpuParams_t;
typedef char wired_sky_material_gpu_layout[(sizeof(renderSkyMaterialGpuParams_t) == 3616u) ? 1 : -1];

// Canonical ProductDraw block shared by the portable vertex and sky fragment.
typedef struct {
	float viewOriginDrawSpace[4], viewForward[4], viewLeft[4], viewUp[4];
	float projectionLightmap[4], viewportAlpha[4];
	float atmosphereEyeDensity[4], atmosphereColorVisibility[4], atmosphereHeightSky[4];
	uint32_t atmosphereFroxelGrid[4];
	float localSh[4][4];
	uint32_t staticLighting[4];
	float emissiveRadiance[4];
} renderProductDrawGpuParams_t;
typedef char wired_product_draw_gpu_layout[(sizeof(renderProductDrawGpuParams_t) == 256u) ? 1 : -1];

// Resolve semantic stage handles, never native resources. Shared clock and
// loop/clamp rules; texture residency stays backend-owned. Outputs are valid
// only on success, so callers must not submit a rejected packet.
static inline qboolean wired_sky_material_pack(const renderMaterialSnapshot_t *material,
		qhandle_t lightmap, int timeMs, renderSkyMaterialGpuParams_t *params,
		qhandle_t images[RENDER_MATERIAL_MAX_STAGES]) {
	if (!material || !params || !images || !material->ready || !material->sky
			|| material->handle <= 0 || material->stageCount > RENDER_MATERIAL_MAX_STAGES)
		return qfalse;
	memset(params, 0, sizeof(*params));
	params->meta[0] = material->stageCount;
	params->meta[1] = material->skyBox ? 1u : 0u;
	params->timeHeight[0] = (float)timeMs * 0.001f;
	params->timeHeight[1] = material->skyCloudHeight;
	for (uint32_t i = 0; i < RENDER_MATERIAL_MAX_STAGES; ++i) {
		images[i] = material->handle;
		if (i >= material->stageCount) continue;
		const renderMaterialStageSnapshot_t *stage = &material->stages[i];
		if (stage->tcModCount > RAL_MATERIAL_SOURCE_MAX_TCMODS || stage->imageSource > 2u)
			return qfalse;
		wired_material_stage_pack(stage, &params->stages[i]);
		// imageSource: IMAGE=0, LIGHTMAP=1, WHITE=2 in the public stage schema.
		if (stage->imageSource == 2u) continue;
		images[i] = stage->imageSource == 1u ? lightmap : stage->material;
		if (stage->imageSource == 0u && stage->animationFrameCount) {
			if (stage->animationFrameCount > RENDER_MATERIAL_MAX_ANIMATION_FRAMES
					|| !isfinite(stage->animationFrequency) || stage->animationFrequency <= 0)
				return qfalse;
			const double frame = fmax(floor((double)params->timeHeight[0] * stage->animationFrequency), 0.0);
			const uint32_t index = stage->animationLooping
				? (uint32_t)fmod(frame, (double)stage->animationFrameCount)
				: (uint32_t)fmin(frame, (double)stage->animationFrameCount - 1.0);
			images[i] = stage->animationMaterials[index];
		}
		if (images[i] <= 0) return qfalse;
	}
	return qtrue;
}

#endif
