// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_temporal_generic_catalog.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK failed: %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

#define VK_TEMPORAL_BLOB(name, size) const unsigned char name[size] = { 0 };
#define VK_TEMPORAL_PAIR(tx, family, env, fog, shadow, ordinaryVS, ordinaryFS, temporalVS, writeFS, invalidateFS)
#include "../code/render/ral/backends/vulkan/renderer/shaders/spirv/temporal_generic_catalog.inc"
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
			key.environment=(qboolean)env; key.shaderFog=(qboolean)fog; key.shadowReceiver=qfalse;
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
	// Sun-shadow receiver cohort: no environment variant; tx0 has plain/ident/fixed, tx1 plain/cl/ident/fixed, tx2 plain/cl.
	accepted = 0;
	for ( tx = 0; tx < 3; ++tx ) for ( family = 0; family < 5; ++family )
		for ( env = 0; env < 2; ++env ) for ( fog = 0; fog < 2; ++fog ) {
			qboolean expected = env == 0 && family != VK_TEMPORAL_GENERIC_ENT
				&& ( tx == 0 ? family != VK_TEMPORAL_GENERIC_CL
					: tx == 1 ? qtrue
					: ( family == VK_TEMPORAL_GENERIC_PLAIN || family == VK_TEMPORAL_GENERIC_CL ) );
			key.textureCount=tx; key.family=(vkTemporalGenericFamily_t)family;
			key.environment=(qboolean)env; key.shaderFog=(qboolean)fog; key.shadowReceiver=qtrue;
			memset(&out,0xA5,sizeof(out)); before=out;
			CHECK( VK_TemporalGenericCatalogSelect(&key,&out) == expected );
			if ( !expected ) { CHECK(memcmp(&out,&before,sizeof(out))==0); continue; }
			accepted++;
			CHECK(memcmp(&out.key,&key,sizeof(key))==0);
			CHECK(BlobGood(out.ordinaryVertex) && BlobGood(out.ordinaryFragment));
			CHECK(BlobGood(out.temporalVertex) && BlobGood(out.temporalWriteFragment) && BlobGood(out.temporalInvalidateFragment));
			CHECK(out.temporalWriteFragment.bytes != out.temporalInvalidateFragment.bytes);
		}
	CHECK(accepted == 18u);
	// A shadow receiver is a different pipeline from its plain sibling.
	{
		vkTemporalGenericCatalogEntry_t plain, shadow; vkTemporalGenericKey_t a2;
		memset(&a2,0,sizeof(a2)); a2.textureCount=1; a2.family=VK_TEMPORAL_GENERIC_IDENT;
		CHECK(VK_TemporalGenericCatalogSelect(&a2,&plain)); a2.shadowReceiver=qtrue;
		CHECK(VK_TemporalGenericCatalogSelect(&a2,&shadow));
		CHECK(plain.ordinaryVertex.bytes!=shadow.ordinaryVertex.bytes && plain.ordinaryFragment.bytes!=shadow.ordinaryFragment.bytes);
		CHECK(plain.temporalVertex.bytes!=shadow.temporalVertex.bytes && plain.temporalWriteFragment.bytes!=shadow.temporalWriteFragment.bytes);
	}

	// Quake 1 lightstyle cohort (TASK-270): LS and LSA exist only as tx0 / no env / no fog / no shadow; they share the ordinary
	// and the temporal vertex (q1_ls.vert) and differ in the fragments (q1_ls.frag / q1_ls_array.frag).
	{
		vkTemporalGenericCatalogEntry_t ls, lsa, rejected; vkTemporalGenericKey_t k2;
		uint32_t f2;
		for ( f2 = VK_TEMPORAL_GENERIC_LS; f2 <= VK_TEMPORAL_GENERIC_LSA; ++f2 ) for ( tx = 0; tx < 3; ++tx )
			for ( env = 0; env < 2; ++env ) for ( fog = 0; fog < 2; ++fog ) {
				uint32_t shadow;
				for ( shadow = 0; shadow < 2; ++shadow ) {
					qboolean expected = tx == 0 && env == 0 && fog == 0 && shadow == 0 ? qtrue : qfalse;
					memset(&k2,0,sizeof(k2)); k2.textureCount=tx; k2.family=(vkTemporalGenericFamily_t)f2;
					k2.environment=(qboolean)env; k2.shaderFog=(qboolean)fog; k2.shadowReceiver=(qboolean)shadow;
					CHECK( VK_TemporalGenericCatalogSelect(&k2,&rejected) == expected );
				}
			}
		memset(&k2,0,sizeof(k2)); k2.family=VK_TEMPORAL_GENERIC_LS; CHECK(VK_TemporalGenericCatalogSelect(&k2,&ls));
		k2.family=VK_TEMPORAL_GENERIC_LSA; CHECK(VK_TemporalGenericCatalogSelect(&k2,&lsa));
		CHECK(BlobGood(ls.ordinaryVertex) && BlobGood(ls.ordinaryFragment) && BlobGood(ls.temporalVertex));
		CHECK(BlobGood(ls.temporalWriteFragment) && BlobGood(ls.temporalInvalidateFragment) && BlobGood(lsa.temporalWriteFragment));
		CHECK(ls.ordinaryVertex.bytes==lsa.ordinaryVertex.bytes && ls.temporalVertex.bytes==lsa.temporalVertex.bytes);
		CHECK(ls.ordinaryFragment.bytes!=lsa.ordinaryFragment.bytes && ls.temporalWriteFragment.bytes!=lsa.temporalWriteFragment.bytes);
		CHECK(ls.temporalWriteFragment.bytes!=ls.temporalInvalidateFragment.bytes);
		k2.family=(vkTemporalGenericFamily_t)( VK_TEMPORAL_GENERIC_LSA + 1 ); CHECK(!VK_TemporalGenericCatalogSelect(&k2,&rejected));
	}

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

	// The inverse resolver is generation-bound and proves a unique exact
	// pointer+size match for both ordinary stages. Byte-equal impostors, a
	// duplicate record, an incomplete generation or a stale size fail closed.
	key=(vkTemporalGenericKey_t){1,VK_TEMPORAL_GENERIC_PLAIN,qfalse,qfalse};
	CHECK(VK_TemporalGenericCatalogSelect(&key,&out));
	{
		vkTemporalShaderModuleRecord_t records[3];
		vkTemporalShaderModuleRegistryView_t registry;
		VkShaderModule vs=(VkShaderModule)(uintptr_t)0x101;
		VkShaderModule fs=(VkShaderModule)(uintptr_t)0x202;
		VkShaderModule gotVs=(VkShaderModule)(uintptr_t)0xA5;
		VkShaderModule gotFs=(VkShaderModule)(uintptr_t)0x5A;
		uint32_t generation=0xDEADBEEFu;
		memset(records,0,sizeof(records));memset(&registry,0,sizeof(registry));
		records[0].module=vs;records[0].blob=out.ordinaryVertex;
		records[1].module=fs;records[1].blob=out.ordinaryFragment;
		registry.records=records;registry.count=2;registry.generation=7;
		registry.ready=qtrue;registry.complete=qtrue;
		CHECK(VK_TemporalGenericCatalogResolveOrdinaryModules(&out,&registry,
			&gotVs,&gotFs,&generation));
		CHECK(gotVs==vs&&gotFs==fs&&generation==7u);
		registry.ready=qfalse;gotVs=(VkShaderModule)(uintptr_t)0xA5;
		gotFs=(VkShaderModule)(uintptr_t)0x5A;generation=0xDEADBEEFu;
		CHECK(!VK_TemporalGenericCatalogResolveOrdinaryModules(&out,&registry,
			&gotVs,&gotFs,&generation));
		CHECK(gotVs==(VkShaderModule)(uintptr_t)0xA5
			&&gotFs==(VkShaderModule)(uintptr_t)0x5A&&generation==0xDEADBEEFu);
		registry.ready=qtrue;registry.complete=qfalse;
		CHECK(!VK_TemporalGenericCatalogResolveOrdinaryModules(&out,&registry,
			&gotVs,&gotFs,&generation));
		registry.complete=qtrue;records[1].blob.size-=4u;
		CHECK(!VK_TemporalGenericCatalogResolveOrdinaryModules(&out,&registry,
			&gotVs,&gotFs,&generation));
		records[1].blob=out.ordinaryFragment;
		records[2]=records[0];records[2].module=(VkShaderModule)(uintptr_t)0x303;
		registry.count=3;
		CHECK(!VK_TemporalGenericCatalogResolveOrdinaryModules(&out,&registry,
			&gotVs,&gotFs,&generation));
	}
	puts("vk temporal generic catalog contract: PASS");
	return 0;
}
