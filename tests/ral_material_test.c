// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;} } while(0)

static int Key( uint64_t generation, ralTextureAssetColorEncoding_t encoding,
		ralTextureChannelSemantic_t semantic, ralTextureCacheKey_t *out ) {
	ralTextureAssetRequest_t q; ralTextureAssetReceipt_t r;
	memset(&q,0,sizeof(q));q.schemaVersion=RAL_TEXTURE_ASSET_SCHEMA_VERSION;
	q.assetGeneration=generation;q.provenanceHash=generation*101u;
	q.dimension=RAL_TEXTURE_ASSET_2D;q.width=q.height=8u;q.depth=q.layers=1u;
	q.sourceMipLevels=1u;q.colorEncoding=encoding;q.channelSemantic=semantic;
	q.sourceEncoding=RAL_TEXTURE_SOURCE_UNCOMPRESSED;q.mipPolicy=RAL_TEXTURE_MIPS_SOURCE;
	q.residency=RAL_TEXTURE_RESIDENCY_STREAMED;q.preferenceCount=1u;
	q.preferences[0]=RAL_TEXTURE_COMPRESSION_UNCOMPRESSED;q.allowUncompressedFallback=qtrue;
	return Ral_ResolveTextureAsset(&q,&r)&&Ral_TextureCacheKeyBuild(&r,out);
}

static ralMaterialDescription_t Base( ralMaterialSource_t source ) {
	ralMaterialDescription_t d;memset(&d,0,sizeof(d));
	d.schemaVersion=RAL_MATERIAL_SCHEMA_VERSION;d.materialGeneration=7u;d.provenanceHash=77u;
	d.source=source;d.factors.baseColor[0]=d.factors.baseColor[1]=1.0f;
	d.factors.baseColor[2]=d.factors.baseColor[3]=1.0f;d.factors.metallic=0.1f;
	d.factors.roughness=0.7f;d.factors.normalScale=1.0f;d.factors.occlusionStrength=1.0f;
	d.renderPolicy.depthTest=qtrue;d.renderPolicy.depthWrite=qtrue;return d;
}

static int Pbr( ralMaterialDescription_t *d ) {
	*d=Base(RAL_MATERIAL_SOURCE_PBR);d->textureCount=3u;
	d->textures[0].role=RAL_MATERIAL_TEXTURE_BASE_COLOR;d->textures[0].channels=RAL_MATERIAL_CHANNEL_RGBA;
	d->textures[1].role=RAL_MATERIAL_TEXTURE_NORMAL;d->textures[1].channels=RAL_MATERIAL_CHANNEL_XY_NORMAL;
	d->textures[2].role=RAL_MATERIAL_TEXTURE_ORM;d->textures[2].channels=RAL_MATERIAL_CHANNEL_RGB_ORM;
	return Key(1u,RAL_TEXTURE_ENCODING_SRGB,RAL_TEXTURE_CHANNEL_COLOR,&d->textures[0].texture)
		&&Key(2u,RAL_TEXTURE_ENCODING_LINEAR,RAL_TEXTURE_CHANNEL_NORMAL_XY,&d->textures[1].texture)
		&&Key(3u,RAL_TEXTURE_ENCODING_LINEAR,RAL_TEXTURE_CHANNEL_ORM,&d->textures[2].texture);
}

int main(void){
	ralMaterialDescription_t d,bad,gltf,q3,alternate;
	ralMaterialReceipt_t receipt,exact,before,second;
	CHECK(Pbr(&d));memset(&receipt,0xA5,sizeof(receipt));
	CHECK(Ral_MaterialCompile(&d,&receipt)&&Ral_MaterialReceiptValid(&receipt));
	exact=receipt;CHECK(Ral_MaterialReceiptExact(&receipt,&exact));
	bad=d;bad.textures[0].texture.keyHash++;before=receipt;
	CHECK(!Ral_MaterialCompile(&bad,&receipt)&&!memcmp(&receipt,&before,sizeof(receipt)));
	bad=d;{ralMaterialTexture_t t=bad.textures[0];bad.textures[0]=bad.textures[1];bad.textures[1]=t;}
	CHECK(!Ral_MaterialCompile(&bad,&receipt));
	bad=d;bad.textures[1].channels=RAL_MATERIAL_CHANNEL_RGBA;CHECK(!Ral_MaterialCompile(&bad,&receipt));
	bad=d;bad.textureCount=4u;bad.textures[3].role=RAL_MATERIAL_TEXTURE_OCCLUSION;
	bad.textures[3].channels=RAL_MATERIAL_CHANNEL_R_OCCLUSION;
	CHECK(Key(4u,RAL_TEXTURE_ENCODING_LINEAR,RAL_TEXTURE_CHANNEL_DATA,&bad.textures[3].texture));
	CHECK(!Ral_MaterialCompile(&bad,&receipt));
	bad=d;bad.factors.roughness=NAN;CHECK(!Ral_MaterialCompile(&bad,&receipt));
	bad=d;bad.renderPolicy.alphaMode=RAL_MATERIAL_ALPHA_MASK;bad.renderPolicy.alphaCutoff=0.5f;
	CHECK(Ral_MaterialCompile(&bad,&second)&&second.variantKey!=before.variantKey);

	/* glTF keeps occlusion R and metallic-roughness GB explicit and ordered. */
	gltf=Base(RAL_MATERIAL_SOURCE_GLTF);gltf.textureCount=3u;
	gltf.textures[0]=d.textures[0];gltf.textures[1].role=RAL_MATERIAL_TEXTURE_OCCLUSION;
	gltf.textures[1].channels=RAL_MATERIAL_CHANNEL_R_OCCLUSION;
	gltf.textures[2].role=RAL_MATERIAL_TEXTURE_METALLIC_ROUGHNESS;
	gltf.textures[2].channels=RAL_MATERIAL_CHANNEL_GB_ROUGHNESS_METALLIC;
	CHECK(Key(5u,RAL_TEXTURE_ENCODING_LINEAR,RAL_TEXTURE_CHANNEL_DATA,&gltf.textures[1].texture));
	CHECK(Key(6u,RAL_TEXTURE_ENCODING_LINEAR,RAL_TEXTURE_CHANNEL_ORM,&gltf.textures[2].texture));
	CHECK(Ral_MaterialCompile(&gltf,&second));

	q3=Base(RAL_MATERIAL_SOURCE_Q3);q3.textureCount=1u;
	q3.textures[0].role=RAL_MATERIAL_TEXTURE_LEGACY_STAGE0;
	q3.textures[0].channels=RAL_MATERIAL_CHANNEL_LEGACY_RGBA;
	CHECK(Key(7u,RAL_TEXTURE_ENCODING_SRGB,RAL_TEXTURE_CHANNEL_COLOR,&q3.textures[0].texture));
	CHECK(Ral_MaterialCompile(&q3,&second));

	/* Asset identity changes artifact identity, not shader variant semantics. */
	alternate=d;CHECK(Key(11u,RAL_TEXTURE_ENCODING_SRGB,RAL_TEXTURE_CHANNEL_COLOR,
		&alternate.textures[0].texture));
	CHECK(Ral_MaterialCompile(&alternate,&second)&&second.variantKey==before.variantKey
		&&second.artifactHash!=before.artifactHash);
	bad=d;bad.factors.emissive[0]=-0.0f;
	CHECK(Ral_MaterialCompile(&bad,&second)&&second.variantKey==before.variantKey);
	puts("ral_material_test: ok");return 0;
}
