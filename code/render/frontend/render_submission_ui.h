// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_SUBMISSION_UI_H
#define WIRED_RENDER_FRONTEND_SUBMISSION_UI_H

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_SUBMISSION_MAX_UI_PRIMITIVES 4096u

typedef enum {
	RENDER_UI_QUAD = 1,
	RENDER_UI_LINE
} renderUiPrimitiveKind_t;

typedef struct {
	renderUiPrimitiveKind_t kind;
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

const renderUiPrimitive_t *RenderSubmission_UiPrimitives(
	const renderSubmissionState_t *state, uint32_t *outCount );

#ifdef __cplusplus
}
#endif

#endif
