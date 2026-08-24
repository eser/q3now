// SPDX-License-Identifier: GPL-3.0-or-later

#include "vk_temporal_main_rendering.h"

#include <stdio.h>
#include <string.h>
#include "../code/render/ral/backends/vulkan/include/vulkan/vulkan.h"

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; \
} } while ( 0 )

int main( void ) {
	ralTexture_t *scene=(ralTexture_t *)(uintptr_t)1;
	ralTexture_t *velocity=(ralTexture_t *)(uintptr_t)2;
	ralTexture_t *validity=(ralTexture_t *)(uintptr_t)3;
	ralTexture_t *depth=(ralTexture_t *)(uintptr_t)4;
	ralRenderingInfo_t info, sentinel;
	ralMemoryBarrier_t memory;
	ralPipelineBarrierInfo_t barrier;
	CHECK( VK_TemporalMainRenderingBuildInitial( scene, depth, 1280, 720,
		qtrue, qtrue, &info ) );
	CHECK( info.numColorAttachments==1 && info.colorLoadOps[0]==RAL_LOAD_OP_CLEAR
		&& info.depthStoreOp==RAL_STORE_OP_STORE
		&& info.stencilStoreOp==RAL_STORE_OP_STORE );
	CHECK( VK_TemporalMainRenderingBuildExact3( scene, velocity, validity, depth,
		1280, 720, qtrue, qtrue, &info ) );
	CHECK( info.numColorAttachments==3 && info.colorLoadOps[0]==RAL_LOAD_OP_LOAD
		&& info.colorLoadOps[1]==RAL_LOAD_OP_CLEAR
		&& info.colorLoadOps[2]==RAL_LOAD_OP_CLEAR
		&& info.depthLoadOp==RAL_LOAD_OP_LOAD
		&& info.depthStoreOp==RAL_STORE_OP_STORE
		&& info.stencilLoadOp==RAL_LOAD_OP_LOAD
		&& info.stencilStoreOp==RAL_STORE_OP_STORE );
	CHECK( VK_TemporalMainRenderingBuildExact3( scene, velocity, validity, depth,
		1280, 720, qfalse, qfalse, &info ) );
	CHECK( info.colorLoadOps[1]==RAL_LOAD_OP_LOAD
		&& info.colorLoadOps[2]==RAL_LOAD_OP_LOAD
		&& info.stencilLoadOp==RAL_LOAD_OP_DONT_CARE
		&& info.stencilStoreOp==RAL_STORE_OP_DONT_CARE );
	CHECK( VK_TemporalMainRenderingBuildResume( scene, depth, 1280, 720,
		qtrue, &info ) );
	CHECK( info.numColorAttachments==1 && info.colorLoadOps[0]==RAL_LOAD_OP_LOAD
		&& info.depthLoadOp==RAL_LOAD_OP_LOAD
		&& info.stencilLoadOp==RAL_LOAD_OP_LOAD );
	CHECK( VK_TemporalMainRenderingBuildAttachmentBarrier( &memory, &barrier ) );
	CHECK( barrier.memoryBarrierCount==1 && barrier.memoryBarriers==&memory
		&& (barrier.srcStageMask & RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)
		&& (barrier.srcStageMask & RAL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT)
		&& (barrier.srcStageMask & RAL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT)
		&& barrier.dependencyFlags==VK_DEPENDENCY_BY_REGION_BIT
		&& (memory.srcAccessMask & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
		&& (memory.srcAccessMask & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
		&& (memory.dstAccessMask & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
		&& (memory.dstAccessMask & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT) );
	memset( &sentinel, 0xA5, sizeof( sentinel ) ); info=sentinel;
	CHECK( !VK_TemporalMainRenderingBuildExact3( scene, scene, validity, depth,
		1280, 720, qtrue, qtrue, &info ) );
	CHECK( memcmp( &info, &sentinel, sizeof( info ) )==0 );
	puts( "vk_temporal_main_rendering_test: PASS" );
	return 0;
}
