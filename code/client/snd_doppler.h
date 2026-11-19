// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_SND_DOPPLER_H
#define WIRED_SND_DOPPLER_H

#include <math.h>

/* Existing sound pitch budget, shared by the legacy bookkeeping and adapter. */
#define MAX_DOPPLER_SCALE 50.0f

/* miniaudio's stationary-listener Doppler ratio is c / (c - dot(v, dir)).
 * Its sonic clamp permits a zero denominator. Keep every possible projection
 * below that singularity, with the existing maximum pitch/work budget. This
 * is NOT a world-unit conversion: ordinary velocities and attenuation stay
 * unchanged. Scale the vector uniformly so rotation and direction survive.
 * Called on the main thread, before publishing the complete velocity vector.
 */
static inline void wired_audio_bound_doppler_velocity(float v[3], float speedOfSound)
{
	double lengthSquared, limit, scale;
	if (!isfinite(v[0]) || !isfinite(v[1]) || !isfinite(v[2]) ||
	    !isfinite(speedOfSound) || speedOfSound <= 0.0f) {
		v[0] = v[1] = v[2] = 0.0f;
		return;
	}
	limit = (double)speedOfSound * (1.0 - 1.0 / MAX_DOPPLER_SCALE);
	/* Double intermediates also handle finite FLT_MAX input without overflow. */
	lengthSquared = (double)v[0]*v[0] + (double)v[1]*v[1] + (double)v[2]*v[2];
	if (lengthSquared <= limit*limit)
		return;
	scale = limit / sqrt(lengthSquared);
	v[0] = (float)(v[0] * scale);
	v[1] = (float)(v[1] * scale);
	v[2] = (float)(v[2] * scale);
}

#endif
