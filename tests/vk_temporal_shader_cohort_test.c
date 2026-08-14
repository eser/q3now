// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_temporal_shader_cohort.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

static const unsigned char s_blob0[4] = {1};
static const unsigned char s_blob1[8] = {2};
static const unsigned char s_blob2[12] = {3};
static const unsigned char s_blob3[16] = {4};
static const unsigned char s_blob4[20] = {5};
static const unsigned char s_blob5[24] = {6};
static const unsigned char s_blob6[28] = {7};
static vkTemporalShaderBlob_t Blob( uint32_t i ) {
	static const unsigned char *const bytes[7] = {s_blob0,s_blob1,s_blob2,s_blob3,s_blob4,s_blob5,s_blob6};
	static const uint32_t sizes[7] = {sizeof(s_blob0),sizeof(s_blob1),sizeof(s_blob2),sizeof(s_blob3),sizeof(s_blob4),sizeof(s_blob5),sizeof(s_blob6)};
	vkTemporalShaderBlob_t b = { bytes[i], sizes[i] }; return b;
}

static vkTemporalShaderRecipeInput_t Valid( vkTemporalShaderRecipeKind_t kind ) {
	vkTemporalShaderRecipeInput_t in;
	uintptr_t i;
	memset( &in, 0, sizeof( in ) );
	in.kind = kind;
	in.sceneFormat = RAL_FORMAT_R16G16B16A16_SFLOAT;
	in.sceneBlend.blendEnable = qfalse;
	in.sceneBlend.srcColor = RAL_BLEND_SRC_ALPHA;
	in.sceneBlend.dstColor = RAL_BLEND_ONE_MINUS_SRC_ALPHA;
	in.sceneBlend.writeMask = RAL_COLOR_WRITE_ALL;
	in.sceneBlend.writeMaskExplicit = qtrue;
	for ( i = 0; i < 4; ++i ) in.genericSetLayouts[i] = (const void *)(i + 1u);
	for ( i = 0; i < 2; ++i ) in.iqmSetLayouts[i] = (const void *)(i + 9u);
	in.ordinaryVertex = Blob(0); in.ordinaryFragment = Blob(1); in.temporalVertex = Blob(2);
	in.temporalWriteFragment = Blob(3); in.temporalInvalidateFragment = Blob(4);
	in.iqmVertex = Blob(5); in.iqmInvalidateFragment = Blob(6);
	return in;
}

int main( void ) {
	vkTemporalShaderRecipeInput_t in;
	vkTemporalShaderRecipe_t out, before;
	uint32_t i;

	for ( i = VK_TEMPORAL_SHADER_PRESERVE; i <= VK_TEMPORAL_SHADER_IQM_INVALIDATE; ++i ) {
		in = Valid( (vkTemporalShaderRecipeKind_t)i );
		memset( &out, 0xA5, sizeof( out ) );
		CHECK( VK_TemporalShaderRecipeBuild( &in, &out ) );
		CHECK( out.kind == i && out.numColorAttachments == 3u );
		CHECK( out.colorFormats[0] == in.sceneFormat );
		CHECK( out.colorFormats[1] == RAL_FORMAT_R16G16_SFLOAT );
		CHECK( out.colorFormats[2] == RAL_FORMAT_R8_UNORM );
		CHECK( memcmp( &out.colorBlends[0], &in.sceneBlend, sizeof( in.sceneBlend ) ) == 0 );
		CHECK( out.colorBlends[1].writeMaskExplicit && out.colorBlends[2].writeMaskExplicit );
		CHECK( !out.colorBlends[1].blendEnable && !out.colorBlends[2].blendEnable );
		CHECK( out.colorBlends[1].srcColor == RAL_BLEND_ZERO && out.colorBlends[1].dstColor == RAL_BLEND_ZERO );
		CHECK( out.colorBlends[1].colorOp == RAL_BLEND_OP_ADD && out.colorBlends[1].srcAlpha == RAL_BLEND_ZERO );
		CHECK( out.colorBlends[1].dstAlpha == RAL_BLEND_ZERO && out.colorBlends[1].alphaOp == RAL_BLEND_OP_ADD );
		CHECK( out.colorBlends[2].srcColor == RAL_BLEND_ZERO && out.colorBlends[2].dstColor == RAL_BLEND_ZERO );
		CHECK( out.colorBlends[2].colorOp == RAL_BLEND_OP_ADD && out.colorBlends[2].srcAlpha == RAL_BLEND_ZERO );
		CHECK( out.colorBlends[2].dstAlpha == RAL_BLEND_ZERO && out.colorBlends[2].alphaOp == RAL_BLEND_OP_ADD );
		CHECK( out.colorBlends[1].writeMask == (i == VK_TEMPORAL_SHADER_PRESERVE ? 0u : (RAL_COLOR_WRITE_R | RAL_COLOR_WRITE_G)) );
		CHECK( out.colorBlends[2].writeMask == (i == VK_TEMPORAL_SHADER_PRESERVE ? 0u : RAL_COLOR_WRITE_R) );
		CHECK( out.directSpirvOverride == (i == VK_TEMPORAL_SHADER_PRESERVE ? qfalse : qtrue) );
		CHECK( out.numSetLayouts == (i == VK_TEMPORAL_SHADER_IQM_INVALIDATE ? 2u : 4u) );
		if ( i == VK_TEMPORAL_SHADER_IQM_INVALIDATE ) {
			CHECK( out.setLayouts[0] == in.iqmSetLayouts[0] && out.setLayouts[1] == in.iqmSetLayouts[1] );
			CHECK( memcmp( &out.vertex, &in.iqmVertex, sizeof(out.vertex) ) == 0 );
			CHECK( memcmp( &out.fragment, &in.iqmInvalidateFragment, sizeof(out.fragment) ) == 0 );
		} else {
			CHECK( memcmp( out.setLayouts, in.genericSetLayouts, sizeof(in.genericSetLayouts) ) == 0 );
			CHECK( memcmp( &out.vertex, i == VK_TEMPORAL_SHADER_PRESERVE ? &in.ordinaryVertex : &in.temporalVertex, sizeof(out.vertex) ) == 0 );
			CHECK( memcmp( &out.fragment, i == VK_TEMPORAL_SHADER_PRESERVE ? &in.ordinaryFragment : (i == VK_TEMPORAL_SHADER_WRITE ? &in.temporalWriteFragment : &in.temporalInvalidateFragment), sizeof(out.fragment) ) == 0 );
		}
		CHECK( out.numPushRanges == 0u );
	}

	in = Valid( VK_TEMPORAL_SHADER_WRITE ); in.fog = qtrue;
	CHECK( VK_TemporalShaderRecipeBuild( &in, &out ) );
	CHECK( out.numPushRanges == 1u && out.pushOffset == 64u && out.pushSize == 32u );
	CHECK( out.pushStages == RAL_STAGE_FRAGMENT );
	in = Valid( VK_TEMPORAL_SHADER_IQM_INVALIDATE ); in.fog = qtrue;
	memset(&out,0x5A,sizeof(out)); before=out;
	CHECK( !VK_TemporalShaderRecipeBuild(&in,&out) && memcmp(&out,&before,sizeof(out)) == 0 );

#define REJECT_MUTATION(stmt) do { in = Valid(VK_TEMPORAL_SHADER_WRITE); stmt; memset(&out,0x5A,sizeof(out)); before=out; CHECK(!VK_TemporalShaderRecipeBuild(&in,&out)); CHECK(memcmp(&out,&before,sizeof(out))==0); } while(0)
	REJECT_MUTATION( in.alphaTested = qtrue );
	REJECT_MUTATION( in.depthOnly = qtrue );
	REJECT_MUTATION( in.blended = qtrue );
	REJECT_MUTATION( in.special = qtrue );
	REJECT_MUTATION( in.dynamicDiscard = qtrue );
	REJECT_MUTATION( in.sceneBlend.blendEnable = qtrue );
	REJECT_MUTATION( in.sceneFormat = RAL_FORMAT_UNDEFINED );
	REJECT_MUTATION( in.genericSetLayouts[2] = NULL );
	REJECT_MUTATION( in.temporalVertex.bytes = NULL );
	REJECT_MUTATION( in.temporalWriteFragment.size = 3u );
	in = Valid( (vkTemporalShaderRecipeKind_t)99 ); memset(&out,0x5A,sizeof(out)); before=out;
	CHECK( !VK_TemporalShaderRecipeBuild( &in, &out ) && memcmp(&out,&before,sizeof(out)) == 0 );

	puts( "vk temporal shader cohort contract: PASS" );
	return 0;
}
