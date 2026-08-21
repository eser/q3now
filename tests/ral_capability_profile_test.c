// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_capability.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

static void NativeFacts( ralCapabilityFact_t *facts ) {
	uint32_t i;
	memset(facts,0,sizeof(*facts)*RAL_CAP_COUNT);
	for(i=0u;i<RAL_CAP_COUNT;++i) {
		facts[i].id=(ralCapabilityId_t)i; facts[i].nativeSupport=qtrue;
		facts[i].nativeLimit=8192u;
	}
}

int main( void ) {
	ralCapabilityFact_t facts[RAL_CAP_COUNT];
	ralCapabilityProfile_t profile, exact, before;
	ralCaps_t caps;

	NativeFacts(facts);
	CHECK(Ral_CapabilityProfileBuild(RAL_BACKEND_VULKAN,1u,facts,RAL_CAP_COUNT,&profile));
	CHECK(profile.schemaVersion==1u&&profile.entryCount==RAL_CAP_COUNT&&profile.ready);
	CHECK(profile.entries[RAL_CAP_DYNAMIC_RENDERING].requirement==RAL_CAP_REQUIREMENT_REQUIRED);
	CHECK(profile.entries[RAL_CAP_VARIABLE_RATE_SHADING].requirement==RAL_CAP_REQUIREMENT_OPTIONAL);
	CHECK(profile.entries[RAL_CAP_TEXTURE_COMPRESSION_BC].outcome==RAL_CAP_OUTCOME_NATIVE);
	NativeFacts(facts);
	facts[RAL_CAP_INLINE_DATA].nativeLimit=64u;
	facts[RAL_CAP_INLINE_DATA].emulationSupport=qtrue;
	facts[RAL_CAP_INLINE_DATA].emulationLimit=256u;
	CHECK(Ral_CapabilityProfileBuild(RAL_BACKEND_VULKAN,1u,facts,RAL_CAP_COUNT,&profile));
	CHECK(profile.entries[RAL_CAP_INLINE_DATA].outcome==RAL_CAP_OUTCOME_EMULATED);
	CHECK(profile.entries[RAL_CAP_INLINE_DATA].limit==256u);
	exact=profile;CHECK(Ral_CapabilityProfileExact(&profile,&exact));
	exact.entries[RAL_CAP_HDR10_PRESENTATION].outcome=RAL_CAP_OUTCOME_DISABLED;
	CHECK(!Ral_CapabilityProfileExact(&profile,&exact));
	exact=profile;exact.generation=2u;CHECK(!Ral_CapabilityProfileExact(&profile,&exact));
	exact=profile;exact.entries[0].id=RAL_CAP_GRAPHICS_QUEUE;CHECK(!Ral_CapabilityProfileExact(&exact,&exact));

#define REJECT_BUILD() do { memset(&profile,0x5a,sizeof(profile));before=profile; \
	CHECK(!Ral_CapabilityProfileBuild(RAL_BACKEND_VULKAN,1u,facts,RAL_CAP_COUNT,&profile)); \
	CHECK(memcmp(&profile,&before,sizeof(profile))==0); } while(0)
	facts[RAL_CAP_DYNAMIC_RENDERING].nativeSupport=qfalse;REJECT_BUILD();NativeFacts(facts);
	facts[RAL_CAP_DYNAMIC_RENDERING].nativeSupport=qfalse;
	facts[RAL_CAP_DYNAMIC_RENDERING].emulationSupport=qtrue;
	facts[RAL_CAP_DYNAMIC_RENDERING].emulationLimit=1u;REJECT_BUILD();NativeFacts(facts);
	facts[RAL_CAP_MAX_COLOR_ATTACHMENTS].nativeLimit=3u;REJECT_BUILD();NativeFacts(facts);
	facts[RAL_CAP_GRAPHICS_QUEUE].id=RAL_CAP_BIND_GROUPS;REJECT_BUILD();NativeFacts(facts);
	facts[RAL_CAP_BIND_GROUPS].nativeSupport=(qboolean)2;REJECT_BUILD();NativeFacts(facts);
	memset(&profile,0x5a,sizeof(profile));before=profile;
	CHECK(!Ral_CapabilityProfileBuild(RAL_BACKEND_VULKAN,1u,facts,RAL_CAP_COUNT-1u,&profile));
	CHECK(memcmp(&profile,&before,sizeof(profile))==0);
	CHECK(!Ral_CapabilityProfileBuild(RAL_BACKEND_VULKAN,0u,facts,RAL_CAP_COUNT,&profile));
	CHECK(!Ral_CapabilityProfileBuild(RAL_BACKEND_VULKAN,UINT64_MAX,facts,RAL_CAP_COUNT,&profile));
#undef REJECT_BUILD

	memset(&caps,0,sizeof(caps));
	caps.dynamicRendering=qtrue;caps.maxColorAttachments=8u;caps.maxTextureDimension2D=16384u;
	caps.maxPushConstantSize=128u;caps.bindlessTextures=qtrue;caps.maxBindlessTextures=4096u;
	caps.timelineSemaphores=qtrue;caps.drawIndirectCount=qtrue;caps.textureCompressionBC=qtrue;
	CHECK(Ral_CapabilityProfileFromCaps(RAL_BACKEND_VULKAN,&caps,7u,&profile));
	CHECK(profile.entries[RAL_CAP_INLINE_DATA].outcome==RAL_CAP_OUTCOME_NATIVE);
	CHECK(profile.entries[RAL_CAP_INLINE_DATA].limit==128u);
	CHECK(profile.entries[RAL_CAP_ASYNC_COMPUTE].outcome==RAL_CAP_OUTCOME_EMULATED);
	CHECK(profile.entries[RAL_CAP_TEXTURE_COMPRESSION_BC].outcome==RAL_CAP_OUTCOME_NATIVE);
	CHECK(profile.entries[RAL_CAP_TEXTURE_COMPRESSION_ASTC].outcome==RAL_CAP_OUTCOME_EMULATED);
	CHECK(profile.entries[RAL_CAP_VARIABLE_RATE_SHADING].outcome==RAL_CAP_OUTCOME_DISABLED);
	puts("ral capability profile: PASS");
	return 0;
}
