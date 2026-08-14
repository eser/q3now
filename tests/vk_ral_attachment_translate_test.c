// SPDX-License-Identifier: GPL-3.0-or-later

#include "vk_ral_attachment_translate.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); failures++; } } while ( 0 )

static void TestHeterogeneous( void ) {
	const ralFormat_t formats[3] = {
		RAL_FORMAT_R16G16B16A16_SFLOAT,
		RAL_FORMAT_R16G16_SFLOAT,
		RAL_FORMAT_R8_UNORM
	};
	VkPipelineColorBlendAttachmentState vkStates[3];
	VkPipelineColorBlendStateCreateInfo vkBlend;
	vkRalAttachmentContract_t out;
	memset( vkStates, 0, sizeof( vkStates ) );
	vkStates[0].blendEnable = VK_TRUE;
	vkStates[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	vkStates[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	vkStates[0].colorBlendOp = VK_BLEND_OP_ADD;
	vkStates[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	vkStates[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	vkStates[0].alphaBlendOp = VK_BLEND_OP_ADD;
	vkStates[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
	                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	vkStates[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;
	vkStates[1].blendEnable = VK_TRUE;
	vkStates[1].srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
	vkStates[1].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	vkStates[1].colorBlendOp = VK_BLEND_OP_SUBTRACT;
	vkStates[1].srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
	vkStates[1].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	vkStates[1].alphaBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;
	vkStates[2].colorWriteMask = 0;
	vkStates[2].blendEnable = VK_TRUE;
	vkStates[2].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
	vkStates[2].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	vkStates[2].colorBlendOp = VK_BLEND_OP_MIN;
	vkStates[2].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
	vkStates[2].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	vkStates[2].alphaBlendOp = VK_BLEND_OP_MAX;
	memset( &vkBlend, 0, sizeof( vkBlend ) );
	vkBlend.attachmentCount = 3;
	vkBlend.pAttachments = vkStates;
	memset( &out, 0xA5, sizeof( out ) );
	CHECK( VK_RalAttachmentContractFromVk( formats, 3, RAL_FORMAT_D32_SFLOAT, &vkBlend, &out ) );
	CHECK( out.numColorAttachments == 3 );
	CHECK( memcmp( out.colorFormats, formats, sizeof( formats ) ) == 0 );
	CHECK( out.colorBlends[0].blendEnable );
	CHECK( out.colorBlends[0].srcColor == RAL_BLEND_SRC_ALPHA );
	CHECK( out.colorBlends[0].dstColor == RAL_BLEND_ONE_MINUS_SRC_ALPHA );
	CHECK( out.colorBlends[0].colorOp == RAL_BLEND_OP_ADD );
	CHECK( out.colorBlends[0].srcAlpha == RAL_BLEND_ONE );
	CHECK( out.colorBlends[0].dstAlpha == RAL_BLEND_ZERO );
	CHECK( out.colorBlends[0].alphaOp == RAL_BLEND_OP_ADD );
	CHECK( out.colorBlends[0].writeMask == RAL_COLOR_WRITE_ALL );
	CHECK( out.colorBlends[0].writeMaskExplicit );
	CHECK( out.colorBlends[1].blendEnable );
	CHECK( out.colorBlends[1].srcColor == RAL_BLEND_DST_COLOR );
	CHECK( out.colorBlends[1].dstColor == RAL_BLEND_ONE_MINUS_DST_COLOR );
	CHECK( out.colorBlends[1].colorOp == RAL_BLEND_OP_SUBTRACT );
	CHECK( out.colorBlends[1].srcAlpha == RAL_BLEND_DST_ALPHA );
	CHECK( out.colorBlends[1].dstAlpha == RAL_BLEND_ONE_MINUS_DST_ALPHA );
	CHECK( out.colorBlends[1].alphaOp == RAL_BLEND_OP_REVERSE_SUBTRACT );
	CHECK( out.colorBlends[1].writeMask == ( RAL_COLOR_WRITE_R | RAL_COLOR_WRITE_G ) );
	CHECK( out.colorBlends[1].writeMaskExplicit );
	CHECK( out.colorBlends[2].blendEnable );
	CHECK( out.colorBlends[2].srcColor == RAL_BLEND_SRC_COLOR );
	CHECK( out.colorBlends[2].dstColor == RAL_BLEND_ONE_MINUS_SRC_COLOR );
	CHECK( out.colorBlends[2].colorOp == RAL_BLEND_OP_MIN );
	CHECK( out.colorBlends[2].srcAlpha == RAL_BLEND_SRC_ALPHA_SATURATE );
	CHECK( out.colorBlends[2].dstAlpha == RAL_BLEND_ONE );
	CHECK( out.colorBlends[2].alphaOp == RAL_BLEND_OP_MAX );
	CHECK( out.colorBlends[2].writeMask == 0 );
	CHECK( out.colorBlends[2].writeMaskExplicit );
}

static void TestRejectionAtomicity( void ) {
	ralFormat_t formats[3] = { RAL_FORMAT_R8_UNORM, RAL_FORMAT_R16G16_SFLOAT, RAL_FORMAT_R8_UNORM };
	VkPipelineColorBlendAttachmentState states[3];
	VkPipelineColorBlendStateCreateInfo blend;
	vkRalAttachmentContract_t sentinel, out, authored;
	memset( states, 0, sizeof( states ) );
	memset( &blend, 0, sizeof( blend ) ); blend.attachmentCount = 3; blend.pAttachments = states;
	memset( &sentinel, 0x5A, sizeof( sentinel ) ); out = sentinel;
	CHECK( !VK_RalAttachmentContractFromVk( NULL, 3, RAL_FORMAT_UNDEFINED, &blend, &out ) );
	CHECK( memcmp( &out, &sentinel, sizeof( out ) ) == 0 );
	blend.attachmentCount = 2;
	CHECK( !VK_RalAttachmentContractFromVk( formats, 3, RAL_FORMAT_UNDEFINED, &blend, &out ) );
	CHECK( memcmp( &out, &sentinel, sizeof( out ) ) == 0 );
	blend.attachmentCount = 3; blend.pAttachments = NULL;
	CHECK( !VK_RalAttachmentContractFromVk( formats, 3, RAL_FORMAT_UNDEFINED, &blend, &out ) );
	CHECK( !VK_RalAttachmentContractFromVk( formats, RAL_MAX_COLOR_ATTACHMENTS + 1u, RAL_FORMAT_UNDEFINED, &blend, &out ) );
	formats[1] = RAL_FORMAT_UNDEFINED; blend.pAttachments = states;
	CHECK( !VK_RalAttachmentContractFromVk( formats, 3, RAL_FORMAT_UNDEFINED, &blend, &out ) );

	memset( &authored, 0, sizeof( authored ) ); authored.numColorAttachments = RAL_MAX_COLOR_ATTACHMENTS + 1u;
	CHECK( !VK_RalAttachmentContractCopy( &authored, &out ) );
	authored.numColorAttachments = 1; authored.colorFormats[0] = RAL_FORMAT_UNDEFINED;
	CHECK( !VK_RalAttachmentContractCopy( &authored, &out ) );
	CHECK( memcmp( &out, &sentinel, sizeof( out ) ) == 0 );
	authored.colorFormats[0] = RAL_FORMAT_R8_UNORM;
	authored.colorBlends[0].writeMask = 0;
	authored.colorBlends[0].writeMaskExplicit = qfalse;
	CHECK( !VK_RalAttachmentContractCopy( &authored, &out ) );
	CHECK( memcmp( &out, &sentinel, sizeof( out ) ) == 0 );
	authored.colorBlends[0].writeMaskExplicit = qtrue;
	CHECK( VK_RalAttachmentContractCopy( &authored, &out ) );
	CHECK( out.colorBlends[0].writeMask == 0 && out.colorBlends[0].writeMaskExplicit );
}

static void TestDepthOnlyAndSingle( void ) {
	vkRalAttachmentContract_t out;
	VkPipelineColorBlendStateCreateInfo empty;
	VkPipelineColorBlendAttachmentState state;
	ralFormat_t format = RAL_FORMAT_R8G8B8A8_UNORM;
	memset( &empty, 0, sizeof( empty ) );
	CHECK( VK_RalAttachmentContractFromVk( NULL, 0, RAL_FORMAT_D32_SFLOAT, &empty, &out ) );
	CHECK( out.numColorAttachments == 0 && out.depthFormat == RAL_FORMAT_D32_SFLOAT );
	empty.attachmentCount = 1;
	CHECK( !VK_RalAttachmentContractFromVk( NULL, 0, RAL_FORMAT_D32_SFLOAT, &empty, &out ) );
	memset( &state, 0, sizeof( state ) ); state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	empty.pAttachments = &state;
	CHECK( VK_RalAttachmentContractFromVk( &format, 1, RAL_FORMAT_UNDEFINED, &empty, &out ) );
	CHECK( out.numColorAttachments == 1 && out.colorFormats[0] == format && out.colorBlends[0].writeMaskExplicit );
}

int main( void ) {
	TestHeterogeneous(); TestRejectionAtomicity(); TestDepthOnlyAndSingle();
	if ( failures ) return 1;
	puts( "PASS exact Vulkan-to-RAL attachment translation contract" );
	return 0;
}
