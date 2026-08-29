// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_METAL_MODULE_H
#define WIRED_RAL_METAL_MODULE_H

#include "ral_frame_shell.h"
#include "ral_atmosphere.h"
#include "ral_presentation_host.h"
#include "ral_presentation_policy.h"
#include "ral_color_output.h"
#include "ral_metal_present.h"
#include "render_submission.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_METAL_MODULE_SCHEMA_VERSION 3u

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t moduleGeneration;
	uint64_t frameGeneration;
	ralFrameShellReceipt_t frame;
	ralPresentationHostReceipt_t host;
	ralPresentationSurfaceBorrow_t surface;
	ralMetalDrawableReceipt_t drawable;
	ralMetalPresentReceipt_t presentation;
	renderSubmissionReceipt_t frontend;
	ralAtmospherePlanReceipt_t atmosphere;
	qboolean ready;
} ralMetalModuleFrameReceipt_t;

Q_EXPORT qboolean WiredMetal_GetFrameReceipt(
	ralMetalModuleFrameReceipt_t *outReceipt );
Q_EXPORT qboolean RalMetal_ModuleFrameReceiptExact(
	const ralMetalModuleFrameReceipt_t *a,
	const ralMetalModuleFrameReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
