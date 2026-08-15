// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_iqm_command.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; \
} } while ( 0 )

typedef struct {
	uint32_t events[32];
	uint32_t count;
	qboolean preflightResult;
	vkTemporalIqmExact3Push_t push;
	uint32_t drawCount, firstIndex, firstInstance;
	ralLoadOp_t velocityLoad, validityLoad;
	ralLoadOp_t sceneLoad, depthLoad;
	ralStoreOp_t sceneStore, velocityStore, validityStore, depthStore;
	qboolean failed;
} Capture;

qboolean VK_TemporalIqmExact3FactoryReceiptExact(
		const vkTemporalIqmExact3FactoryReceipt_t *a,
		const vkTemporalIqmExact3FactoryReceipt_t *b ) {
	return a && b && a->ready == qtrue && b->ready == qtrue
		&& !memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}

static void Event( Capture *c, uint32_t event ) { c->events[c->count++] = event; }

static temporalEntityPose_t Pose( int frame, float x ) {
	temporalEntityPose_t pose;
	memset( &pose, 0, sizeof( pose ) );
	pose.hModel = 7; pose.modelToken = 0x1000u; pose.modelDataToken = 0x2000u;
	pose.modelType = 4u; pose.modelTopology = 9u;
	pose.modelAllocationGeneration = 2u; pose.modelContentDigest = 0x1234u;
	pose.frame = frame; pose.oldframe = frame; pose.origin[0] = x;
	pose.axis[0] = pose.axis[4] = pose.axis[8] = 1.0f;
	return pose;
}

static refEntityMotion_t Identity( void ) {
	refEntityMotion_t identity;
	memset( &identity, 0, sizeof( identity ) );
	identity.structSize = sizeof( identity );
	identity.version = REF_ENTITY_MOTION_VERSION;
	identity.ownerId = 8; identity.generation = 3;
	identity.role = REF_ENTITY_MOTION_ROLE_PLAYER_BODY;
	return identity;
}
static qboolean Preflight( void *p,
		const vkTemporalMainIqmActivationPlan_t *plan,
		const vkTemporalIqmCommandResources_t *resources ) {
	Capture *c = (Capture *)p;
	return c && plan && resources ? c->preflightResult : qfalse;
}
static void EndOrdinary( void *p ) { Event( p, 1 ); }
static void Barrier( void *p ) { Event( p, 2 ); }
static void BeginExact( void *p, const ralRenderingInfo_t *info ) {
	Capture *c = p; Event( c, 3 );
	c->sceneLoad = info->colorLoadOps[0];
	c->velocityLoad = info->colorLoadOps[1];
	c->validityLoad = info->colorLoadOps[2];
	c->sceneStore = info->colorStoreOps[0];
	c->velocityStore = info->colorStoreOps[1];
	c->validityStore = info->colorStoreOps[2];
	c->depthLoad = info->depthLoadOp;
	c->depthStore = info->depthStoreOp;
}
static void Dynamic( void *p ) { Event( p, 4 ); }
static void Pipeline( void *p, ralPipeline_t *pipeline ) {
	Capture *c = p;
	if ( pipeline != (ralPipeline_t *)(uintptr_t)15 ) c->failed = qtrue;
	Event( c, 5 );
}
static void Group( void *p, uint32_t set, ralBindGroup_t *group ) {
	Capture *c = p;
	if ( !( ( set == 0 && group == (ralBindGroup_t *)(uintptr_t)20 )
			|| ( set == 1 && group == (ralBindGroup_t *)(uintptr_t)21 ) ) )
		c->failed = qtrue;
	Event( c, set ? 7 : 6 );
}
static void Push( void *p, const vkTemporalIqmExact3Push_t *push ) {
	Capture *c = p; c->push = *push; Event( c, 8 );
}
static void Vertex( void *p, ralBuffer_t *buffer ) {
	Capture *c = p;
	if ( buffer != (ralBuffer_t *)(uintptr_t)22 ) c->failed = qtrue;
	Event( c, 9 );
}
static void Index( void *p, ralBuffer_t *buffer ) {
	Capture *c = p;
	if ( buffer != (ralBuffer_t *)(uintptr_t)23 ) c->failed = qtrue;
	Event( c, 10 );
}
static void Draw( void *p, uint32_t count, uint32_t first, uint32_t instance ) {
	Capture *c = p; c->drawCount = count; c->firstIndex = first;
	c->firstInstance = instance; Event( c, 11 );
}
static void EndExact( void *p ) { Event( p, 12 ); }
static void BeginResume( void *p, const ralRenderingInfo_t *info ) {
	Capture *c = p;
	if ( !info || info->colorLoadOps[0] != RAL_LOAD_OP_LOAD ) c->failed = qtrue;
	Event( c, 13 );
}
static void Reset( void *p ) { Event( p, 14 ); }

static vkTemporalIqmCommandOps_t Ops( void ) {
	vkTemporalIqmCommandOps_t ops;
	memset( &ops, 0, sizeof( ops ) );
	ops.preflight = Preflight; ops.endOrdinary = EndOrdinary;
	ops.barrier = Barrier; ops.beginExact = BeginExact;
	ops.setDynamicState = Dynamic; ops.bindPipeline = Pipeline;
	ops.bindGroup = Group; ops.push = Push; ops.bindVertex = Vertex;
	ops.bindIndex = Index; ops.drawIndexed = Draw; ops.endExact = EndExact;
	ops.beginResume = BeginResume; ops.resetOrdinaryState = Reset;
	return ops;
}

static void Fixture( vkTemporalMainIqmActivationPlan_t *plan,
		vkTemporalIqmCommandResources_t *resources ) {
	memset( plan, 0, sizeof( *plan ) ); memset( resources, 0, sizeof( *resources ) );
	plan->authority.token = 1; plan->authority.frameId = 2;
	plan->authority.width = 1280; plan->authority.height = 720;
	plan->planSerial = 3; plan->ordinal = 4; plan->recordIndex = 5;
	plan->sequenceDigest = 6; plan->entry.facts.ordinal = 4;
	plan->entry.recordIndex = 5; plan->entry.facts.firstIndex = 7;
	plan->entry.facts.indexCount = 9;
	plan->entry.facts.entityIndex = 8;
	plan->entry.facts.identity = Identity();
	plan->entry.facts.currentPose = Pose( 1, 2.0f );
	plan->entry.facts.previousPose = Pose( 0, 1.0f );
	plan->entry.facts.modelContentDigest = 0x1234u;
	plan->entry.facts.modelTopologyGeneration = 9u;
	plan->entry.facts.modelAllocationGeneration = 2u;
	plan->entry.facts.rawVertexBuffer = 30; plan->entry.facts.rawIndexBuffer = 31;
	plan->entry.facts.ralVertexBuffer = 22; plan->entry.facts.ralIndexBuffer = 23;
	plan->entry.facts.vertexBufferBytes = TEMPORAL_IQM_VERTEX_STRIDE * 10u;
	plan->entry.facts.indexBufferBytes = sizeof( uint32_t ) * 32u;
	plan->entry.facts.geometryBackend = 10;
	plan->entry.facts.geometryGeneration = 6;
	plan->entry.facts.geometryAllocationGeneration = 4;
	plan->entry.facts.textureSlot = 11; plan->entry.facts.samplerSlot = 12;
	plan->entry.facts.bindless.setIdentity = 21;
	plan->entry.facts.bindless.samplerPoolIdentity = 50;
	plan->entry.facts.bindless.imageViewIdentity = 51;
	plan->entry.facts.bindless.samplerIdentity = 52;
	plan->entry.facts.bindless.imageOwnerIdentity = 53;
	plan->entry.facts.bindless.ordinaryDescriptorIdentity = 54;
	plan->entry.facts.bindless.imageOwnerGeneration = 2;
	plan->entry.facts.bindless.samplerDefinitionDigest = 3;
	plan->entry.facts.bindless.setGeneration = 4;
	plan->entry.facts.bindless.samplerPoolGeneration = 5;
	plan->entry.facts.bindless.imagePublicationGeneration = 6;
	plan->entry.facts.bindless.imageTransactionGeneration = 7;
	plan->entry.facts.bindless.samplerPublicationGeneration = 8;
	plan->entry.facts.bindless.samplerTransactionGeneration = 9;
	plan->entry.facts.bindless.imageSlotGeneration = 10;
	plan->entry.facts.bindless.samplerSlotGeneration = 11;
	plan->entry.facts.bindless.imageSlot = 11;
	plan->entry.facts.bindless.samplerSlot = 12;
	plan->entry.facts.bindless.imageBinding = 0;
	plan->entry.facts.bindless.samplerBinding = 1;
	plan->entry.facts.bindless.ready = qtrue;
	plan->entry.facts.outcome = TEMPORAL_MOTION_WRITE_VALID;
	plan->entry.facts.previousValid = qtrue;
	plan->kind = VK_TEMPORAL_SHADER_WRITE; plan->pipeline = (ralPipeline_t *)(uintptr_t)15;
	plan->payloadGroup = (ralBindGroup_t *)(uintptr_t)20;
	plan->factory.backend = (ralBackend_t *)(uintptr_t)10;
	plan->factory.device = (VkDevice)(uintptr_t)11;
	plan->factory.payloadOwner = (vkTemporalIqmPayloadOwner_t *)(uintptr_t)12;
	plan->factory.payloadLayout = (ralBindGroupLayout_t *)(uintptr_t)13;
	plan->factory.bindless.setIdentity = (void *)(uintptr_t)21;
	plan->factory.writePipeline = plan->pipeline;
	plan->factory.invalidatePipeline = (ralPipeline_t *)(uintptr_t)16;
	plan->factory.ready = qtrue;
	plan->endOrdinary = plan->beginExact3 = plan->endExact3 = plan->resumeOrdinary = qtrue;
	plan->clearAuxiliary = qtrue;
	resources->commandBuffer = (ralCommandBuffer_t *)(uintptr_t)40;
	resources->scene = (ralTexture_t *)(uintptr_t)41;
	resources->velocity = (ralTexture_t *)(uintptr_t)42;
	resources->validity = (ralTexture_t *)(uintptr_t)43;
	resources->depth = (ralTexture_t *)(uintptr_t)44;
	resources->payloadGroup = plan->payloadGroup;
	resources->bindlessGroup = (ralBindGroup_t *)plan->factory.bindless.setIdentity;
	resources->vertexBuffer = (ralBuffer_t *)(uintptr_t)22;
	resources->indexBuffer = (ralBuffer_t *)(uintptr_t)23;
	resources->currentFactory = plan->factory;
	resources->width = 1280; resources->height = 720;
	resources->hasStencil = qtrue; resources->ordinaryOpen = qtrue;
}

int main( void ) {
	vkTemporalMainIqmActivationPlan_t plan, mutatedPlan;
	vkTemporalIqmCommandResources_t resources, mutatedResources;
	vkTemporalIqmCommandOps_t ops, mutatedOps;
	Capture capture;
	const uint32_t expected[] = { 1,2,3,4,5,6,7,8,9,10,11,12,2,13,4,14 };
	Fixture( &plan, &resources ); ops = Ops();
	memset( &capture, 0, sizeof( capture ) ); capture.preflightResult = qtrue;
	CHECK( VK_TemporalIqmCommandExecute( &plan, &resources, &capture, &ops ) );
	CHECK( capture.count == sizeof( expected ) / sizeof( expected[0] ) );
	CHECK( !capture.failed );
	CHECK( !memcmp( capture.events, expected, sizeof( expected ) ) );
	CHECK( capture.velocityLoad == RAL_LOAD_OP_CLEAR
		&& capture.validityLoad == RAL_LOAD_OP_CLEAR );
	CHECK( capture.sceneLoad == RAL_LOAD_OP_LOAD
		&& capture.sceneStore == RAL_STORE_OP_STORE
		&& capture.velocityStore == RAL_STORE_OP_STORE
		&& capture.validityStore == RAL_STORE_OP_STORE
		&& capture.depthLoad == RAL_LOAD_OP_LOAD
		&& capture.depthStore == RAL_STORE_OP_STORE );
	CHECK( capture.push.imageSlot == 11 && capture.push.samplerSlot == 12 );
	CHECK( capture.drawCount == 9 && capture.firstIndex == 7
		&& capture.firstInstance == 5 );

	plan.clearAuxiliary = qfalse;
	memset( &capture, 0, sizeof( capture ) ); capture.preflightResult = qtrue;
	CHECK( VK_TemporalIqmCommandExecute( &plan, &resources, &capture, &ops ) );
	CHECK( !capture.failed && capture.velocityLoad == RAL_LOAD_OP_LOAD
		&& capture.validityLoad == RAL_LOAD_OP_LOAD
		&& capture.sceneLoad == RAL_LOAD_OP_LOAD
		&& capture.depthLoad == RAL_LOAD_OP_LOAD );
	plan.clearAuxiliary = qtrue;

	memset( &capture, 0, sizeof( capture ) ); capture.preflightResult = qfalse;
	CHECK( !VK_TemporalIqmCommandExecute( &plan, &resources, &capture, &ops ) );
	CHECK( capture.count == 0 );

	for ( uint32_t field = 0; field < 18u; ++field ) {
		mutatedResources = resources;
		switch ( field ) {
		case 0: mutatedResources.commandBuffer = NULL; break;
		case 1: mutatedResources.scene = NULL; break;
		case 2: mutatedResources.velocity = mutatedResources.scene; break;
		case 3: mutatedResources.payloadGroup = (ralBindGroup_t *)(uintptr_t)99; break;
		case 4: mutatedResources.bindlessGroup = (ralBindGroup_t *)(uintptr_t)99; break;
		case 5: mutatedResources.vertexBuffer = (ralBuffer_t *)(uintptr_t)99; break;
		case 6: mutatedResources.indexBuffer = (ralBuffer_t *)(uintptr_t)99; break;
		case 7: mutatedResources.width++; break;
		case 8: mutatedResources.height++; break;
		case 9: mutatedResources.ordinaryOpen = qfalse; break;
		case 10: mutatedResources.currentFactory.writePipeline =
			(ralPipeline_t *)(uintptr_t)99; break;
		case 11: mutatedResources.vertexBuffer = mutatedResources.indexBuffer; break;
		case 12: mutatedResources.scene = mutatedResources.validity; break;
		case 13: mutatedResources.velocity = mutatedResources.validity; break;
		case 14: mutatedResources.depth = mutatedResources.scene; break;
		case 15: mutatedResources.depth = mutatedResources.velocity; break;
		case 16: mutatedResources.depth = mutatedResources.validity; break;
		default: mutatedResources.payloadGroup = mutatedResources.bindlessGroup; break;
		}
		memset( &capture, 0, sizeof( capture ) ); capture.preflightResult = qtrue;
		CHECK( !VK_TemporalIqmCommandExecute(
			&plan, &mutatedResources, &capture, &ops ) && capture.count == 0 );
	}

	for ( uint32_t field = 0; field < 12u; ++field ) {
		mutatedPlan = plan;
		switch ( field ) {
		case 0: mutatedPlan.planSerial = 0; break;
		case 1: mutatedPlan.sequenceDigest = 0; break;
		case 2: mutatedPlan.recordIndex++; break;
		case 3: mutatedPlan.ordinal++; break;
		case 4: mutatedPlan.pipeline = mutatedPlan.factory.invalidatePipeline; break;
		case 5: mutatedPlan.endOrdinary = qfalse; break;
		case 6: mutatedPlan.beginExact3 = qfalse; break;
		case 7: mutatedPlan.endExact3 = qfalse; break;
		case 8: mutatedPlan.resumeOrdinary = qfalse; break;
		case 9: mutatedPlan.entry.facts.indexCount = 0; break;
		case 10: mutatedPlan.entry.facts.firstIndex = 30; break;
		default: mutatedPlan.entry.facts.textureSlot = 4096; break;
		}
		memset( &capture, 0, sizeof( capture ) ); capture.preflightResult = qtrue;
		CHECK( !VK_TemporalIqmCommandExecute(
			&mutatedPlan, &resources, &capture, &ops ) && capture.count == 0 );
	}

	for ( uint32_t field = 0; field < 14u; ++field ) {
		mutatedOps = ops;
		switch ( field ) {
		case 0: mutatedOps.preflight = NULL; break;
		case 1: mutatedOps.endOrdinary = NULL; break;
		case 2: mutatedOps.barrier = NULL; break;
		case 3: mutatedOps.beginExact = NULL; break;
		case 4: mutatedOps.setDynamicState = NULL; break;
		case 5: mutatedOps.bindPipeline = NULL; break;
		case 6: mutatedOps.bindGroup = NULL; break;
		case 7: mutatedOps.push = NULL; break;
		case 8: mutatedOps.bindVertex = NULL; break;
		case 9: mutatedOps.bindIndex = NULL; break;
		case 10: mutatedOps.drawIndexed = NULL; break;
		case 11: mutatedOps.endExact = NULL; break;
		case 12: mutatedOps.beginResume = NULL; break;
		default: mutatedOps.resetOrdinaryState = NULL; break;
		}
		memset( &capture, 0, sizeof( capture ) ); capture.preflightResult = qtrue;
		CHECK( !VK_TemporalIqmCommandExecute(
			&plan, &resources, &capture, &mutatedOps ) && capture.count == 0 );
	}

	puts( "vk_temporal_iqm_command_test: PASS" );
	return 0;
}
