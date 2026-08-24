// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_submission.h"

const renderUiPrimitive_t *RenderSubmission_UiPrimitives(
		const renderSubmissionState_t *state, uint32_t *outCount ) {
	if ( !state || !outCount || !state->initialized
			|| ( !state->frameOpen && !state->frameSealed ) ) return NULL;
	*outCount = state->uiPrimitiveCount;
	return state->uiPrimitives;
}
