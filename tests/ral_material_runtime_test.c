// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material_runtime.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;} } while(0)

static int Key(uint64_t generation,ralTextureAssetColorEncoding_t encoding,
		ralTextureChannelSemantic_t semantic,ralTextureCacheKey_t*out){
	ralTextureAssetRequest_t q;ralTextureAssetReceipt_t r;memset(&q,0,sizeof(q));
	q.schemaVersion=RAL_TEXTURE_ASSET_SCHEMA_VERSION;q.assetGeneration=generation;
	q.provenanceHash=generation*103u;q.dimension=RAL_TEXTURE_ASSET_2D;
	q.width=q.height=8u;q.depth=q.layers=1u;q.sourceMipLevels=1u;
	q.colorEncoding=encoding;q.channelSemantic=semantic;q.sourceEncoding=RAL_TEXTURE_SOURCE_UNCOMPRESSED;
	q.mipPolicy=RAL_TEXTURE_MIPS_SOURCE;q.residency=RAL_TEXTURE_RESIDENCY_STREAMED;
	q.preferenceCount=1u;q.preferences[0]=RAL_TEXTURE_COMPRESSION_UNCOMPRESSED;
	q.allowUncompressedFallback=qtrue;return Ral_ResolveTextureAsset(&q,&r)&&Ral_TextureCacheKeyBuild(&r,out);
}

static int Material(ralMaterialReceipt_t*out){
	ralMaterialDescription_t d;memset(&d,0,sizeof(d));d.schemaVersion=RAL_MATERIAL_SCHEMA_VERSION;
	d.materialGeneration=7u;d.provenanceHash=71u;d.source=RAL_MATERIAL_SOURCE_PBR;
	d.textureCount=2u;d.textures[0].role=RAL_MATERIAL_TEXTURE_BASE_COLOR;
	d.textures[0].channels=RAL_MATERIAL_CHANNEL_RGBA;d.textures[1].role=RAL_MATERIAL_TEXTURE_NORMAL;
	d.textures[1].channels=RAL_MATERIAL_CHANNEL_XY_NORMAL;d.factors.baseColor[0]=1.0f;
	d.factors.baseColor[1]=d.factors.baseColor[2]=d.factors.baseColor[3]=1.0f;
	d.factors.roughness=d.factors.normalScale=d.factors.occlusionStrength=1.0f;
	d.renderPolicy.depthTest=d.renderPolicy.depthWrite=qtrue;
	return Key(1u,RAL_TEXTURE_ENCODING_SRGB,RAL_TEXTURE_CHANNEL_COLOR,&d.textures[0].texture)
		&&Key(2u,RAL_TEXTURE_ENCODING_LINEAR,RAL_TEXTURE_CHANNEL_NORMAL_XY,&d.textures[1].texture)
		&&Ral_MaterialCompile(&d,out);
}

static ralTextureResourceReceipt_t Resource(const ralMaterialTexture_t*t,
		ralBackendType_t backend,uintptr_t identity,uint64_t generation){
	ralTextureResourceReceipt_t r;memset(&r,0,sizeof(r));r.schemaVersion=RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION;
	r.backendType=backend;r.textureIdentity=identity;r.resourceGeneration=generation;r.type=RAL_TEXTURE_2D;
	r.format=t->texture.asset.targetFormat;r.usage=(ralTextureUsage_t)(RAL_TEXTURE_USAGE_SAMPLED|RAL_TEXTURE_USAGE_TRANSFER_DST);
	r.width=t->texture.asset.width;r.height=t->texture.asset.height;r.mipLevels=t->texture.asset.targetMipLevels;
	r.arrayLayers=1u;r.ready=qtrue;return r;
}

static int BackendFixture(ralBackendType_t backend){
	ralMaterialReceipt_t material;ralTextureResourceReceipt_t resources[2],bad[2];
	ralMaterialRuntimePlan_t plan,before,exact;ralMaterialRuntimeCohort_t cohort,cohortBefore;
	ralMaterialDependencyEvent_t event;qboolean invalidated;
	CHECK(Material(&material));resources[0]=Resource(&material.material.textures[0],backend,(uintptr_t)0x1000u,3u);
	resources[1]=Resource(&material.material.textures[1],backend,(uintptr_t)0x2000u,4u);
	CHECK(Ral_MaterialRuntimePlanBuild(&material,resources,2u,11u,&plan));
	CHECK(Ral_MaterialRuntimePlanValid(&plan)&&plan.bindings[0].bindingSlot==0u
		&&plan.bindings[1].bindingSlot==1u&&plan.material.variantKey==material.variantKey);
	exact=plan;CHECK(Ral_MaterialRuntimePlanExact(&plan,&exact));
	before=plan;memcpy(bad,resources,sizeof(bad));bad[1].format=bad[0].format;
	CHECK(!Ral_MaterialRuntimePlanBuild(&material,bad,2u,12u,&plan)&&!memcmp(&plan,&before,sizeof(plan)));
	memcpy(bad,resources,sizeof(bad));bad[1].textureIdentity=bad[0].textureIdentity;
	CHECK(!Ral_MaterialRuntimePlanBuild(&material,bad,2u,12u,&plan));
	CHECK(!Ral_MaterialRuntimePlanBuild(&material,resources,1u,12u,&plan));
	memcpy(bad,resources,sizeof(bad));bad[1].backendType=backend==RAL_BACKEND_VULKAN?RAL_BACKEND_WEBGPU:RAL_BACKEND_VULKAN;
	CHECK(!Ral_MaterialRuntimePlanBuild(&material,bad,2u,12u,&plan));
	CHECK(Ral_MaterialRuntimeCohortInit(&before,1u,10u,&cohort)&&Ral_MaterialRuntimeCohortValid(&cohort));
	memset(&event,0,sizeof(event));event.kind=RAL_MATERIAL_DEPENDENCY_TEXTURE;
	event.dependencyGeneration=11u;event.textureIdentity=(uintptr_t)0x9999u;
	event.previousGeneration=3u;event.nextGeneration=4u;cohortBefore=cohort;invalidated=qtrue;
	CHECK(Ral_MaterialRuntimeInvalidate(&cohort,&event,&invalidated)&&!invalidated
		&&!memcmp(&cohort,&cohortBefore,sizeof(cohort)));
	event.textureIdentity=resources[0].textureIdentity;invalidated=qfalse;
	CHECK(Ral_MaterialRuntimeInvalidate(&cohort,&event,&invalidated)&&invalidated
		&&cohort.state==RAL_MATERIAL_RUNTIME_INVALIDATED);
	cohortBefore=cohort;event.dependencyGeneration=10u;invalidated=qtrue;
	CHECK(!Ral_MaterialRuntimeInvalidate(&cohort,&event,&invalidated)
		&&!memcmp(&cohort,&cohortBefore,sizeof(cohort))&&invalidated);
	return 0;
}

int main(void){
	ralMaterialReceipt_t material;ralMaterialRuntimePlan_t plan;
	ralMaterialRuntimeCohort_t cohort;ralMaterialDependencyEvent_t event;qboolean invalidated=qfalse;
	CHECK(BackendFixture(RAL_BACKEND_VULKAN)==0&&BackendFixture(RAL_BACKEND_WEBGPU)==0);
	CHECK(Material(&material));CHECK(Ral_MaterialRuntimePlanBuild(&material,NULL,0u,20u,&plan)==qfalse);
	/* A textureless material remains valid and does not invent a backend. */
	material.material.textureCount=0u;memset(material.material.textures,0,sizeof(material.material.textures));
	CHECK(Ral_MaterialCompile(&material.material,&material));
	CHECK(Ral_MaterialRuntimePlanBuild(&material,NULL,0u,20u,&plan));
	CHECK(Ral_MaterialRuntimeCohortInit(&plan,2u,20u,&cohort));
	memset(&event,0,sizeof(event));event.kind=RAL_MATERIAL_DEPENDENCY_ARTIFACT;
	event.dependencyGeneration=21u;event.artifactHash=material.artifactHash;
	event.previousGeneration=material.material.materialGeneration;event.nextGeneration=8u;
	CHECK(Ral_MaterialRuntimeInvalidate(&cohort,&event,&invalidated)&&invalidated);
	puts("ral_material_runtime_test: ok");return 0;
}
