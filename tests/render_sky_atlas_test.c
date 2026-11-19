// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "../code/render/frontend/render_sky_atlas.h"
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)
static int live, peak, calls, failFace = -1, malformed = -1, rejectAtlas;
static uint32_t sourceWidth = 3, sourceHeight = 2;
static void *allocate(size_t bytes) {
	void *p;
	if (rejectAtlas && bytes == 3 * 2 * 6 * 4) return NULL;
	p = malloc((size_t)bytes);
	if (p) { ++live; if (live > peak) peak = live; }
	return p;
}
static void release(void *p) { if (p) { --live; free(p); } }

qboolean RenderImage_DecodeRgba8(const char *name, byte **pixels,
		uint32_t *width, uint32_t *height, char resolved[MAX_QPATH]) {
	static const char *paths[] = {"env/test_rt", "env/test_bk", "env/test_lf",
		"env/test_ft", "env/test_up", "env/test_dn"};
	const int face = calls++;
	if (face >= 6 || strcmp(name, paths[face])) return qfalse;
	/* Small allocation also tests rejecting malicious dimensions before reads. */
	*pixels = allocate(3 * 2 * 4);
	if (!*pixels) return qfalse;
	*width = sourceWidth; *height = sourceHeight;
	if (malformed == face) ++*width;
	strcpy(resolved, paths[face]);
	for (unsigned row = 0; row < 2; ++row)
		for (unsigned col = 0; col < 3; ++col) {
			byte *p = *pixels + (row * 3 + col) * 4;
			p[0] = (byte)(face * 30 + col); p[1] = (byte)(row * 80);
			p[2] = (byte)(200 - face); p[3] = 255;
		}
	/* Even a decoder returning false with allocated output must be cleaned. */
	return face == failFace ? qfalse : qtrue;
}

int main(void) {
	refimport_t imports = {0};
	byte sentinel, *atlas;
	uint32_t width, height;
	imports.Malloc = allocate; imports.Free = release;
	for (unsigned cycle = 0; cycle < 32; ++cycle) {
		calls = peak = 0; atlas = NULL; width = height = 0;
		CHECK(wired_sky_atlas_decode(&imports, "env/test", &atlas, &width, &height));
		CHECK(calls == 6 && live == 1 && peak == 2 && width == 9 && height == 4);
		for (unsigned face = 0; face < 6; ++face)
			for (unsigned row = 0; row < 2; ++row)
				for (unsigned col = 0; col < 3; ++col) {
					const byte *p = atlas + (((face / 3) * 2 + row) * 9
						+ (face % 3) * 3 + col) * 4;
					CHECK(p[0] == face * 30 + col && p[1] == row * 80
						&& p[2] == 200 - face && p[3] == 255);
				}
		release(atlas); CHECK(live == 0);
	}
	for (int face = 0; face < 6; ++face) {
		calls = 0; failFace = face; atlas = &sentinel; width = 71; height = 73;
		CHECK(!wired_sky_atlas_decode(&imports, "env/test", &atlas, &width, &height));
		CHECK(calls == face + 1 && live == 0 && atlas == &sentinel && width == 71 && height == 73);
	}
	failFace = -1;
	for (int face = 1; face < 6; ++face) {
		calls = 0; malformed = face;
		CHECK(!wired_sky_atlas_decode(&imports, "env/test", &atlas, &width, &height));
		CHECK(calls == face + 1 && live == 0 && atlas == &sentinel && width == 71 && height == 73);
	}
	malformed = -1; rejectAtlas = 1; calls = 0;
	CHECK(!wired_sky_atlas_decode(&imports, "env/test", &atlas, &width, &height));
	CHECK(calls == 1 && live == 0 && atlas == &sentinel);
	rejectAtlas = 0;
	const uint32_t invalid[][2] = {{0,2}, {3,0}, {UINT32_MAX,2}, {3,UINT32_MAX},
		{RENDER_SUBMISSION_MAX_IMAGE_DIMENSION,2},
		{RENDER_SUBMISSION_MAX_IMAGE_DIMENSION/3,RENDER_SUBMISSION_MAX_IMAGE_DIMENSION/2}};
	for (unsigned i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
		calls = 0; sourceWidth = invalid[i][0]; sourceHeight = invalid[i][1];
		CHECK(!wired_sky_atlas_decode(&imports, "env/test", &atlas, &width, &height));
		CHECK(calls == 1 && live == 0 && atlas == &sentinel && width == 71 && height == 73);
	}
	calls = 0;
	CHECK(!wired_sky_atlas_decode(NULL, "env/test", &atlas, &width, &height));
	CHECK(!wired_sky_atlas_decode(&imports, "", &atlas, &width, &height));
	CHECK(!wired_sky_atlas_decode(&imports, "env/test", NULL, &width, &height));
	CHECK(!wired_sky_atlas_decode(&imports, "env/test", &atlas, NULL, &height));
	CHECK(!wired_sky_atlas_decode(&imports, "env/test", &atlas, &width, NULL));
	char longName[MAX_QPATH + 1]; memset(longName, 'x', sizeof(longName)-1);
	longName[sizeof(longName)-1] = 0;
	CHECK(!wired_sky_atlas_decode(&imports, longName, &atlas, &width, &height));
	CHECK(calls == 0 && live == 0);
	puts("sky atlas: six asymmetric faces, 32 cycles, bounded live buffers, atomic failures PASS");
	return 0;
}
