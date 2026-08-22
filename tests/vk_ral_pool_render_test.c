// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_ral_pool_render.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; \
} } while ( 0 )

enum { EV_VIEWPORT = 1, EV_SCISSOR, EV_PIPELINE, EV_GROUP, EV_DRAW,
	EV_DISPATCH, EV_BARRIER };

typedef struct {
	uint32_t events[32];
	uint32_t eventCount;
	ralPipeline_t *pipelines[3];
	ralBindGroup_t *groups[3];
	uint32_t setIndices[3];
	uint32_t drawCount;
	uint32_t dispatchCount;
	uint32_t vertexCount[3], instanceCount[3], firstVertex[3], firstInstance[3];
	uint32_t groupCountX, groupCountY, groupCountZ;
	ralBarrierScope_t barrierScope;
	ralViewport_t viewport;
	ralRect_t scissor;
} Capture;

static Capture capture;

static void Event( uint32_t event ) { capture.events[capture.eventCount++] = event; }

void Ral_CmdSetViewport( ralCommandBuffer_t *cb, const ralViewport_t *viewport ) {
	if ( cb && viewport ) capture.viewport = *viewport;
	Event( EV_VIEWPORT );
}

void Ral_CmdSetScissor( ralCommandBuffer_t *cb, const ralRect_t *scissor ) {
	if ( cb && scissor ) capture.scissor = *scissor;
	Event( EV_SCISSOR );
}

void Ral_CmdBindPipeline( ralCommandBuffer_t *cb, ralPipeline_t *pipeline ) {
	(void)cb;
	capture.pipelines[capture.drawCount] = pipeline;
	Event( EV_PIPELINE );
}

void Ral_CmdBindBindGroup( ralCommandBuffer_t *cb, uint32_t setIndex,
		ralBindGroup_t *group ) {
	(void)cb;
	capture.groups[capture.drawCount] = group;
	capture.setIndices[capture.drawCount] = setIndex;
	Event( EV_GROUP );
}

void Ral_CmdDraw( ralCommandBuffer_t *cb, uint32_t vertexCount,
		uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance ) {
	uint32_t i = capture.drawCount++;
	(void)cb;
	capture.vertexCount[i] = vertexCount;
	capture.instanceCount[i] = instanceCount;
	capture.firstVertex[i] = firstVertex;
	capture.firstInstance[i] = firstInstance;
	Event( EV_DRAW );
}

void Ral_CmdDispatch( ralCommandBuffer_t *cb, uint32_t groupCountX,
		uint32_t groupCountY, uint32_t groupCountZ ) {
	(void)cb;
	capture.dispatchCount++;
	capture.groupCountX = groupCountX;
	capture.groupCountY = groupCountY;
	capture.groupCountZ = groupCountZ;
	Event( EV_DISPATCH );
}

void Ral_CmdPipelineBarrier( ralCommandBuffer_t *cb, ralBarrierScope_t scope ) {
	(void)cb;
	capture.barrierScope = scope;
	Event( EV_BARRIER );
}

static vkRalPoolRenderPlan_t Fixture( uint32_t pipelineCount ) {
	vkRalPoolRenderPlan_t plan;
	uint32_t i;
	memset( &plan, 0, sizeof( plan ) );
	plan.commandBuffer = (ralCommandBuffer_t *)(uintptr_t)1;
	plan.bindGroup = (ralBindGroup_t *)(uintptr_t)2;
	plan.pipelineCount = pipelineCount;
	for ( i = 0; i < pipelineCount && i < 3u; i++ )
		plan.pipelines[i] = (ralPipeline_t *)(uintptr_t)( 10u + i );
	plan.viewport.x = 7.0f; plan.viewport.y = 9.0f;
	plan.viewport.width = 1280.0f; plan.viewport.height = 720.0f;
	plan.viewport.minDepth = 0.0f; plan.viewport.maxDepth = 1.0f;
	plan.scissor.x = 7; plan.scissor.y = 9;
	plan.scissor.width = 1280; plan.scissor.height = 720;
	plan.vertexCount = 6; plan.instanceCount = 16384;
	plan.firstVertex = 3; plan.firstInstance = 5;
	return plan;
}

static int CheckSuccess( uint32_t pipelineCount ) {
	static const uint32_t expected[] = {
		EV_VIEWPORT, EV_SCISSOR,
		EV_PIPELINE, EV_GROUP, EV_DRAW,
		EV_PIPELINE, EV_GROUP, EV_DRAW,
		EV_PIPELINE, EV_GROUP, EV_DRAW
	};
	vkRalPoolRenderPlan_t plan = Fixture( pipelineCount );
	vkRalPoolRenderPlan_t before = plan;
	uint32_t i;
	memset( &capture, 0, sizeof( capture ) );
	CHECK( VK_RalPoolRenderExecute( &plan ) == qtrue );
	CHECK( !memcmp( &plan, &before, sizeof( plan ) ) );
	CHECK( capture.eventCount == 2u + pipelineCount * 3u );
	CHECK( !memcmp( capture.events, expected,
		capture.eventCount * sizeof( expected[0] ) ) );
	CHECK( capture.drawCount == pipelineCount );
	CHECK( !memcmp( &capture.viewport, &plan.viewport, sizeof( plan.viewport ) ) );
	CHECK( !memcmp( &capture.scissor, &plan.scissor, sizeof( plan.scissor ) ) );
	for ( i = 0; i < pipelineCount; i++ ) {
		CHECK( capture.pipelines[i] == plan.pipelines[i] );
		CHECK( capture.groups[i] == plan.bindGroup );
		CHECK( capture.setIndices[i] == 0u );
		CHECK( capture.vertexCount[i] == plan.vertexCount );
		CHECK( capture.instanceCount[i] == plan.instanceCount );
		CHECK( capture.firstVertex[i] == plan.firstVertex );
		CHECK( capture.firstInstance[i] == plan.firstInstance );
	}
	return 0;
}

static int RejectsWithoutCommands( vkRalPoolRenderPlan_t plan ) {
	memset( &capture, 0, sizeof( capture ) );
	CHECK( VK_RalPoolRenderExecute( &plan ) == qfalse );
	CHECK( capture.eventCount == 0u );
	return 0;
}

static vkRalComputePlan_t ComputeFixture( void ) {
	vkRalComputePlan_t plan;
	memset( &plan, 0, sizeof( plan ) );
	plan.commandBuffer = (ralCommandBuffer_t *)(uintptr_t)1;
	plan.pipeline = (ralPipeline_t *)(uintptr_t)2;
	plan.bindGroup = (ralBindGroup_t *)(uintptr_t)3;
	plan.groupCountX = 256u;
	plan.groupCountY = 1u;
	plan.groupCountZ = 1u;
	return plan;
}

static int CheckComputeSuccess( void ) {
	static const uint32_t expected[] = {
		EV_PIPELINE, EV_GROUP, EV_DISPATCH, EV_BARRIER
	};
	vkRalComputePlan_t plan = ComputeFixture();
	vkRalComputePlan_t before = plan;
	memset( &capture, 0, sizeof( capture ) );
	CHECK( VK_RalComputeExecute( &plan ) == qtrue );
	CHECK( !memcmp( &plan, &before, sizeof( plan ) ) );
	CHECK( capture.eventCount == 4u );
	CHECK( !memcmp( capture.events, expected, sizeof( expected ) ) );
	CHECK( capture.pipelines[0] == plan.pipeline );
	CHECK( capture.groups[0] == plan.bindGroup );
	CHECK( capture.setIndices[0] == 0u );
	CHECK( capture.dispatchCount == 1u );
	CHECK( capture.groupCountX == plan.groupCountX );
	CHECK( capture.groupCountY == plan.groupCountY );
	CHECK( capture.groupCountZ == plan.groupCountZ );
	CHECK( capture.barrierScope == RAL_BARRIER_COMPUTE_TO_GRAPHICS );
	return 0;
}

static int RejectsComputeWithoutCommands( vkRalComputePlan_t plan ) {
	memset( &capture, 0, sizeof( capture ) );
	CHECK( VK_RalComputeExecute( &plan ) == qfalse );
	CHECK( capture.eventCount == 0u );
	return 0;
}

int main( void ) {
	vkRalPoolRenderPlan_t plan;
	vkRalComputePlan_t compute;
	CHECK( CheckSuccess( 1u ) == 0 );
	CHECK( CheckSuccess( 2u ) == 0 );
	CHECK( CheckSuccess( 3u ) == 0 );
	CHECK( CheckComputeSuccess() == 0 );

	plan = Fixture( 2u ); plan.commandBuffer = NULL; CHECK( RejectsWithoutCommands( plan ) == 0 );
	plan = Fixture( 2u ); plan.bindGroup = NULL; CHECK( RejectsWithoutCommands( plan ) == 0 );
	plan = Fixture( 2u ); plan.pipelineCount = 0u; CHECK( RejectsWithoutCommands( plan ) == 0 );
	plan = Fixture( 2u ); plan.pipelineCount = 4u; CHECK( RejectsWithoutCommands( plan ) == 0 );
	plan = Fixture( 2u ); plan.pipelines[1] = NULL; CHECK( RejectsWithoutCommands( plan ) == 0 );
	plan = Fixture( 2u ); plan.viewport.width = 0.0f; CHECK( RejectsWithoutCommands( plan ) == 0 );
	plan = Fixture( 2u ); plan.viewport.height = -1.0f; CHECK( RejectsWithoutCommands( plan ) == 0 );
	plan = Fixture( 2u ); plan.viewport.minDepth = -0.1f; CHECK( RejectsWithoutCommands( plan ) == 0 );
	plan = Fixture( 2u ); plan.viewport.maxDepth = 1.1f; CHECK( RejectsWithoutCommands( plan ) == 0 );
	plan = Fixture( 2u ); plan.viewport.minDepth = 0.8f; plan.viewport.maxDepth = 0.2f; CHECK( RejectsWithoutCommands( plan ) == 0 );
	plan = Fixture( 2u ); plan.scissor.width = 0u; CHECK( RejectsWithoutCommands( plan ) == 0 );
	plan = Fixture( 2u ); plan.scissor.height = 0u; CHECK( RejectsWithoutCommands( plan ) == 0 );
	plan = Fixture( 2u ); plan.vertexCount = 0u; CHECK( RejectsWithoutCommands( plan ) == 0 );
	plan = Fixture( 2u ); plan.instanceCount = 0u; CHECK( RejectsWithoutCommands( plan ) == 0 );
	memset( &capture, 0, sizeof( capture ) );
	CHECK( VK_RalPoolRenderExecute( NULL ) == qfalse && capture.eventCount == 0u );

	compute = ComputeFixture(); compute.commandBuffer = NULL; CHECK( RejectsComputeWithoutCommands( compute ) == 0 );
	compute = ComputeFixture(); compute.pipeline = NULL; CHECK( RejectsComputeWithoutCommands( compute ) == 0 );
	compute = ComputeFixture(); compute.bindGroup = NULL; CHECK( RejectsComputeWithoutCommands( compute ) == 0 );
	compute = ComputeFixture(); compute.groupCountX = 0u; CHECK( RejectsComputeWithoutCommands( compute ) == 0 );
	compute = ComputeFixture(); compute.groupCountY = 0u; CHECK( RejectsComputeWithoutCommands( compute ) == 0 );
	compute = ComputeFixture(); compute.groupCountZ = 0u; CHECK( RejectsComputeWithoutCommands( compute ) == 0 );
	compute = ComputeFixture(); compute.groupCountX = VK_RAL_COMPUTE_MAX_GROUPS_PER_DIMENSION + 1u; CHECK( RejectsComputeWithoutCommands( compute ) == 0 );
	compute = ComputeFixture(); compute.groupCountY = VK_RAL_COMPUTE_MAX_GROUPS_PER_DIMENSION + 1u; CHECK( RejectsComputeWithoutCommands( compute ) == 0 );
	compute = ComputeFixture(); compute.groupCountZ = VK_RAL_COMPUTE_MAX_GROUPS_PER_DIMENSION + 1u; CHECK( RejectsComputeWithoutCommands( compute ) == 0 );
	memset( &capture, 0, sizeof( capture ) );
	CHECK( VK_RalComputeExecute( NULL ) == qfalse && capture.eventCount == 0u );

	puts( "vk RAL pool render + compute contract: PASS" );
	return 0;
}
