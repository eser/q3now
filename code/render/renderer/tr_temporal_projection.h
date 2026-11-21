// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_TR_TEMPORAL_PROJECTION_H
#define WIRED_TR_TEMPORAL_PROJECTION_H

#include <stdint.h>

// Applies a physical-pixel jitter to the renderer's perspective projection.
// Pixel X grows right and pixel Y grows down.  The resulting NDC translation
// is returned for diagnostics.  Invalid input leaves both outputs untouched.
int R_TemporalProjectionApply( float projection[16], uint32_t width,
		uint32_t height, const float jitterPixels[2], float outNdcOffset[2] );

#endif // WIRED_TR_TEMPORAL_PROJECTION_H
