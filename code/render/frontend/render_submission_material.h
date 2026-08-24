// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_SUBMISSION_MATERIAL_H
#define WIRED_RENDER_FRONTEND_SUBMISSION_MATERIAL_H

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_SUBMISSION_MAX_MATERIALS 1024u
#define RENDER_SUBMISSION_MAX_MATERIAL_BYTES (128u * 1024u * 1024u)
#define RENDER_SUBMISSION_MAX_IMAGE_DIMENSION 16384u

typedef enum {
	RENDER_ALPHA_OPAQUE = 0,
	RENDER_ALPHA_MASK,
	RENDER_ALPHA_BLEND
} renderAlphaMode_t;

typedef struct {
	qhandle_t handle;
	uint64_t generation;
	uint64_t digest;
	uint32_t width;
	uint32_t height;
	uint32_t rowBytes;
	uint32_t byteCount;
	const byte *rgba8;
	qboolean clampToEdge;
	qboolean msdf;
	qboolean srgb;
	renderAlphaMode_t alphaMode;
	float alphaCutoff;
	qboolean depthWrite;
	qboolean ready;
} renderMaterialSnapshot_t;

typedef struct {
	renderMaterialSnapshot_t snapshot;
	char name[MAX_QPATH];
} renderMaterialRecord_t;

typedef struct renderSubmissionState_s renderSubmissionState_t;

qboolean RenderSubmission_MaterialSnapshot(
	const renderSubmissionState_t *state, qhandle_t handle,
	renderMaterialSnapshot_t *outSnapshot );
qhandle_t RenderSubmission_MaterialHandle( const renderSubmissionState_t *state,
	const char *name );

#ifdef __cplusplus
}
#endif

#endif
