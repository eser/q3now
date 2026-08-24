// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_PLANAR_WATER_H
#define WIRED_RAL_PLANAR_WATER_H

#include "ral_resource.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_PLANAR_WATER_SCHEMA_VERSION 1u
#define RAL_PLANAR_WATER_RECEIPT_SCHEMA_VERSION 1u
#define RAL_PLANAR_WATER_MAX_SURFACES 8u
#define RAL_PLANAR_WATER_Q16_ONE 65536

typedef enum { RAL_PLANAR_CLIP_KEEP_POSITIVE=1, RAL_PLANAR_CLIP_KEEP_NEGATIVE } ralPlanarClipPolicy_t;
typedef struct { int32_t x,y,z,w; } ralPlanarPlaneQ16_t;

typedef struct {
	uint64_t surfaceId,surfaceGeneration,targetGraphGeneration;
	uint32_t priority,visiblePixels;
	uint64_t lastCaptureFrame;
	ralPlanarPlaneQ16_t plane;
	uint32_t fresnelF0Q16,dudvStrengthQ16;
	ralTextureResourceReceipt_t reflectionTarget,refractionTarget;
} ralPlanarWaterCandidate_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t frameGraphGeneration,frameIndex;
	uint32_t maxSurfaces,maxPixelWork,candidateCount;
} ralPlanarWaterRequest_t;

typedef struct {
	ralPlanarClipPolicy_t clipPolicy;
	ralPlanarPlaneQ16_t plane;
	ralTextureResourceReceipt_t target;
} ralPlanarCapturePass_t;

typedef struct {
	uint64_t surfaceId,surfaceGeneration;
	uint32_t pixelWork,fresnelF0Q16,dudvStrengthQ16;
	ralPlanarCapturePass_t reflection,refraction;
} ralPlanarWaterUpdate_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t frameGraphGeneration,frameIndex;
	uint32_t updateCount,totalPixelWork;
	ralPlanarWaterUpdate_t updates[RAL_PLANAR_WATER_MAX_SURFACES];
	qboolean ready;
} ralPlanarWaterReceipt_t;

qboolean Ral_PlanarWaterPlan( const ralPlanarWaterRequest_t *request,
	const ralPlanarWaterCandidate_t *candidates,uint32_t outputCapacity,
	ralPlanarWaterReceipt_t *outReceipt );
qboolean Ral_PlanarWaterReceiptExact( const ralPlanarWaterReceipt_t *a,
	const ralPlanarWaterReceipt_t *b );

#ifdef __cplusplus
}
#endif
#endif
