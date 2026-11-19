// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "../code/render/frontend/render_material_stage_gpu.h"
#include <limits.h>
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(void) {
	renderMaterialSnapshot_t material = {0};
	renderSkyMaterialGpuParams_t packet;
	qhandle_t handles[RENDER_MATERIAL_MAX_STAGES];
	material.ready = qtrue; material.sky = qtrue; material.skyBox = qtrue;
	material.handle = 17; material.skyCloudHeight = 256;
	material.stageCount = RENDER_MATERIAL_MAX_STAGES;
	for (unsigned i=0; i<RENDER_MATERIAL_MAX_STAGES; ++i) {
		renderMaterialStageSnapshot_t *s = &material.stages[i];
		s->material = 100+(int)i;
		s->scaleScroll[0]=1+i; s->scaleScroll[1]=2+i;
		s->tcModCount=1; s->tcMods[0].type=1;
		s->tcMods[0].parameters[0]=.25f+i;
	}
	CHECK(wired_sky_material_pack(&material, 37, 1250, &packet, handles));
	CHECK(packet.meta[0]==8 && packet.meta[1]==1);
	CHECK(packet.timeHeight[0]==1.25f && packet.timeHeight[1]==256);
	for (unsigned i=0; i<RENDER_MATERIAL_MAX_STAGES; ++i) {
		CHECK(handles[i]==100+(int)i);
		CHECK(packet.stages[i].scaleScroll[0]==1+i);
		CHECK(packet.stages[i].tcMods[0].parameters[0]==.25f+i);
	}
	// Never replace $lightmap with the owner, but $whiteimage is not sampled.
	material.stages[1].imageSource=1;
	material.stages[2].imageSource=2; material.stages[2].material=0;
	CHECK(wired_sky_material_pack(&material, 37, 1250, &packet, handles));
	CHECK(handles[1]==37 && handles[2]==17);
	CHECK(!wired_sky_material_pack(&material, 0, 1250, &packet, handles));
	material.stages[1].imageSource=0;
	renderMaterialStageSnapshot_t *last=&material.stages[7];
	last->animationFrameCount=3; last->animationFrequency=2;
	last->animationMaterials[0]=201; last->animationMaterials[1]=202;
	last->animationMaterials[2]=203; last->animationLooping=qtrue;
	const struct { int time, looping, clamped; } cases[] = {
		{-1000,201,201},{0,201,201},{499,201,201},{500,202,202},
		{1000,203,203},{1500,201,203},{2000,202,203},{2000,202,203},
		{500,202,202}
	};
	for (unsigned i=0; i<sizeof(cases)/sizeof(cases[0]); ++i) {
		last->animationLooping=qtrue;
		CHECK(wired_sky_material_pack(&material, 0, cases[i].time, &packet, handles));
		CHECK(handles[7]==cases[i].looping);
		last->animationLooping=qfalse;
		CHECK(wired_sky_material_pack(&material, 0, cases[i].time, &packet, handles));
		CHECK(handles[7]==cases[i].clamped);
	}
	// Large clocks/rates must reduce in double before any integer conversion.
	last->animationFrequency=1e20f; last->animationLooping=qtrue;
	CHECK(wired_sky_material_pack(&material, 0, INT_MAX, &packet, handles));
	CHECK(handles[7]>=201 && handles[7]<=203);
	last->animationLooping=qfalse;
	CHECK(wired_sky_material_pack(&material, 0, INT_MAX, &packet, handles));
	CHECK(handles[7]==203);
	last->animationFrequency=NAN;
	CHECK(!wired_sky_material_pack(&material, 0, 0, &packet, handles));
	last->animationFrequency=2; last->animationFrameCount=RENDER_MATERIAL_MAX_ANIMATION_FRAMES+1;
	CHECK(!wired_sky_material_pack(&material, 0, 0, &packet, handles));
	last->animationFrameCount=0; last->tcModCount=RAL_MATERIAL_SOURCE_MAX_TCMODS+1;
	CHECK(!wired_sky_material_pack(&material, 0, 0, &packet, handles));
	last->tcModCount=0; last->imageSource=3;
	CHECK(!wired_sky_material_pack(&material, 0, 0, &packet, handles));
	material.stageCount=RENDER_MATERIAL_MAX_STAGES+1;
	CHECK(!wired_sky_material_pack(&material, 0, 0, &packet, handles));
	material.stageCount=0;
	memset(&packet, 0xa5, sizeof(packet));
	CHECK(wired_sky_material_pack(&material, 0, 0, &packet, handles));
	const renderMaterialStageGpuParams_t empty={0};
	for (unsigned i=0; i<RENDER_MATERIAL_MAX_STAGES; ++i) {
		CHECK(handles[i]==17);
		CHECK(!memcmp(&packet.stages[i], &empty, sizeof(empty)));
	}
	material.sky=qfalse;
	CHECK(!wired_sky_material_pack(&material, 0, 0, &packet, handles));
	puts("shared sky packet: eight stages, semantic handles, pause/rewind/loop/clamp, invalid bounds PASS");
	return 0;
}
