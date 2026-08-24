// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_OPENGL_PRODUCT_H
#define WIRED_RAL_OPENGL_PRODUCT_H

#include "ral_opengl_world.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_OPENGL_PRODUCT_SCHEMA_VERSION 2u

typedef struct ralOpenGlProduct_s ralOpenGlProduct_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t productGeneration;
	uint64_t frameGeneration;
	uint32_t vertexShaderByteCount;
	uint32_t fragmentShaderByteCount;
	uint64_t vertexShaderDigestLane0;
	uint64_t vertexShaderDigestLane1;
	uint64_t fragmentShaderDigestLane0;
	uint64_t fragmentShaderDigestLane1;
	ralOpenGlFrontendPlanReceipt_t plan;
	ralOpenGlWorldReceipt_t native;
	uint32_t unresolvedCount;
	uint32_t fallbackCount;
	uint32_t fatalCount;
	qboolean ready;
} ralOpenGlProductFrameReceipt_t;

qboolean RalOpenGl_ProductCreate( ralOpenGlCore_t *core,
	const ralOpenGlCoreReceipt_t *coreReceipt, uint64_t generation,
	ralOpenGlProduct_t **outProduct );
void RalOpenGl_ProductDestroy( ralOpenGlProduct_t *product );
qboolean RalOpenGl_ProductSetOutputExtent( ralOpenGlProduct_t *product,
	uint32_t width, uint32_t height );
qboolean RalOpenGl_ProductReadbackRgb( ralOpenGlProduct_t *product,
	byte *outPixels, uint32_t byteCount );
qboolean RalOpenGl_ProductRender( ralOpenGlProduct_t *product,
	const renderSubmissionState_t *frontend,
	const renderSubmissionReceipt_t *submission, uint64_t frameGeneration,
	ralOpenGlProductFrameReceipt_t *outReceipt );
qboolean RalOpenGl_ProductFrameReceiptExact(
	const ralOpenGlProductFrameReceipt_t *a,
	const ralOpenGlProductFrameReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
