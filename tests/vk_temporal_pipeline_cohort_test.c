// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_pipeline_cohort.h"
#include "ral.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL temporal pipeline cohort line %d: %s\n", __LINE__, #x ); return 1; } } while ( 0 )

struct ralBackend_s { int id; };
struct ralPipelineLayout_s { int id; };
struct ralPipeline_s { int id; };

static ralCaps_t caps;
static int supportScene = 1, supportVelocity = 1, supportValidity = 1;
static int badUsageQuery, exactCreateCalls, exactCreateSucceeds = 1;
static VkGraphicsPipelineCreateInfo capturedCreate;
static VkPipelineColorBlendStateCreateInfo capturedBlend;
static VkPipelineColorBlendAttachmentState capturedAttachments[3];
static ralFormat_t capturedFormats[3], capturedDepth;
static ralPipelineLayout_t *capturedLayout;
static const char *capturedName;
static struct ralPipeline_s fakePipeline = { 7 };

const ralCaps_t *Ral_GetCaps( ralBackend_t *backend ) {
	return backend ? &caps : NULL;
}

qboolean Ral_TextureFormatSupports( ralBackend_t *backend, ralFormat_t format,
		ralTextureUsage_t usage ) {
	const ralTextureUsage_t aux = (ralTextureUsage_t)(
		RAL_TEXTURE_USAGE_COLOR_ATTACHMENT | RAL_TEXTURE_USAGE_SAMPLED
		| RAL_TEXTURE_USAGE_TRANSFER_SRC );
	if ( !backend ) badUsageQuery = 1;
	if ( format == RAL_FORMAT_R16G16B16A16_SFLOAT ) {
		if ( usage != RAL_TEXTURE_USAGE_COLOR_ATTACHMENT ) badUsageQuery = 1;
		return supportScene;
	}
	if ( format == RAL_FORMAT_R16G16_SFLOAT ) {
		if ( usage != aux ) badUsageQuery = 1;
		return supportVelocity;
	}
	if ( format == RAL_FORMAT_R8_UNORM ) {
		if ( usage != aux ) badUsageQuery = 1;
		return supportValidity;
	}
	badUsageQuery = 1;
	return qfalse;
}

static ralPipeline_t *FakeExactCreate(
		const VkGraphicsPipelineCreateInfo *createInfo,
		ralPipelineLayout_t *layout, const ralFormat_t *colorFormats,
		uint32_t numColorFormats, ralFormat_t depthFormat,
		const char *debugName ) {
	exactCreateCalls++;
	if ( !createInfo || !createInfo->pColorBlendState || !colorFormats
			|| numColorFormats != 3 ) return NULL;
	capturedCreate = *createInfo;
	capturedBlend = *createInfo->pColorBlendState;
	memcpy( capturedAttachments, capturedBlend.pAttachments,
		sizeof( capturedAttachments ) );
	memcpy( capturedFormats, colorFormats, sizeof( capturedFormats ) );
	capturedLayout = layout;
	capturedDepth = depthFormat;
	capturedName = debugName;
	return exactCreateSucceeds ? &fakePipeline : NULL;
}

int main( void ) {
	VkPipelineColorBlendAttachmentState base;
	vkTemporalPreserveContract_t out, sentinel;
	struct ralBackend_s backend = { 1 };
	struct ralPipelineLayout_s layout = { 2 };
	VkPipelineColorBlendStateCreateInfo blend;
	VkGraphicsPipelineCreateInfo create;
	VkPipelineShaderStageCreateInfo stage;
	VkPipelineVertexInputStateCreateInfo vertexInput;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineViewportStateCreateInfo viewport;
	VkPipelineRasterizationStateCreateInfo raster;
	VkPipelineMultisampleStateCreateInfo multisample;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineDynamicStateCreateInfo dynamic;
	VkGraphicsPipelineCreateInfo createBefore;
	VkPipelineColorBlendStateCreateInfo blendBefore;
	VkPipelineColorBlendAttachmentState baseBefore;
	ralPipeline_t *pipeline, *sentinelPipeline = (ralPipeline_t *)(uintptr_t)0x1;

	memset( &base, 0, sizeof( base ) );
	base.blendEnable = VK_TRUE;
	base.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
	base.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	base.colorBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;
	base.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	base.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	base.alphaBlendOp = VK_BLEND_OP_MAX;
	base.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_B_BIT;
	memset( &sentinel, 0xa5, sizeof( sentinel ) );
	out = sentinel;
	CHECK( !VK_TemporalPreserveContractBuild( RAL_FORMAT_UNDEFINED, &base, &out ) );
	CHECK( memcmp( &out, &sentinel, sizeof( out ) ) == 0 );
	CHECK( !VK_TemporalPreserveContractBuild( RAL_FORMAT_R16G16B16A16_SFLOAT,
		NULL, &out ) );
	CHECK( memcmp( &out, &sentinel, sizeof( out ) ) == 0 );
	CHECK( !VK_TemporalPreserveContractBuild( RAL_FORMAT_R16G16B16A16_SFLOAT,
		&base, NULL ) );

	CHECK( VK_TemporalPreserveContractBuild(
		RAL_FORMAT_R16G16B16A16_SFLOAT, &base, &out ) );
	CHECK( out.count == 3 );
	CHECK( out.formats[0] == RAL_FORMAT_R16G16B16A16_SFLOAT );
	CHECK( out.formats[1] == RAL_FORMAT_R16G16_SFLOAT );
	CHECK( out.formats[2] == RAL_FORMAT_R8_UNORM );
	CHECK( memcmp( &out.blends[0], &base, sizeof( base ) ) == 0 );
	CHECK( out.blends[1].blendEnable == VK_FALSE
		&& out.blends[1].colorWriteMask == 0 );
	CHECK( out.blends[2].blendEnable == VK_FALSE
		&& out.blends[2].colorWriteMask == 0 );
	CHECK( out.blends[1].srcColorBlendFactor == VK_BLEND_FACTOR_ZERO
		&& out.blends[1].dstColorBlendFactor == VK_BLEND_FACTOR_ZERO
		&& out.blends[1].colorBlendOp == VK_BLEND_OP_ADD
		&& out.blends[1].srcAlphaBlendFactor == VK_BLEND_FACTOR_ZERO
		&& out.blends[1].dstAlphaBlendFactor == VK_BLEND_FACTOR_ZERO
		&& out.blends[1].alphaBlendOp == VK_BLEND_OP_ADD );
	CHECK( memcmp( &out.blends[1], &out.blends[2],
		sizeof( out.blends[1] ) ) == 0 );

	memset( &caps, 0, sizeof( caps ) );
	caps.independentBlend = qtrue;
	caps.maxColorAttachments = 8;
	memset( &blend, 0, sizeof( blend ) );
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.flags = 0x12u;
	blend.logicOpEnable = VK_TRUE;
	blend.logicOp = VK_LOGIC_OP_XOR;
	blend.blendConstants[0] = 0.25f;
	blend.blendConstants[3] = 0.75f;
	blend.attachmentCount = 1;
	blend.pAttachments = &base;
	memset( &stage, 0, sizeof( stage ) );
	memset( &vertexInput, 0, sizeof( vertexInput ) );
	memset( &inputAssembly, 0, sizeof( inputAssembly ) );
	memset( &viewport, 0, sizeof( viewport ) );
	memset( &raster, 0, sizeof( raster ) );
	memset( &multisample, 0, sizeof( multisample ) );
	memset( &depthStencil, 0, sizeof( depthStencil ) );
	memset( &dynamic, 0, sizeof( dynamic ) );
	memset( &create, 0, sizeof( create ) );
	create.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create.flags = 0x345u;
	create.stageCount = 1;
	create.pStages = &stage;
	create.pVertexInputState = &vertexInput;
	create.pInputAssemblyState = &inputAssembly;
	create.pViewportState = &viewport;
	create.pRasterizationState = &raster;
	create.pMultisampleState = &multisample;
	create.pDepthStencilState = &depthStencil;
	create.pColorBlendState = &blend;
	create.pDynamicState = &dynamic;
	create.layout = (VkPipelineLayout)(uintptr_t)0x1234;
	create.renderPass = (VkRenderPass)(uintptr_t)0x5678;
	create.subpass = 3;
	create.basePipelineHandle = (VkPipeline)(uintptr_t)0x9abc;
	create.basePipelineIndex = -9;
	pipeline = NULL;
	CHECK( !VK_TemporalPreservePipelineCreate( NULL, &create, &layout,
		RAL_FORMAT_R16G16B16A16_SFLOAT, RAL_FORMAT_D32_SFLOAT, "cohort",
		FakeExactCreate, &pipeline ) );
	CHECK( !VK_TemporalPreservePipelineCreate( &backend, NULL, &layout,
		RAL_FORMAT_R16G16B16A16_SFLOAT, RAL_FORMAT_D32_SFLOAT, "cohort",
		FakeExactCreate, &pipeline ) );
	pipeline = sentinelPipeline;
	CHECK( !VK_TemporalPreservePipelineCreate( &backend, &create, &layout,
		RAL_FORMAT_R16G16B16A16_SFLOAT, RAL_FORMAT_D32_SFLOAT, "cohort",
		FakeExactCreate, &pipeline ) );
	CHECK( pipeline == sentinelPipeline && exactCreateCalls == 0 );
	pipeline = NULL;
	caps.independentBlend = qfalse;
	CHECK( !VK_TemporalPreservePipelineCreate( &backend, &create, &layout,
		RAL_FORMAT_R16G16B16A16_SFLOAT, RAL_FORMAT_D32_SFLOAT, "cohort",
		FakeExactCreate, &pipeline ) );
	caps.independentBlend = qtrue;
	caps.maxColorAttachments = 2;
	CHECK( !VK_TemporalPreservePipelineCreate( &backend, &create, &layout,
		RAL_FORMAT_R16G16B16A16_SFLOAT, RAL_FORMAT_D32_SFLOAT, "cohort",
		FakeExactCreate, &pipeline ) );
	caps.maxColorAttachments = 8;
	blend.attachmentCount = 2;
	CHECK( !VK_TemporalPreservePipelineCreate( &backend, &create, &layout,
		RAL_FORMAT_R16G16B16A16_SFLOAT, RAL_FORMAT_D32_SFLOAT, "cohort",
		FakeExactCreate, &pipeline ) );
	blend.attachmentCount = 1;
	supportVelocity = 0;
	CHECK( !VK_TemporalPreservePipelineCreate( &backend, &create, &layout,
		RAL_FORMAT_R16G16B16A16_SFLOAT, RAL_FORMAT_D32_SFLOAT, "cohort",
		FakeExactCreate, &pipeline ) );
	supportVelocity = 1;
	supportValidity = 0;
	CHECK( !VK_TemporalPreservePipelineCreate( &backend, &create, &layout,
		RAL_FORMAT_R16G16B16A16_SFLOAT, RAL_FORMAT_D32_SFLOAT, "cohort",
		FakeExactCreate, &pipeline ) );
	supportValidity = 1;
	supportScene = 0;
	CHECK( !VK_TemporalPreservePipelineCreate( &backend, &create, &layout,
		RAL_FORMAT_R16G16B16A16_SFLOAT, RAL_FORMAT_D32_SFLOAT, "cohort",
		FakeExactCreate, &pipeline ) );
	supportScene = 1;
	CHECK( exactCreateCalls == 0 );
	createBefore = create;
	blendBefore = blend;
	baseBefore = base;
	exactCreateSucceeds = 0;
	CHECK( !VK_TemporalPreservePipelineCreate( &backend, &create, &layout,
		RAL_FORMAT_R16G16B16A16_SFLOAT, RAL_FORMAT_D32_SFLOAT, "cohort",
		FakeExactCreate, &pipeline ) );
	CHECK( pipeline == NULL && exactCreateCalls == 1 );
	exactCreateSucceeds = 1;
	CHECK( VK_TemporalPreservePipelineCreate( &backend, &create, &layout,
		RAL_FORMAT_R16G16B16A16_SFLOAT, RAL_FORMAT_D32_SFLOAT, "cohort",
		FakeExactCreate, &pipeline ) );
	CHECK( pipeline == &fakePipeline && exactCreateCalls == 2 && !badUsageQuery );
	CHECK( memcmp( &create, &createBefore, sizeof( create ) ) == 0
		&& memcmp( &blend, &blendBefore, sizeof( blend ) ) == 0
		&& memcmp( &base, &baseBefore, sizeof( base ) ) == 0 );
	CHECK( capturedCreate.sType == create.sType
		&& capturedCreate.flags == create.flags
		&& capturedCreate.stageCount == create.stageCount
		&& capturedCreate.pStages == create.pStages
		&& capturedCreate.pVertexInputState == create.pVertexInputState
		&& capturedCreate.pInputAssemblyState == create.pInputAssemblyState
		&& capturedCreate.pViewportState == create.pViewportState
		&& capturedCreate.pRasterizationState == create.pRasterizationState
		&& capturedCreate.pMultisampleState == create.pMultisampleState
		&& capturedCreate.pDepthStencilState == create.pDepthStencilState
		&& capturedCreate.pDynamicState == create.pDynamicState
		&& capturedCreate.layout == create.layout
		&& capturedCreate.renderPass == create.renderPass
		&& capturedCreate.subpass == create.subpass
		&& capturedCreate.basePipelineHandle == create.basePipelineHandle
		&& capturedCreate.basePipelineIndex == create.basePipelineIndex );
	CHECK( capturedBlend.attachmentCount == 3 );
	CHECK( capturedBlend.sType == blend.sType
		&& capturedBlend.flags == blend.flags
		&& capturedBlend.logicOpEnable == blend.logicOpEnable
		&& capturedBlend.logicOp == blend.logicOp
		&& memcmp( capturedBlend.blendConstants, blend.blendConstants,
			sizeof( blend.blendConstants ) ) == 0 );
	CHECK( capturedFormats[0] == RAL_FORMAT_R16G16B16A16_SFLOAT
		&& capturedFormats[1] == RAL_FORMAT_R16G16_SFLOAT
		&& capturedFormats[2] == RAL_FORMAT_R8_UNORM );
	CHECK( capturedDepth == RAL_FORMAT_D32_SFLOAT
		&& capturedLayout == &layout && strcmp( capturedName, "cohort" ) == 0 );
	CHECK( memcmp( &capturedAttachments[0], &base, sizeof( base ) ) == 0 );
	CHECK( capturedAttachments[1].colorWriteMask == 0
		&& capturedAttachments[2].colorWriteMask == 0
		&& capturedAttachments[1].blendEnable == VK_FALSE
		&& capturedAttachments[2].blendEnable == VK_FALSE );

	puts( "PASS inert temporal pipeline cohort contract" );
	return 0;
}
