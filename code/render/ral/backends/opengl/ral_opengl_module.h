// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_OPENGL_MODULE_H
#define WIRED_RAL_OPENGL_MODULE_H

#include "ral_opengl_product.h"
#include "ral_atmosphere.h"
#include "render_submission.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_OPENGL_MODULE_SCHEMA_VERSION 2u

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t moduleGeneration;
	uint64_t frameGeneration;
	uint64_t presentedFrames;
	ralOpenGlCoreReceipt_t core;
	renderSubmissionReceipt_t frontend;
	ralOpenGlProductFrameReceipt_t product;
	ralAtmospherePlanReceipt_t atmosphere;
	qboolean ready;
} ralOpenGlModuleFrameReceipt_t;

Q_EXPORT qboolean WiredOpenGl_GetFrameReceipt(
	ralOpenGlModuleFrameReceipt_t *outReceipt );
Q_EXPORT qboolean RalOpenGl_ModuleFrameReceiptExact(
	const ralOpenGlModuleFrameReceipt_t *a,
	const ralOpenGlModuleFrameReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
