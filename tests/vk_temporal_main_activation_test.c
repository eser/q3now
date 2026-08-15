// SPDX-License-Identifier: GPL-3.0-or-later

#include "vk_temporal_main_activation.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; \
} } while ( 0 )

ralBindGroupLayout_t *Ral_CreateBindGroupLayout( ralBackend_t *b,
		const ralBindGroupLayoutCreateInfo_t *ci ){(void)b;(void)ci;return NULL;}
void Ral_DestroyBindGroupLayout( ralBindGroupLayout_t *l ){(void)l;}
ralBuffer_t *Ral_CreateBuffer( ralBackend_t *b,const ralBufferCreateInfo_t *ci ){
	(void)b;(void)ci;return NULL;}
void Ral_DestroyBuffer( ralBuffer_t *b ){(void)b;}
void *Ral_MapBuffer( ralBuffer_t *b ){(void)b;return NULL;}
void Ral_UnmapBuffer( ralBuffer_t *b ){(void)b;}
ralBindGroup_t *Ral_CreateBindGroup( ralBackend_t *b,
		const ralBindGroupCreateInfo_t *ci ){(void)b;(void)ci;return NULL;}
void Ral_DestroyBindGroup( ralBindGroup_t *g ){(void)g;}

qboolean VK_TemporalIqmExact3FactoryReceiptExact(
		const vkTemporalIqmExact3FactoryReceipt_t *a,
		const vkTemporalIqmExact3FactoryReceipt_t *b ) {
	return a && b && a->ready == qtrue && b->ready == qtrue
		&& a->backend && a->payloadOwner && a->payloadLayout
		&& a->writePipeline && a->invalidatePipeline
		&& memcmp( a, b, sizeof( *a ) ) == 0 ? qtrue : qfalse;
}

static vkTemporalMotionRecordingAuthority_t ValidAuthority( void ) {
	vkTemporalMotionRecordingAuthority_t a;
	memset( &a, 0, sizeof( a ) );
	a.token = 11; a.frameId = 12; a.worldIndex = 2;
	a.width = 1280; a.height = 720; a.topologyEpoch = 3; a.planGeneration = 4;
	a.geometryBufferSize = 65536; a.uniformItemSize = 256;
	a.requiredCapacity = 256; a.rawEntMatCapacity = 512;
	a.rawEntMatAllocationGeneration = 5; a.payloadAllocationGeneration = 6;
	a.payloadLayoutGeneration = 7; a.targetAllocationGeneration = 8;
	a.pipelineLayoutAllocationGeneration = 9; a.materializationGeneration = 10;
	a.pipelineTableGeneration = 11; a.frameIndex = 1;
	return a;
}

static vkTemporalGenericPipelineReceipt_t ValidPipelineReceipt( void ) {
	vkTemporalGenericPipelineReceipt_t r;
	memset( &r, 0, sizeof( r ) );
	r.tableGeneration = 11; r.slot = 3; r.entryGeneration = 4;
	r.recipeOwnerEpoch = 5; r.recipeEntryGeneration = 6;
	r.factoryAllocationGeneration = 7; r.catalogId = 8;
	return r;
}

static vkTemporalMotionRecordingReceipt_t RecordingReceipt(
		const vkTemporalMotionRecordingAuthority_t *a ) {
	vkTemporalMotionRecordingReceipt_t r;
	memset( &r, 0, sizeof( r ) );
	r.authority = *a; r.prepared = 3; r.appended = 2;
	r.preserved = 1; r.invalidated = 1; r.ready = qtrue;
	return r;
}

typedef struct {
	uint32_t expectedSlot;
	const ralPipeline_t *expectedOrdinary;
	vkTemporalGenericPipelineReceipt_t receipt;
	ralPipeline_t *pipelines[3];
	qboolean allow;
} PreflightContext;

static qboolean Preflight( void *context, uint32_t pipelineSlot,
		const ralPipeline_t *ordinaryPipeline,
		vkTemporalGenericPipelineReceipt_t *outReceipt,
		ralPipeline_t *outPipelines[3] ) {
	PreflightContext *c = (PreflightContext *)context;
	if ( !c || !c->allow || pipelineSlot != c->expectedSlot
			|| ordinaryPipeline != c->expectedOrdinary ) return qfalse;
	*outReceipt = c->receipt;
	memcpy( outPipelines, c->pipelines, sizeof( c->pipelines ) );
	return qtrue;
}

static qboolean AppendRecordingDraw(
		vkTemporalMotionRecordingReceipt_t *recording,
		uint32_t absoluteSlot, uint32_t pipelineSlot,
		temporalMotionOutcome_t outcome,
		const vkTemporalGenericPipelineReceipt_t *pipelineReceipt ) {
	return VK_TemporalMotionDrawSequenceAppend( &recording->drawSequence,
		absoluteSlot, pipelineSlot, outcome, pipelineReceipt );
}

static unsigned char s_iqmPayload[TEMPORAL_IQM_SLOT_BYTES];

static uint64_t HashIqmRecords( const void *data, uint32_t count ) {
	const unsigned char *p = data; uint64_t h = UINT64_C(1469598103934665603);
	for ( uint32_t i = 0; i < count; ++i ) {
		const unsigned char *index = (const unsigned char *)&i;
		for ( size_t j = 0; j < sizeof( i ); ++j ) { h ^= index[j]; h *= UINT64_C(1099511628211); }
		for ( size_t j = 0; j < TEMPORAL_IQM_RECORD_SIZE; ++j ) {
			h ^= p[(size_t)i * TEMPORAL_IQM_RECORD_SIZE + j];
			h *= UINT64_C(1099511628211);
		}
	}
	return h;
}

static void Identity4( float out[16] ) {
	memset( out, 0, 16u * sizeof( float ) );
	out[0] = out[5] = out[10] = out[15] = 1.0f;
}

static temporalEntityPose_t IqmPose( int32_t frame ) {
	temporalEntityPose_t p; memset( &p, 0, sizeof( p ) );
	p.hModel = 4; p.modelToken = 0x8100; p.modelDataToken = 0x8200;
	p.modelType = 4; p.modelTopology = 3; p.modelAllocationGeneration = 5;
	p.modelContentDigest = 0x8300; p.frame = frame; p.oldframe = frame;
	p.axis[0] = p.axis[4] = p.axis[8] = 1.0f; return p;
}

static vkBindlessOrdinaryReceipt_t IqmBindless( void ) {
	vkBindlessOrdinaryReceipt_t r; memset( &r, 0, sizeof( r ) );
	r.setIdentity = 0x9100; r.samplerPoolIdentity = 0x9200;
	r.imageViewIdentity = 0x9300; r.samplerIdentity = 0x9400;
	r.imageOwnerIdentity = 0x9500; r.ordinaryDescriptorIdentity = 0x9600;
	r.imageOwnerGeneration = 2; r.samplerDefinitionDigest = 3;
	r.setGeneration = 4; r.samplerPoolGeneration = 5;
	r.imagePublicationGeneration = 6; r.imageTransactionGeneration = 7;
	r.samplerPublicationGeneration = 8; r.samplerTransactionGeneration = 9;
	r.imageSlotGeneration = 10; r.samplerSlotGeneration = 11;
	r.imageSlot = 12; r.samplerSlot = 3; r.imageBinding = 0;
	r.samplerBinding = 1; r.ready = qtrue; return r;
}

typedef struct {
	temporalIqmSequence_t sequence;
	vkTemporalIqmPayloadOwner_t payloadOwner;
	vkTemporalIqmPayloadContentReceipt_t content;
	vkTemporalIqmExact3FactoryReceipt_t factory;
	vkTemporalMainIqmPassReceipt_t pass;
} IqmFixture;

static IqmFixture ValidIqmFixture(
		const vkTemporalMotionRecordingAuthority_t *authority ) {
	IqmFixture f; temporalIqmDrawFacts_t draw;
	temporalIqmSequenceAuthority_t sequenceAuthority;
	vkTemporalIqmPayloadReceipt_t payload;
	memset( &f, 0, sizeof( f ) ); memset( &draw, 0, sizeof( draw ) );
	memset( s_iqmPayload, 0, sizeof( s_iqmPayload ) );
	draw.identity.structSize = sizeof( draw.identity );
	draw.identity.version = REF_ENTITY_MOTION_VERSION;
	draw.identity.ownerId = 2; draw.identity.generation = 3;
	draw.identity.role = REF_ENTITY_MOTION_ROLE_PLAYER_BODY;
	draw.currentPose = IqmPose( 1 ); draw.previousPose = IqmPose( 0 );
	draw.modelContentDigest = draw.currentPose.modelContentDigest;
	draw.modelTopologyGeneration = draw.currentPose.modelTopology;
	draw.modelAllocationGeneration = draw.currentPose.modelAllocationGeneration;
	draw.indexCount = 3; draw.rawVertexBuffer = 0xa100;
	draw.rawIndexBuffer = 0xa200; draw.ralVertexBuffer = 0xa300;
	draw.ralIndexBuffer = 0xa400; draw.vertexBufferBytes = 680;
	draw.indexBufferBytes = 12; draw.geometryBackend = 0xb100;
	draw.geometryGeneration = 6; draw.geometryAllocationGeneration = 7;
	draw.textureSlot = 12; draw.samplerSlot = 3;
	draw.bindless = IqmBindless(); draw.outcome = TEMPORAL_MOTION_WRITE_VALID;
	draw.previousValid = qtrue;
	sequenceAuthority.token = authority->token;
	sequenceAuthority.frameId = authority->frameId;
	sequenceAuthority.worldIndex = authority->worldIndex;
	sequenceAuthority.commandSlot = authority->frameIndex;
	sequenceAuthority.frameCount = 2;
	(void)R_TemporalIqmSequenceBuild( &sequenceAuthority, &draw, 1,
		&f.sequence );
	memset( &payload, 0, sizeof( payload ) );
	payload.backend = (ralBackend_t *)(uintptr_t)draw.geometryBackend;
	payload.layout = (ralBindGroupLayout_t *)(uintptr_t)0xb200;
	payload.buffer = (ralBuffer_t *)(uintptr_t)0xb300;
	payload.mappedIdentity = s_iqmPayload;
	payload.group = (ralBindGroup_t *)(uintptr_t)0xb400;
	payload.descriptorRange = TEMPORAL_IQM_SLOT_BYTES;
	payload.recordCapacity = TEMPORAL_IQM_MAX_RECORDS;
	payload.recordBytes = TEMPORAL_IQM_RECORD_SIZE;
	payload.ownerAllocationGeneration = 9; payload.slotAllocationGeneration = 10;
	payload.prepareGeneration = 11; payload.commandSlot = authority->frameIndex;
	payload.frameCount = 2; payload.ready = qtrue;
	f.payloadOwner.key.backend = payload.backend;
	f.payloadOwner.key.maxStorageBufferRange = TEMPORAL_IQM_SLOT_BYTES;
	f.payloadOwner.key.frameCount = 2; f.payloadOwner.layout = payload.layout;
	f.payloadOwner.slots[payload.commandSlot].buffer = payload.buffer;
	f.payloadOwner.slots[payload.commandSlot].mapped = payload.mappedIdentity;
	f.payloadOwner.slots[payload.commandSlot].group = payload.group;
	f.payloadOwner.slots[payload.commandSlot].allocationGeneration =
		payload.slotAllocationGeneration;
	f.payloadOwner.slots[payload.commandSlot].prepareGeneration =
		payload.prepareGeneration;
	f.payloadOwner.ownerAllocationGeneration = payload.ownerAllocationGeneration;
	f.payloadOwner.nextOwnerAllocationGeneration = payload.ownerAllocationGeneration;
	f.payloadOwner.nextSlotAllocationGeneration[payload.commandSlot] =
		payload.slotAllocationGeneration;
	f.payloadOwner.nextPrepareGeneration[payload.commandSlot] = payload.prepareGeneration;
	f.payloadOwner.preparedSlot = payload.commandSlot;
	f.payloadOwner.readySlotMask = 1u << payload.commandSlot;
	f.payloadOwner.initialized = f.payloadOwner.ready = qtrue;
	f.content.authority = sequenceAuthority; f.content.payload = payload;
	f.content.camera.frameId = authority->frameId;
	f.content.camera.previousFrameId = authority->frameId - 1u;
	Identity4( f.content.camera.current.projection );
	Identity4( f.content.camera.current.worldModel );
	Identity4( f.content.camera.previous.projection );
	Identity4( f.content.camera.previous.worldModel );
	f.content.camera.valid = f.content.camera.previousValid = qtrue;
	Identity4( f.content.jitteredProjection );
	f.content.sequenceDigest = f.sequence.orderedDigest;
	f.content.contentDigest = HashIqmRecords( s_iqmPayload, 1 );
	f.content.recordCount = 1; f.content.ready = qtrue;
	f.factory.backend = payload.backend; f.factory.device = (VkDevice)(uintptr_t)0xb500;
	f.factory.payloadOwner = &f.payloadOwner; f.factory.payloadLayout = payload.layout;
	f.factory.payloadRawLayout = (void *)(uintptr_t)0xb600;
	f.factory.bindlessRawLayout = (void *)(uintptr_t)0xb700;
	f.factory.payloadLayoutGeneration = payload.ownerAllocationGeneration;
	f.factory.bindless.backend = payload.backend;
	f.factory.bindless.layout = (ralBindGroupLayout_t *)(uintptr_t)0xb800;
	f.factory.bindless.setIdentity = (void *)(uintptr_t)draw.bindless.setIdentity;
	f.factory.bindless.setGeneration = draw.bindless.setGeneration;
	f.factory.bindless.ready = qtrue;
	f.factory.rawLayout = (VkPipelineLayout)(uintptr_t)0xb900;
	f.factory.adoptedLayout = (ralPipelineLayout_t *)(uintptr_t)0xba00;
	f.factory.writePipeline = (ralPipeline_t *)(uintptr_t)0xbb00;
	f.factory.invalidatePipeline = (ralPipeline_t *)(uintptr_t)0xbc00;
	f.factory.shaderGeneration = 2; f.factory.pipelineGeneration = 3;
	f.factory.topologyGeneration = authority->topologyEpoch;
	f.factory.allocationGeneration = 4;
	f.factory.sceneFormat = RAL_FORMAT_R16G16B16A16_SFLOAT;
	f.factory.depthFormat = RAL_FORMAT_D32_SFLOAT;
	f.factory.reversedDepth = qtrue; f.factory.ready = qtrue;
	f.pass.topologyGeneration = authority->topologyEpoch;
	f.pass.sceneFormat = f.factory.sceneFormat; f.pass.depthFormat = f.factory.depthFormat;
	f.pass.reversedDepth = qtrue; f.pass.ready = qtrue;
	return f;
}

static void RepairIqmFixture( IqmFixture *f ) {
	if ( f ) f->factory.payloadOwner = &f->payloadOwner;
}

typedef struct {
	vkTemporalIqmGeometryReceipt_t geometry;
	vkBindlessOrdinaryReceipt_t bindless;
	int geometryMutation;
	int bindlessMutation;
} IqmLiveContext;

static IqmLiveContext LiveContextFor(
		const temporalIqmSequenceEntry_t *entry ) {
	IqmLiveContext live;
	const temporalIqmDrawFacts_t *facts = &entry->facts;
	memset( &live, 0, sizeof( live ) );
	live.geometry.key.backend = (ralBackend_t *)facts->geometryBackend;
	live.geometry.key.nativeVertexBuffer = (void *)facts->rawVertexBuffer;
	live.geometry.key.nativeIndexBuffer = (void *)facts->rawIndexBuffer;
	live.geometry.key.vertexBytes = facts->vertexBufferBytes;
	live.geometry.key.indexBytes = facts->indexBufferBytes;
	live.geometry.key.modelAllocationGeneration = facts->modelAllocationGeneration;
	live.geometry.key.geometryGeneration = facts->geometryGeneration;
	live.geometry.key.contentDigest = facts->modelContentDigest;
	live.geometry.vertex = (ralBuffer_t *)facts->ralVertexBuffer;
	live.geometry.index = (ralBuffer_t *)facts->ralIndexBuffer;
	live.geometry.allocationGeneration = facts->geometryAllocationGeneration;
	live.geometry.ready = qtrue;
	live.bindless = facts->bindless;
	return live;
}

static qboolean RevalidateGeometry( void *context,
		const temporalIqmDrawFacts_t *expected,
		vkTemporalIqmGeometryReceipt_t *outCurrent ) {
	IqmLiveContext *live = context;
	if ( !expected || !outCurrent || !live ) return qfalse;
	*outCurrent = live->geometry;
	switch ( live->geometryMutation ) {
	case 1: outCurrent->key.backend = (ralBackend_t *)(uintptr_t)0xee00; break;
	case 2: outCurrent->key.nativeVertexBuffer = (void *)(uintptr_t)0xee01; break;
	case 3: outCurrent->key.nativeIndexBuffer = (void *)(uintptr_t)0xee02; break;
	case 4: outCurrent->key.vertexBytes += TEMPORAL_IQM_VERTEX_STRIDE; break;
	case 5: outCurrent->key.indexBytes += sizeof( uint32_t ); break;
	case 6: outCurrent->key.modelAllocationGeneration++; break;
	case 7: outCurrent->key.geometryGeneration++; break;
	case 8: outCurrent->key.contentDigest++; break;
	case 9: outCurrent->vertex = (ralBuffer_t *)(uintptr_t)0xee03; break;
	case 10: outCurrent->index = (ralBuffer_t *)(uintptr_t)0xee04; break;
	case 11: outCurrent->allocationGeneration++; break;
	case 12: outCurrent->ready = qfalse; break;
	}
	return qtrue;
}

static qboolean RevalidateBindless( void *context,
		const temporalIqmDrawFacts_t *expected,
		vkBindlessOrdinaryReceipt_t *outCurrent ) {
	IqmLiveContext *live = context;
	if ( !expected || !outCurrent || !live ) return qfalse;
	*outCurrent = live->bindless;
	switch ( live->bindlessMutation ) {
	case 1: outCurrent->setIdentity++; break;
	case 2: outCurrent->samplerPoolIdentity++; break;
	case 3: outCurrent->imageViewIdentity++; break;
	case 4: outCurrent->samplerIdentity++; break;
	case 5: outCurrent->imageOwnerIdentity++; break;
	case 6: outCurrent->ordinaryDescriptorIdentity++; break;
	case 7: outCurrent->imageOwnerGeneration++; break;
	case 8: outCurrent->samplerDefinitionDigest++; break;
	case 9: outCurrent->setGeneration++; break;
	case 10: outCurrent->samplerPoolGeneration++; break;
	case 11: outCurrent->imagePublicationGeneration++; break;
	case 12: outCurrent->imageTransactionGeneration++; break;
	case 13: outCurrent->samplerPublicationGeneration++; break;
	case 14: outCurrent->samplerTransactionGeneration++; break;
	case 15: outCurrent->imageSlotGeneration++; break;
	case 16: outCurrent->samplerSlotGeneration++; break;
	case 17: outCurrent->imageSlot++; break;
	case 18: outCurrent->samplerSlot++; break;
	case 19: outCurrent->imageBinding++; break;
	case 20: outCurrent->samplerBinding++; break;
	case 21: outCurrent->ready = qfalse; break;
	}
	return qtrue;
}

static const vkTemporalMainIqmActivationOps_t iqmRevalidateOps = {
	RevalidateGeometry, RevalidateBindless
};

static uint64_t OracleTaggedMix( uint64_t state, uint64_t value ) {
	state ^= value + UINT64_C( 0x9e3779b97f4a7c15 ) + ( state << 6 )
		+ ( state >> 2 );
	state ^= state >> 30; state *= UINT64_C( 0xbf58476d1ce4e5b9 );
	state ^= state >> 27; state *= UINT64_C( 0x94d049bb133111eb );
	return state ^ ( state >> 31 );
}

static uint64_t OracleGenericDigest( temporalMotionOutcome_t outcome,
		uint32_t absoluteSlot, uint32_t pipelineSlot,
		const vkTemporalGenericPipelineReceipt_t *receipt ) {
	uint64_t digest = UINT64_C( 0x510e527fade682d1 );
	const uint64_t words[10] = {
		(uint32_t)outcome, absoluteSlot, pipelineSlot,
		receipt ? receipt->tableGeneration : 0,
		receipt ? receipt->slot : 0, receipt ? receipt->entryGeneration : 0,
		receipt ? receipt->recipeOwnerEpoch : 0,
		receipt ? receipt->recipeEntryGeneration : 0,
		receipt ? receipt->factoryAllocationGeneration : 0,
		receipt ? receipt->catalogId : 0
	};
	for ( uint32_t i = 0; i < 10u; ++i )
		digest = OracleTaggedMix( digest, words[i] );
	return digest;
}

static uint64_t OracleFactoryDigest(
		const vkTemporalIqmExact3FactoryReceipt_t *factory ) {
	uint64_t digest = UINT64_C( 0x1f83d9abfb41bd6b );
	const uint64_t words[] = {
		(uintptr_t)factory->backend, (uintptr_t)factory->device,
		(uintptr_t)factory->payloadOwner, (uintptr_t)factory->payloadLayout,
		(uintptr_t)factory->payloadRawLayout, (uintptr_t)factory->bindlessRawLayout,
		factory->payloadLayoutGeneration, (uintptr_t)factory->bindless.backend,
		(uintptr_t)factory->bindless.layout, (uintptr_t)factory->bindless.setIdentity,
		factory->bindless.setGeneration, (uintptr_t)factory->rawLayout,
		(uintptr_t)factory->adoptedLayout, (uintptr_t)factory->writePipeline,
		(uintptr_t)factory->invalidatePipeline, factory->shaderGeneration,
		factory->pipelineGeneration, factory->topologyGeneration,
		factory->allocationGeneration, (uint32_t)factory->sceneFormat,
		(uint32_t)factory->depthFormat, (uint32_t)factory->reversedDepth
	};
	for ( uint32_t i = 0; i < sizeof( words ) / sizeof( words[0] ); ++i )
		digest = OracleTaggedMix( digest, words[i] );
	return digest;
}

static uint64_t OracleIqmDigest( const temporalIqmSequence_t *sequence,
		const temporalIqmSequenceEntry_t *entry,
		const vkTemporalIqmExact3FactoryReceipt_t *factory ) {
	uint64_t digest = OracleTaggedMix(
		sequence->orderedDigest, OracleFactoryDigest( factory ) );
	digest = OracleTaggedMix( digest, entry->facts.ordinal );
	digest = OracleTaggedMix( digest, entry->recordIndex );
	return OracleTaggedMix( digest, (uint32_t)entry->facts.outcome );
}

static qboolean OracleTaggedAppend( vkTemporalMainTaggedSequence_t *sequence,
		vkTemporalMainEventKind_t kind, uint32_t ordinal,
		uint32_t recordIndex, vkTemporalShaderRecipeKind_t recipe,
		uint64_t familyDigest ) {
	uint64_t words[7];
	if ( !sequence ) return qfalse;
	words[0] = (uint32_t)kind; words[1] = ordinal; words[2] = recordIndex;
	words[3] = (uint32_t)recipe; words[4] = familyDigest;
	words[5] = familyDigest >> 32; words[6] = sequence->count + 1u;
	if ( !sequence->count ) {
		sequence->lane0 = UINT64_C( 0x6a09e667f3bcc909 );
		sequence->lane1 = UINT64_C( 0xbb67ae8584caa73b );
	}
	for ( uint32_t i = 0; i < 7u; ++i ) {
		sequence->lane0 = OracleTaggedMix( sequence->lane0, words[i] );
		sequence->lane1 = OracleTaggedMix( sequence->lane1,
			words[6u - i] ^ UINT64_C( 0x3c6ef372fe94f82b ) );
	}
	sequence->count++;
	if ( kind == VK_TEMPORAL_MAIN_EVENT_GENERIC ) sequence->genericCount++;
	else sequence->iqmCount++;
	return qtrue;
}

static qboolean RunMixedGenericIqmGeneric(
		const vkTemporalMotionRecordingAuthority_t *authority,
		IqmFixture *iqm, IqmLiveContext *live,
		uint32_t firstAbsoluteSlot, const ralPipeline_t *ordinary,
		PreflightContext *preflight,
		const vkTemporalMainActivationOps_t *ops,
		const vkTemporalGenericPipelineReceipt_t *pipelineReceipt,
		vkTemporalMainTaggedSequence_t *outTagged ) {
	vkTemporalMainActivationOwner_t owner;
	vkTemporalMainActivationPlan_t generic;
	vkTemporalMainIqmActivationPlan_t direct;
	vkTemporalMainActivationReceipt_t pending;
	vkTemporalMotionRecordingReceipt_t recording;
	const temporalIqmDrawFacts_t *observed;
	if ( !authority || !iqm || !live || !ordinary || !preflight || !ops
			|| !pipelineReceipt || !outTagged ) return qfalse;
	observed = &iqm->sequence.entries[0].facts;
	VK_TemporalMainActivationInit( &owner );
	if ( !VK_TemporalMainActivationBegin( &owner, authority )
			|| !VK_TemporalMainActivationBindIqm( &owner, &iqm->sequence,
				&iqm->content, &iqm->payloadOwner, &iqm->factory, &iqm->pass,
				live, &iqmRevalidateOps )
			|| !VK_TemporalMainActivationPlanDraw( &owner,
				TEMPORAL_MOTION_WRITE_VALID, firstAbsoluteSlot, 3, ordinary,
				preflight, ops, &generic )
			|| !VK_TemporalMainActivationCommitDraw( &owner, &generic )
			|| !VK_TemporalMainActivationPlanIqmDraw(
				&owner, observed, &direct )
			|| !VK_TemporalMainActivationCommitIqmDraw( &owner, &direct )
			|| !VK_TemporalMainActivationPlanDraw( &owner,
				TEMPORAL_MOTION_INVALIDATE_OPAQUE, 12, 3, ordinary,
				preflight, ops, &generic )
			|| !VK_TemporalMainActivationCommitDraw( &owner, &generic ) )
		return qfalse;
	recording = RecordingReceipt( authority ); recording.prepared = 2;
	recording.appended = 2; recording.preserved = 0; recording.invalidated = 1;
	if ( !AppendRecordingDraw( &recording, firstAbsoluteSlot, 3,
			TEMPORAL_MOTION_WRITE_VALID, pipelineReceipt )
			|| !AppendRecordingDraw( &recording, 12, 3,
				TEMPORAL_MOTION_INVALIDATE_OPAQUE, pipelineReceipt )
			|| !VK_TemporalMainActivationFinishIqm( &owner, &recording,
				&iqm->sequence, &iqm->payloadOwner, &iqm->factory )
			|| !VK_TemporalMainActivationPeekPendingReceipt( &owner, &pending ) )
		return qfalse;
	*outTagged = pending.taggedSequence;
	return qtrue;
}

int main( void ) {
	int mutation;
	vkTemporalMainActivationOwner_t owner, before;
	vkTemporalMainActivationPlan_t plan, stale, planSentinel;
	vkTemporalMainActivationReceipt_t receipt, sentinel;
	vkTemporalMotionRecordingAuthority_t authority = ValidAuthority();
	vkTemporalMotionRecordingReceipt_t recording;
	vkTemporalGenericPipelineReceipt_t pipelineReceipt = ValidPipelineReceipt();
	const ralPipeline_t *ordinary = (ralPipeline_t *)(uintptr_t)0x4000;
	ralPipeline_t *pipelines[3] = {
		(ralPipeline_t *)(uintptr_t)0x1000,
		(ralPipeline_t *)(uintptr_t)0x2000,
		(ralPipeline_t *)(uintptr_t)0x3000
	};
	PreflightContext preflight = { 3, ordinary, { 0 }, { NULL, NULL, NULL }, qtrue };
	const vkTemporalMainActivationOps_t ops = { Preflight };
	preflight.receipt = pipelineReceipt;
	memcpy( preflight.pipelines, pipelines, sizeof( pipelines ) );

	VK_TemporalMainActivationInit( &owner );
	CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
	CHECK( VK_TemporalMainActivationRequiresDepthStencilStore( &owner ) );
	CHECK( VK_TemporalMainActivationPlanDraw( &owner, TEMPORAL_MOTION_PRESERVE,
		10, 3, NULL, NULL, NULL, &plan ) );
	CHECK( plan.kind == VK_TEMPORAL_SHADER_PRESERVE && !plan.endOrdinary
		&& !plan.beginExact3 && !plan.pipeline );
	CHECK( VK_TemporalMainActivationCommitDraw( &owner, &plan ) );

	CHECK( VK_TemporalMainActivationPlanDraw( &owner, TEMPORAL_MOTION_WRITE_VALID,
		11, 3, ordinary, &preflight, &ops, &plan ) );
	CHECK( plan.kind == VK_TEMPORAL_SHADER_WRITE && plan.pipeline == pipelines[1] );
	CHECK( plan.endOrdinary && plan.beginExact3 && plan.clearAuxiliary
		&& plan.endExact3 && plan.resumeOrdinary );
	stale = plan; stale.planSerial++;
	before = owner;
	CHECK( !VK_TemporalMainActivationCommitDraw( &owner, &stale ) );
	CHECK( owner.poisoned );
	owner = before;
	CHECK( VK_TemporalMainActivationCommitDraw( &owner, &plan ) );

	CHECK( VK_TemporalMainActivationPlanDraw( &owner,
		TEMPORAL_MOTION_INVALIDATE_OPAQUE, 12, 3, ordinary,
		&preflight, &ops, &plan ) );
	CHECK( plan.kind == VK_TEMPORAL_SHADER_INVALIDATE
		&& plan.pipeline == pipelines[2] && !plan.clearAuxiliary );
	CHECK( VK_TemporalMainActivationCommitDraw( &owner, &plan ) );

	recording = RecordingReceipt( &authority );
	CHECK( AppendRecordingDraw( &recording, 10, 3,
		TEMPORAL_MOTION_PRESERVE, NULL ) );
	CHECK( AppendRecordingDraw( &recording, 11, 3,
		TEMPORAL_MOTION_WRITE_VALID, &pipelineReceipt ) );
	CHECK( AppendRecordingDraw( &recording, 12, 3,
		TEMPORAL_MOTION_INVALIDATE_OPAQUE, &pipelineReceipt ) );
	CHECK( VK_TemporalMainActivationFinish( &owner, &recording ) );
	memset( &receipt, 0, sizeof( receipt ) );
	CHECK( !VK_TemporalMainActivationGetReceipt( &owner, &receipt ) );
	CHECK( VK_TemporalMainActivationPeekPendingReceipt( &owner, &receipt ) );
	CHECK( receipt.ready && receipt.written == 1 && receipt.invalidated == 1 );
	CHECK( VK_TemporalMainActivationResolveSubmit( &owner, qtrue ) );
	CHECK( VK_TemporalMainActivationGetReceipt( &owner, &receipt ) );
	CHECK( receipt.ready && receipt.prepared == 3 && receipt.temporalSegments == 2
		&& receipt.preserved == 1 && receipt.written == 1
		&& receipt.invalidated == 1 && receipt.auxiliaryCleared
		&& receipt.depthStoreRequired && receipt.stencilStoreRequired
		&& VK_TemporalMotionDrawSequenceEqual( &receipt.drawSequence,
			&recording.drawSequence ) );
	// A rejected new transaction cannot expose the prior transaction receipt.
	authority.rawEntMatAllocationGeneration = UINT32_MAX;
	CHECK( !VK_TemporalMainActivationBegin( &owner, &authority ) );
	memset( &sentinel, 0xA5, sizeof( sentinel ) ); receipt = sentinel;
	CHECK( !VK_TemporalMainActivationGetReceipt( &owner, &receipt ) );
	CHECK( memcmp( &receipt, &sentinel, sizeof( receipt ) ) == 0 );
	authority = ValidAuthority();
	// Reusing the owner starts a fresh ordered sequence rather than carrying the
	// prior frame's digest into the next transaction.
	CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
	CHECK( VK_TemporalMainActivationPlanDraw( &owner, TEMPORAL_MOTION_WRITE_VALID,
		21, 3, ordinary, &preflight, &ops, &plan ) );
	CHECK( VK_TemporalMainActivationCommitDraw( &owner, &plan ) );
	recording = RecordingReceipt( &authority ); recording.prepared = 1;
	recording.appended = 1; recording.preserved = 0; recording.invalidated = 0;
	CHECK( AppendRecordingDraw( &recording, 21, 3,
		TEMPORAL_MOTION_WRITE_VALID, &pipelineReceipt ) );
	CHECK( VK_TemporalMainActivationFinish( &owner, &recording ) );
	CHECK( VK_TemporalMainActivationResolveSubmit( &owner, qtrue ) );

	// Every generation/key/count mismatch leaves the output receipt untouched.
	VK_TemporalMainActivationInit( &owner );
	CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
	CHECK( VK_TemporalMainActivationPlanDraw( &owner, TEMPORAL_MOTION_WRITE_VALID,
		11, 3, ordinary, &preflight, &ops, &plan ) );
	CHECK( VK_TemporalMainActivationCommitDraw( &owner, &plan ) );
	recording = RecordingReceipt( &authority ); recording.prepared = 1;
	recording.appended = 1; recording.preserved = 0; recording.invalidated = 0;
	CHECK( AppendRecordingDraw( &recording, 11, 3,
		TEMPORAL_MOTION_WRITE_VALID, &pipelineReceipt ) );
	recording.authority.pipelineLayoutAllocationGeneration++;
	CHECK( !VK_TemporalMainActivationFinish( &owner, &recording ) );

	// Preserve is still correlated to a physical entMat slot and cannot escape
	// the command-wide capacity proof.
	VK_TemporalMainActivationInit( &owner );
	CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
	CHECK( !VK_TemporalMainActivationPlanDraw( &owner,
		TEMPORAL_MOTION_PRESERVE, authority.requiredCapacity, 3,
		NULL, NULL, NULL, &plan ) && owner.poisoned );
	memset( &sentinel, 0xA5, sizeof( sentinel ) ); receipt = sentinel;
	CHECK( !VK_TemporalMainActivationGetReceipt( &owner, &receipt ) );
	CHECK( memcmp( &receipt, &sentinel, sizeof( receipt ) ) == 0 );

	// Exact3 receipt and alias mutations poison before publication.
	VK_TemporalMainActivationInit( &owner );
	CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
	preflight.receipt.tableGeneration++;
	memset( &planSentinel, 0xA5, sizeof( planSentinel ) ); plan = planSentinel;
	CHECK( !VK_TemporalMainActivationPlanDraw( &owner, TEMPORAL_MOTION_WRITE_VALID,
		11, 3, ordinary, &preflight, &ops, &plan ) && owner.poisoned );
	CHECK( memcmp( &plan, &planSentinel, sizeof( plan ) ) == 0 );
	preflight.receipt = ValidPipelineReceipt();
	VK_TemporalMainActivationInit( &owner );
	CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
	CHECK( !VK_TemporalMainActivationPlanDraw( &owner, TEMPORAL_MOTION_WRITE_VALID,
		11, 4, ordinary, &preflight, &ops, &plan ) && owner.poisoned );
	VK_TemporalMainActivationInit( &owner );
	CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
	preflight.pipelines[2] = preflight.pipelines[1];
	CHECK( !VK_TemporalMainActivationPlanDraw( &owner,
		TEMPORAL_MOTION_INVALIDATE_OPAQUE, 12, 3, ordinary,
		&preflight, &ops, &plan )
		&& owner.poisoned );
	preflight.pipelines[2] = pipelines[2];
	VK_TemporalMainActivationInit( &owner );
	CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
	CHECK( !VK_TemporalMainActivationPlanDraw( &owner,
		TEMPORAL_MOTION_WRITE_VALID, 11, 3,
		(ralPipeline_t *)(uintptr_t)0x5000, &preflight, &ops, &plan )
		&& owner.poisoned );

	// Every receipt identity, NULL candidate and alias pair is independently
	// fail-closed and leaves the caller's plan output untouched.
	for ( mutation = 0; mutation < 13; ++mutation ) {
		preflight.expectedSlot = 3;
		preflight.expectedOrdinary = ordinary;
		preflight.receipt = pipelineReceipt;
		memcpy( preflight.pipelines, pipelines, sizeof( pipelines ) );
		switch ( mutation ) {
		case 0: preflight.receipt.tableGeneration++; break;
		case 1: preflight.receipt.slot++; break;
		case 2: preflight.receipt.entryGeneration = 0; break;
		case 3: preflight.receipt.recipeOwnerEpoch = 0; break;
		case 4: preflight.receipt.recipeEntryGeneration = 0; break;
		case 5: preflight.receipt.factoryAllocationGeneration = 0; break;
		case 6: preflight.receipt.catalogId = 0; break;
		case 7: preflight.pipelines[0] = NULL; break;
		case 8: preflight.pipelines[1] = NULL; break;
		case 9: preflight.pipelines[2] = NULL; break;
		case 10: preflight.pipelines[1] = preflight.pipelines[0]; break;
		case 11: preflight.pipelines[2] = preflight.pipelines[0]; break;
		case 12: preflight.pipelines[2] = preflight.pipelines[1]; break;
		}
		VK_TemporalMainActivationInit( &owner );
		CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
		memset( &planSentinel, 0xA5, sizeof( planSentinel ) );
		plan = planSentinel;
		CHECK( !VK_TemporalMainActivationPlanDraw( &owner,
			TEMPORAL_MOTION_WRITE_VALID, 11, 3, ordinary,
			&preflight, &ops, &plan ) && owner.poisoned );
		CHECK( memcmp( &plan, &planSentinel, sizeof( plan ) ) == 0 );
	}
	preflight.receipt = pipelineReceipt;
	memcpy( preflight.pipelines, pipelines, sizeof( pipelines ) );

	// Equal aggregate counts cannot authorize a different ordered slot sequence.
	VK_TemporalMainActivationInit( &owner );
	CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
	CHECK( VK_TemporalMainActivationPlanDraw( &owner, TEMPORAL_MOTION_WRITE_VALID,
		11, 3, ordinary, &preflight, &ops, &plan ) );
	CHECK( VK_TemporalMainActivationCommitDraw( &owner, &plan ) );
	recording = RecordingReceipt( &authority ); recording.prepared = 1;
	recording.appended = 1; recording.preserved = 0; recording.invalidated = 0;
	CHECK( AppendRecordingDraw( &recording, 13, 3,
		TEMPORAL_MOTION_WRITE_VALID, &pipelineReceipt ) );
	CHECK( !VK_TemporalMainActivationFinish( &owner, &recording ) );

	// Authored target state is cancelled rather than published on submit failure.
	VK_TemporalMainActivationInit( &owner );
	CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
	CHECK( VK_TemporalMainActivationPlanDraw( &owner, TEMPORAL_MOTION_WRITE_VALID,
		11, 3, ordinary, &preflight, &ops, &plan ) );
	CHECK( VK_TemporalMainActivationCommitDraw( &owner, &plan ) );
	recording = RecordingReceipt( &authority ); recording.prepared = 1;
	recording.appended = 1; recording.preserved = 0; recording.invalidated = 0;
	CHECK( AppendRecordingDraw( &recording, 11, 3,
		TEMPORAL_MOTION_WRITE_VALID, &pipelineReceipt ) );
	CHECK( VK_TemporalMainActivationFinish( &owner, &recording ) );
	CHECK( !VK_TemporalMainActivationGetReceipt( &owner, &receipt ) );
	CHECK( !VK_TemporalMainActivationResolveSubmit( &owner, qfalse ) );
	CHECK( !VK_TemporalMainActivationGetReceipt( &owner, &receipt ) );

	// Empty/preserve-only frames never publish auxiliary target authority.
	VK_TemporalMainActivationInit( &owner );
	CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
	CHECK( VK_TemporalMainActivationPlanDraw( &owner, TEMPORAL_MOTION_PRESERVE,
		10, 3, NULL, NULL, NULL, &plan ) );
	CHECK( VK_TemporalMainActivationCommitDraw( &owner, &plan ) );
	recording = RecordingReceipt( &authority ); recording.prepared = 1;
	recording.appended = 0; recording.preserved = 1; recording.invalidated = 0;
	CHECK( AppendRecordingDraw( &recording, 10, 3,
		TEMPORAL_MOTION_PRESERVE, NULL ) );
	CHECK( !VK_TemporalMainActivationFinish( &owner, &recording ) );

	// Generic and IQM logical draws author one online command-adjacent tagged
	// stream. The immutable IQM verifier advances only after a successful
	// logical draw commit, and the first committed temporal segment owns CLEAR.
	{
		IqmFixture iqm = ValidIqmFixture( &authority );
		vkTemporalMainIqmActivationPlan_t iqmPlan, badIqmPlan, iqmSentinel;
		vkTemporalMainActivationOwner_t plannedOwner;
		vkTemporalMainTaggedSequence_t genericFirstTagged;
		vkTemporalMainTaggedSequence_t genericFirstOracle, iqmFirstOracle;
		temporalIqmDrawFacts_t observed;
		IqmLiveContext live;
		RepairIqmFixture( &iqm );
		live = LiveContextFor( &iqm.sequence.entries[0] );
		observed = iqm.sequence.entries[0].facts;
		CHECK( R_TemporalIqmSequenceExact( &iqm.sequence, &iqm.sequence ) );
		CHECK( VK_TemporalIqmPayloadContentReceiptExact( &iqm.content,
			&iqm.content ) );

		VK_TemporalMainActivationInit( &owner );
		CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
		CHECK( VK_TemporalMainActivationBindIqm( &owner, &iqm.sequence,
			&iqm.content, &iqm.payloadOwner, &iqm.factory, &iqm.pass,
			&live, &iqmRevalidateOps ) );
		CHECK( VK_TemporalMainActivationPlanDraw( &owner,
			TEMPORAL_MOTION_WRITE_VALID, 11, 3, ordinary,
			&preflight, &ops, &plan ) && plan.clearAuxiliary );
		CHECK( VK_TemporalMainActivationCommitDraw( &owner, &plan ) );
		CHECK( VK_TemporalMainActivationPlanIqmDraw(
			&owner, &observed, &iqmPlan ) );
		CHECK( !iqmPlan.clearAuxiliary
			&& iqmPlan.payloadGroup == iqm.content.payload.group
			&& iqmPlan.entry.facts.sourceDrawSurfOrdinal
				== observed.sourceDrawSurfOrdinal );
		plannedOwner = owner;
		for ( mutation = 0; mutation < 19; ++mutation ) {
			owner = plannedOwner; badIqmPlan = iqmPlan;
			switch ( mutation ) {
			case 0: badIqmPlan.entry.facts.rawVertexBuffer++; break;
			case 1: badIqmPlan.entry.facts.rawIndexBuffer++; break;
			case 2: badIqmPlan.entry.facts.ralVertexBuffer++; break;
			case 3: badIqmPlan.entry.facts.ralIndexBuffer++; break;
			case 4: badIqmPlan.entry.facts.vertexBufferBytes += TEMPORAL_IQM_VERTEX_STRIDE; break;
			case 5: badIqmPlan.entry.facts.indexBufferBytes += sizeof( uint32_t ); break;
			case 6: badIqmPlan.entry.facts.firstIndex++; break;
			case 7: badIqmPlan.entry.facts.indexCount++; break;
			case 8: badIqmPlan.entry.facts.textureSlot++; break;
			case 9: badIqmPlan.entry.facts.samplerSlot++; break;
			case 10: badIqmPlan.entry.recordIndex++; break;
			case 11: badIqmPlan.entry.facts.outcome = TEMPORAL_MOTION_INVALIDATE_OPAQUE; break;
			case 12: badIqmPlan.payloadGroup = (ralBindGroup_t *)(uintptr_t)0xdd00; break;
			case 13: badIqmPlan.pipeline = badIqmPlan.factory.invalidatePipeline; break;
			case 14: badIqmPlan.factory.allocationGeneration++; break;
			case 15: badIqmPlan.factory.bindless.setGeneration++; break;
			case 16: badIqmPlan.entry.facts.geometryGeneration++; break;
			case 17: badIqmPlan.entry.facts.bindless.imageSlotGeneration++; break;
			case 18: badIqmPlan.entry.facts.sourceDrawSurfOrdinal++; break;
			}
			CHECK( !VK_TemporalMainActivationCommitIqmDraw(
				&owner, &badIqmPlan ) && owner.poisoned );
		}
		owner = plannedOwner;
		CHECK( VK_TemporalMainActivationCommitIqmDraw( &owner, &iqmPlan ) );
		CHECK( owner.iqmVerifier.cursor == 1 );
		CHECK( VK_TemporalMainActivationPlanDraw( &owner,
			TEMPORAL_MOTION_INVALIDATE_OPAQUE, 12, 3, ordinary,
			&preflight, &ops, &plan ) && !plan.clearAuxiliary );
		CHECK( VK_TemporalMainActivationCommitDraw( &owner, &plan ) );
		recording = RecordingReceipt( &authority ); recording.prepared = 2;
		recording.appended = 2; recording.preserved = 0; recording.invalidated = 1;
		CHECK( AppendRecordingDraw( &recording, 11, 3,
			TEMPORAL_MOTION_WRITE_VALID, &pipelineReceipt ) );
		CHECK( AppendRecordingDraw( &recording, 12, 3,
			TEMPORAL_MOTION_INVALIDATE_OPAQUE, &pipelineReceipt ) );
		CHECK( VK_TemporalMainActivationFinishIqm( &owner, &recording,
			&iqm.sequence, &iqm.payloadOwner, &iqm.factory ) );
		CHECK( VK_TemporalMainActivationPeekPendingReceipt( &owner, &receipt ) );
		CHECK( receipt.iqm.ready && receipt.iqm.prepared == 1
			&& receipt.iqm.written == 1 && receipt.taggedSequence.count == 3
			&& receipt.taggedSequence.genericCount == 2
			&& receipt.taggedSequence.iqmCount == 1 );
		genericFirstTagged = receipt.taggedSequence;
		memset( &genericFirstOracle, 0, sizeof( genericFirstOracle ) );
		CHECK( OracleTaggedAppend( &genericFirstOracle,
			VK_TEMPORAL_MAIN_EVENT_GENERIC, 0, 11, VK_TEMPORAL_SHADER_WRITE,
			OracleGenericDigest( TEMPORAL_MOTION_WRITE_VALID, 11, 3,
				&pipelineReceipt ) ) );
		CHECK( OracleTaggedAppend( &genericFirstOracle,
			VK_TEMPORAL_MAIN_EVENT_IQM, 0, iqm.sequence.entries[0].recordIndex,
			VK_TEMPORAL_SHADER_WRITE, OracleIqmDigest( &iqm.sequence,
				&iqm.sequence.entries[0], &iqm.factory ) ) );
		CHECK( OracleTaggedAppend( &genericFirstOracle,
			VK_TEMPORAL_MAIN_EVENT_GENERIC, 1, 12,
			VK_TEMPORAL_SHADER_INVALIDATE,
			OracleGenericDigest( TEMPORAL_MOTION_INVALIDATE_OPAQUE, 12, 3,
				&pipelineReceipt ) ) );
		CHECK( memcmp( &receipt.taggedSequence, &genericFirstOracle,
			sizeof( genericFirstOracle ) ) == 0 );
		before = owner;
		for ( mutation = 0; mutation < 2; ++mutation ) {
			owner = before;
			if ( mutation == 0 ) owner.pendingReceipt.taggedSequence.lane0 = 0;
			else owner.pendingReceipt.taggedSequence.lane1 = 0;
			memset( &receipt, 0xA5, sizeof( receipt ) ); sentinel = receipt;
			CHECK( !VK_TemporalMainActivationPeekPendingReceipt( &owner, &receipt ) );
			CHECK( memcmp( &receipt, &sentinel, sizeof( receipt ) ) == 0 );
			CHECK( !VK_TemporalMainActivationResolveSubmit( &owner, qtrue ) );
			CHECK( !VK_TemporalMainActivationGetReceipt( &owner, &receipt ) );
		}
		owner = before;
		CHECK( VK_TemporalMainActivationResolveSubmit( &owner, qtrue ) );
		CHECK( VK_TemporalMainActivationGetReceipt( &owner, &receipt ) );
		before = owner;
		for ( mutation = 0; mutation < 2; ++mutation ) {
			owner = before;
			if ( mutation == 0 ) owner.receipt.taggedSequence.lane0 = 0;
			else owner.receipt.taggedSequence.lane1 = 0;
			memset( &receipt, 0xA5, sizeof( receipt ) ); sentinel = receipt;
			CHECK( !VK_TemporalMainActivationGetReceipt( &owner, &receipt ) );
			CHECK( memcmp( &receipt, &sentinel, sizeof( receipt ) ) == 0 );
		}

		// IQM-first is also valid, but produces a different observed-order digest.
		VK_TemporalMainActivationInit( &owner );
		CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
		CHECK( VK_TemporalMainActivationBindIqm( &owner, &iqm.sequence,
			&iqm.content, &iqm.payloadOwner, &iqm.factory, &iqm.pass,
			&live, &iqmRevalidateOps ) );
		CHECK( VK_TemporalMainActivationPlanIqmDraw(
			&owner, &observed, &iqmPlan ) && iqmPlan.clearAuxiliary );
		CHECK( VK_TemporalMainActivationCommitIqmDraw( &owner, &iqmPlan ) );
		CHECK( VK_TemporalMainActivationPlanDraw( &owner,
			TEMPORAL_MOTION_WRITE_VALID, 11, 3, ordinary,
			&preflight, &ops, &plan ) && !plan.clearAuxiliary );
		CHECK( VK_TemporalMainActivationCommitDraw( &owner, &plan ) );
		recording = RecordingReceipt( &authority ); recording.prepared = 1;
		recording.appended = 1; recording.preserved = 0; recording.invalidated = 0;
		CHECK( AppendRecordingDraw( &recording, 11, 3,
			TEMPORAL_MOTION_WRITE_VALID, &pipelineReceipt ) );
		CHECK( VK_TemporalMainActivationFinishIqm( &owner, &recording,
			&iqm.sequence, &iqm.payloadOwner, &iqm.factory ) );
		CHECK( VK_TemporalMainActivationPeekPendingReceipt( &owner, &receipt ) );
		CHECK( receipt.taggedSequence.count == 2
			&& ( receipt.taggedSequence.lane0 != genericFirstTagged.lane0
				|| receipt.taggedSequence.lane1 != genericFirstTagged.lane1 ) );
		memset( &iqmFirstOracle, 0, sizeof( iqmFirstOracle ) );
		CHECK( OracleTaggedAppend( &iqmFirstOracle,
			VK_TEMPORAL_MAIN_EVENT_IQM, 0, iqm.sequence.entries[0].recordIndex,
			VK_TEMPORAL_SHADER_WRITE, OracleIqmDigest( &iqm.sequence,
				&iqm.sequence.entries[0], &iqm.factory ) ) );
		CHECK( OracleTaggedAppend( &iqmFirstOracle,
			VK_TEMPORAL_MAIN_EVENT_GENERIC, 0, 11, VK_TEMPORAL_SHADER_WRITE,
			OracleGenericDigest( TEMPORAL_MOTION_WRITE_VALID, 11, 3,
				&pipelineReceipt ) ) );
		CHECK( memcmp( &receipt.taggedSequence, &iqmFirstOracle,
			sizeof( iqmFirstOracle ) ) == 0 );
		CHECK( VK_TemporalMainActivationResolveSubmit( &owner, qtrue ) );

		// Independent valid source/factory/generic operand substitutions must
		// produce different online lanes from the fixed G-I-G oracle.
		{
			IqmFixture variant;
			IqmLiveContext variantLive;
			vkTemporalMainTaggedSequence_t variantTagged;
			temporalIqmDrawFacts_t changed;
			variant = ValidIqmFixture( &authority ); RepairIqmFixture( &variant );
			changed = variant.sequence.entries[0].facts;
			changed.sourceDrawSurfOrdinal = 17;
			CHECK( R_TemporalIqmSequenceBuild( &variant.sequence.authority,
				&changed, 1, &variant.sequence ) );
			variant.content.sequenceDigest = variant.sequence.orderedDigest;
			variantLive = LiveContextFor( &variant.sequence.entries[0] );
			CHECK( RunMixedGenericIqmGeneric( &authority, &variant, &variantLive,
				11, ordinary, &preflight, &ops, &pipelineReceipt, &variantTagged ) );
			CHECK( variantTagged.lane0 != genericFirstTagged.lane0
				|| variantTagged.lane1 != genericFirstTagged.lane1 );

			variant = ValidIqmFixture( &authority ); RepairIqmFixture( &variant );
			variant.factory.allocationGeneration++;
			variantLive = LiveContextFor( &variant.sequence.entries[0] );
			CHECK( RunMixedGenericIqmGeneric( &authority, &variant, &variantLive,
				11, ordinary, &preflight, &ops, &pipelineReceipt, &variantTagged ) );
			CHECK( variantTagged.lane0 != genericFirstTagged.lane0
				|| variantTagged.lane1 != genericFirstTagged.lane1 );

			variant = ValidIqmFixture( &authority ); RepairIqmFixture( &variant );
			variantLive = LiveContextFor( &variant.sequence.entries[0] );
			CHECK( RunMixedGenericIqmGeneric( &authority, &variant, &variantLive,
				13, ordinary, &preflight, &ops, &pipelineReceipt, &variantTagged ) );
			CHECK( variantTagged.lane0 != genericFirstTagged.lane0
				|| variantTagged.lane1 != genericFirstTagged.lane1 );
		}

		// A two-entry IQM sequence proves G-I-G-I success plus omission,
		// reorder and duplicate rejection against the same borrowed verifier.
		{
			IqmFixture pair = ValidIqmFixture( &authority );
			temporalIqmDrawFacts_t pairDraws[2];
			IqmLiveContext pairLive;
			RepairIqmFixture( &pair );
			pairDraws[0] = pair.sequence.entries[0].facts;
			pairDraws[1] = pairDraws[0]; pairDraws[1].ordinal = 1;
			pairDraws[1].sourceDrawSurfOrdinal = 9;
			pairDraws[1].surfaceIndex++;
			CHECK( R_TemporalIqmSequenceBuild( &pair.sequence.authority,
				pairDraws, 2, &pair.sequence ) );
			pair.content.sequenceDigest = pair.sequence.orderedDigest;
			pairLive = LiveContextFor( &pair.sequence.entries[0] );

			VK_TemporalMainActivationInit( &owner );
			CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
			CHECK( VK_TemporalMainActivationBindIqm( &owner, &pair.sequence,
				&pair.content, &pair.payloadOwner, &pair.factory, &pair.pass,
				&pairLive, &iqmRevalidateOps ) );
			CHECK( !VK_TemporalMainActivationPlanIqmDraw(
				&owner, &pairDraws[1], &iqmPlan ) && owner.poisoned );

			VK_TemporalMainActivationInit( &owner );
			CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
			CHECK( VK_TemporalMainActivationBindIqm( &owner, &pair.sequence,
				&pair.content, &pair.payloadOwner, &pair.factory, &pair.pass,
				&pairLive, &iqmRevalidateOps ) );
			CHECK( VK_TemporalMainActivationPlanIqmDraw(
				&owner, &pairDraws[0], &iqmPlan ) );
			CHECK( VK_TemporalMainActivationCommitIqmDraw( &owner, &iqmPlan ) );
			CHECK( !VK_TemporalMainActivationPlanIqmDraw(
				&owner, &pairDraws[0], &iqmPlan ) && owner.poisoned );

			VK_TemporalMainActivationInit( &owner );
			CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
			CHECK( VK_TemporalMainActivationBindIqm( &owner, &pair.sequence,
				&pair.content, &pair.payloadOwner, &pair.factory, &pair.pass,
				&pairLive, &iqmRevalidateOps ) );
			CHECK( VK_TemporalMainActivationPlanDraw( &owner,
				TEMPORAL_MOTION_WRITE_VALID, 11, 3, ordinary,
				&preflight, &ops, &plan ) );
			CHECK( VK_TemporalMainActivationCommitDraw( &owner, &plan ) );
			CHECK( VK_TemporalMainActivationPlanIqmDraw(
				&owner, &pairDraws[0], &iqmPlan ) );
			CHECK( VK_TemporalMainActivationCommitIqmDraw( &owner, &iqmPlan ) );
			CHECK( VK_TemporalMainActivationPlanDraw( &owner,
				TEMPORAL_MOTION_INVALIDATE_OPAQUE, 12, 3, ordinary,
				&preflight, &ops, &plan ) );
			CHECK( VK_TemporalMainActivationCommitDraw( &owner, &plan ) );
			recording = RecordingReceipt( &authority ); recording.prepared = 2;
			recording.appended = 2; recording.preserved = 0; recording.invalidated = 1;
			CHECK( AppendRecordingDraw( &recording, 11, 3,
				TEMPORAL_MOTION_WRITE_VALID, &pipelineReceipt ) );
			CHECK( AppendRecordingDraw( &recording, 12, 3,
				TEMPORAL_MOTION_INVALIDATE_OPAQUE, &pipelineReceipt ) );
			CHECK( !VK_TemporalMainActivationFinishIqm( &owner, &recording,
				&pair.sequence, &pair.payloadOwner, &pair.factory ) );

			VK_TemporalMainActivationInit( &owner );
			CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
			CHECK( VK_TemporalMainActivationBindIqm( &owner, &pair.sequence,
				&pair.content, &pair.payloadOwner, &pair.factory, &pair.pass,
				&pairLive, &iqmRevalidateOps ) );
			CHECK( VK_TemporalMainActivationPlanDraw( &owner,
				TEMPORAL_MOTION_WRITE_VALID, 11, 3, ordinary,
				&preflight, &ops, &plan ) );
			CHECK( VK_TemporalMainActivationCommitDraw( &owner, &plan ) );
			for ( uint32_t i = 0; i < 2; ++i ) {
				CHECK( VK_TemporalMainActivationPlanIqmDraw(
					&owner, &pairDraws[i], &iqmPlan ) );
				CHECK( VK_TemporalMainActivationCommitIqmDraw( &owner, &iqmPlan ) );
				if ( i == 0 ) {
					CHECK( VK_TemporalMainActivationPlanDraw( &owner,
						TEMPORAL_MOTION_INVALIDATE_OPAQUE, 12, 3, ordinary,
						&preflight, &ops, &plan ) );
					CHECK( VK_TemporalMainActivationCommitDraw( &owner, &plan ) );
				}
			}
			CHECK( VK_TemporalMainActivationFinishIqm( &owner, &recording,
				&pair.sequence, &pair.payloadOwner, &pair.factory ) );
			CHECK( VK_TemporalMainActivationPeekPendingReceipt( &owner, &receipt )
				&& receipt.taggedSequence.count == 4 );
		}

		// An observed draw-surf ordinal mismatch poisons before plan publication.
		VK_TemporalMainActivationInit( &owner );
		CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
		CHECK( VK_TemporalMainActivationBindIqm( &owner, &iqm.sequence,
			&iqm.content, &iqm.payloadOwner, &iqm.factory, &iqm.pass,
			&live, &iqmRevalidateOps ) );
		observed.sourceDrawSurfOrdinal++;
		memset( &iqmPlan, 0xA5, sizeof( iqmPlan ) ); iqmSentinel = iqmPlan;
		CHECK( !VK_TemporalMainActivationPlanIqmDraw(
			&owner, &observed, &iqmPlan ) && owner.poisoned );
		CHECK( memcmp( &iqmPlan, &iqmSentinel, sizeof( iqmPlan ) ) == 0 );
		observed = iqm.sequence.entries[0].facts;

		// Current geometry and bindless state are independently revalidated at
		// Bind and again immediately before every logical IQM plan.
		for ( mutation = 1; mutation <= 12; ++mutation ) {
			IqmLiveContext staleLive = live;
			staleLive.geometryMutation = mutation;
			VK_TemporalMainActivationInit( &owner );
			CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
			CHECK( !VK_TemporalMainActivationBindIqm( &owner, &iqm.sequence,
				&iqm.content, &iqm.payloadOwner, &iqm.factory, &iqm.pass,
				&staleLive, &iqmRevalidateOps ) && owner.poisoned );
		}
		for ( mutation = 1; mutation <= 21; ++mutation ) {
			IqmLiveContext staleLive = live;
			staleLive.bindlessMutation = mutation;
			VK_TemporalMainActivationInit( &owner );
			CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
			CHECK( !VK_TemporalMainActivationBindIqm( &owner, &iqm.sequence,
				&iqm.content, &iqm.payloadOwner, &iqm.factory, &iqm.pass,
				&staleLive, &iqmRevalidateOps ) && owner.poisoned );
		}
		for ( mutation = 1; mutation <= 12; ++mutation ) {
			IqmLiveContext staleLive = live;
			VK_TemporalMainActivationInit( &owner );
			CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
			CHECK( VK_TemporalMainActivationBindIqm( &owner, &iqm.sequence,
				&iqm.content, &iqm.payloadOwner, &iqm.factory, &iqm.pass,
				&staleLive, &iqmRevalidateOps ) );
			staleLive.geometryMutation = mutation;
			CHECK( !VK_TemporalMainActivationPlanIqmDraw(
				&owner, &observed, &iqmPlan ) && owner.poisoned );
		}
		for ( mutation = 1; mutation <= 21; ++mutation ) {
			IqmLiveContext staleLive = live;
			VK_TemporalMainActivationInit( &owner );
			CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
			CHECK( VK_TemporalMainActivationBindIqm( &owner, &iqm.sequence,
				&iqm.content, &iqm.payloadOwner, &iqm.factory, &iqm.pass,
				&staleLive, &iqmRevalidateOps ) );
			staleLive.bindlessMutation = mutation;
			CHECK( !VK_TemporalMainActivationPlanIqmDraw(
				&owner, &observed, &iqmPlan ) && owner.poisoned );
		}

		// Omission and duplicate observation fail closed. A successful commit is
		// the sole verifier advance; Peek alone never consumes the expected draw.
		VK_TemporalMainActivationInit( &owner );
		CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
		CHECK( VK_TemporalMainActivationBindIqm( &owner, &iqm.sequence,
			&iqm.content, &iqm.payloadOwner, &iqm.factory, &iqm.pass,
			&live, &iqmRevalidateOps ) );
		recording = RecordingReceipt( &authority ); recording.prepared = 0;
		recording.appended = 0; recording.preserved = 0; recording.invalidated = 0;
		CHECK( !VK_TemporalMainActivationFinishIqm( &owner, &recording,
			&iqm.sequence, &iqm.payloadOwner, &iqm.factory ) );
		VK_TemporalMainActivationInit( &owner );
		CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
		CHECK( VK_TemporalMainActivationBindIqm( &owner, &iqm.sequence,
			&iqm.content, &iqm.payloadOwner, &iqm.factory, &iqm.pass,
			&live, &iqmRevalidateOps ) );
		CHECK( VK_TemporalMainActivationPlanIqmDraw(
			&owner, &observed, &iqmPlan ) );
		CHECK( owner.iqmVerifier.cursor == 0 );
		CHECK( VK_TemporalMainActivationCommitIqmDraw( &owner, &iqmPlan ) );
		CHECK( owner.iqmVerifier.cursor == 1 );
		CHECK( !VK_TemporalMainActivationPlanIqmDraw(
			&owner, &observed, &iqmPlan ) && owner.poisoned );

		// Advancing the actual payload epoch invalidates the bound content before
		// Finish, even when bytes are identical.
		iqm = ValidIqmFixture( &authority ); RepairIqmFixture( &iqm );
		live = LiveContextFor( &iqm.sequence.entries[0] );
		observed = iqm.sequence.entries[0].facts;
		VK_TemporalMainActivationInit( &owner );
		CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
		CHECK( VK_TemporalMainActivationBindIqm( &owner, &iqm.sequence,
			&iqm.content, &iqm.payloadOwner, &iqm.factory, &iqm.pass,
			&live, &iqmRevalidateOps ) );
		CHECK( VK_TemporalMainActivationPlanIqmDraw(
			&owner, &observed, &iqmPlan ) );
		CHECK( VK_TemporalMainActivationCommitIqmDraw( &owner, &iqmPlan ) );
		CHECK( VK_TemporalIqmPayloadPrepareAfterFence( &iqm.payloadOwner,
			&iqm.payloadOwner.key, authority.frameIndex, qtrue, qfalse ) );
		recording = RecordingReceipt( &authority ); recording.prepared = 0;
		recording.appended = 0; recording.preserved = 0; recording.invalidated = 0;
		CHECK( !VK_TemporalMainActivationFinishIqm( &owner, &recording,
			&iqm.sequence, &iqm.payloadOwner, &iqm.factory ) );

		// Submit cancellation erases the candidate; the next Begin has no stale
		// IQM verifier or tagged residue.
		iqm = ValidIqmFixture( &authority ); RepairIqmFixture( &iqm );
		live = LiveContextFor( &iqm.sequence.entries[0] );
		observed = iqm.sequence.entries[0].facts;
		VK_TemporalMainActivationInit( &owner );
		CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
		CHECK( VK_TemporalMainActivationBindIqm( &owner, &iqm.sequence,
			&iqm.content, &iqm.payloadOwner, &iqm.factory, &iqm.pass,
			&live, &iqmRevalidateOps ) );
		CHECK( VK_TemporalMainActivationPlanIqmDraw(
			&owner, &observed, &iqmPlan ) );
		CHECK( VK_TemporalMainActivationCommitIqmDraw( &owner, &iqmPlan ) );
		recording = RecordingReceipt( &authority ); recording.prepared = 0;
		recording.appended = 0; recording.preserved = 0; recording.invalidated = 0;
		CHECK( VK_TemporalMainActivationFinishIqm( &owner, &recording,
			&iqm.sequence, &iqm.payloadOwner, &iqm.factory ) );
		CHECK( !VK_TemporalMainActivationResolveSubmit( &owner, qfalse ) );
		CHECK( VK_TemporalMainActivationBegin( &owner, &authority ) );
		CHECK( !owner.iqmVerifier.active && !owner.iqm.ready
			&& !owner.taggedSequence.count );
		VK_TemporalMainActivationPoison( &owner );
	}

	CHECK( sizeof( temporalIqmSequenceVerifier_t ) <= 32u
		&& sizeof( vkTemporalMainActivationReceipt_t ) <= 2048u
		&& sizeof( vkTemporalMainActivationOwner_t ) <= 8192u );

	printf( "vk_temporal_main_activation_test: PASS\n" );
	return 0;
}
