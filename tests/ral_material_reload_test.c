// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material_reload.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)

static int Key( uint64_t generation, ralTextureAssetColorEncoding_t encoding,
		ralTextureChannelSemantic_t semantic, ralTextureCacheKey_t *out ) {
	ralTextureAssetRequest_t q; ralTextureAssetReceipt_t r; memset( &q, 0, sizeof( q ) );
	q.schemaVersion=RAL_TEXTURE_ASSET_SCHEMA_VERSION;q.assetGeneration=generation;q.provenanceHash=generation*103u;
	q.dimension=RAL_TEXTURE_ASSET_2D;q.width=q.height=8u;q.depth=q.layers=1u;q.sourceMipLevels=1u;
	q.colorEncoding=encoding;q.channelSemantic=semantic;q.sourceEncoding=RAL_TEXTURE_SOURCE_UNCOMPRESSED;
	q.mipPolicy=RAL_TEXTURE_MIPS_SOURCE;q.residency=RAL_TEXTURE_RESIDENCY_STREAMED;q.preferenceCount=1u;
	q.preferences[0]=RAL_TEXTURE_COMPRESSION_UNCOMPRESSED;q.allowUncompressedFallback=qtrue;
	return Ral_ResolveTextureAsset(&q,&r)&&Ral_TextureCacheKeyBuild(&r,out);
}

static int Description( uint64_t generation, ralMaterialDescription_t *d ) {
	memset(d,0,sizeof(*d));d->schemaVersion=RAL_MATERIAL_SCHEMA_VERSION;d->materialGeneration=generation;
	d->provenanceHash=71u;d->source=RAL_MATERIAL_SOURCE_PBR;d->textureCount=2u;
	d->textures[0].role=RAL_MATERIAL_TEXTURE_BASE_COLOR;d->textures[0].channels=RAL_MATERIAL_CHANNEL_RGBA;
	d->textures[1].role=RAL_MATERIAL_TEXTURE_NORMAL;d->textures[1].channels=RAL_MATERIAL_CHANNEL_XY_NORMAL;
	d->factors.baseColor[0]=d->factors.baseColor[1]=d->factors.baseColor[2]=d->factors.baseColor[3]=1.0f;
	d->factors.roughness=d->factors.normalScale=d->factors.occlusionStrength=1.0f;
	d->renderPolicy.depthTest=d->renderPolicy.depthWrite=qtrue;
	return Key(1u,RAL_TEXTURE_ENCODING_SRGB,RAL_TEXTURE_CHANNEL_COLOR,&d->textures[0].texture)
		&&Key(2u,RAL_TEXTURE_ENCODING_LINEAR,RAL_TEXTURE_CHANNEL_NORMAL_XY,&d->textures[1].texture);
}

static ralTextureResourceReceipt_t Resource( const ralMaterialTexture_t *t,
		ralBackendType_t backend, uintptr_t identity, uint64_t generation ) {
	ralTextureResourceReceipt_t r;memset(&r,0,sizeof(r));r.schemaVersion=RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION;
	r.backendType=backend;r.textureIdentity=identity;r.resourceGeneration=generation;r.type=RAL_TEXTURE_2D;
	r.format=t->texture.asset.targetFormat;r.usage=(ralTextureUsage_t)(RAL_TEXTURE_USAGE_SAMPLED|RAL_TEXTURE_USAGE_TRANSFER_DST);
	r.width=t->texture.asset.width;r.height=t->texture.asset.height;r.mipLevels=t->texture.asset.targetMipLevels;
	r.arrayLayers=1u;r.ready=qtrue;return r;
}

static int Fallback( ralMaterialRuntimePlan_t *out ) {
	ralMaterialDescription_t d;ralMaterialReceipt_t m;memset(&d,0,sizeof(d));
	d.schemaVersion=RAL_MATERIAL_SCHEMA_VERSION;d.materialGeneration=1u;d.provenanceHash=1u;
	d.source=RAL_MATERIAL_SOURCE_PBR;d.factors.baseColor[0]=1.0f;d.factors.baseColor[2]=1.0f;
	d.factors.baseColor[3]=1.0f;d.factors.roughness=d.factors.normalScale=d.factors.occlusionStrength=1.0f;
	d.renderPolicy.depthTest=d.renderPolicy.depthWrite=qtrue;
	return Ral_MaterialCompile(&d,&m)&&Ral_MaterialRuntimePlanBuild(&m,NULL,0u,1u,out);
}

static ralMaterialProxyProgram_t Program( const ralMaterialReceipt_t *m ) {
	ralMaterialProxyProgram_t p;memset(&p,0,sizeof(p));p.schemaVersion=RAL_MATERIAL_PROXY_SCHEMA_VERSION;
	p.materialArtifactHash=m->artifactHash;p.instructionCount=1u;
	p.instructions[0].target=RAL_MATERIAL_PROXY_EMISSIVE_R;p.instructions[0].opcode=RAL_MATERIAL_PROXY_SINE;
	p.instructions[0].input=RAL_MATERIAL_PROXY_INPUT_TIME;p.instructions[0].aQ16=RAL_MATERIAL_PROXY_Q16_ONE;
	p.instructions[0].bQ16=RAL_MATERIAL_PROXY_Q16_ONE/2;p.instructions[0].cQ16=RAL_MATERIAL_PROXY_Q16_ONE;
	return p;
}

static int BackendFixture( ralBackendType_t backend ) {
	ralMaterialRuntimePlan_t fallback;ralMaterialReloadOwner_t owner,beforeOwner;
	ralMaterialDescription_t d,bad;ralMaterialReceipt_t compiled;ralTextureResourceReceipt_t resources[2];
	ralMaterialProxyProgram_t proxy;ralMaterialReloadCandidate_t candidate;
	ralMaterialReloadReceipt_t receipt,beforeReceipt,exact,wrong;
	CHECK(Fallback(&fallback));CHECK(Ral_MaterialReloadOwnerInit(&fallback,1u,&owner));
	CHECK(Ral_MaterialReloadOwnerValid(&owner)&&owner.usingFallback);
	memset(&candidate,0,sizeof(candidate));bad=fallback.material.material;bad.schemaVersion=0u;
	candidate.description=&bad;candidate.runtimeGeneration=2u;beforeOwner=owner;memset(&receipt,0xa5,sizeof(receipt));beforeReceipt=receipt;
	CHECK(!Ral_MaterialReloadTry(&owner,&candidate,2u,&receipt)&&!memcmp(&owner,&beforeOwner,sizeof(owner))
		&&!memcmp(&receipt,&beforeReceipt,sizeof(receipt))&&owner.usingFallback);

	CHECK(Description(2u,&d)&&Ral_MaterialCompile(&d,&compiled));resources[0]=Resource(&d.textures[0],backend,0x1000u,10u);
	resources[1]=Resource(&d.textures[1],backend,0x2000u,20u);proxy=Program(&compiled);
	candidate.description=&d;candidate.resources=resources;candidate.resourceCount=2u;
	candidate.runtimeGeneration=2u;candidate.proxyProgram=&proxy;
	CHECK(Ral_MaterialReloadTry(&owner,&candidate,2u,&receipt)&&Ral_MaterialReloadReceiptValid(&receipt));
	CHECK(!owner.usingFallback&&owner.hasProxy&&receipt.retirementCount==0u&&!owner.retirementPending);
	exact=receipt;CHECK(Ral_MaterialReloadReceiptExact(&receipt,&exact));
	beforeOwner=owner;bad=d;bad.schemaVersion=0u;candidate.description=&bad;beforeReceipt=receipt;
	CHECK(!Ral_MaterialReloadTry(&owner,&candidate,3u,&receipt)&&!memcmp(&owner,&beforeOwner,sizeof(owner))
		&&!memcmp(&receipt,&beforeReceipt,sizeof(receipt)));

	CHECK(Description(3u,&d));resources[1]=Resource(&d.textures[1],backend,0x2000u,21u);
	candidate.description=&d;candidate.runtimeGeneration=3u;candidate.proxyProgram=NULL;
	CHECK(Ral_MaterialReloadTry(&owner,&candidate,3u,&receipt)&&receipt.retirementCount==1u);
	CHECK(owner.retirementPending&&receipt.retirements[0].textureIdentity==(uintptr_t)0x2000u
		&&receipt.retirements[0].resourceGeneration==20u);
	beforeOwner=owner;beforeReceipt=receipt;
	CHECK(!Ral_MaterialReloadTry(&owner,&candidate,4u,&receipt)&&!memcmp(&owner,&beforeOwner,sizeof(owner)));
	wrong=beforeReceipt;wrong.retirements[0].resourceGeneration++;
	CHECK(!Ral_MaterialReloadAcknowledge(&owner,&wrong)&&!memcmp(&owner,&beforeOwner,sizeof(owner)));
	CHECK(Ral_MaterialReloadAcknowledge(&owner,&beforeReceipt)&&!owner.retirementPending);

	CHECK(Description(4u,&d));resources[0]=Resource(&d.textures[0],backend,0x1000u,11u);
	resources[1]=Resource(&d.textures[1],backend,0x2000u,22u);candidate.description=&d;
	candidate.runtimeGeneration=4u;CHECK(Ral_MaterialReloadTry(&owner,&candidate,4u,&receipt));
	CHECK(receipt.retirementCount==2u&&owner.retirementPending);
	CHECK(Ral_MaterialReloadAcknowledge(&owner,&receipt)&&Ral_MaterialReloadOwnerValid(&owner));
	return 0;
}

int main(void){CHECK(BackendFixture(RAL_BACKEND_VULKAN)==0);CHECK(BackendFixture(RAL_BACKEND_WEBGPU)==0);
	puts("ral_material_reload_test: ok");return 0;}
