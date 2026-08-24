// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#ifndef WIRED_RAL_SSR_POLICY_H
#define WIRED_RAL_SSR_POLICY_H
#include "ral_reflection_probe.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RAL_SSR_SCHEMA_VERSION 1u
#define RAL_SSR_RECEIPT_SCHEMA_VERSION 1u
#define RAL_SSR_MAX_SURFACES 16u
#define RAL_SSR_Q16_ONE 65536
typedef enum { RAL_SSR_TIER_OFF=0,RAL_SSR_TIER_BALANCED,RAL_SSR_TIER_QUALITY } ralSsrTier_t;
typedef enum { RAL_SSR_RESULT_DISABLED=1,RAL_SSR_RESULT_PROBE_FALLBACK,RAL_SSR_RESULT_TRACE } ralSsrResult_t;
typedef enum { RAL_SSR_REJECT_DISABLED=1u<<0,RAL_SSR_REJECT_ROUGHNESS=1u<<1,
	RAL_SSR_REJECT_DEPTH=1u<<2,RAL_SSR_REJECT_HISTORY=1u<<3,RAL_SSR_REJECT_BUDGET=1u<<4 } ralSsrRejectBit_t;
typedef struct { uint64_t surfaceId,surfaceGeneration;uint32_t pixelCount,roughnessQ16;
	qboolean depthValid;ralReflectionProbeReceipt_t fallback; } ralSsrSurface_t;
typedef struct { uint32_t schemaVersion;uint64_t frameGraphGeneration,frameIndex,historyFrame;
	ralSsrTier_t tier;uint32_t maxRoughnessQ16,maxTraceSteps,maxRayPixels,maxHistoryAge,surfaceCount;
	ralTextureResourceReceipt_t sceneColor,sceneDepth,historyColor; } ralSsrRequest_t;
typedef struct { uint64_t surfaceId,surfaceGeneration;ralSsrResult_t result;uint32_t rejectionBits;
	uint32_t traceSteps,rayPixels;ralReflectionProbeReceipt_t fallback; } ralSsrDecision_t;
typedef struct { uint32_t schemaVersion;uint64_t frameGraphGeneration,frameIndex;ralSsrTier_t tier;
	uint32_t decisionCount,totalRayPixels;ralSsrDecision_t decisions[RAL_SSR_MAX_SURFACES];qboolean ready; } ralSsrReceipt_t;
qboolean Ral_SsrResolve(const ralSsrRequest_t*request,const ralSsrSurface_t*surfaces,
	uint32_t outputCapacity,ralSsrReceipt_t*outReceipt);
qboolean Ral_SsrReceiptExact(const ralSsrReceipt_t*a,const ralSsrReceipt_t*b);
#ifdef __cplusplus
}
#endif
#endif
