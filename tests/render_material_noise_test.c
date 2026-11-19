// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_material_noise.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void R_NoiseInit( void );
float R_NoiseGet4f( float x, float y, float z, double t );
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static float values[256];
static unsigned char permutation[256];
static uint32_t next( uint32_t *state ) {
	uint32_t x = *state;
	x ^= x << 13; x ^= x >> 17; x ^= x << 5;
	return *state = x;
}

/* Independent corner-weight sum, not the production nested lerps. */
static double reference( double x, double y, double z, double t ) {
	const double position[4] = { x, y, z, t };
	int cell[4]; double fraction[4], result = 0;
	for (unsigned i = 0; i < 4; ++i) {
		cell[i] = (int)floor(position[i]); fraction[i] = position[i] - cell[i];
	}
	for (unsigned corner = 0; corner < 16; ++corner) {
		unsigned index = 0; double weight = 1;
		for (int axis = 3; axis >= 0; --axis) {
			unsigned upper = (corner >> axis) & 1u;
			index = permutation[(cell[axis] + upper + index) & 255u];
			weight *= upper ? fraction[axis] : 1 - fraction[axis];
		}
		result += weight * values[index];
	}
	return result;
}

int main( void ) {
	uint32_t seed = 0x6D6F6953u;
	for (unsigned i = 0; i < 256; ++i) {
		values[i] = ((float)(next(&seed) & 0xFFFFFFu) / (float)0xFFFFFF) * 2.0f - 1.0f;
		permutation[i] = (unsigned char)(next(&seed) & 255u);
		CHECK(values[i] == wired_material_noise_values[i]);
		CHECK(permutation[i] == wired_material_noise_permutation[i]);
	}
	/* Ready before init; repeated renderer init neither changes values nor RNG. */
	const float before = R_NoiseGet4f(.125f, -.5f, 2.25f, -256.375);
	srand(17); const int expectedRandom = rand(); srand(17);
	R_NoiseInit(); R_NoiseInit();
	CHECK(rand() == expectedRandom);
	CHECK(before == R_NoiseGet4f(.125f, -.5f, 2.25f, -256.375));
	for (int i = -512; i < 512; ++i) {
		const double time = i + .375;
		const float actual = R_NoiseGet4f(0, 0, 0, time);
		CHECK(fabs(actual - reference(0, 0, 0, time)) < 1e-6);
		CHECK(actual == R_NoiseGet4f(0, 0, 0, time + 256));
		CHECK(fabs(R_NoiseGet4f(.125f, -.5f, 2.25f, time)
			- reference(.125, -.5, 2.25, time)) < 1e-6);
	}
	puts("PASS: frozen Q3 noise tables, 4D interpolation, wrap and initialization identity");
	return 0;
}
