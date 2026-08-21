// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_graph_record.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
#define ID(type,value) ((type *)(uintptr_t)(value))

typedef struct { char kind; uint32_t batch; uint32_t count; } event_t;
typedef struct {
	event_t events[64];uint32_t eventCount;
	uint32_t operationAttempt,failOperationAttempt,cancelCount,preflightCount;
	qboolean corruptReleaseReceipt;
	ralCommandBuffer_t *commands[RAL_FRAME_GRAPH_MAX_PASSES];
	ralCommandReceipt_t current[RAL_FRAME_GRAPH_MAX_PASSES];
	uint32_t commandCount;
} capture_t;

static capture_t capture;

static int CommandIndex( ralCommandBuffer_t *command ) {
	uint32_t i;
	for(i=0u;i<capture.commandCount;++i)if(capture.commands[i]==command)return(int)i;
	return -1;
}

static void Event( char kind, ralCommandBuffer_t *command, uint32_t count ) {
	const int index=CommandIndex(command);
	if(capture.eventCount<64u)capture.events[capture.eventCount++]
		=(event_t){kind,index<0?UINT32_MAX:(uint32_t)index,count};
}

static qboolean FailOperation( char kind, ralCommandBuffer_t *command,
		uint32_t count ) {
	capture.operationAttempt++;Event(kind,command,count);
	return capture.failOperationAttempt!=0u
		&&capture.operationAttempt==capture.failOperationAttempt?qtrue:qfalse;
}

ralResult_t Ral_GetCommandBufferReceipt( const ralCommandBuffer_t *command,
		ralCommandReceipt_t *outReceipt ) {
	const int index=CommandIndex((ralCommandBuffer_t *)command);
	if(index<0||!outReceipt)return ralErrorInvalidArgument;
	*outReceipt=capture.current[index];return ralSuccess;
}

ralResult_t Ral_CmdTransitionResources( ralCommandBuffer_t *command,
		const ralResourceTransitionBatch_t *batch ) {
	if(!batch||(batch->bufferTransitionCount==0u&&batch->textureTransitionCount==0u))
		return ralErrorInvalidArgument;
	return FailOperation('T',command,batch->bufferTransitionCount
		+batch->textureTransitionCount)?ralErrorDeviceLost:ralSuccess;
}

static void TransferReceipt( const void *identity,
		ralQueueTransferResourceType_t type, const ralResourceState_t *before,
		const ralResourceState_t *after, ralQueueType_t source,
		ralQueueType_t destination, ralQueueTransferReceipt_t *out ) {
	static uint64_t generation=0u;
	memset(out,0,sizeof(*out));out->resourceIdentity=identity;
	out->generation=++generation;out->resourceType=type;out->before=*before;
	out->after=*after;out->sourceQueue=source;out->destinationQueue=destination;
	out->ready=qtrue;if(capture.corruptReleaseReceipt)out->generation=0u;
}

ralResult_t Ral_CmdReleaseBufferOwnership( ralCommandBuffer_t *command,
		const ralBufferTransition_t *transition,
		ralQueueTransferReceipt_t *outReceipt ) {
	if(FailOperation('R',command,1u))return ralErrorDeviceLost;
	TransferReceipt(transition->buffer,RAL_QUEUE_TRANSFER_RESOURCE_BUFFER,
		&transition->before,&transition->after,transition->sourceQueue,
		transition->destinationQueue,outReceipt);return ralSuccess;
}

ralResult_t Ral_CmdAcquireBufferOwnership( ralCommandBuffer_t *command,
		const ralBufferTransition_t *transition,
		const ralQueueTransferReceipt_t *receipt ) {
	if(!receipt||receipt->resourceIdentity!=transition->buffer)return ralErrorInvalidArgument;
	return FailOperation('A',command,1u)?ralErrorDeviceLost:ralSuccess;
}

ralResult_t Ral_CancelBufferOwnershipTransfer( ralBuffer_t *buffer,
		const ralQueueTransferReceipt_t *receipt ) {
	(void)buffer;(void)receipt;capture.cancelCount++;Event('C',NULL,1u);
	return ralSuccess;
}

ralResult_t Ral_CmdReleaseTextureOwnership( ralCommandBuffer_t *command,
		const ralTextureTransition_t *transition,
		ralQueueTransferReceipt_t *outReceipt ) {
	if(FailOperation('r',command,1u))return ralErrorDeviceLost;
	TransferReceipt(transition->texture,RAL_QUEUE_TRANSFER_RESOURCE_TEXTURE,
		&transition->before,&transition->after,transition->sourceQueue,
		transition->destinationQueue,outReceipt);return ralSuccess;
}

ralResult_t Ral_CmdAcquireTextureOwnership( ralCommandBuffer_t *command,
		const ralTextureTransition_t *transition,
		const ralQueueTransferReceipt_t *receipt ) {
	if(!receipt||receipt->resourceIdentity!=transition->texture)return ralErrorInvalidArgument;
	return FailOperation('a',command,1u)?ralErrorDeviceLost:ralSuccess;
}

ralResult_t Ral_CancelTextureOwnershipTransfer( ralTexture_t *texture,
		const ralQueueTransferReceipt_t *receipt ) {
	(void)texture;(void)receipt;capture.cancelCount++;Event('c',NULL,1u);
	return ralSuccess;
}

static qboolean PassPreflight( void *context,
		const ralFrameGraphExecutionPass_t *pass,
		const ralFrameGraphExecutionPlan_t *plan, ralCommandBuffer_t *command ) {
	const uint32_t failPass=*(const uint32_t *)context;
	(void)plan;(void)command;capture.preflightCount++;
	return pass->passId==failPass?qfalse:qtrue;
}

static void PassRecord( void *context, const ralFrameGraphExecutionPass_t *pass,
		const ralFrameGraphExecutionPlan_t *plan, ralCommandBuffer_t *command ) {
	(void)context;(void)plan;Event('P',command,pass->passId);
}

static void GraphFixture( ralBackendType_t backend,
		ralFrameGraphDescription_t *description ) {
	memset(description,0,sizeof(*description));description->schemaVersion=1u;
	description->generation=7u;description->backendType=backend;
	description->resourceCount=2u;
	description->resources[0]=(ralFrameGraphResource_t){1u,11u,(uintptr_t)101u,
		RAL_FRAME_GRAPH_RESOURCE_BUFFER,RAL_FRAME_GRAPH_RESOURCE_TRANSIENT,
		1u,1024u,256u,qfalse,{RAL_RESOURCE_USAGE_UNDEFINED,0u},RAL_QUEUE_COMPUTE};
	description->resources[1]=(ralFrameGraphResource_t){2u,12u,(uintptr_t)102u,
		RAL_FRAME_GRAPH_RESOURCE_TEXTURE,RAL_FRAME_GRAPH_RESOURCE_TRANSIENT,
		2u,2048u,256u,qfalse,{RAL_RESOURCE_USAGE_UNDEFINED,0u},RAL_QUEUE_GRAPHICS};
	description->passCount=3u;
	description->passes[0]=(ralFrameGraphPass_t){10u,RAL_QUEUE_COMPUTE};
	description->passes[1]=(ralFrameGraphPass_t){20u,RAL_QUEUE_GRAPHICS};
	description->passes[2]=(ralFrameGraphPass_t){30u,RAL_QUEUE_TRANSFER};
	description->useCount=5u;
	description->uses[0]=(ralFrameGraphUse_t){10u,1u,RAL_FRAME_GRAPH_ACCESS_WRITE,
		{RAL_RESOURCE_USAGE_STORAGE_WRITE,RAL_STAGE_COMPUTE}};
	description->uses[1]=(ralFrameGraphUse_t){20u,1u,RAL_FRAME_GRAPH_ACCESS_READ,
		{RAL_RESOURCE_USAGE_VERTEX_BUFFER,0u}};
	description->uses[2]=(ralFrameGraphUse_t){20u,2u,RAL_FRAME_GRAPH_ACCESS_WRITE,
		{RAL_RESOURCE_USAGE_COLOR_ATTACHMENT,0u}};
	description->uses[3]=(ralFrameGraphUse_t){30u,1u,RAL_FRAME_GRAPH_ACCESS_WRITE,
		{RAL_RESOURCE_USAGE_COPY_DESTINATION,0u}};
	description->uses[4]=(ralFrameGraphUse_t){30u,2u,RAL_FRAME_GRAPH_ACCESS_READ,
		{RAL_RESOURCE_USAGE_COPY_SOURCE,0u}};
	description->transientPolicy.backendType=backend;
	description->transientPolicy.generation=description->generation;
	description->transientPolicy.budgetBytes=8192u;
	description->transientPolicy.explicitAliasing
		=backend==RAL_BACKEND_WEBGPU?qfalse:qtrue;
}

static int Fixture( ralBackendType_t backend, ralFrameGraphPlan_t *graph,
		ralFrameGraphExecutionPlan_t *execution,
		ralFrameGraphRecordDescription_t *record, uint32_t *failPass ) {
	ralFrameGraphDescription_t graphDescription;
	ralFrameGraphExecutionDescription_t executionDescription;
	uint32_t i;
	GraphFixture(backend,&graphDescription);CHECK(RalFrameGraph_Compile(&graphDescription,graph));
	memset(&executionDescription,0,sizeof(executionDescription));
	executionDescription.schemaVersion=1u;executionDescription.generation=17u;
	executionDescription.backendIdentity=ID(ralBackend_t,0x1000u);
	executionDescription.graphPlan=graph;
	executionDescription.physicalQueueForLogical[0]=RAL_QUEUE_GRAPHICS;
	executionDescription.physicalQueueForLogical[1]
		=backend==RAL_BACKEND_WEBGPU?RAL_QUEUE_GRAPHICS:RAL_QUEUE_COMPUTE;
	executionDescription.physicalQueueForLogical[2]
		=backend==RAL_BACKEND_WEBGPU?RAL_QUEUE_GRAPHICS:RAL_QUEUE_TRANSFER;
	executionDescription.timelineBaseValues[0]=5u;
	executionDescription.timelineBaseValues[1]=6u;
	executionDescription.timelineBaseValues[2]=7u;
	executionDescription.bindingCount=2u;
	executionDescription.bindings[0]=(ralFrameGraphResourceBinding_t){1u,101u,11u,
		RAL_FRAME_GRAPH_RESOURCE_BUFFER,ID(ralBuffer_t,0x2000u),NULL,1024u,0u,0u,0u};
	executionDescription.bindings[1]=(ralFrameGraphResourceBinding_t){2u,102u,12u,
		RAL_FRAME_GRAPH_RESOURCE_TEXTURE,NULL,ID(ralTexture_t,0x3000u),0u,
		RAL_TEXTURE_ASPECT_COLOR,1u,1u};
	CHECK(RalFrameGraphExecution_Compile(&executionDescription,execution));
	memset(&capture,0,sizeof(capture));memset(record,0,sizeof(*record));
	record->schemaVersion=1u;record->generation=27u;record->executionPlan=execution;
	record->batchCommandCount=execution->submissionBatchCount;
	for(i=0u;i<record->batchCommandCount;++i){
		ralCommandBuffer_t *command=ID(ralCommandBuffer_t,0x5000u+i*0x100u);
		ralCommandReceipt_t receipt={executionDescription.backendIdentity,command,
			100u+i,execution->submissionBatches[i].physicalQueue,
			RAL_COMMAND_RECORDING,qtrue};
		record->batchCommands[i]=(ralFrameGraphBatchCommand_t){i,command,receipt};
		capture.commands[i]=command;capture.current[i]=receipt;
	}
	capture.commandCount=record->batchCommandCount;
	record->ops=RalFrameGraphRecord_RalOps();
	record->passCallbacks=(ralFrameGraphPassCallbacks_t){PassPreflight,PassRecord,failPass};
	return 0;
}

static int PreflightRejections( void ) {
	ralFrameGraphPlan_t graph;ralFrameGraphExecutionPlan_t execution;
	ralFrameGraphRecordDescription_t description,bad;
	ralFrameGraphRecordingReceipt_t output,before;
	ralFrameGraphRecordOps_t badOps;uint32_t failPass=0u;
	CHECK(Fixture(RAL_BACKEND_VULKAN,&graph,&execution,&description,&failPass)==0);
#define REJECT(statement) do{bad=description;statement;memset(&output,0x5a,sizeof(output));before=output;memset(capture.events,0,sizeof(capture.events));capture.eventCount=0u;capture.operationAttempt=0u;capture.preflightCount=0u;CHECK(!RalFrameGraphRecord_Record(&bad,&output));CHECK(capture.eventCount==0u&&memcmp(&output,&before,sizeof(output))==0);}while(0)
	REJECT(bad.schemaVersion++);
	REJECT(bad.generation=UINT64_MAX);
	REJECT(bad.executionPlan=NULL);
	REJECT(bad.batchCommandCount--);
	REJECT(bad.batchCommands[0].submissionBatchIndex=1u);
	REJECT(bad.batchCommands[1].commandBuffer=bad.batchCommands[0].commandBuffer;
		bad.batchCommands[1].recordingReceipt.commandIdentity=bad.batchCommands[0].commandBuffer);
	REJECT(bad.batchCommands[0].recordingReceipt.state=RAL_COMMAND_EXECUTABLE);
	REJECT(bad.batchCommands[0].recordingReceipt.queue=RAL_QUEUE_GRAPHICS);
	REJECT(bad.batchCommands[0].recordingReceipt.backendIdentity=ID(ralBackend_t,9u));
	REJECT(bad.batchCommands[3].submissionBatchIndex=9u);
	REJECT(bad.ops=NULL);
	badOps=*description.ops;badOps.transitionResources=NULL;
	REJECT(bad.ops=&badOps);
	REJECT(bad.passCallbacks.preflight=NULL);
	REJECT(bad.passCallbacks.record=NULL);
	capture.current[0].generation++;
	REJECT((void)0);
	capture.current[0]=description.batchCommands[0].recordingReceipt;
	failPass=20u;
	REJECT((void)0);
#undef REJECT
	return 0;
}

static int DedicatedSuccessAndFailure( void ) {
	static const char expectedKinds[]={'T','P','R','A','T','P','R','r','A','a','P'};
	ralFrameGraphPlan_t graph;ralFrameGraphExecutionPlan_t execution,badExecution;
	ralFrameGraphRecordDescription_t description;
	ralFrameGraphRecordingReceipt_t receipt,badReceipt,sentinel,before;
	uint32_t failPass=0u,i;
	CHECK(Fixture(RAL_BACKEND_VULKAN,&graph,&execution,&description,&failPass)==0);
	CHECK(RalFrameGraphRecord_Record(&description,&receipt));
	CHECK(capture.preflightCount==3u&&capture.eventCount==sizeof(expectedKinds));
	for(i=0u;i<sizeof(expectedKinds);++i)CHECK(capture.events[i].kind==expectedKinds[i]);
	CHECK(receipt.passCount==3u&&receipt.submissionBatchCount==3u
		&&receipt.transitionBatchCount==2u&&receipt.ownershipReleaseCount==3u
		&&receipt.ownershipAcquireCount==3u&&receipt.recordingCommandCount==3u);
	CHECK(RalFrameGraphRecord_ReceiptExact(&receipt,&receipt));
#define MUTATE_RECEIPT(statement) do{badReceipt=receipt;statement;CHECK(!RalFrameGraphRecord_ReceiptExact(&badReceipt,&receipt));}while(0)
	MUTATE_RECEIPT(badReceipt.backendIdentity=ID(ralBackend_t,9u));
	MUTATE_RECEIPT(badReceipt.opsIdentity=NULL);
	MUTATE_RECEIPT(badReceipt.passRecordIdentity=NULL);
	MUTATE_RECEIPT(badReceipt.passContextIdentity=ID(void,9u));
	MUTATE_RECEIPT(badReceipt.executionGeneration++);
	MUTATE_RECEIPT(badReceipt.graphGeneration++);
	MUTATE_RECEIPT(badReceipt.recordingGeneration++);
	MUTATE_RECEIPT(badReceipt.transitionBatchCount++);
	MUTATE_RECEIPT(badReceipt.ownershipAcquireCount--);
	MUTATE_RECEIPT(badReceipt.recordingCommands[0].generation++);
	MUTATE_RECEIPT(badReceipt.ready=qfalse);
#undef MUTATE_RECEIPT
	badExecution=execution;badExecution.passes[0].passId++;
	description.executionPlan=&badExecution;
	memset(&sentinel,0x77,sizeof(sentinel));before=sentinel;
	CHECK(!RalFrameGraphRecord_Record(&description,&sentinel));
	CHECK(memcmp(&sentinel,&before,sizeof(sentinel))==0);
	CHECK(Fixture(RAL_BACKEND_VULKAN,&graph,&execution,&description,&failPass)==0);
	capture.failOperationAttempt=3u;
	memset(&sentinel,0x66,sizeof(sentinel));before=sentinel;
	CHECK(!RalFrameGraphRecord_Record(&description,&sentinel));
	CHECK(capture.cancelCount==1u&&capture.events[capture.eventCount-1u].kind=='C'
		&&memcmp(&sentinel,&before,sizeof(sentinel))==0);
	CHECK(Fixture(RAL_BACKEND_VULKAN,&graph,&execution,&description,&failPass)==0);
	capture.corruptReleaseReceipt=qtrue;
	CHECK(!RalFrameGraphRecord_Record(&description,&sentinel));
	CHECK(capture.cancelCount==1u);
	return 0;
}

static int SharedQueueSuccess( void ) {
	static const char expectedKinds[]={'T','P','T','P','T','P'};
	ralFrameGraphPlan_t graph;ralFrameGraphExecutionPlan_t execution;
	ralFrameGraphRecordDescription_t description;
	ralFrameGraphRecordingReceipt_t receipt;uint32_t failPass=0u,i;
	CHECK(Fixture(RAL_BACKEND_WEBGPU,&graph,&execution,&description,&failPass)==0);
	CHECK(description.batchCommandCount==1u);
	CHECK(RalFrameGraphRecord_Record(&description,&receipt));
	CHECK(capture.eventCount==sizeof(expectedKinds)&&capture.cancelCount==0u);
	for(i=0u;i<sizeof(expectedKinds);++i)CHECK(capture.events[i].kind==expectedKinds[i]);
	CHECK(receipt.transitionBatchCount==3u&&receipt.ownershipReleaseCount==0u
		&&receipt.ownershipAcquireCount==0u&&receipt.recordingCommandCount==1u);
	return 0;
}

int main( void ) {
	CHECK(PreflightRejections()==0);
	CHECK(DedicatedSuccessAndFailure()==0);
	CHECK(SharedQueueSuccess()==0);
	puts("ral frame graph record: PASS");
	return 0;
}
