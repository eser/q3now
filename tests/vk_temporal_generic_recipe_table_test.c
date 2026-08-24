// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_temporal_generic_recipe_table.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL recipe table %d: %s\n",__LINE__,#x); return 1; } } while (0)
#define VK_TEMPORAL_BLOB(name, size) const unsigned char name[size] = { 0 };
#define VK_TEMPORAL_PAIR(tx, family, env, fog, ordinaryVS, ordinaryFS, temporalVS, writeFS, invalidateFS)
#include "../code/render/ral/backends/vulkan/renderer/shaders/spirv/temporal_generic_catalog.inc"
#undef VK_TEMPORAL_PAIR
#undef VK_TEMPORAL_BLOB

static int s_allocs, s_frees, s_failAlloc;
static void *FakeAlloc( size_t bytes ) {
	if ( s_failAlloc ) return NULL;
	s_allocs++; return malloc( bytes );
}
static void FakeFree( void *p ) { s_frees++; free( p ); }

typedef struct {
	VkPipelineShaderStageCreateInfo stages[2];
	VkVertexInputBindingDescription bindings[2];
	VkVertexInputAttributeDescription attrs[2];
	VkPipelineVertexInputStateCreateInfo vi;
	VkPipelineInputAssemblyStateCreateInfo ia;
	VkPipelineViewportStateCreateInfo vp;
	VkPipelineRasterizationStateCreateInfo rs;
	VkPipelineMultisampleStateCreateInfo ms;
	VkPipelineDepthStencilStateCreateInfo ds;
	VkPipelineColorBlendAttachmentState attachment;
	VkPipelineColorBlendStateCreateInfo cb;
	VkDynamicState dyns[2];
	VkPipelineDynamicStateCreateInfo dyn;
	VkGraphicsPipelineCreateInfo gp;
	vkGenericSpecializationGraph_t specs;
	uint32_t vsWord, fsWords[VK_GENERIC_FRAGMENT_SPEC_COUNT];
} Fixture;

static void FloatWord( uint32_t *out, float value ) { memcpy( out, &value, sizeof(value) ); }
static qboolean ExpectedKey( uint32_t tx, uint32_t family ) {
	if ( tx == 0u ) return family <= VK_TEMPORAL_GENERIC_ENT;
	if ( tx == 1u ) return family == VK_TEMPORAL_GENERIC_PLAIN
		|| family == VK_TEMPORAL_GENERIC_IDENT || family == VK_TEMPORAL_GENERIC_FIXED
		|| family == VK_TEMPORAL_GENERIC_CL;
	if ( tx == 2u ) return family == VK_TEMPORAL_GENERIC_PLAIN
		|| family == VK_TEMPORAL_GENERIC_CL;
	return qfalse;
}

static qboolean MakeFixture( Fixture *f ) {
	memset( f, 0, sizeof(*f) );
	f->vsWord=1; FloatWord(&f->fsWords[2],0.85f); FloatWord(&f->fsWords[8],1.0f);
	FloatWord(&f->fsWords[9],1.0f); FloatWord(&f->fsWords[11],2.0f);
	if ( !VK_GenericSpecializationAuthor(&f->specs,&f->vsWord,f->fsWords) ) return qfalse;
	f->stages[0].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	f->stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;f->stages[0].module=(VkShaderModule)(uintptr_t)1;
	f->stages[0].pName="main";f->stages[0].pSpecializationInfo=&f->specs.vertexInfo;
	f->stages[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	f->stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;f->stages[1].module=(VkShaderModule)(uintptr_t)2;
	f->stages[1].pName="main";f->stages[1].pSpecializationInfo=&f->specs.fragmentInfo;
	f->bindings[0]=(VkVertexInputBindingDescription){0,16,VK_VERTEX_INPUT_RATE_VERTEX};
	f->bindings[1]=(VkVertexInputBindingDescription){2,8,VK_VERTEX_INPUT_RATE_VERTEX};
	f->attrs[0]=(VkVertexInputAttributeDescription){0,0,VK_FORMAT_R32G32B32A32_SFLOAT,0};
	f->attrs[1]=(VkVertexInputAttributeDescription){2,2,VK_FORMAT_R32G32_SFLOAT,0};
	f->vi.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	f->vi.vertexBindingDescriptionCount=2;f->vi.pVertexBindingDescriptions=f->bindings;
	f->vi.vertexAttributeDescriptionCount=2;f->vi.pVertexAttributeDescriptions=f->attrs;
	f->ia.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	f->ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	f->vp.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	f->vp.viewportCount=1;f->vp.scissorCount=1;
	f->rs.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	f->rs.polygonMode=VK_POLYGON_MODE_FILL;f->rs.cullMode=VK_CULL_MODE_BACK_BIT;
	f->rs.frontFace=VK_FRONT_FACE_CLOCKWISE;f->rs.lineWidth=1.0f;
	f->ms.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	f->ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;f->ms.minSampleShading=1.0f;
	f->ds.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	f->ds.depthTestEnable=VK_TRUE;f->ds.depthWriteEnable=VK_TRUE;
	f->ds.depthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;f->ds.maxDepthBounds=1.0f;
	f->attachment.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT
		|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
	f->cb.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	f->cb.logicOp=VK_LOGIC_OP_COPY;f->cb.attachmentCount=1;f->cb.pAttachments=&f->attachment;
	f->dyns[0]=VK_DYNAMIC_STATE_VIEWPORT;f->dyns[1]=VK_DYNAMIC_STATE_SCISSOR;
	f->dyn.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	f->dyn.dynamicStateCount=2;f->dyn.pDynamicStates=f->dyns;
	f->gp.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	f->gp.stageCount=2;f->gp.pStages=f->stages;f->gp.pVertexInputState=&f->vi;
	f->gp.pInputAssemblyState=&f->ia;f->gp.pViewportState=&f->vp;
	f->gp.pRasterizationState=&f->rs;f->gp.pMultisampleState=&f->ms;
	f->gp.pDepthStencilState=&f->ds;f->gp.pColorBlendState=&f->cb;
	f->gp.pDynamicState=&f->dyn;f->gp.layout=(VkPipelineLayout)(uintptr_t)3;
	f->gp.basePipelineIndex=-1;
	return qtrue;
}

static qboolean StencilEqual( const VkStencilOpState *a,
		const VkStencilOpState *b ) {
	return a->failOp == b->failOp && a->passOp == b->passOp
		&& a->depthFailOp == b->depthFailOp && a->compareOp == b->compareOp
		&& a->compareMask == b->compareMask && a->writeMask == b->writeMask
		&& a->reference == b->reference;
}

static qboolean RecipeSemanticsEqual( const vkTemporalGenericRecipe_t *a,
		const vkTemporalGenericRecipe_t *b ) {
	uint32_t i;
	if ( a->catalogId != b->catalogId
			|| a->key.textureCount != b->key.textureCount
			|| a->key.family != b->key.family
			|| a->key.environment != b->key.environment
			|| a->key.shaderFog != b->key.shaderFog
			|| a->alphaTested != b->alphaTested
			|| a->numBindings != b->numBindings || a->numAttributes != b->numAttributes
			|| a->topology != b->topology || a->primitiveRestart != b->primitiveRestart
			|| a->depthClampEnable != b->depthClampEnable
			|| a->rasterizerDiscardEnable != b->rasterizerDiscardEnable
			|| a->polygonMode != b->polygonMode || a->cullMode != b->cullMode
			|| a->frontFace != b->frontFace || a->depthBiasEnable != b->depthBiasEnable
			|| a->depthBiasConstant != b->depthBiasConstant
			|| a->depthBiasClamp != b->depthBiasClamp
			|| a->depthBiasSlope != b->depthBiasSlope || a->lineWidth != b->lineWidth
			|| a->sampleCount != b->sampleCount
			|| a->sampleShadingEnable != b->sampleShadingEnable
			|| a->minSampleShading != b->minSampleShading
			|| a->sampleMaskPresent != b->sampleMaskPresent
			|| a->alphaToCoverageEnable != b->alphaToCoverageEnable
			|| a->alphaToOneEnable != b->alphaToOneEnable
			|| a->depthTestEnable != b->depthTestEnable
			|| a->depthWriteEnable != b->depthWriteEnable
			|| a->depthCompareOp != b->depthCompareOp
			|| a->depthBoundsTestEnable != b->depthBoundsTestEnable
			|| a->stencilTestEnable != b->stencilTestEnable
			|| !StencilEqual( &a->stencilFront, &b->stencilFront )
			|| !StencilEqual( &a->stencilBack, &b->stencilBack )
			|| a->minDepthBounds != b->minDepthBounds
			|| a->maxDepthBounds != b->maxDepthBounds
			|| a->logicOpEnable != b->logicOpEnable || a->logicOp != b->logicOp
			|| a->sceneBlend.blendEnable != b->sceneBlend.blendEnable
			|| a->sceneBlend.srcColorBlendFactor != b->sceneBlend.srcColorBlendFactor
			|| a->sceneBlend.dstColorBlendFactor != b->sceneBlend.dstColorBlendFactor
			|| a->sceneBlend.colorBlendOp != b->sceneBlend.colorBlendOp
			|| a->sceneBlend.srcAlphaBlendFactor != b->sceneBlend.srcAlphaBlendFactor
			|| a->sceneBlend.dstAlphaBlendFactor != b->sceneBlend.dstAlphaBlendFactor
			|| a->sceneBlend.alphaBlendOp != b->sceneBlend.alphaBlendOp
			|| a->sceneBlend.colorWriteMask != b->sceneBlend.colorWriteMask
			|| a->dynamicStates[0] != b->dynamicStates[0]
			|| a->dynamicStates[1] != b->dynamicStates[1]
			|| a->sceneFormat != b->sceneFormat || a->depthFormat != b->depthFormat
			|| a->layoutClass != b->layoutClass
			|| a->capturedAuthority.token != b->capturedAuthority.token
			|| a->capturedAuthority.frameId != b->capturedAuthority.frameId
			|| a->capturedAuthority.planGeneration != b->capturedAuthority.planGeneration
			|| a->capturedAuthority.materializationGeneration
				!= b->capturedAuthority.materializationGeneration ) return qfalse;
	for ( i = 0; i < VK_GENERIC_TOTAL_SPEC_COUNT; ++i )
		if ( a->specializationWords[i] != b->specializationWords[i] ) return qfalse;
	for ( i = 0; i < a->numBindings; ++i )
		if ( a->bindings[i].binding != b->bindings[i].binding
				|| a->bindings[i].stride != b->bindings[i].stride
				|| a->bindings[i].inputRate != b->bindings[i].inputRate ) return qfalse;
	for ( i = 0; i < a->numAttributes; ++i )
		if ( a->attributes[i].location != b->attributes[i].location
				|| a->attributes[i].binding != b->attributes[i].binding
				|| a->attributes[i].format != b->attributes[i].format
				|| a->attributes[i].offset != b->attributes[i].offset ) return qfalse;
	for ( i = 0; i < 4u; ++i )
		if ( a->blendConstants[i] != b->blendConstants[i] ) return qfalse;
	return qtrue;
}

int main( void ) {
	vkTemporalGenericRecipeTable_t owner, beforeOwner;
	vkTemporalRecipeTableOps_t ops={FakeAlloc,FakeFree};
	vkTemporalRecipeBatchAuthority_t auth={11,12,13,14}, badAuth;
	vkTemporalGenericRecipeCaptureInput_t in;
	vkTemporalGenericRecipe_t got, gotAgain, gotBefore;
	vkTemporalGenericRecipe_t badRecipe;
	vkTemporalGenericRecipeView_t view, viewBefore;
	vkTemporalGenericCatalogEntry_t entry;
	vkTemporalGenericKey_t identified, key={1,VK_TEMPORAL_GENERIC_PLAIN,qfalse,qfalse};
	Fixture f, equivalentFixture;
	vkTemporalGenericRecipe_t equivalentRecipe;
	vkTemporalGenericRecipeReceipt_t receipt, receiptBefore;
	uint32_t id, gen, epoch, tx, family, env, fog, accepted=0;
	qboolean seen[VK_TEMPORAL_GENERIC_CATALOG_COUNT + 1u];

	CHECK(MakeFixture(&f));
	CHECK(VK_TemporalGenericCatalogSelect(&key,&entry));
	memset(seen,0,sizeof(seen));
	for(tx=0;tx<3u;++tx)for(family=0;family<5u;++family)
		for(env=0;env<2u;++env)for(fog=0;fog<2u;++fog){
			vkTemporalGenericKey_t loopKey={tx,(vkTemporalGenericFamily_t)family,
				(qboolean)env,(qboolean)fog};vkTemporalGenericCatalogEntry_t loopEntry;
			vkTemporalGenericKey_t loopFound;uint32_t loopId;
			qboolean expected=ExpectedKey(tx,family);
			CHECK(VK_TemporalGenericCatalogSelect(&loopKey,&loopEntry)==expected);
			if(!expected)continue;
			CHECK(VK_TemporalGenericCatalogIdentify(loopEntry.ordinaryVertex,
				loopEntry.ordinaryFragment,&loopFound,&loopId));
			CHECK(loopId>0u&&loopId<=VK_TEMPORAL_GENERIC_CATALOG_COUNT&&!seen[loopId]);seen[loopId]=qtrue;
			CHECK(loopFound.textureCount==tx&&loopFound.family==(vkTemporalGenericFamily_t)family
				&&loopFound.environment==(qboolean)env&&loopFound.shaderFog==(qboolean)fog);
			// Every admitted vertex identity rejects a deliberately foreign
			// texture-family/opposite-fog fragment identity.
			{ vkTemporalGenericKey_t crossKey;
				vkTemporalGenericCatalogEntry_t crossEntry;
				vkTemporalGenericKey_t crossOut={9,(vkTemporalGenericFamily_t)9,9,9};
				uint32_t crossId=99;
				if(tx==0u)crossKey=(vkTemporalGenericKey_t){2,VK_TEMPORAL_GENERIC_CL,
					qfalse,(qboolean)!fog};
				else crossKey=(vkTemporalGenericKey_t){0,VK_TEMPORAL_GENERIC_ENT,
					qfalse,(qboolean)!fog};
				CHECK(VK_TemporalGenericCatalogSelect(&crossKey,&crossEntry));
				CHECK(!VK_TemporalGenericCatalogIdentify(loopEntry.ordinaryVertex,
					crossEntry.ordinaryFragment,&crossOut,&crossId));
				CHECK(crossOut.textureCount==9&&crossOut.family==(vkTemporalGenericFamily_t)9
					&&crossId==99); }
			accepted++;
		}
	CHECK(accepted==VK_TEMPORAL_GENERIC_CATALOG_COUNT);
	{ vkTemporalGenericKey_t fixedKey={0,VK_TEMPORAL_GENERIC_FIXED,qfalse,qfalse};
		vkTemporalGenericCatalogEntry_t fixedEntry;vkTemporalGenericKey_t untouched={9,(vkTemporalGenericFamily_t)9,9,9};
		uint32_t untouchedId=99;CHECK(VK_TemporalGenericCatalogSelect(&fixedKey,&fixedEntry));
		CHECK(!VK_TemporalGenericCatalogIdentify(fixedEntry.ordinaryVertex,
			entry.ordinaryFragment,&untouched,&untouchedId));
		CHECK(untouched.textureCount==9&&untouched.family==(vkTemporalGenericFamily_t)9
			&&untouchedId==99); }
	memset(&identified,0xA5,sizeof(identified));id=UINT32_MAX;
	CHECK(VK_TemporalGenericCatalogIdentify(entry.ordinaryVertex,entry.ordinaryFragment,&identified,&id));
	CHECK(identified.textureCount==key.textureCount&&identified.family==key.family
		&&identified.environment==key.environment&&identified.shaderFog==key.shaderFog
		&&id>0u&&id<=VK_TEMPORAL_GENERIC_CATALOG_COUNT);
	{ vkTemporalGenericKey_t kbefore=identified;uint32_t ibefore=id;
		vkTemporalShaderBlob_t wrong=entry.ordinaryVertex;wrong.size-=4;
		CHECK(!VK_TemporalGenericCatalogIdentify(wrong,entry.ordinaryFragment,&identified,&id));
		CHECK(identified.textureCount==kbefore.textureCount&&identified.family==kbefore.family
			&&identified.environment==kbefore.environment&&identified.shaderFog==kbefore.shaderFog
			&&id==ibefore); }

	VK_TemporalGenericRecipeTableInit(&owner);
	CHECK(!owner.records&&!owner.ownerEpoch&&!owner.lastEntryGeneration);
	CHECK(VK_TemporalGenericRecipeTableEvictRange(&owner,123,456));
	CHECK(VK_TemporalGenericRecipeTableEvictRange(&owner,UINT32_MAX,UINT32_MAX));
	CHECK(!VK_TemporalGenericRecipeTableEvictRange(&owner,456,123));
	badAuth=auth;badAuth.token=0;beforeOwner=owner;
	CHECK(!VK_TemporalGenericRecipeTablePrepare(&owner,4,&badAuth,&ops)
		&&owner.records==beforeOwner.records&&owner.ownerEpoch==beforeOwner.ownerEpoch
		&&owner.lastEntryGeneration==beforeOwner.lastEntryGeneration&&!s_allocs);
	CHECK(VK_TemporalGenericRecipeTablePrepare(&owner,4,&auth,&ops));
	CHECK(owner.records&&owner.armed&&owner.ownerEpoch==1&&s_allocs==1);
	memset(&in,0,sizeof(in));in.slot=2;in.catalogId=id;in.key=key;in.base=&f.gp;
	in.sceneFormat=RAL_FORMAT_R16G16B16A16_SFLOAT;in.depthFormat=RAL_FORMAT_D32_SFLOAT;
	in.layoutClass=VK_TEMPORAL_RECIPE_LAYOUT_GENERIC_MAIN;
	CHECK(VK_TemporalGenericRecipeTableCapture(&owner,&in,&receipt));
	gen=owner.lastEntryGeneration;epoch=owner.ownerEpoch;
	CHECK(receipt.ownerEpoch==epoch&&receipt.slot==2&&receipt.entryGeneration==gen
		&&receipt.catalogId==id);
	CHECK(VK_TemporalGenericRecipeTableGet(&owner,epoch,2,gen,&got));
	CHECK(got.valid&&got.slot==2&&got.catalogId==id&&got.ownerEpoch==epoch
		&&got.entryGeneration==gen&&got.numBindings==2&&got.numAttributes==2
		&&got.specializationWords[0]==1&&got.sceneBlend.colorWriteMask==f.attachment.colorWriteMask
		&&got.capturedAuthority.frameId==auth.frameId);
	CHECK(VK_TemporalGenericRecipeBuildView(&got,
		(VkShaderModule)(uintptr_t)101,(VkShaderModule)(uintptr_t)102,
		(VkPipelineLayout)(uintptr_t)103,&view));
	CHECK(view.gp.pStages==view.stages
		&&view.gp.pVertexInputState==&view.vertexInput
		&&view.gp.pInputAssemblyState==&view.inputAssembly
		&&view.gp.pViewportState==&view.viewport
		&&view.gp.pRasterizationState==&view.rasterization
		&&view.gp.pMultisampleState==&view.multisample
		&&view.gp.pDepthStencilState==&view.depthStencil
		&&view.gp.pColorBlendState==&view.colorBlend
		&&view.gp.pDynamicState==&view.dynamic
		&&view.stages[0].pSpecializationInfo==&view.specialization.vertexInfo
		&&view.stages[1].pSpecializationInfo==&view.specialization.fragmentInfo
		&&view.specialization.vertexInfo.pMapEntries==view.specialization.vertexMap
		&&view.specialization.fragmentInfo.pMapEntries==view.specialization.fragmentMaps
		&&view.specialization.vertexInfo.pData==&view.vertexWord
		&&view.specialization.fragmentInfo.pData==view.fragmentWords
		&&view.vertexInput.pVertexBindingDescriptions==view.bindings
		&&view.vertexInput.pVertexAttributeDescriptions==view.attributes
		&&view.colorBlend.pAttachments==&view.sceneBlend
		&&view.dynamic.pDynamicStates==view.dynamicStates
		&&view.stages[0].module==(VkShaderModule)(uintptr_t)101
		&&view.stages[1].module==(VkShaderModule)(uintptr_t)102
		&&view.gp.layout==(VkPipelineLayout)(uintptr_t)103);
	{ vkGenericSpecializationFacts_t facts={got.key.textureCount,got.key.shaderFog,
		got.alphaTested};
		CHECK(VK_GenericTemporalSpecializationValidate(&view.gp,&facts,NULL)); }
	badRecipe=got;badRecipe.valid=qfalse;memset(&view,0xA5,sizeof(view));viewBefore=view;
	CHECK(!VK_TemporalGenericRecipeBuildView(&badRecipe,
		(VkShaderModule)(uintptr_t)101,(VkShaderModule)(uintptr_t)102,
		(VkPipelineLayout)(uintptr_t)103,&view));
	CHECK(memcmp(&view,&viewBefore,sizeof(view))==0);
	// The recipe owns every nested array/value: poison all borrowed graph seams
	// after capture and prove the retrieved record is unchanged field-wise.
	f.bindings[0].stride=999;f.bindings[1].binding=99;
	f.attrs[0].format=VK_FORMAT_UNDEFINED;f.attrs[1].offset=999;
	f.vsWord=0;
	{ uint32_t poisonIndex;for(poisonIndex=0;poisonIndex<VK_GENERIC_FRAGMENT_SPEC_COUNT;
		++poisonIndex)f.fsWords[poisonIndex]=0xdead0000u+poisonIndex; }
	f.stages[0].pName="poison-vs";f.stages[1].pName="poison-fs";
	f.rs.lineWidth=99.0f;f.ds.depthCompareOp=VK_COMPARE_OP_NEVER;
	f.attachment.colorWriteMask=0;
	f.dyns[0]=VK_DYNAMIC_STATE_LINE_WIDTH;f.dyns[1]=VK_DYNAMIC_STATE_LINE_WIDTH;
	CHECK(VK_TemporalGenericRecipeTableGet(&owner,epoch,2,gen,&gotAgain));
	CHECK(RecipeSemanticsEqual(&got,&gotAgain));
	CHECK(gotAgain.bindings[0].stride==got.bindings[0].stride
		&&gotAgain.attributes[0].format==got.attributes[0].format
		&&gotAgain.specializationWords[0]==got.specializationWords[0]
		&&gotAgain.specializationWords[3]==got.specializationWords[3]
		&&gotAgain.lineWidth==got.lineWidth&&gotAgain.depthCompareOp==got.depthCompareOp
		&&gotAgain.sceneBlend.colorWriteMask==got.sceneBlend.colorWriteMask
		&&gotAgain.dynamicStates[0]==got.dynamicStates[0]);
	// A semantically identical nested graph at unrelated addresses normalizes to
	// the same pointer-free recipe (identity/generation fields intentionally differ).
	CHECK(MakeFixture(&equivalentFixture));in.base=&equivalentFixture.gp;in.slot=1;
	CHECK(VK_TemporalGenericRecipeTableCapture(&owner,&in,&receipt));
	CHECK(VK_TemporalGenericRecipeTableGet(&owner,receipt.ownerEpoch,receipt.slot,
		receipt.entryGeneration,&equivalentRecipe));
	CHECK(RecipeSemanticsEqual(&got,&equivalentRecipe));
	in.slot=2;
	CHECK(MakeFixture(&f));in.base=&f.gp;

	// Every malformed Prepare attempt disarms an already armed owner before it
	// validates the new authority. Stale batch authority can never capture.
#define CHECK_DISARMED_CAPTURE_REJECTS() do { \
		vkTemporalGenericRecipeReceipt_t sentinel = { 71, 72, 73, 74 }; \
		CHECK(!owner.armed && !owner.authority.token && !owner.authority.frameId \
			&& !owner.authority.planGeneration \
			&& !owner.authority.materializationGeneration); \
		CHECK(!VK_TemporalGenericRecipeTableCapture(&owner,&in,&sentinel)); \
		CHECK(sentinel.ownerEpoch==71&&sentinel.slot==72 \
			&&sentinel.entryGeneration==73&&sentinel.catalogId==74); \
	} while (0)
	badAuth=auth;badAuth.token=0;
	CHECK(!VK_TemporalGenericRecipeTablePrepare(&owner,4,&badAuth,&ops));
	CHECK_DISARMED_CAPTURE_REJECTS();
	CHECK(VK_TemporalGenericRecipeTablePrepare(&owner,4,&auth,&ops));
	CHECK(!VK_TemporalGenericRecipeTablePrepare(&owner,0,&auth,&ops));
	CHECK_DISARMED_CAPTURE_REJECTS();
	CHECK(VK_TemporalGenericRecipeTablePrepare(&owner,4,&auth,&ops));
	CHECK(!VK_TemporalGenericRecipeTablePrepare(&owner,4,NULL,&ops));
	CHECK_DISARMED_CAPTURE_REJECTS();
	CHECK(VK_TemporalGenericRecipeTablePrepare(&owner,4,&auth,&ops));
	CHECK(!VK_TemporalGenericRecipeTablePrepare(&owner,4,&auth,NULL));
	CHECK_DISARMED_CAPTURE_REJECTS();
	CHECK(VK_TemporalGenericRecipeTablePrepare(&owner,4,&auth,&ops));
	{ vkTemporalRecipeTableOps_t badOps={NULL,FakeFree};
		CHECK(!VK_TemporalGenericRecipeTablePrepare(&owner,4,&auth,&badOps)); }
	CHECK_DISARMED_CAPTURE_REJECTS();
	CHECK(VK_TemporalGenericRecipeTablePrepare(&owner,4,&auth,&ops));
	{ vkTemporalRecipeTableOps_t badOps={FakeAlloc,NULL};
		CHECK(!VK_TemporalGenericRecipeTablePrepare(&owner,4,&auth,&badOps)); }
	CHECK_DISARMED_CAPTURE_REJECTS();
	CHECK(VK_TemporalGenericRecipeTablePrepare(&owner,4,&auth,&ops));
#undef CHECK_DISARMED_CAPTURE_REJECTS
	memset(&got,0x5A,sizeof(got));gotBefore=got;
	CHECK(!VK_TemporalGenericRecipeTableGet(&owner,epoch,2,gen+1,&got)
		&&got.ownerEpoch==gotBefore.ownerEpoch&&got.slot==gotBefore.slot
		&&got.entryGeneration==gotBefore.entryGeneration&&got.catalogId==gotBefore.catalogId);
	CHECK(VK_TemporalGenericRecipeTableGetSlotReceipt(&owner,2,&receipt));
	receiptBefore=(vkTemporalGenericRecipeReceipt_t){91,92,93,94};receipt=receiptBefore;
	CHECK(!VK_TemporalGenericRecipeTableGetSlotReceipt(&owner,3,&receipt)
		&&receipt.ownerEpoch==receiptBefore.ownerEpoch&&receipt.slot==receiptBefore.slot
		&&receipt.entryGeneration==receiptBefore.entryGeneration&&receipt.catalogId==receiptBefore.catalogId);

	// Rejected replacement invalidates the old slot and consumes no generation.
	{ uint32_t generationBeforeReject=owner.lastEntryGeneration;
		f.attachment.blendEnable=VK_TRUE;
		CHECK(!VK_TemporalGenericRecipeTableCapture(&owner,&in,&receipt));
		CHECK(owner.lastEntryGeneration==generationBeforeReject
			&&!VK_TemporalGenericRecipeTableGet(&owner,epoch,2,gen,&got));
		CHECK(MakeFixture(&f));in.base=&f.gp;
		CHECK(VK_TemporalGenericRecipeTableCapture(&owner,&in,&receipt));
		CHECK(owner.lastEntryGeneration==generationBeforeReject+1u); }
	gen=owner.lastEntryGeneration;
	// Stored scalar poison is rejected before publication.
#define REJECT_FIXTURE_POISON(statement) do { \
		CHECK(MakeFixture(&f));in.base=&f.gp;statement; \
		CHECK(!VK_TemporalGenericRecipeTableCapture(&owner,&in,&receipt)); \
	} while (0)
	REJECT_FIXTURE_POISON(f.rs.lineWidth=0.0f);
	REJECT_FIXTURE_POISON(f.rs.lineWidth=NAN);
	REJECT_FIXTURE_POISON(f.rs.depthBiasConstantFactor=NAN);
	REJECT_FIXTURE_POISON(f.rs.depthBiasClamp=INFINITY);
	REJECT_FIXTURE_POISON(f.rs.depthBiasSlopeFactor=-INFINITY);
	REJECT_FIXTURE_POISON(f.rs.polygonMode=VK_POLYGON_MODE_LINE);
	REJECT_FIXTURE_POISON(f.rs.cullMode=(VkCullModeFlags)0x80000000u);
	REJECT_FIXTURE_POISON(f.rs.frontFace=(VkFrontFace)99);
	REJECT_FIXTURE_POISON(f.ms.minSampleShading=0.0f);
	REJECT_FIXTURE_POISON(f.ms.minSampleShading=NAN);
	REJECT_FIXTURE_POISON(f.ds.minDepthBounds=NAN);
	REJECT_FIXTURE_POISON(f.ds.maxDepthBounds=INFINITY);
	REJECT_FIXTURE_POISON(f.ds.maxDepthBounds=2.0f);
	REJECT_FIXTURE_POISON(f.ds.depthCompareOp=VK_COMPARE_OP_NEVER);
	REJECT_FIXTURE_POISON(f.ds.front.failOp=VK_STENCIL_OP_ZERO);
	REJECT_FIXTURE_POISON(f.cb.blendConstants[0]=-0.0f);
	REJECT_FIXTURE_POISON(f.cb.blendConstants[1]=NAN);
	REJECT_FIXTURE_POISON(f.cb.logicOp=VK_LOGIC_OP_CLEAR);
	REJECT_FIXTURE_POISON(f.bindings[0].inputRate=VK_VERTEX_INPUT_RATE_INSTANCE);
	REJECT_FIXTURE_POISON(f.attrs[0].format=VK_FORMAT_UNDEFINED);
#undef REJECT_FIXTURE_POISON
	CHECK(MakeFixture(&f));in.base=&f.gp;
	CHECK(VK_TemporalGenericRecipeTableEvictRange(&owner,2,3));
	CHECK(!VK_TemporalGenericRecipeTableGet(&owner,epoch,2,gen,&got));
	CHECK(!VK_TemporalGenericRecipeTableEvictRange(&owner,3,2));

	// Stable storage re-arm, then release/re-enable epoch and generation monotonicity.
	auth.token++;auth.frameId++;CHECK(VK_TemporalGenericRecipeTablePrepare(&owner,4,&auth,&ops));
	CHECK(s_allocs==1&&owner.ownerEpoch==epoch&&owner.armed);
	CHECK(!VK_TemporalGenericRecipeTablePrepare(&owner,5,&auth,&ops)&&!owner.armed);
	CHECK(VK_TemporalGenericRecipeTableRelease(&owner,&ops));
	CHECK(!owner.records&&owner.ownerEpoch==epoch&&owner.lastEntryGeneration==gen&&s_frees==1);
	CHECK(VK_TemporalGenericRecipeTablePrepare(&owner,4,&auth,&ops));
	CHECK(owner.ownerEpoch==epoch+1&&s_allocs==2);
	owner.lastEntryGeneration=UINT32_MAX;
	receipt=(vkTemporalGenericRecipeReceipt_t){71,72,73,74};receiptBefore=receipt;
	CHECK(!VK_TemporalGenericRecipeTableCapture(&owner,&in,&receipt)
		&&receipt.ownerEpoch==receiptBefore.ownerEpoch&&receipt.slot==receiptBefore.slot
		&&receipt.entryGeneration==receiptBefore.entryGeneration&&receipt.catalogId==receiptBefore.catalogId);
	VK_TemporalGenericRecipeTableDisarm(&owner);CHECK(!owner.armed);
	CHECK(!VK_TemporalGenericRecipeTableCapture(&owner,&in,&receipt));
	CHECK(VK_TemporalGenericRecipeTableRelease(&owner,&ops)&&s_frees==2);

	// Allocation and epoch saturation are fail-closed and allocation-atomic.
	s_failAlloc=1;beforeOwner=owner;
	CHECK(!VK_TemporalGenericRecipeTablePrepare(&owner,4,&auth,&ops)
		&&!owner.records&&owner.ownerEpoch==beforeOwner.ownerEpoch);
	s_failAlloc=0;owner.ownerEpoch=UINT32_MAX;
	CHECK(!VK_TemporalGenericRecipeTablePrepare(&owner,4,&auth,&ops)&&!owner.records);
	puts("vk temporal generic recipe table contract: PASS");return 0;
}
