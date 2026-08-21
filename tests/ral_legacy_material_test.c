// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_legacy_material.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; \
} } while (0)

typedef struct {
	const char *name;
	ralLegacyMaterialFacts_t facts;
	ralLegacyMaterialVariant_t variant;
	ralLegacyMaterialOutcome_t outcome;
	ralLegacyFallbackReason_t reason;
} materialCase_t;

static ralLegacyMaterialFacts_t BaseFacts( void ) {
	ralLegacyMaterialFacts_t facts;
	memset(&facts,0,sizeof(facts));
	facts.textureCount = 1u;
	return facts;
}

static ralLegacyMaterialVariant_t BaseVariant( void ) {
	ralLegacyMaterialVariant_t variant;
	memset(&variant,0,sizeof(variant));
	variant.entityStorageTransform = qtrue;
	variant.depthFragment = 0.85f;
	variant.textureDomainMask = 3u;
	variant.fixedColor = 0.25f;
	variant.fixedAlpha = 0.75f;
	variant.depthFadeScale = 2.0f;
	variant.lightmapSlot = 2u;
	return variant;
}

static int CheckCase( const materialCase_t *testCase ) {
	ralLegacyMaterialReceipt_t receipt;
	memset(&receipt,0xa5,sizeof(receipt));
	CHECK(Ral_LegacyMaterialResolve(&testCase->facts,&testCase->variant,&receipt));
	CHECK(receipt.ready && receipt.outcome == testCase->outcome
		&& receipt.fallbackReason == testCase->reason);
	CHECK(memcmp(&receipt.facts,&testCase->facts,sizeof(receipt.facts)) == 0);
	CHECK(memcmp(&receipt.variant,&testCase->variant,sizeof(receipt.variant)) == 0);
	return 0;
}

int main( void ) {
	materialCase_t cases[16];
	ralLegacyMaterialFacts_t facts = BaseFacts();
	ralLegacyMaterialVariant_t variant = BaseVariant();
	ralLegacyMaterialReceipt_t receipt, exact, before;
	uint32_t count = 0u, i;

#define ADD_CASE(label, expectedOutcome, expectedReason) do { \
	cases[count].name=(label);cases[count].facts=facts;cases[count].variant=variant; \
	cases[count].outcome=(expectedOutcome);cases[count].reason=(expectedReason);++count; \
} while (0)
	facts.textureCount=0u;variant.textureDomainMask=1u;variant.lightmapSlot=0u;
	ADD_CASE("opaque",RAL_LEGACY_MATERIAL_DIRECT,RAL_LEGACY_FALLBACK_NONE);
	facts=BaseFacts();variant=BaseVariant();
	ADD_CASE("lightmap",RAL_LEGACY_MATERIAL_DIRECT,RAL_LEGACY_FALLBACK_NONE);
	variant.combine=RAL_LEGACY_COMBINE_ADD_IDENTITY;ADD_CASE("additive",RAL_LEGACY_MATERIAL_DIRECT,RAL_LEGACY_FALLBACK_NONE);
	variant.combine=RAL_LEGACY_COMBINE_MIX_SOURCE_ALPHA;ADD_CASE("alpha-blend",RAL_LEGACY_MATERIAL_DIRECT,RAL_LEGACY_FALLBACK_NONE);
	variant=BaseVariant();variant.fixedColor=1.0f;variant.fixedAlpha=0.5f;ADD_CASE("fixed-color",RAL_LEGACY_MATERIAL_DIRECT,RAL_LEGACY_FALLBACK_NONE);
	facts.shaderFog=qtrue;variant.fogFactor=3u;ADD_CASE("fog",RAL_LEGACY_MATERIAL_DIRECT,RAL_LEGACY_FALLBACK_NONE);
	facts=BaseFacts();variant=BaseVariant();facts.environmentMapping=qtrue;ADD_CASE("environment",RAL_LEGACY_MATERIAL_DIRECT,RAL_LEGACY_FALLBACK_NONE);
	facts.textureCoordinateAnimation=qtrue;ADD_CASE("tcmod",RAL_LEGACY_MATERIAL_DIRECT,RAL_LEGACY_FALLBACK_NONE);
	facts.vertexDeform=qtrue;ADD_CASE("vertex-deform",RAL_LEGACY_MATERIAL_DIRECT,RAL_LEGACY_FALLBACK_NONE);
	facts=BaseFacts();variant=BaseVariant();variant.alphaTest=RAL_LEGACY_ALPHA_TEST_KEEP_GREATER_EQUAL;variant.alphaTestValue=0.5f;ADD_CASE("atest",RAL_LEGACY_MATERIAL_FALLBACK,RAL_LEGACY_FALLBACK_ALPHA_TEST);
	variant=BaseVariant();variant.alphaToCoverage=qtrue;ADD_CASE("alpha-to-coverage",RAL_LEGACY_MATERIAL_FALLBACK,RAL_LEGACY_FALLBACK_ALPHA_TO_COVERAGE);
	variant=BaseVariant();variant.discardMode=2u;ADD_CASE("discard",RAL_LEGACY_MATERIAL_FALLBACK,RAL_LEGACY_FALLBACK_DISCARD);
	variant=BaseVariant();variant.normalFormat=2u;ADD_CASE("normal-map",RAL_LEGACY_MATERIAL_FALLBACK,RAL_LEGACY_FALLBACK_NORMAL_MAP);
	variant=BaseVariant();variant.iblEnabled=qtrue;ADD_CASE("ibl",RAL_LEGACY_MATERIAL_FALLBACK,RAL_LEGACY_FALLBACK_IBL);
	variant=BaseVariant();variant.entityStorageTransform=qfalse;ADD_CASE("uniform-entity",RAL_LEGACY_MATERIAL_FALLBACK,RAL_LEGACY_FALLBACK_ENTITY_UNIFORM);
#undef ADD_CASE
	CHECK(count == 15u);
	for (i=0u;i<count;++i) { (void)cases[i].name; CHECK(CheckCase(&cases[i]) == 0); }

	facts=BaseFacts();variant=BaseVariant();
	CHECK(Ral_LegacyMaterialResolve(&facts,&variant,&receipt)); exact=receipt;
	CHECK(Ral_LegacyMaterialReceiptExact(&receipt,&exact));
	exact.variant.fixedAlpha=0.25f;CHECK(!Ral_LegacyMaterialReceiptExact(&receipt,&exact));exact=receipt;
	exact.facts.textureCoordinateAnimation=qtrue;CHECK(!Ral_LegacyMaterialReceiptExact(&receipt,&exact));exact=receipt;
	exact.outcome=RAL_LEGACY_MATERIAL_FALLBACK;CHECK(!Ral_LegacyMaterialReceiptExact(&receipt,&exact));exact=receipt;
	exact.fallbackReason=RAL_LEGACY_FALLBACK_IBL;CHECK(!Ral_LegacyMaterialReceiptExact(&receipt,&exact));

#define REJECT() do { memset(&receipt,0x5a,sizeof(receipt));before=receipt; \
	CHECK(!Ral_LegacyMaterialResolve(&facts,&variant,&receipt)); \
	CHECK(memcmp(&receipt,&before,sizeof(receipt)) == 0); } while (0)
	facts.textureCount=3u;REJECT();facts=BaseFacts();
	facts.shaderFog=(qboolean)2;REJECT();facts=BaseFacts();
	variant=BaseVariant();variant.textureDomainMask=4u;REJECT();
	variant=BaseVariant();variant.lightmapSlot=3u;REJECT();
	variant=BaseVariant();variant.combine=(ralLegacyCombine_t)8;REJECT();
	variant=BaseVariant();variant.fixedColor=NAN;REJECT();
	variant=BaseVariant();variant.depthFadeScale=0.0f;REJECT();
	facts.textureCount=0u;variant=BaseVariant();variant.textureDomainMask=1u;
	variant.lightmapSlot=1u;variant.combine=RAL_LEGACY_COMBINE_ADD_IDENTITY;REJECT();
	facts=BaseFacts();variant=BaseVariant();facts.shaderFog=qfalse;variant.fogFactor=1u;REJECT();
#undef REJECT
	puts("ral legacy material contract: PASS");
	return 0;
}
