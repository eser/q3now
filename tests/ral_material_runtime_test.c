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

static int Cooked(ralMaterialCookReceipt_t*out){
	ralMaterialSourceIr_t ir;ralMaterialSourceReceipt_t source;
	ralMaterialCookBinding_t bindings[5];uint32_t i;
	memset(&ir,0,sizeof(ir));ir.schemaVersion=RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION;
	ir.generation=7u;ir.provenance=RAL_MATERIAL_PROVENANCE_WIRED_LUA;
	ir.declaration.sourceId=71u;ir.declaration.line=1u;ir.declaration.column=1u;
	(void)snprintf(ir.semanticName,sizeof(ir.semanticName),"materials/runtime/test");
	ir.diffuseReflectance[0]=ir.diffuseReflectance[1]=ir.diffuseReflectance[2]=1.0f;
	ir.pbrFactors.baseColor[0]=ir.pbrFactors.baseColor[1]=1.0f;
	ir.pbrFactors.baseColor[2]=ir.pbrFactors.baseColor[3]=1.0f;
	ir.pbrFactors.roughness=ir.pbrFactors.normalScale=1.0f;
	ir.pbrFactors.occlusionStrength=1.0f;
	ir.pbrRenderPolicy.depthTest=ir.pbrRenderPolicy.depthWrite=qtrue;
	ir.dependencyCount=5u;ir.pbrTextureCount=2u;
	for(i=0u;i<5u;i++){
		ir.dependencies[i].role=RAL_MATERIAL_DEPENDENCY_TEXTURE;
		ir.dependencies[i].sourceId=101u+i;ir.dependencies[i].fsGeneration=3u;
		ir.dependencies[i].size=4096u+i;ir.dependencies[i].creationOptionsHash=201u+i;
		ir.dependencies[i].span=ir.declaration;
		(void)snprintf(ir.dependencies[i].canonicalPath,
			sizeof(ir.dependencies[i].canonicalPath),"textures/runtime/%u.ktx2",i);
		if(i<2u){ir.pbrTextures[i].span=ir.declaration;ir.pbrTextures[i].dependencyIndex=i;}
	}
	ir.dependencies[2].role=RAL_MATERIAL_DEPENDENCY_RENDER_PROGRAM;
	ir.dependencies[3].role=RAL_MATERIAL_DEPENDENCY_LIGHTING;
	ir.dependencies[4].role=RAL_MATERIAL_DEPENDENCY_RESIDENCY;
	ir.pbrTextures[0].role=RAL_MATERIAL_TEXTURE_BASE_COLOR;
	ir.pbrTextures[0].channels=RAL_MATERIAL_CHANNEL_RGBA;
	ir.pbrTextures[1].role=RAL_MATERIAL_TEXTURE_NORMAL;
	ir.pbrTextures[1].channels=RAL_MATERIAL_CHANNEL_XY_NORMAL;
	if(!Ral_MaterialSourceCompile(&ir,&source,NULL))return 0;
	memset(bindings,0,sizeof(bindings));
	for(i=0u;i<5u;i++){
		bindings[i].dependencyIndex=i;bindings[i].role=source.source.dependencies[i].role;
		bindings[i].sourceId=source.source.dependencies[i].sourceId;
		bindings[i].fsGeneration=source.source.dependencies[i].fsGeneration;
		bindings[i].creationOptionsHash=source.source.dependencies[i].creationOptionsHash;
	}
	if(!Key(1u,RAL_TEXTURE_ENCODING_SRGB,RAL_TEXTURE_CHANNEL_COLOR,&bindings[0].texture)
			||!Key(2u,RAL_TEXTURE_ENCODING_LINEAR,RAL_TEXTURE_CHANNEL_NORMAL_XY,&bindings[1].texture))return 0;
	bindings[0].artifactHash=bindings[0].texture.keyHash;
	bindings[1].artifactHash=bindings[1].texture.keyHash;
	bindings[2].artifactHash=UINT64_C(0x20000002);
	bindings[3].artifactHash=UINT64_C(0x30000003);
	bindings[4].artifactHash=UINT64_C(0x40000004);
	return Ral_MaterialCook(&source,bindings,5u,out,NULL);
}

static ralTextureResourceReceipt_t ResourceKey(const ralTextureCacheKey_t*key,
		ralBackendType_t backend,uintptr_t identity,uint64_t generation){
	ralTextureResourceReceipt_t r;memset(&r,0,sizeof(r));r.schemaVersion=RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION;
	r.backendType=backend;r.textureIdentity=identity;r.resourceGeneration=generation;r.type=RAL_TEXTURE_2D;
	r.format=key->asset.targetFormat;r.usage=(ralTextureUsage_t)(RAL_TEXTURE_USAGE_SAMPLED|RAL_TEXTURE_USAGE_TRANSFER_DST);
	r.width=key->asset.width;r.height=key->asset.height;r.mipLevels=key->asset.targetMipLevels;
	r.arrayLayers=1u;r.ready=qtrue;return r;
}

static ralTextureResourceReceipt_t Resource(const ralMaterialTexture_t*t,
		ralBackendType_t backend,uintptr_t identity,uint64_t generation){
	return ResourceKey(&t->texture,backend,identity,generation);
}

static int SkyCooked(ralMaterialCookReceipt_t*out){
	ralMaterialSourceIr_t ir;ralMaterialSourceReceipt_t source;
	ralMaterialCookBinding_t bindings[7];
	memset(&ir,0,sizeof(ir));ir.schemaVersion=RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION;
	ir.generation=17u;ir.provenance=RAL_MATERIAL_PROVENANCE_Q3_SHADER;
	ir.declaration.sourceId=701u;ir.declaration.line=1u;ir.declaration.column=1u;
	(void)snprintf(ir.semanticName,sizeof(ir.semanticName),"textures/runtime/sky");
	ir.diffuseReflectance[0]=ir.diffuseReflectance[1]=ir.diffuseReflectance[2]=1.0f;
	ir.sky=qtrue;ir.skyCloudHeight=512.0f;ir.skySource.outerBox=qtrue;
	ir.skySource.hasSun=qtrue;ir.skySource.sunColor[0]=1.0f;
	ir.skySource.sunColor[1]=0.8f;ir.skySource.sunColor[2]=0.6f;
	ir.skySource.sunIntensity=120.0f;ir.skySource.sunAzimuthDegrees=90.0f;
	ir.skySource.sunElevationDegrees=35.0f;ir.dependencyCount=7u;
	for(uint32_t i=0u;i<7u;++i){
		ir.dependencies[i].role=RAL_MATERIAL_DEPENDENCY_TEXTURE;
		ir.dependencies[i].sourceId=710u+i;ir.dependencies[i].fsGeneration=5u;
		ir.dependencies[i].size=4096u;ir.dependencies[i].creationOptionsHash=810u+i;
		ir.dependencies[i].span=ir.declaration;
		(void)snprintf(ir.dependencies[i].canonicalPath,
			sizeof(ir.dependencies[i].canonicalPath),"textures/runtime/sky/%u.png",i);
		if(i<RAL_MATERIAL_SKY_FACE_COUNT)ir.skySource.outerDependencies[i]=i;
	}
	ir.stageCount=1u;ir.stages[0].span=ir.declaration;
	ir.stages[0].mapKind=RAL_MATERIAL_MAP_IMAGE;ir.stages[0].dependencyIndex=6u;
	ir.stages[0].tcGen=RAL_MATERIAL_TCGEN_TEXTURE;ir.stages[0].sourceBlend=1u;
	ir.stages[0].depthWrite=qtrue;ir.stages[0].tcModCount=1u;
	ir.stages[0].tcMods[0].span=ir.declaration;
	ir.stages[0].tcMods[0].type=RAL_MATERIAL_TCMOD_SCROLL;
	ir.stages[0].tcMods[0].parameters[0]=0.025f;
	if(!Ral_MaterialSourceCompile(&ir,&source,NULL))return 0;
	memset(bindings,0,sizeof(bindings));
	for(uint32_t i=0u;i<7u;++i){
		bindings[i].dependencyIndex=i;bindings[i].role=RAL_MATERIAL_DEPENDENCY_TEXTURE;
		bindings[i].sourceId=source.source.dependencies[i].sourceId;
		bindings[i].fsGeneration=source.source.dependencies[i].fsGeneration;
		bindings[i].creationOptionsHash=source.source.dependencies[i].creationOptionsHash;
		if(!Key(30u+i,RAL_TEXTURE_ENCODING_SRGB,RAL_TEXTURE_CHANNEL_COLOR,
				&bindings[i].texture))return 0;
		bindings[i].artifactHash=bindings[i].texture.keyHash;
	}
	return Ral_MaterialCook(&source,bindings,7u,out,NULL);
}

/* Q3 hellfogdense: two independently scrolling stages, one image allocation. */
static int SharedStageFixture(ralBackendType_t backend){
	ralMaterialCookReceipt_t seed,cooked;
	ralMaterialSourceIr_t ir;ralMaterialSourceReceipt_t source;
	ralMaterialCookBinding_t binding;
	ralMaterialRuntimeResource_t resource;
	ralMaterialRuntimePlan_t plan,before;
	ralMaterialRuntimeBuildDiagnostic_t diagnostic;
	ralTextureResourceReceipt_t ordered[2];
	CHECK(SkyCooked(&seed));ir=seed.source.source;
	ir.sky=qfalse;ir.skyCloudHeight=0.0f;
	memset(&ir.skySource,0,sizeof(ir.skySource));
	ir.dependencyCount=1u;
	memset(&ir.dependencies[1],0,sizeof(ir.dependencies)-sizeof(ir.dependencies[0]));
	ir.stageCount=2u;ir.stages[0].dependencyIndex=0u;
	ir.stages[1]=ir.stages[0];ir.stages[1].destinationBlend=1u;
	ir.stages[1].tcMods[0].parameters[0]=-0.05f;
	CHECK(Ral_MaterialSourceCompile(&ir,&source,NULL));
	binding=seed.bindings[0];
	CHECK(Ral_MaterialCook(&source,&binding,1u,&cooked,NULL));
	CHECK(cooked.bindingCount==1u&&cooked.material.material.textureCount==2u);
	resource.cacheKeyHash=binding.texture.keyHash;
	resource.resource=ResourceKey(&binding.texture,backend,0xA000u,3u);
	memset(&plan,0xA5,sizeof(plan));
	CHECK(Ral_MaterialRuntimePlanBuildCooked(&cooked,&resource,1u,51u,&plan,&diagnostic));
	CHECK(Ral_MaterialRuntimePlanValid(&plan)&&plan.bindingCount==2u
		&&plan.dependencyCount==1u&&plan.bindings[0].bindingSlot!=plan.bindings[1].bindingSlot
		&&plan.bindings[0].resource.textureIdentity==plan.bindings[1].resource.textureIdentity);
	before=plan;ordered[0]=ordered[1]=resource.resource;
	ordered[1].resourceGeneration++;
	CHECK(!Ral_MaterialRuntimePlanBuild(&cooked.material,ordered,2u,52u,&plan)
		&&!memcmp(&plan,&before,sizeof(plan)));
	return 0;
}

static int SkyFixture(ralBackendType_t backend){
	ralMaterialCookReceipt_t cooked;ralMaterialRuntimeResource_t resources[7],bad[7];
	ralMaterialRuntimePlan_t plan,before;ralMaterialRuntimeBuildDiagnostic_t diagnostic;
	ralTextureResourceReceipt_t resolved;ralMaterialRuntimeCohort_t cohort;
	ralMaterialDependencyEvent_t event;qboolean invalidated=qfalse;
	CHECK(SkyCooked(&cooked));
	for(uint32_t i=0u;i<7u;++i){
		resources[6u-i].cacheKeyHash=cooked.bindings[i].texture.keyHash;
		resources[6u-i].resource=ResourceKey(&cooked.bindings[i].texture,backend,
			(uintptr_t)(0x9000u+i*0x100u),40u+i);
	}
	CHECK(Ral_MaterialRuntimePlanBuildCooked(&cooked,resources,7u,51u,&plan,&diagnostic));
	CHECK(Ral_MaterialRuntimePlanValid(&plan)&&plan.sky.present==qtrue
		&&plan.sky.authored.outerBox==qtrue&&plan.sky.stageCount==1u
		&&plan.sky.stages[0].dependencyIndex==6u&&plan.bindingCount==1u);
	before=plan;plan.sky.stages[0].animationFrameCount=
		RAL_MATERIAL_SOURCE_MAX_ANIMATION_FRAMES+1u;
	CHECK(!Ral_MaterialRuntimePlanValid(&plan));plan=before;
	plan.sky.stages[0].tcModCount=RAL_MATERIAL_SOURCE_MAX_TCMODS+1u;
	CHECK(!Ral_MaterialRuntimePlanValid(&plan));plan=before;
	plan.sky.authored.sunColor[0]=NAN;
	CHECK(!Ral_MaterialRuntimePlanValid(&plan));plan=before;
	for(uint32_t face=0u;face<RAL_MATERIAL_SKY_FACE_COUNT;++face){
		const uint32_t dependency=plan.sky.authored.outerDependencies[face];
		CHECK(dependency==face
			&&Ral_MaterialRuntimePlanResolveDependencyTexture(&plan,dependency,&resolved)
			&&resolved.textureIdentity==(uintptr_t)(0x9000u+face*0x100u));
	}
	before=plan;memcpy(bad,resources,sizeof(bad));bad[0].cacheKeyHash=UINT64_C(0xBAD5A7);
	CHECK(!Ral_MaterialRuntimePlanBuildCooked(&cooked,bad,7u,52u,&plan,&diagnostic)
		&&diagnostic.reason==RAL_MATERIAL_RUNTIME_BUILD_MISSING_RESOURCE
		&&!memcmp(&plan,&before,sizeof(plan)));
	memcpy(bad,resources,sizeof(bad));bad[1].resource.textureIdentity=bad[0].resource.textureIdentity;
	CHECK(!Ral_MaterialRuntimePlanBuildCooked(&cooked,bad,7u,52u,&plan,&diagnostic)
		&&diagnostic.reason==RAL_MATERIAL_RUNTIME_BUILD_INVALID_RESOURCE_SET
		&&!memcmp(&plan,&before,sizeof(plan)));
	CHECK(Ral_MaterialRuntimeCohortInit(&before,2u,50u,&cohort));
	memset(&event,0,sizeof(event));event.kind=RAL_MATERIAL_RUNTIME_DEPENDENCY_TEXTURE;
	event.dependencyGeneration=51u;
	event.textureIdentity=before.dependencies[0].textureResource.textureIdentity;
	event.previousGeneration=before.dependencies[0].textureResource.resourceGeneration;
	event.nextGeneration=event.previousGeneration+1u;
	CHECK(Ral_MaterialRuntimeInvalidate(&cohort,&event,&invalidated)&&invalidated);
	return 0;
}

static int BackendFixture(ralBackendType_t backend){
	ralMaterialReceipt_t material;ralTextureResourceReceipt_t resources[2],bad[2];
	ralMaterialRuntimePlan_t plan,before,exact;ralMaterialRuntimeCohort_t cohort,cohortBefore;
	ralMaterialCookReceipt_t cooked,badCooked;
	ralMaterialRuntimeResource_t keyed[2],badKeyed[2],built,builtBefore;
	ralMaterialRuntimeBuildDiagnostic_t diagnostic;
	ralMaterialDependencyEvent_t event;qboolean invalidated;
	CHECK(Material(&material));resources[0]=Resource(&material.material.textures[0],backend,(uintptr_t)0x1000u,3u);
	resources[1]=Resource(&material.material.textures[1],backend,(uintptr_t)0x2000u,4u);
	CHECK(Ral_TextureResourceReceiptValid(&resources[0])
		&&Ral_TextureResourceReceiptExact(&resources[0],&resources[0]));
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
	CHECK(Cooked(&cooked));
	memset(&built,0xa5,sizeof(built));builtBefore=built;
	CHECK(Ral_MaterialRuntimeResourceBuild(
		&cooked.material.material.textures[0].texture,
		&resources[0],&built)
		&&built.cacheKeyHash==cooked.material.material.textures[0].texture.keyHash
		&&Ral_TextureResourceReceiptExact(&built.resource,&resources[0]));
	/* Vulkan may retain a truthful UNORM allocation and perform the sRGB
	 * transfer in shader code.  The explicit sampling receipt is equivalent to
	 * native sRGB for this cooked colour binding, while plain UNORM is not. */
	bad[0]=resources[0];
	bad[0].format=RAL_FORMAT_R8G8B8A8_UNORM;
	bad[0].sampleEncoding=RAL_TEXTURE_SAMPLE_ENCODING_MANUAL_SRGB;
	CHECK(Ral_MaterialRuntimeResourceBuild(
		&cooked.material.material.textures[0].texture,&bad[0],&built));
	bad[0].sampleEncoding=RAL_TEXTURE_SAMPLE_ENCODING_NATIVE;
	CHECK(!Ral_MaterialRuntimeResourceBuild(
		&cooked.material.material.textures[0].texture,&bad[0],&built));
	bad[0]=resources[0];bad[0].width++;
	builtBefore=built;
	CHECK(!Ral_MaterialRuntimeResourceBuild(
		&cooked.material.material.textures[0].texture,&bad[0],&built)
		&&!memcmp(&built,&builtBefore,sizeof(built)));
	keyed[0].cacheKeyHash=cooked.material.material.textures[1].texture.keyHash;
	keyed[0].resource=Resource(&cooked.material.material.textures[1],backend,(uintptr_t)0x4000u,6u);
	keyed[1].cacheKeyHash=cooked.material.material.textures[0].texture.keyHash;
	keyed[1].resource=Resource(&cooked.material.material.textures[0],backend,(uintptr_t)0x3000u,5u);
	CHECK(Ral_MaterialRuntimePlanBuildCooked(&cooked,keyed,2u,13u,&plan,&diagnostic));
	CHECK(diagnostic.reason==RAL_MATERIAL_RUNTIME_BUILD_OK
		&&plan.bindings[0].cacheKeyHash==keyed[1].cacheKeyHash
		&&plan.bindings[0].resource.textureIdentity==keyed[1].resource.textureIdentity
		&&plan.bindings[1].cacheKeyHash==keyed[0].cacheKeyHash
		&&plan.dependencyCount==5u
		&&plan.dependencies[2].role==RAL_MATERIAL_DEPENDENCY_RENDER_PROGRAM
		&&plan.dependencies[3].role==RAL_MATERIAL_DEPENDENCY_LIGHTING
		&&plan.dependencies[4].role==RAL_MATERIAL_DEPENDENCY_RESIDENCY);
	CHECK(Ral_MaterialRuntimeCohortInit(&plan,1u,10u,&cohort)
		&&Ral_MaterialRuntimeCohortValid(&cohort));
	for(uint32_t roleIndex=2u;roleIndex<5u;roleIndex++){
		const ralMaterialRuntimeDependency_t *dependency=&plan.dependencies[roleIndex];
		memset(&event,0,sizeof(event));event.kind=RAL_MATERIAL_RUNTIME_DEPENDENCY_SOURCE;
		event.dependencyGeneration=11u;event.role=dependency->role;
		event.sourceId=dependency->sourceId;
		event.creationOptionsHash=dependency->creationOptionsHash;
		event.artifactHash=dependency->artifactHash;
		event.previousGeneration=dependency->fsGeneration;
		event.nextGeneration=dependency->fsGeneration+1u;
		invalidated=qfalse;
		CHECK(Ral_MaterialRuntimeInvalidate(&cohort,&event,&invalidated)&&invalidated);
		CHECK(Ral_MaterialRuntimeCohortInit(&plan,1u,10u,&cohort));
	}
	before=plan;memcpy(badKeyed,keyed,sizeof(badKeyed));badKeyed[1].cacheKeyHash=badKeyed[0].cacheKeyHash;
	CHECK(!Ral_MaterialRuntimePlanBuildCooked(&cooked,badKeyed,2u,14u,&plan,&diagnostic)
		&&diagnostic.reason==RAL_MATERIAL_RUNTIME_BUILD_DUPLICATE_RESOURCE
		&&!memcmp(&plan,&before,sizeof(plan)));
	memcpy(badKeyed,keyed,sizeof(badKeyed));badKeyed[1].cacheKeyHash=UINT64_C(0xBAD);
	CHECK(!Ral_MaterialRuntimePlanBuildCooked(&cooked,badKeyed,2u,14u,&plan,&diagnostic)
		&&diagnostic.reason==RAL_MATERIAL_RUNTIME_BUILD_MISSING_RESOURCE
		&&diagnostic.cacheKeyHash==cooked.material.material.textures[0].texture.keyHash
		&&!memcmp(&plan,&before,sizeof(plan)));
	badCooked=cooked;badCooked.artifactHash^=1u;
	CHECK(!Ral_MaterialRuntimePlanBuildCooked(&badCooked,keyed,2u,14u,&plan,&diagnostic)
		&&diagnostic.reason==RAL_MATERIAL_RUNTIME_BUILD_INVALID_COOKED
		&&!memcmp(&plan,&before,sizeof(plan)));
	CHECK(Ral_MaterialRuntimeCohortInit(&before,1u,10u,&cohort)&&Ral_MaterialRuntimeCohortValid(&cohort));
	memset(&event,0,sizeof(event));event.kind=RAL_MATERIAL_RUNTIME_DEPENDENCY_TEXTURE;
	event.dependencyGeneration=11u;event.textureIdentity=(uintptr_t)0x9999u;
	event.previousGeneration=3u;event.nextGeneration=4u;cohortBefore=cohort;invalidated=qtrue;
	CHECK(Ral_MaterialRuntimeInvalidate(&cohort,&event,&invalidated)&&!invalidated
		&&!memcmp(&cohort,&cohortBefore,sizeof(cohort)));
	event.textureIdentity=before.bindings[0].resource.textureIdentity;
	event.previousGeneration=before.bindings[0].resource.resourceGeneration;
	invalidated=qfalse;
	CHECK(Ral_MaterialRuntimeInvalidate(&cohort,&event,&invalidated)&&invalidated
		&&cohort.state==RAL_MATERIAL_RUNTIME_INVALIDATED);
	cohortBefore=cohort;event.dependencyGeneration=10u;invalidated=qtrue;
	CHECK(!Ral_MaterialRuntimeInvalidate(&cohort,&event,&invalidated)
		&&!memcmp(&cohort,&cohortBefore,sizeof(cohort))&&invalidated);
	CHECK(SkyFixture(backend)==0);CHECK(SharedStageFixture(backend)==0);return 0;
}

int main(void){
	ralMaterialReceipt_t material;ralMaterialRuntimePlan_t plan;
	ralMaterialRuntimeCohort_t cohort;ralMaterialDependencyEvent_t event;qboolean invalidated=qfalse;
	CHECK(BackendFixture(RAL_BACKEND_VULKAN)==0&&BackendFixture(RAL_BACKEND_OPENGL)==0
		&&BackendFixture(RAL_BACKEND_METAL)==0&&BackendFixture(RAL_BACKEND_WEBGPU)==0);
	CHECK(Material(&material));CHECK(Ral_MaterialRuntimePlanBuild(&material,NULL,0u,20u,&plan)==qfalse);
	/* A textureless material remains valid and does not invent a backend. */
	material.material.textureCount=0u;memset(material.material.textures,0,sizeof(material.material.textures));
	CHECK(Ral_MaterialCompile(&material.material,&material));
	CHECK(Ral_MaterialRuntimePlanBuild(&material,NULL,0u,20u,&plan));
	CHECK(Ral_MaterialRuntimeCohortInit(&plan,2u,20u,&cohort));
	memset(&event,0,sizeof(event));event.kind=RAL_MATERIAL_RUNTIME_DEPENDENCY_ARTIFACT;
	event.dependencyGeneration=21u;event.artifactHash=material.artifactHash;
	event.previousGeneration=material.material.materialGeneration;event.nextGeneration=8u;
	CHECK(Ral_MaterialRuntimeInvalidate(&cohort,&event,&invalidated)&&invalidated);
	puts("ral_material_runtime_test: ok");return 0;
}
