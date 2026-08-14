// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_temporal_generic_catalog.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK failed: %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

#define VK_TEMPORAL_BLOB(name, size) const unsigned char name[size] = { 0 };
#define VK_TEMPORAL_PAIR(tx, family, env, fog, ordinaryVS, ordinaryFS, temporalVS, writeFS, invalidateFS)
#include "../code/renderervk/shaders/spirv/temporal_generic_catalog.inc"
#undef VK_TEMPORAL_PAIR
#undef VK_TEMPORAL_BLOB

static qboolean Expected( uint32_t tx, vkTemporalGenericFamily_t family ) {
	if ( tx == 0u ) return family <= VK_TEMPORAL_GENERIC_ENT ? qtrue : qfalse;
	if ( tx == 1u ) return family == VK_TEMPORAL_GENERIC_PLAIN || family == VK_TEMPORAL_GENERIC_IDENT
		|| family == VK_TEMPORAL_GENERIC_FIXED || family == VK_TEMPORAL_GENERIC_CL;
	if ( tx == 2u ) return family == VK_TEMPORAL_GENERIC_PLAIN || family == VK_TEMPORAL_GENERIC_CL;
	return qfalse;
}

static qboolean BlobGood( vkTemporalShaderBlob_t b ) {
	return b.bytes && b.size && !(b.size & 3u);
}

int main( void ) {
	vkTemporalGenericKey_t key;
	vkTemporalGenericCatalogEntry_t out, before, fixed, ent, env0, env1, fog0, fog1;
	uint32_t tx, family, env, fog, accepted = 0;
	for ( tx = 0; tx < 3; ++tx ) for ( family = 0; family < 5; ++family )
		for ( env = 0; env < 2; ++env ) for ( fog = 0; fog < 2; ++fog ) {
			qboolean expected = Expected( tx, (vkTemporalGenericFamily_t)family );
			key.textureCount=tx; key.family=(vkTemporalGenericFamily_t)family;
			key.environment=(qboolean)env; key.shaderFog=(qboolean)fog;
			memset(&out,0xA5,sizeof(out)); before=out;
			CHECK( VK_TemporalGenericCatalogSelect(&key,&out) == expected );
			if ( !expected ) { CHECK(memcmp(&out,&before,sizeof(out))==0); continue; }
			accepted++;
			CHECK(memcmp(&out.key,&key,sizeof(key))==0);
			CHECK(BlobGood(out.ordinaryVertex) && BlobGood(out.ordinaryFragment));
			CHECK(BlobGood(out.temporalVertex) && BlobGood(out.temporalWriteFragment));
			CHECK(BlobGood(out.temporalInvalidateFragment));
			CHECK(out.temporalWriteFragment.bytes != out.temporalInvalidateFragment.bytes);
		}
	CHECK(accepted == 40u);

	key=(vkTemporalGenericKey_t){0,VK_TEMPORAL_GENERIC_FIXED,qfalse,qfalse};
	CHECK(VK_TemporalGenericCatalogSelect(&key,&fixed));
	key.family=VK_TEMPORAL_GENERIC_ENT; CHECK(VK_TemporalGenericCatalogSelect(&key,&ent));
	CHECK(fixed.ordinaryVertex.bytes==ent.ordinaryVertex.bytes && fixed.ordinaryVertex.size==ent.ordinaryVertex.size);
	CHECK(fixed.temporalVertex.bytes==ent.temporalVertex.bytes && fixed.temporalVertex.size==ent.temporalVertex.size);
	CHECK(fixed.ordinaryFragment.bytes!=ent.ordinaryFragment.bytes);

	key=(vkTemporalGenericKey_t){1,VK_TEMPORAL_GENERIC_PLAIN,qfalse,qfalse};
	CHECK(VK_TemporalGenericCatalogSelect(&key,&env0)); key.environment=qtrue;
	CHECK(VK_TemporalGenericCatalogSelect(&key,&env1));
	CHECK(env0.ordinaryVertex.bytes!=env1.ordinaryVertex.bytes && env0.temporalVertex.bytes!=env1.temporalVertex.bytes);
	CHECK(env0.ordinaryFragment.bytes==env1.ordinaryFragment.bytes);
	CHECK(env0.temporalWriteFragment.bytes==env1.temporalWriteFragment.bytes);
	key.environment=qfalse; CHECK(VK_TemporalGenericCatalogSelect(&key,&fog0)); key.shaderFog=qtrue;
	CHECK(VK_TemporalGenericCatalogSelect(&key,&fog1));
	CHECK(fog0.ordinaryVertex.bytes!=fog1.ordinaryVertex.bytes);
	CHECK(fog0.ordinaryFragment.bytes!=fog1.ordinaryFragment.bytes);

	memset(&out,0x5A,sizeof(out)); before=out; key.textureCount=3;
	CHECK(!VK_TemporalGenericCatalogSelect(&key,&out) && memcmp(&out,&before,sizeof(out))==0);
	key.textureCount=0; key.family=(vkTemporalGenericFamily_t)99;
	CHECK(!VK_TemporalGenericCatalogSelect(&key,&out) && memcmp(&out,&before,sizeof(out))==0);
	CHECK(!VK_TemporalGenericCatalogSelect(NULL,&out)); CHECK(!VK_TemporalGenericCatalogSelect(&key,NULL));
	puts("vk temporal generic catalog contract: PASS");
	return 0;
}
