// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_graph_submit.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
#define ID(type,value) ((type *)(uintptr_t)(value))

typedef struct {
	char kind;uint32_t batch;ralQueueType_t queue;uint32_t waitCount;
	ralSemaphore_t *waits[3];uint64_t waitValues[3];
	ralSemaphore_t *signal;uint64_t signalValue;
} event_t;
typedef struct {
	event_t events[64];uint32_t eventCount;
	ralCommandBuffer_t *commands[32];ralCommandReceipt_t current[32];uint32_t commandCount;
	ralSemaphore_t *timelines[3];uint64_t timelineValues[3];
	uint32_t failEndBatch,failSubmitBatch,failCancelBatch;
	uint64_t submissionGeneration[3];
} capture_t;

static capture_t capture;

static int CommandIndex( const ralCommandBuffer_t *command ) {
	uint32_t i;
	for(i=0u;i<capture.commandCount;++i)
		if(capture.commands[i]==command)return(int)i;
	return -1;
}

static int TimelineIndex( const ralSemaphore_t *semaphore ) {
	uint32_t i;for(i=0u;i<3u;++i)if(capture.timelines[i]==semaphore)return(int)i;
	return -1;
}

static void Event( char kind, uint32_t batch, ralQueueType_t queue ) {
	if(capture.eventCount<64u)capture.events[capture.eventCount++]
		=(event_t){.kind=kind,.batch=batch,.queue=queue};
}

ralResult_t Ral_GetCommandBufferReceipt( const ralCommandBuffer_t *command,
		ralCommandReceipt_t *outReceipt ) {
	const int index=CommandIndex(command);
	if(index<0||!outReceipt||capture.current[index].state==RAL_COMMAND_IDLE)
		return ralErrorInvalidArgument;
	*outReceipt=capture.current[index];return ralSuccess;
}

uint64_t Ral_GetTimelineValue( ralSemaphore_t *semaphore ) {
	const int index=TimelineIndex(semaphore);
	return index<0?UINT64_MAX:capture.timelineValues[index];
}

ralResult_t Ral_EndCommandBufferExact( ralCommandBuffer_t *command,
		const ralCommandReceipt_t *recording, ralCommandReceipt_t *outExecutable ) {
	const int index=CommandIndex(command);Event('E',(uint32_t)index,recording->queue);
	if(index<0||(uint32_t)index==capture.failEndBatch
			||!Ral_CommandReceiptExact(&capture.current[index],recording))
		return ralErrorDeviceLost;
	capture.current[index].state=RAL_COMMAND_EXECUTABLE;
	*outExecutable=capture.current[index];return ralSuccess;
}

ralResult_t Ral_CancelCommandBuffer( ralCommandBuffer_t *command,
		const ralCommandReceipt_t *authority ) {
	const int index=CommandIndex(command);Event('C',(uint32_t)index,authority->queue);
	if(index<0||(uint32_t)index==capture.failCancelBatch
			||!Ral_CommandReceiptExact(&capture.current[index],authority))
		return ralErrorDeviceLost;
	capture.current[index].state=RAL_COMMAND_IDLE;return ralSuccess;
}

ralResult_t Ral_SubmitExact( ralBackend_t *backend, ralQueueType_t queue,
		const ralSubmitInfo_t *submit, const ralCommandReceipt_t *executable,
		ralSubmissionReceipt_t *outReceipt ) {
	const int index=CommandIndex(submit->commandBuffers[0]);
	event_t *event;
	Event('S',(uint32_t)index,queue);event=&capture.events[capture.eventCount-1u];
	event->waitCount=submit->numWaitSemaphores;
	if(event->waitCount>0u){memcpy(event->waits,submit->waitSemaphores,
		event->waitCount*sizeof(event->waits[0]));memcpy(event->waitValues,
		submit->waitValues,event->waitCount*sizeof(event->waitValues[0]));}
	if(submit->numSignalSemaphores==1u){event->signal=submit->signalSemaphores[0];
		event->signalValue=submit->signalValues[0];}
	if(index<0||(uint32_t)index==capture.failSubmitBatch
			||!Ral_CommandReceiptExact(&capture.current[index],executable))
		return ralErrorDeviceLost;
	memset(outReceipt,0,sizeof(*outReceipt));outReceipt->backendIdentity=backend;
	outReceipt->generation=++capture.submissionGeneration[(uint32_t)queue];
	outReceipt->queue=queue;outReceipt->commandCount=1u;
	outReceipt->commands[0]=*executable;outReceipt->commands[0].state=RAL_COMMAND_SUBMITTED;
	outReceipt->ready=qtrue;capture.current[index]=outReceipt->commands[0];
	return ralSuccess;
}

static qboolean DummyPreflight( void *context,
		const ralFrameGraphExecutionPass_t *pass,
		const ralFrameGraphExecutionPlan_t *plan,ralCommandBuffer_t *command ) {
	(void)context;(void)pass;(void)plan;(void)command;return qtrue;
}
static void DummyRecord( void *context,const ralFrameGraphExecutionPass_t *pass,
		const ralFrameGraphExecutionPlan_t *plan,ralCommandBuffer_t *command ) {
	(void)context;(void)pass;(void)plan;(void)command;
}

static void GraphFixture( ralBackendType_t backend,
		ralFrameGraphDescription_t *description ) {
	memset(description,0,sizeof(*description));description->schemaVersion=1u;
	description->generation=7u;description->backendType=backend;
	description->resourceCount=2u;
	description->resources[0]=(ralFrameGraphResource_t){1u,11u,101u,
		RAL_FRAME_GRAPH_RESOURCE_BUFFER,RAL_FRAME_GRAPH_RESOURCE_TRANSIENT,
		1u,1024u,256u,qfalse,{RAL_RESOURCE_USAGE_UNDEFINED,0u},RAL_QUEUE_COMPUTE};
	description->resources[1]=(ralFrameGraphResource_t){2u,12u,102u,
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
	description->transientPolicy.explicitAliasing=backend==RAL_BACKEND_WEBGPU?qfalse:qtrue;
}

static int Fixture( ralBackendType_t backend, qboolean timelineSupported,
		ralFrameGraphPlan_t *graph,ralFrameGraphExecutionPlan_t *execution,
		ralFrameGraphRecordingReceipt_t *recording,
		ralFrameGraphSubmitDescription_t *submit ) {
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
	memset(&capture,0,sizeof(capture));
	capture.failEndBatch=UINT32_MAX;capture.failSubmitBatch=UINT32_MAX;
	capture.failCancelBatch=UINT32_MAX;
	memset(recording,0,sizeof(*recording));
	recording->schemaVersion=1u;recording->backendIdentity=executionDescription.backendIdentity;
	recording->executionPlanIdentity=execution;
	recording->opsIdentity=ID(ralFrameGraphRecordOps_t,0x4000u);
	recording->passPreflightIdentity=DummyPreflight;recording->passRecordIdentity=DummyRecord;
	recording->executionGeneration=executionDescription.generation;
	recording->graphGeneration=graphDescription.generation;
	recording->recordingGeneration=27u;recording->passCount=execution->passCount;
	recording->submissionBatchCount=execution->submissionBatchCount;
	recording->transitionBatchCount=backend==RAL_BACKEND_WEBGPU?3u:2u;
	recording->ownershipReleaseCount=backend==RAL_BACKEND_WEBGPU?0u:3u;
	recording->ownershipAcquireCount=recording->ownershipReleaseCount;
	recording->recordingCommandCount=execution->submissionBatchCount;
	memset(submit,0,sizeof(*submit));submit->schemaVersion=1u;submit->generation=37u;
	submit->executionPlan=execution;submit->recordingReceipt=recording;
	submit->batchCommandCount=execution->submissionBatchCount;
	for(i=0u;i<submit->batchCommandCount;++i){
		ralCommandBuffer_t *command=ID(ralCommandBuffer_t,0x5000u+i*0x100u);
		ralCommandReceipt_t receipt={executionDescription.backendIdentity,command,100u+i,
			execution->submissionBatches[i].physicalQueue,RAL_COMMAND_RECORDING,qtrue};
		submit->batchCommands[i]=(ralFrameGraphBatchCommand_t){i,command,receipt};
		recording->recordingCommands[i]=receipt;capture.commands[i]=command;
		capture.current[i]=receipt;
	}
	capture.commandCount=submit->batchCommandCount;recording->ready=qtrue;
	submit->timelineSemaphoresSupported=timelineSupported;
	if(timelineSupported){for(i=0u;i<3u;++i){capture.timelines[i]
		=ID(ralSemaphore_t,0x8000u+i*0x100u);capture.timelineValues[i]
		=executionDescription.timelineBaseValues[i];submit->queueTimelines[i]
		=capture.timelines[i];}}
	submit->ops=RalFrameGraphSubmit_RalOps();
	return 0;
}

static int PreflightRejections( void ) {
	ralFrameGraphPlan_t graph;ralFrameGraphExecutionPlan_t execution;
	ralFrameGraphRecordingReceipt_t recording;
	ralFrameGraphSubmitDescription_t description,bad;
	ralFrameGraphSubmissionReceipt_t complete,completeBefore;
	ralFrameGraphSubmissionFailureReceipt_t failure,failureBefore;
	ralFrameGraphSubmitOps_t badOps;
	CHECK(Fixture(RAL_BACKEND_VULKAN,qtrue,&graph,&execution,&recording,&description)==0);
#define REJECT(statement) do{bad=description;statement;memset(&complete,0x55,sizeof(complete));completeBefore=complete;memset(&failure,0x66,sizeof(failure));failureBefore=failure;capture.eventCount=0u;CHECK(RalFrameGraphSubmit_Submit(&bad,&complete,&failure)==RAL_FRAME_GRAPH_SUBMIT_REJECTED);CHECK(capture.eventCount==0u&&memcmp(&complete,&completeBefore,sizeof(complete))==0&&memcmp(&failure,&failureBefore,sizeof(failure))==0);}while(0)
	REJECT(bad.schemaVersion++);
	REJECT(bad.generation=UINT64_MAX);
	REJECT(bad.executionPlan=NULL);
	REJECT(bad.recordingReceipt=NULL);
	REJECT(bad.batchCommandCount--);
	REJECT(bad.batchCommands[0].recordingReceipt.state=RAL_COMMAND_EXECUTABLE);
	REJECT(bad.batchCommands[1].commandBuffer=bad.batchCommands[0].commandBuffer;
		bad.batchCommands[1].recordingReceipt.commandIdentity=bad.batchCommands[0].commandBuffer);
	REJECT(bad.batchCommands[3].submissionBatchIndex=9u);
	REJECT(bad.timelineSemaphoresSupported=(qboolean)2);
	REJECT(bad.queueTimelines[0]=NULL);
	REJECT(bad.queueTimelines[1]=bad.queueTimelines[0]);
	REJECT(bad.ops=NULL);
	badOps=*description.ops;badOps.submitExact=NULL;REJECT(bad.ops=&badOps);
	capture.current[0].generation++;REJECT((void)0);
	capture.current[0]=description.batchCommands[0].recordingReceipt;
	capture.timelineValues[0]++;REJECT((void)0);
#undef REJECT
	return 0;
}

static int TimelineSuccess( void ) {
	ralFrameGraphPlan_t graph;ralFrameGraphExecutionPlan_t execution;
	ralFrameGraphRecordingReceipt_t recording;
	ralFrameGraphSubmitDescription_t description;
	ralFrameGraphSubmissionReceipt_t receipt,badReceipt;
	ralFrameGraphSubmissionFailureReceipt_t failure,failureBefore;
	CHECK(Fixture(RAL_BACKEND_VULKAN,qtrue,&graph,&execution,&recording,&description)==0);
	memset(&failure,0x77,sizeof(failure));failureBefore=failure;
	CHECK(RalFrameGraphSubmit_Submit(&description,&receipt,&failure)
		==RAL_FRAME_GRAPH_SUBMIT_COMPLETE);
	CHECK(memcmp(&failure,&failureBefore,sizeof(failure))==0);
	CHECK(capture.eventCount==6u&&capture.events[0].kind=='E'
		&&capture.events[1].kind=='E'&&capture.events[2].kind=='E'
		&&capture.events[3].kind=='S'&&capture.events[4].kind=='S'
		&&capture.events[5].kind=='S');
	CHECK(capture.events[3].queue==RAL_QUEUE_COMPUTE
		&&capture.events[3].waitCount==0u
		&&capture.events[3].signal==capture.timelines[RAL_QUEUE_COMPUTE]
		&&capture.events[3].signalValue==7u);
	CHECK(capture.events[4].queue==RAL_QUEUE_GRAPHICS
		&&capture.events[4].waitCount==1u
		&&capture.events[4].waits[0]==capture.timelines[RAL_QUEUE_COMPUTE]
		&&capture.events[4].waitValues[0]==7u
		&&capture.events[4].signalValue==6u);
	CHECK(capture.events[5].queue==RAL_QUEUE_TRANSFER
		&&capture.events[5].waitCount==2u
		&&capture.events[5].waits[0]==capture.timelines[RAL_QUEUE_GRAPHICS]
		&&capture.events[5].waitValues[0]==6u
		&&capture.events[5].waits[1]==capture.timelines[RAL_QUEUE_COMPUTE]
		&&capture.events[5].waitValues[1]==7u
		&&capture.events[5].signalValue==8u);
	CHECK(receipt.submissionCount==3u&&receipt.timelineFinalValues[0]==6u
		&&receipt.timelineFinalValues[1]==7u&&receipt.timelineFinalValues[2]==8u);
	CHECK(RalFrameGraphSubmit_ReceiptExact(&receipt,&receipt));
#define MUTATE(statement) do{badReceipt=receipt;statement;CHECK(!RalFrameGraphSubmit_ReceiptExact(&badReceipt,&receipt));}while(0)
	MUTATE(badReceipt.executionGeneration++);
	MUTATE(badReceipt.recordingGeneration++);
	MUTATE(badReceipt.queueTimelines[0]=NULL);
	MUTATE(badReceipt.timelineFinalValues[0]++);
	MUTATE(badReceipt.submissions[0].generation++);
	MUTATE(badReceipt.ready=qfalse);
#undef MUTATE
	return 0;
}

static int FailuresAreHonest( void ) {
	ralFrameGraphPlan_t graph;ralFrameGraphExecutionPlan_t execution;
	ralFrameGraphRecordingReceipt_t recording;
	ralFrameGraphSubmitDescription_t description;
	ralFrameGraphSubmissionReceipt_t complete,completeBefore;
	ralFrameGraphSubmissionFailureReceipt_t failure,badFailure;
	CHECK(Fixture(RAL_BACKEND_VULKAN,qtrue,&graph,&execution,&recording,&description)==0);
	capture.failEndBatch=1u;memset(&complete,0x55,sizeof(complete));completeBefore=complete;
	CHECK(RalFrameGraphSubmit_Submit(&description,&complete,&failure)
		==RAL_FRAME_GRAPH_SUBMIT_FAILED);
	CHECK(memcmp(&complete,&completeBefore,sizeof(complete))==0
		&&failure.stage==RAL_FRAME_GRAPH_SUBMIT_FAILURE_END
		&&failure.failedBatchIndex==1u&&failure.submittedBatchCount==0u
		&&failure.cancellationAttemptCount==3u&&failure.cancellationFailureCount==0u);
	CHECK(RalFrameGraphSubmit_FailureReceiptExact(&failure,&failure));
	badFailure=failure;badFailure.failedBatchIndex++;
	CHECK(!RalFrameGraphSubmit_FailureReceiptExact(&badFailure,&failure));
	CHECK(Fixture(RAL_BACKEND_VULKAN,qtrue,&graph,&execution,&recording,&description)==0);
	capture.failSubmitBatch=1u;capture.failCancelBatch=2u;
	CHECK(RalFrameGraphSubmit_Submit(&description,&complete,&failure)
		==RAL_FRAME_GRAPH_SUBMIT_FAILED);
	CHECK(failure.stage==RAL_FRAME_GRAPH_SUBMIT_FAILURE_QUEUE
		&&failure.failedBatchIndex==1u&&failure.submittedBatchCount==1u
		&&failure.submittedPrefix[0].queue==RAL_QUEUE_COMPUTE
		&&failure.cancellationAttemptCount==2u&&failure.cancellationFailureCount==1u);
	CHECK(RalFrameGraphSubmit_FailureReceiptExact(&failure,&failure));
	return 0;
}

static int TimelineFreeSharedQueue( void ) {
	ralFrameGraphPlan_t graph;ralFrameGraphExecutionPlan_t execution;
	ralFrameGraphRecordingReceipt_t recording;
	ralFrameGraphSubmitDescription_t description,bad;
	ralFrameGraphSubmissionReceipt_t complete;
	ralFrameGraphSubmissionFailureReceipt_t failure;
	CHECK(Fixture(RAL_BACKEND_WEBGPU,qfalse,&graph,&execution,&recording,&description)==0);
	CHECK(description.batchCommandCount==1u);
	CHECK(RalFrameGraphSubmit_Submit(&description,&complete,&failure)
		==RAL_FRAME_GRAPH_SUBMIT_COMPLETE);
	CHECK(capture.eventCount==2u&&capture.events[0].kind=='E'
		&&capture.events[1].kind=='S'&&capture.events[1].waitCount==0u
		&&capture.events[1].signal==NULL&&complete.timelineSemaphoresSupported==qfalse);
	bad=description;bad.timelineSemaphoresSupported=qtrue;
	CHECK(RalFrameGraphSubmit_Submit(&bad,&complete,&failure)
		==RAL_FRAME_GRAPH_SUBMIT_REJECTED);
	return 0;
}

int main( void ) {
	CHECK(PreflightRejections()==0);
	CHECK(TimelineSuccess()==0);
	CHECK(FailuresAreHonest()==0);
	CHECK(TimelineFreeSharedQueue()==0);
	puts("ral frame graph submit: PASS");
	return 0;
}
