// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_capability.h"
#include "ral_resource.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

int main( void ) {
	ralCapabilityFact_t facts[RAL_CAP_COUNT];
	ralCapabilityProfile_t profile;
	uint32_t i;
	memset(facts,0,sizeof(facts));
	for(i=0u;i<RAL_CAP_COUNT;++i) { facts[i].id=(ralCapabilityId_t)i;facts[i].nativeSupport=qtrue;facts[i].nativeLimit=8192u; }
	for(i=0u;i<RAL_CAP_COUNT;++i) facts[i].emulationSupport=qfalse;
	facts[RAL_CAP_BINDING_ARRAYS].nativeSupport=qfalse;facts[RAL_CAP_BINDING_ARRAYS].emulationSupport=qtrue;facts[RAL_CAP_BINDING_ARRAYS].emulationLimit=64u;
	facts[RAL_CAP_INLINE_DATA].nativeSupport=qfalse;facts[RAL_CAP_INLINE_DATA].emulationSupport=qtrue;facts[RAL_CAP_INLINE_DATA].emulationLimit=256u;
	facts[RAL_CAP_SUBMISSION_TIMELINE].nativeSupport=qfalse;facts[RAL_CAP_SUBMISSION_TIMELINE].emulationSupport=qtrue;facts[RAL_CAP_SUBMISSION_TIMELINE].emulationLimit=1u;
	facts[RAL_CAP_ASYNC_COMPUTE].nativeSupport=qfalse;facts[RAL_CAP_ASYNC_COMPUTE].emulationSupport=qtrue;facts[RAL_CAP_ASYNC_COMPUTE].emulationLimit=1u;
	facts[RAL_CAP_ASYNC_TRANSFER].nativeSupport=qfalse;facts[RAL_CAP_ASYNC_TRANSFER].emulationSupport=qtrue;facts[RAL_CAP_ASYNC_TRANSFER].emulationLimit=1u;
	facts[RAL_CAP_DRAW_INDIRECT_COUNT].nativeSupport=qfalse;facts[RAL_CAP_DRAW_INDIRECT_COUNT].nativeLimit=0u;
	facts[RAL_CAP_VARIABLE_RATE_SHADING].nativeSupport=qfalse;facts[RAL_CAP_VARIABLE_RATE_SHADING].nativeLimit=0u;
	facts[RAL_CAP_HDR10_PRESENTATION].nativeSupport=qfalse;facts[RAL_CAP_HDR10_PRESENTATION].nativeLimit=0u;
	facts[RAL_CAP_TEXTURE_COMPRESSION_BC].nativeSupport=qfalse;facts[RAL_CAP_TEXTURE_COMPRESSION_BC].emulationSupport=qtrue;facts[RAL_CAP_TEXTURE_COMPRESSION_BC].emulationLimit=1u;
	facts[RAL_CAP_TEXTURE_COMPRESSION_ASTC].nativeSupport=qfalse;facts[RAL_CAP_TEXTURE_COMPRESSION_ASTC].emulationSupport=qtrue;facts[RAL_CAP_TEXTURE_COMPRESSION_ASTC].emulationLimit=1u;
	facts[RAL_CAP_TEXTURE_COMPRESSION_ETC2].nativeSupport=qtrue;facts[RAL_CAP_TEXTURE_COMPRESSION_ETC2].nativeLimit=1u;
	CHECK(Ral_CapabilityProfileBuild(RAL_BACKEND_WEBGPU,1u,facts,RAL_CAP_COUNT,&profile));
	CHECK(profile.entries[RAL_CAP_BINDING_ARRAYS].outcome==RAL_CAP_OUTCOME_EMULATED);
	CHECK(profile.entries[RAL_CAP_INLINE_DATA].outcome==RAL_CAP_OUTCOME_EMULATED);
	CHECK(profile.entries[RAL_CAP_INLINE_DATA].limit==256u);
	CHECK(profile.entries[RAL_CAP_SUBMISSION_TIMELINE].outcome==RAL_CAP_OUTCOME_EMULATED);
	CHECK(profile.entries[RAL_CAP_ASYNC_COMPUTE].outcome==RAL_CAP_OUTCOME_EMULATED);
	CHECK(profile.entries[RAL_CAP_DRAW_INDIRECT_COUNT].outcome==RAL_CAP_OUTCOME_DISABLED);
	CHECK(profile.entries[RAL_CAP_TEXTURE_COMPRESSION_BC].outcome==RAL_CAP_OUTCOME_EMULATED);
	CHECK(profile.entries[RAL_CAP_TEXTURE_COMPRESSION_ETC2].outcome==RAL_CAP_OUTCOME_NATIVE);
	{
		// WebGPU keeps filterability and blendability as independent per-format
		// capabilities; the shared RAL vocabulary must retain that distinction.
		const ralTextureFormatFeatures_t rgba16f =
			RAL_TEXTURE_FORMAT_FEATURE_SAMPLED
			| RAL_TEXTURE_FORMAT_FEATURE_FILTER_LINEAR
			| RAL_TEXTURE_FORMAT_FEATURE_COLOR_ATTACHMENT
			| RAL_TEXTURE_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND;
		CHECK( ( rgba16f & RAL_TEXTURE_FORMAT_FEATURE_SAMPLED ) != 0u );
		CHECK( ( rgba16f & RAL_TEXTURE_FORMAT_FEATURE_FILTER_LINEAR ) != 0u );
		CHECK( ( rgba16f & RAL_TEXTURE_FORMAT_FEATURE_COLOR_ATTACHMENT ) != 0u );
		CHECK( ( rgba16f & RAL_TEXTURE_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND ) != 0u );
		CHECK( ( rgba16f & RAL_TEXTURE_FORMAT_FEATURE_STORAGE ) == 0u );
		CHECK( ( RAL_TEXTURE_FORMAT_FEATURE_ALL & ( 1u << 31 ) ) == 0u );
	}
	puts("ral WebGPU capability profile: PASS");
	return 0;
}
