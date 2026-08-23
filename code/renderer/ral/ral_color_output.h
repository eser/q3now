// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_COLOR_OUTPUT_H
#define WIRED_RAL_COLOR_OUTPUT_H

#include "ral_swapchain.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_COLOR_OUTPUT_SCHEMA_VERSION 1u

typedef enum {
	RAL_COLOR_TRANSFER_LINEAR = 0,
	RAL_COLOR_TRANSFER_SRGB,
	RAL_COLOR_TRANSFER_PQ,
	RAL_COLOR_TRANSFER_HLG
} ralColorTransfer_t;

typedef enum {
	RAL_TONEMAP_IDENTITY = 0,
	RAL_TONEMAP_PBR_NEUTRAL,
	RAL_TONEMAP_AGX,
	RAL_TONEMAP_LOTTES,
	RAL_TONEMAP_REINHARD
} ralToneMapOperator_t;

typedef enum {
	RAL_HDR_FALLBACK_NONE = 0,
	RAL_HDR_FALLBACK_NOT_REQUESTED,
	RAL_HDR_FALLBACK_SCENE_NOT_HDR,
	RAL_HDR_FALLBACK_OUTPUT_UNAVAILABLE
} ralHdrFallback_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t presentationGeneration;
	ralFormat_t sceneFormat;
	qboolean requestHdrOutput;
	ralSurfaceFormat_t selectedOutput;
	ralToneMapOperator_t toneMapOperator;
	qboolean lutEnabled;
	float hdrPeakNits;
	float hdrMinNits;
} ralColorOutputRequest_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t presentationGeneration;
	ralFormat_t sceneFormat;
	ralColorTransfer_t sceneTransfer;
	ralColorTransfer_t uiTransfer;
	float uiReferenceWhiteNits;
	ralSurfaceFormat_t presentation;
	ralColorTransfer_t presentationTransfer;
	ralColorTransfer_t screenshotTransfer;
	ralColorTransfer_t readbackTransfer;
	ralToneMapOperator_t toneMapOperator;
	qboolean lutEnabled;
	qboolean hdrActive;
	ralHdrFallback_t hdrFallback;
	float hdrPeakNits;
	float hdrMinNits;
} ralColorOutputReceipt_t;

qboolean Ral_ResolveColorOutput( const ralColorOutputRequest_t *request,
	ralColorOutputReceipt_t *outReceipt );
qboolean Ral_ColorOutputReceiptValid( const ralColorOutputReceipt_t *receipt );
qboolean Ral_ColorOutputReceiptExact( const ralColorOutputReceipt_t *a,
	const ralColorOutputReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
