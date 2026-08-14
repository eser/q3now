// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_generic_specialization_contract.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; \
} } while (0)

typedef struct {
	uint32_t vertexWord;
	uint32_t fragmentWords[VK_GENERIC_FRAGMENT_SPEC_COUNT];
	vkGenericSpecializationGraph_t specialization;
	VkPipelineShaderStageCreateInfo stages[2];
	VkVertexInputBindingDescription binding;
	VkVertexInputAttributeDescription attribute;
	VkPipelineVertexInputStateCreateInfo vertexInput;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineViewportStateCreateInfo viewport;
	VkPipelineRasterizationStateCreateInfo raster;
	VkPipelineMultisampleStateCreateInfo multisample;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineColorBlendAttachmentState attachment;
	VkPipelineColorBlendStateCreateInfo colorBlend;
	VkDynamicState dynamicStates[2];
	VkPipelineDynamicStateCreateInfo dynamic;
	VkGraphicsPipelineCreateInfo pipeline;
} fixture_t;

static uint32_t FloatBits( float value ) {
	uint32_t bits;
	memcpy( &bits, &value, sizeof(bits) );
	return bits;
}

static int FixtureInit( fixture_t *f ) {
	memset( f, 0, sizeof(*f) );
	f->vertexWord = 1u;
	f->fragmentWords[2] = FloatBits(0.85f);
	f->fragmentWords[4] = 3u;
	f->fragmentWords[6] = 7u;
	f->fragmentWords[8] = FloatBits(0.25f);
	f->fragmentWords[9] = FloatBits(0.75f);
	f->fragmentWords[11] = FloatBits(2.0f);
	f->fragmentWords[14] = 2u;
	if ( !VK_GenericSpecializationAuthor( &f->specialization,
			&f->vertexWord, f->fragmentWords ) ) return 0;
	f->stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	f->stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	f->stages[0].module = (VkShaderModule)(uintptr_t)51;
	f->stages[0].pName = "main";
	f->stages[0].pSpecializationInfo = &f->specialization.vertexInfo;
	f->stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	f->stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	f->stages[1].module = (VkShaderModule)(uintptr_t)52;
	f->stages[1].pName = "main";
	f->stages[1].pSpecializationInfo = &f->specialization.fragmentInfo;
	f->binding.binding = 0u;
	f->binding.stride = 32u;
	f->binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	f->attribute.location = 0u;
	f->attribute.binding = 0u;
	f->attribute.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	f->vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	f->vertexInput.vertexBindingDescriptionCount = 1u;
	f->vertexInput.pVertexBindingDescriptions = &f->binding;
	f->vertexInput.vertexAttributeDescriptionCount = 1u;
	f->vertexInput.pVertexAttributeDescriptions = &f->attribute;
	f->inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	f->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	f->viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	f->viewport.viewportCount = 1u;
	f->viewport.scissorCount = 1u;
	f->raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	f->raster.polygonMode = VK_POLYGON_MODE_FILL;
	f->raster.cullMode = VK_CULL_MODE_BACK_BIT;
	f->raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
	f->raster.lineWidth = 1.0f;
	f->multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	f->multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	f->multisample.minSampleShading = 1.0f;
	f->depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	f->depthStencil.depthTestEnable = VK_TRUE;
	f->depthStencil.depthWriteEnable = VK_TRUE;
	f->depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	f->attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	f->colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	f->colorBlend.attachmentCount = 1u;
	f->colorBlend.pAttachments = &f->attachment;
	f->dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
	f->dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;
	f->dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	f->dynamic.dynamicStateCount = 2u;
	f->dynamic.pDynamicStates = f->dynamicStates;
	f->pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	f->pipeline.stageCount = 2u;
	f->pipeline.pStages = f->stages;
	f->pipeline.pVertexInputState = &f->vertexInput;
	f->pipeline.pInputAssemblyState = &f->inputAssembly;
	f->pipeline.pViewportState = &f->viewport;
	f->pipeline.pRasterizationState = &f->raster;
	f->pipeline.pMultisampleState = &f->multisample;
	f->pipeline.pDepthStencilState = &f->depthStencil;
	f->pipeline.pColorBlendState = &f->colorBlend;
	f->pipeline.pDynamicState = &f->dynamic;
	f->pipeline.layout = (VkPipelineLayout)(uintptr_t)71;
	f->pipeline.renderPass = (VkRenderPass)(uintptr_t)72;
	f->pipeline.basePipelineIndex = -1;
	return 1;
}

int main( void ) {
	static const uint32_t fragmentIds[VK_GENERIC_FRAGMENT_SPEC_COUNT] = {
		0u,1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,15u,14u,26u
	};
	fixture_t f;
	vkGenericSpecializationFacts_t facts = { 1u, qfalse };
	vkGenericSpecializationReceipt_t receipt, before;
	vkGenericSpecializationGraph_t graph, graphBefore;
	uint32_t vertexBefore, fragmentBefore[VK_GENERIC_FRAGMENT_SPEC_COUNT];
	uint32_t i, saved32;
	size_t savedSize;
	const void *savedPtr;

	CHECK( FixtureInit(&f) );
	CHECK( VK_GENERIC_TOTAL_SPEC_COUNT == 16
		&& VK_GENERIC_TRANSLATOR_SPEC_CAPACITY == 18
		&& VK_GENERIC_TOTAL_SPEC_COUNT <= VK_GENERIC_TRANSLATOR_SPEC_CAPACITY );
	CHECK( f.pipeline.pVertexInputState && f.pipeline.pInputAssemblyState
		&& f.pipeline.pViewportState && f.pipeline.pRasterizationState
		&& f.pipeline.pMultisampleState && f.pipeline.pDepthStencilState
		&& f.pipeline.pColorBlendState && f.pipeline.pDynamicState );
	CHECK( f.specialization.vertexInfo.mapEntryCount == 1u
		&& f.specialization.vertexInfo.dataSize == 4u
		&& f.specialization.vertexInfo.pMapEntries == f.specialization.vertexMap
		&& f.specialization.vertexInfo.pData == &f.vertexWord );
	CHECK( f.specialization.fragmentInfo.mapEntryCount == 15u
		&& f.specialization.fragmentInfo.dataSize == 60u
		&& f.specialization.fragmentInfo.pMapEntries == f.specialization.fragmentMaps
		&& f.specialization.fragmentInfo.pData == f.fragmentWords );
	CHECK( f.specialization.vertexMap[0].constantID == 16u
		&& f.specialization.vertexMap[0].offset == 0u
		&& f.specialization.vertexMap[0].size == 4u );
	for ( i = 0; i < VK_GENERIC_FRAGMENT_SPEC_COUNT; ++i )
		CHECK( f.specialization.fragmentMaps[i].constantID == fragmentIds[i]
			&& f.specialization.fragmentMaps[i].offset == i * 4u
			&& f.specialization.fragmentMaps[i].size == 4u );
	vertexBefore = f.vertexWord;
	memcpy( fragmentBefore, f.fragmentWords, sizeof(fragmentBefore) );
	memset( &graph, 0xa5, sizeof(graph) ); graphBefore = graph;
	CHECK( !VK_GenericSpecializationAuthor(&graph,NULL,f.fragmentWords)
		&& memcmp(&graph,&graphBefore,sizeof(graph)) == 0 );
	CHECK( !VK_GenericSpecializationAuthor(&graph,&f.vertexWord,NULL)
		&& memcmp(&graph,&graphBefore,sizeof(graph)) == 0 );
	CHECK( f.vertexWord == vertexBefore
		&& memcmp(f.fragmentWords,fragmentBefore,sizeof(fragmentBefore)) == 0 );

	memset( &receipt, 0x5a, sizeof(receipt) );
	CHECK( VK_GenericTemporalSpecializationValidate(&f.pipeline,&facts,&receipt) );
	CHECK( receipt.vertexWord == f.vertexWord
		&& memcmp(receipt.fragmentWords,f.fragmentWords,sizeof(f.fragmentWords)) == 0 );

#define REJECT_CURRENT() do { before = receipt; \
	CHECK(!VK_GenericTemporalSpecializationValidate(&f.pipeline,&facts,&receipt)); \
	CHECK(memcmp(&receipt,&before,sizeof(receipt)) == 0); \
} while (0)

	f.pipeline.stageCount = 1u; REJECT_CURRENT(); f.pipeline.stageCount = 2u;
	f.pipeline.stageCount = 3u; REJECT_CURRENT(); f.pipeline.stageCount = 2u;
	savedPtr = f.pipeline.pStages; f.pipeline.pStages = NULL; REJECT_CURRENT(); f.pipeline.pStages = savedPtr;
	f.stages[0].stage = VK_SHADER_STAGE_FRAGMENT_BIT; REJECT_CURRENT(); f.stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	f.stages[1].stage = VK_SHADER_STAGE_VERTEX_BIT; REJECT_CURRENT(); f.stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	savedPtr = f.stages[0].pSpecializationInfo; f.stages[0].pSpecializationInfo = NULL; REJECT_CURRENT(); f.stages[0].pSpecializationInfo = savedPtr;
	savedPtr = f.stages[1].pSpecializationInfo; f.stages[1].pSpecializationInfo = NULL; REJECT_CURRENT(); f.stages[1].pSpecializationInfo = savedPtr;
	{ const VkSpecializationInfo *savedVs=f.stages[0].pSpecializationInfo,*savedFs=f.stages[1].pSpecializationInfo;
		f.stages[0].pSpecializationInfo=savedFs;f.stages[1].pSpecializationInfo=savedVs;REJECT_CURRENT();
		f.stages[0].pSpecializationInfo=savedVs;f.stages[1].pSpecializationInfo=savedFs; }
	f.specialization.vertexInfo.mapEntryCount = 0u; REJECT_CURRENT(); f.specialization.vertexInfo.mapEntryCount = 1u;
	f.specialization.fragmentInfo.mapEntryCount = 14u; REJECT_CURRENT(); f.specialization.fragmentInfo.mapEntryCount = 15u;
	savedSize = f.specialization.vertexInfo.dataSize; f.specialization.vertexInfo.dataSize = 8u; REJECT_CURRENT(); f.specialization.vertexInfo.dataSize = savedSize;
	savedSize = f.specialization.fragmentInfo.dataSize; f.specialization.fragmentInfo.dataSize = 56u; REJECT_CURRENT(); f.specialization.fragmentInfo.dataSize = savedSize;
	savedPtr = f.specialization.vertexInfo.pMapEntries; f.specialization.vertexInfo.pMapEntries = NULL; REJECT_CURRENT(); f.specialization.vertexInfo.pMapEntries = savedPtr;
	savedPtr = f.specialization.fragmentInfo.pMapEntries; f.specialization.fragmentInfo.pMapEntries = NULL; REJECT_CURRENT(); f.specialization.fragmentInfo.pMapEntries = savedPtr;
	savedPtr = f.specialization.vertexInfo.pData; f.specialization.vertexInfo.pData = NULL; REJECT_CURRENT(); f.specialization.vertexInfo.pData = savedPtr;
	savedPtr = f.specialization.fragmentInfo.pData; f.specialization.fragmentInfo.pData = NULL; REJECT_CURRENT(); f.specialization.fragmentInfo.pData = savedPtr;

	saved32 = f.specialization.vertexMap[0].constantID; f.specialization.vertexMap[0].constantID = 0u; REJECT_CURRENT(); f.specialization.vertexMap[0].constantID = saved32;
	saved32 = f.specialization.vertexMap[0].offset; f.specialization.vertexMap[0].offset = 4u; REJECT_CURRENT(); f.specialization.vertexMap[0].offset = saved32;
	savedSize = f.specialization.vertexMap[0].size; f.specialization.vertexMap[0].size = 8u; REJECT_CURRENT(); f.specialization.vertexMap[0].size = savedSize;
	for ( i = 0; i < VK_GENERIC_FRAGMENT_SPEC_COUNT; ++i ) {
		saved32=f.specialization.fragmentMaps[i].constantID;f.specialization.fragmentMaps[i].constantID=99u;REJECT_CURRENT();f.specialization.fragmentMaps[i].constantID=saved32;
		saved32=f.specialization.fragmentMaps[i].offset;f.specialization.fragmentMaps[i].offset+=4u;REJECT_CURRENT();f.specialization.fragmentMaps[i].offset=saved32;
		savedSize=f.specialization.fragmentMaps[i].size;f.specialization.fragmentMaps[i].size=8u;REJECT_CURRENT();f.specialization.fragmentMaps[i].size=savedSize;
	}

	f.vertexWord=0u;REJECT_CURRENT();f.vertexWord=1u;
	f.fragmentWords[0]=1u;REJECT_CURRENT();f.fragmentWords[0]=0u;
	f.fragmentWords[1]=0x80000000u;REJECT_CURRENT();f.fragmentWords[1]=0u;
	f.fragmentWords[2]^=1u;REJECT_CURRENT();f.fragmentWords[2]=FloatBits(0.85f);
	f.fragmentWords[3]=1u;REJECT_CURRENT();f.fragmentWords[3]=0u;
	f.fragmentWords[4]=4u;REJECT_CURRENT();f.fragmentWords[4]=3u;
	f.fragmentWords[5]=1u;REJECT_CURRENT();f.fragmentWords[5]=0u;
	f.fragmentWords[6]=8u;REJECT_CURRENT();
	for(i=0;i<=7u;++i){f.fragmentWords[6]=i;CHECK(VK_GenericTemporalSpecializationValidate(&f.pipeline,&facts,NULL));}f.fragmentWords[6]=7u;
	f.fragmentWords[7]=1u;REJECT_CURRENT();f.fragmentWords[7]=0u;
	f.fragmentWords[8]=FloatBits(-0.01f);REJECT_CURRENT();f.fragmentWords[8]=FloatBits(1.01f);REJECT_CURRENT();f.fragmentWords[8]=0x7fc00000u;REJECT_CURRENT();f.fragmentWords[8]=FloatBits(0.25f);
	f.fragmentWords[9]=FloatBits(-1.0f);REJECT_CURRENT();f.fragmentWords[9]=0x7f800000u;REJECT_CURRENT();f.fragmentWords[9]=FloatBits(0.75f);
	f.fragmentWords[10]=1u;REJECT_CURRENT();facts.shaderFog=qtrue;
	for(i=0;i<=3u;++i){f.fragmentWords[10]=i;CHECK(VK_GenericTemporalSpecializationValidate(&f.pipeline,&facts,NULL));}
	f.fragmentWords[10]=4u;REJECT_CURRENT();f.fragmentWords[10]=0u;facts.shaderFog=qfalse;
	f.fragmentWords[11]=0u;REJECT_CURRENT();f.fragmentWords[11]=FloatBits(-1.0f);REJECT_CURRENT();f.fragmentWords[11]=0x7f800000u;REJECT_CURRENT();f.fragmentWords[11]=FloatBits(2.0f);
	f.fragmentWords[12]=1u;REJECT_CURRENT();f.fragmentWords[12]=0u;
	f.fragmentWords[13]=1u;REJECT_CURRENT();f.fragmentWords[13]=0u;
	f.fragmentWords[14]=3u;REJECT_CURRENT();f.fragmentWords[14]=2u;
	facts.textureCount=0u;f.fragmentWords[4]=1u;f.fragmentWords[6]=0u;f.fragmentWords[14]=1u;CHECK(VK_GenericTemporalSpecializationValidate(&f.pipeline,&facts,NULL));
	for(i=1u;i<=7u;++i){f.fragmentWords[6]=i;REJECT_CURRENT();}f.fragmentWords[6]=0u;f.fragmentWords[4]=2u;REJECT_CURRENT();
	facts.textureCount=2u;f.fragmentWords[4]=7u;f.fragmentWords[14]=3u;
	for(i=0u;i<=7u;++i){f.fragmentWords[6]=i;CHECK(VK_GenericTemporalSpecializationValidate(&f.pipeline,&facts,NULL));}f.fragmentWords[6]=7u;
	f.fragmentWords[4]=8u;REJECT_CURRENT();
	facts.textureCount=3u;REJECT_CURRENT();facts.textureCount=1u;f.fragmentWords[4]=3u;f.fragmentWords[14]=2u;
	CHECK( VK_GenericTemporalSpecializationValidate(&f.pipeline,&facts,&receipt) );

#undef REJECT_CURRENT
	puts("vk generic specialization contract: PASS");
	return 0;
}
