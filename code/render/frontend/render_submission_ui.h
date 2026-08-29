// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_SUBMISSION_UI_H
#define WIRED_RENDER_FRONTEND_SUBMISSION_UI_H

#include "q_shared.h"
#include "tr_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A native HiDPI console can expose roughly 160 columns x 50 rows before
 * panel/HUD decoration. Keep the portable frame bounded, but leave enough
 * headroom that a full scrollback page cannot exhaust the RAL submission and
 * recursively generate "rejected ui-quad" console diagnostics. */
#define RENDER_SUBMISSION_MAX_UI_PRIMITIVES 8192u

typedef enum {
	RENDER_UI_QUAD = 1,
	RENDER_UI_LINE
} renderUiPrimitiveKind_t;

typedef struct {
	renderUiPrimitiveKind_t kind;
	/* Final paint-space TL,TR,BR,BL corners. Consumers use these for every
	 * primitive so UI subtree transforms preserve glyph/icon/bar geometry. */
	float positions[4][2];
	float x;
	float y;
	float width;
	float height;
	float s1;
	float t1;
	float s2;
	float t2;
	float rotation;
	float color[4];
	qhandle_t material;
} renderUiPrimitive_t;

typedef struct renderSubmissionState_s renderSubmissionState_t;

/* Projects one paint-space point onto the four-corner HUD plane described by
 * transform. Every descendant primitive uses this same homography so text,
 * icons, bars and chrome retain one coherent vanishing geometry. */
qboolean RenderUi_ProjectPoint( const refUiTransform_t *transform,
	float *x, float *y );

const renderUiPrimitive_t *RenderSubmission_UiPrimitives(
	const renderSubmissionState_t *state, uint32_t *outCount );

#ifdef __cplusplus
}
#endif

#endif
