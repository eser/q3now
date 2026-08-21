// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_graph_submit.h"

#include <limits.h>
#include <string.h>

static qboolean BoolValid( qboolean value ) {
	return value == qfalse || value == qtrue;
}

static qboolean BatchCommandEmpty( const ralFrameGraphBatchCommand_t *command ) {
	static const ralFrameGraphBatchCommand_t empty;
	return memcmp(command,&empty,sizeof(empty)) == 0 ? qtrue : qfalse;
}

static qboolean OpsValid( const ralFrameGraphSubmitOps_t *ops ) {
	return ops && ops->getCommandReceipt && ops->getTimelineValue
		&& ops->endCommand && ops->cancelCommand && ops->submitExact;
}

static uint32_t QueueBit( ralQueueType_t queue ) {
	return 1u << (uint32_t)queue;
}

static qboolean DescriptionShapeValid(
		const ralFrameGraphSubmitDescription_t *description ) {
	const ralFrameGraphExecutionPlan_t *plan;
	uint32_t usedQueues=0u,i,j;
	if ( !description
			|| description->schemaVersion != RAL_FRAME_GRAPH_SUBMIT_SCHEMA_VERSION
			|| description->generation == 0u || description->generation == UINT64_MAX
			|| !description->executionPlan || !description->recordingReceipt
			|| !RalFrameGraphExecution_PlanExact(description->executionPlan,
				description->executionPlan)
			|| !RalFrameGraphRecord_ReceiptExact(description->recordingReceipt,
				description->recordingReceipt)
			|| !BoolValid(description->timelineSemaphoresSupported)
			|| !OpsValid(description->ops) ) return qfalse;
	plan=description->executionPlan;
	if ( description->recordingReceipt->executionPlanIdentity != plan
			|| description->recordingReceipt->backendIdentity
				!= plan->description.backendIdentity
			|| description->recordingReceipt->executionGeneration
				!= plan->description.generation
			|| description->recordingReceipt->graphGeneration
				!= plan->description.graphPlan->description.generation
			|| description->batchCommandCount != plan->submissionBatchCount
			|| description->batchCommandCount
				!= description->recordingReceipt->recordingCommandCount ) return qfalse;
	for ( i=0u; i<description->batchCommandCount; ++i ) {
		const ralFrameGraphBatchCommand_t *command=&description->batchCommands[i];
		usedQueues|=QueueBit(plan->submissionBatches[i].physicalQueue);
		if ( command->submissionBatchIndex != i || !command->commandBuffer
				|| !Ral_CommandReceiptExact(&command->recordingReceipt,
					&description->recordingReceipt->recordingCommands[i])
				|| command->recordingReceipt.commandIdentity != command->commandBuffer
				|| command->recordingReceipt.queue
					!= plan->submissionBatches[i].physicalQueue
				|| command->recordingReceipt.state != RAL_COMMAND_RECORDING ) return qfalse;
		for ( j=0u; j<i; ++j )
			if ( command->commandBuffer == description->batchCommands[j].commandBuffer )
				return qfalse;
	}
	for ( i=description->batchCommandCount; i<RAL_FRAME_GRAPH_MAX_PASSES; ++i )
		if ( !BatchCommandEmpty(&description->batchCommands[i]) ) return qfalse;
	if ( description->timelineSemaphoresSupported == qtrue ) {
		for ( i=0u; i<RAL_FRAME_GRAPH_PHYSICAL_QUEUE_COUNT; ++i ) {
			const qboolean used=(usedQueues&(1u<<i))!=0u?qtrue:qfalse;
			if ( used != (description->queueTimelines[i]!=NULL?qtrue:qfalse) ) return qfalse;
			if ( !used ) continue;
			for ( j=0u; j<i; ++j )
				if ( description->queueTimelines[i]
						== description->queueTimelines[j] ) return qfalse;
		}
	} else {
		if ( description->batchCommandCount != 1u
				|| plan->submissionBatches[0].waitCount != 0u ) return qfalse;
		for ( i=0u; i<RAL_FRAME_GRAPH_PHYSICAL_QUEUE_COUNT; ++i )
			if ( description->queueTimelines[i] ) return qfalse;
	}
	return qtrue;
}

static qboolean PreflightCurrent(
		const ralFrameGraphSubmitDescription_t *description,
		const ralFrameGraphSubmitOps_t *ops ) {
	const ralFrameGraphExecutionPlan_t *plan=description->executionPlan;
	uint32_t usedQueues=0u,i;
	for ( i=0u; i<description->batchCommandCount; ++i ) {
		ralCommandReceipt_t current;
		memset(&current,0,sizeof(current));
		if ( !ops->getCommandReceipt(description->opsContext,
				description->batchCommands[i].commandBuffer,&current)
				|| !Ral_CommandReceiptExact(&current,
					&description->batchCommands[i].recordingReceipt) ) return qfalse;
		usedQueues|=QueueBit(plan->submissionBatches[i].physicalQueue);
	}
	if ( description->timelineSemaphoresSupported == qtrue )
		for ( i=0u; i<RAL_FRAME_GRAPH_PHYSICAL_QUEUE_COUNT; ++i )
			if ( (usedQueues&(1u<<i))!=0u
					&& ops->getTimelineValue(description->opsContext,
						description->queueTimelines[i])
						!= plan->description.timelineBaseValues[i] ) return qfalse;
	return qtrue;
}

static qboolean ExecutableMatchesRecording( const ralCommandReceipt_t *executable,
		const ralCommandReceipt_t *recording ) {
	return Ral_CommandReceiptValid(executable)
		&& executable->backendIdentity == recording->backendIdentity
		&& executable->commandIdentity == recording->commandIdentity
		&& executable->generation == recording->generation
		&& executable->queue == recording->queue
		&& executable->state == RAL_COMMAND_EXECUTABLE;
}

static qboolean SubmissionMatchesExecutable(
		const ralSubmissionReceipt_t *submission,
		const ralCommandReceipt_t *executable ) {
	return Ral_SubmissionReceiptValid(submission)
		&& submission->backendIdentity == executable->backendIdentity
		&& submission->queue == executable->queue
		&& submission->commandCount == 1u
		&& submission->commands[0].backendIdentity == executable->backendIdentity
		&& submission->commands[0].commandIdentity == executable->commandIdentity
		&& submission->commands[0].generation == executable->generation
		&& submission->commands[0].queue == executable->queue
		&& submission->commands[0].state == RAL_COMMAND_SUBMITTED;
}

static void CancelRange( const ralFrameGraphSubmitDescription_t *description,
		const ralFrameGraphSubmitOps_t *ops, uint32_t first,
		uint32_t *attemptCount, uint32_t *failureCount ) {
	uint32_t i;
	for ( i=first; i<description->batchCommandCount; ++i ) {
		ralCommandReceipt_t current;
		memset(&current,0,sizeof(current));
		(*attemptCount)++;
		if ( !ops->getCommandReceipt(description->opsContext,
				description->batchCommands[i].commandBuffer,&current)
				|| (current.state != RAL_COMMAND_RECORDING
					&& current.state != RAL_COMMAND_EXECUTABLE)
				|| ops->cancelCommand(description->opsContext,
					description->batchCommands[i].commandBuffer,&current) != ralSuccess )
			(*failureCount)++;
	}
}

static qboolean SuccessReceiptValid(
		const ralFrameGraphSubmissionReceipt_t *receipt ) {
	static const ralSubmissionReceipt_t empty;
	uint32_t i;
	if ( !receipt || receipt->schemaVersion != RAL_FRAME_GRAPH_SUBMIT_SCHEMA_VERSION
			|| !receipt->backendIdentity || !receipt->executionPlanIdentity
			|| !receipt->recordingReceiptIdentity || !receipt->opsIdentity
			|| receipt->executionGeneration == 0u
			|| receipt->executionGeneration == UINT64_MAX
			|| receipt->graphGeneration == 0u || receipt->graphGeneration == UINT64_MAX
			|| receipt->recordingGeneration == 0u
			|| receipt->recordingGeneration == UINT64_MAX
			|| receipt->submissionGeneration == 0u
			|| receipt->submissionGeneration == UINT64_MAX
			|| !BoolValid(receipt->timelineSemaphoresSupported)
			|| receipt->submissionCount == 0u
			|| receipt->submissionCount > RAL_FRAME_GRAPH_MAX_PASSES
			|| receipt->ready != qtrue ) return qfalse;
	for ( i=0u; i<receipt->submissionCount; ++i )
		if ( !Ral_SubmissionReceiptValid(&receipt->submissions[i])
				|| receipt->submissions[i].backendIdentity != receipt->backendIdentity )
			return qfalse;
	for ( i=receipt->submissionCount; i<RAL_FRAME_GRAPH_MAX_PASSES; ++i )
		if ( memcmp(&receipt->submissions[i],&empty,sizeof(empty)) != 0 ) return qfalse;
	return qtrue;
}

static qboolean FailureReceiptValid(
		const ralFrameGraphSubmissionFailureReceipt_t *receipt ) {
	static const ralSubmissionReceipt_t empty;
	uint32_t i;
	if ( !receipt || receipt->schemaVersion != RAL_FRAME_GRAPH_SUBMIT_SCHEMA_VERSION
			|| !receipt->backendIdentity || !receipt->executionPlanIdentity
			|| !receipt->recordingReceiptIdentity
			|| receipt->submissionGeneration == 0u
			|| receipt->submissionGeneration == UINT64_MAX
			|| (receipt->stage != RAL_FRAME_GRAPH_SUBMIT_FAILURE_END
				&& receipt->stage != RAL_FRAME_GRAPH_SUBMIT_FAILURE_QUEUE)
			|| receipt->failedBatchIndex >= RAL_FRAME_GRAPH_MAX_PASSES
			|| receipt->submittedBatchCount > receipt->failedBatchIndex
			|| receipt->cancellationFailureCount > receipt->cancellationAttemptCount
			|| receipt->ready != qtrue ) return qfalse;
	if ( receipt->stage == RAL_FRAME_GRAPH_SUBMIT_FAILURE_END
			&& receipt->submittedBatchCount != 0u ) return qfalse;
	for ( i=0u; i<receipt->submittedBatchCount; ++i )
		if ( !Ral_SubmissionReceiptValid(&receipt->submittedPrefix[i])
				|| receipt->submittedPrefix[i].backendIdentity
					!= receipt->backendIdentity ) return qfalse;
	for ( i=receipt->submittedBatchCount; i<RAL_FRAME_GRAPH_MAX_PASSES; ++i )
		if ( memcmp(&receipt->submittedPrefix[i],&empty,sizeof(empty)) != 0 )
			return qfalse;
	return qtrue;
}

qboolean RalFrameGraphSubmit_ReceiptExact(
		const ralFrameGraphSubmissionReceipt_t *a,
		const ralFrameGraphSubmissionReceipt_t *b ) {
	uint32_t i;
	if ( !SuccessReceiptValid(a) || !SuccessReceiptValid(b)
			|| a->backendIdentity != b->backendIdentity
			|| a->executionPlanIdentity != b->executionPlanIdentity
			|| a->recordingReceiptIdentity != b->recordingReceiptIdentity
			|| a->opsIdentity != b->opsIdentity
			|| a->opsContextIdentity != b->opsContextIdentity
			|| a->executionGeneration != b->executionGeneration
			|| a->graphGeneration != b->graphGeneration
			|| a->recordingGeneration != b->recordingGeneration
			|| a->submissionGeneration != b->submissionGeneration
			|| a->timelineSemaphoresSupported != b->timelineSemaphoresSupported
			|| a->submissionCount != b->submissionCount || a->ready != b->ready )
		return qfalse;
	for ( i=0u; i<RAL_FRAME_GRAPH_PHYSICAL_QUEUE_COUNT; ++i )
		if ( a->queueTimelines[i] != b->queueTimelines[i]
				|| a->timelineBaseValues[i] != b->timelineBaseValues[i]
				|| a->timelineFinalValues[i] != b->timelineFinalValues[i] ) return qfalse;
	for ( i=0u; i<a->submissionCount; ++i )
		if ( !Ral_SubmissionReceiptExact(&a->submissions[i],&b->submissions[i]) )
			return qfalse;
	return qtrue;
}

qboolean RalFrameGraphSubmit_FailureReceiptExact(
		const ralFrameGraphSubmissionFailureReceipt_t *a,
		const ralFrameGraphSubmissionFailureReceipt_t *b ) {
	uint32_t i;
	if ( !FailureReceiptValid(a) || !FailureReceiptValid(b)
			|| a->backendIdentity != b->backendIdentity
			|| a->executionPlanIdentity != b->executionPlanIdentity
			|| a->recordingReceiptIdentity != b->recordingReceiptIdentity
			|| a->submissionGeneration != b->submissionGeneration
			|| a->stage != b->stage || a->failedBatchIndex != b->failedBatchIndex
			|| a->submittedBatchCount != b->submittedBatchCount
			|| a->cancellationAttemptCount != b->cancellationAttemptCount
			|| a->cancellationFailureCount != b->cancellationFailureCount
			|| a->ready != b->ready ) return qfalse;
	for ( i=0u; i<a->submittedBatchCount; ++i )
		if ( !Ral_SubmissionReceiptExact(&a->submittedPrefix[i],
			&b->submittedPrefix[i]) ) return qfalse;
	return qtrue;
}

static void PublishFailure( const ralFrameGraphSubmitDescription_t *description,
		ralFrameGraphSubmitFailureStage_t stage, uint32_t failedBatch,
		const ralSubmissionReceipt_t *submitted, uint32_t submittedCount,
		uint32_t cancelAttempts, uint32_t cancelFailures,
		ralFrameGraphSubmissionFailureReceipt_t *outFailure ) {
	ralFrameGraphSubmissionFailureReceipt_t candidate;
	uint32_t i;
	memset(&candidate,0,sizeof(candidate));
	candidate.schemaVersion=RAL_FRAME_GRAPH_SUBMIT_SCHEMA_VERSION;
	candidate.backendIdentity=description->executionPlan->description.backendIdentity;
	candidate.executionPlanIdentity=description->executionPlan;
	candidate.recordingReceiptIdentity=description->recordingReceipt;
	candidate.submissionGeneration=description->generation;
	candidate.stage=stage;candidate.failedBatchIndex=failedBatch;
	candidate.submittedBatchCount=submittedCount;
	for ( i=0u; i<submittedCount; ++i ) candidate.submittedPrefix[i]=submitted[i];
	candidate.cancellationAttemptCount=cancelAttempts;
	candidate.cancellationFailureCount=cancelFailures;
	candidate.ready=qtrue;
	if ( FailureReceiptValid(&candidate) ) *outFailure=candidate;
}

ralFrameGraphSubmitOutcome_t RalFrameGraphSubmit_Submit(
		const ralFrameGraphSubmitDescription_t *description,
		ralFrameGraphSubmissionReceipt_t *outComplete,
		ralFrameGraphSubmissionFailureReceipt_t *outFailure ) {
	ralFrameGraphSubmitDescription_t frozen;
	ralFrameGraphSubmitOps_t ops;
	ralCommandReceipt_t executable[RAL_FRAME_GRAPH_MAX_PASSES];
	ralSubmissionReceipt_t submitted[RAL_FRAME_GRAPH_MAX_PASSES];
	uint32_t i,cancelAttempts=0u,cancelFailures=0u;
	if ( !outComplete || !outFailure || (const void *)outComplete==(const void *)outFailure
			|| !DescriptionShapeValid(description) ) return RAL_FRAME_GRAPH_SUBMIT_REJECTED;
	frozen=*description;ops=*description->ops;
	memset(executable,0,sizeof(executable));memset(submitted,0,sizeof(submitted));
	if ( !PreflightCurrent(&frozen,&ops) ) return RAL_FRAME_GRAPH_SUBMIT_REJECTED;
	for ( i=0u; i<frozen.batchCommandCount; ++i ) {
		if ( ops.endCommand(frozen.opsContext,frozen.batchCommands[i].commandBuffer,
				&frozen.batchCommands[i].recordingReceipt,&executable[i]) != ralSuccess
				|| !ExecutableMatchesRecording(&executable[i],
					&frozen.batchCommands[i].recordingReceipt) ) {
			CancelRange(&frozen,&ops,0u,&cancelAttempts,&cancelFailures);
			PublishFailure(&frozen,RAL_FRAME_GRAPH_SUBMIT_FAILURE_END,i,submitted,0u,
				cancelAttempts,cancelFailures,outFailure);
			return RAL_FRAME_GRAPH_SUBMIT_FAILED;
		}
	}
	if ( memcmp(description,&frozen,sizeof(frozen)) != 0
			|| description->ops != frozen.ops
			|| memcmp(frozen.ops,&ops,sizeof(ops)) != 0
			|| !RalFrameGraphExecution_PlanExact(frozen.executionPlan,
				frozen.executionPlan)
			|| !RalFrameGraphRecord_ReceiptExact(frozen.recordingReceipt,
				frozen.recordingReceipt) ) {
		CancelRange(&frozen,&ops,0u,&cancelAttempts,&cancelFailures);
		PublishFailure(&frozen,RAL_FRAME_GRAPH_SUBMIT_FAILURE_END,0u,submitted,0u,
			cancelAttempts,cancelFailures,outFailure);
		return RAL_FRAME_GRAPH_SUBMIT_FAILED;
	}
	for ( i=0u; i<frozen.executionPlan->submissionBatchCount; ++i ) {
		const ralFrameGraphSubmissionBatch_t *batch
			=&frozen.executionPlan->submissionBatches[i];
		ralCommandBuffer_t *commands[1]={frozen.batchCommands[i].commandBuffer};
		ralSemaphore_t *waits[RAL_FRAME_GRAPH_MAX_SUBMISSION_WAITS];
		uint64_t waitValues[RAL_FRAME_GRAPH_MAX_SUBMISSION_WAITS];
		ralSemaphore_t *signals[1];uint64_t signalValues[1];
		ralSubmitInfo_t submit;uint32_t j;
		memset(&submit,0,sizeof(submit));submit.commandBuffers=commands;
		submit.numCommandBuffers=1u;
		if ( frozen.timelineSemaphoresSupported == qtrue ) {
			for ( j=0u; j<batch->waitCount; ++j ) {
				waits[j]=frozen.queueTimelines[(uint32_t)batch->waits[j].sourcePhysicalQueue];
				waitValues[j]=batch->waits[j].value;
			}
			signals[0]=frozen.queueTimelines[(uint32_t)batch->physicalQueue];
			signalValues[0]=batch->signalValue;
			submit.waitSemaphores=waits;submit.numWaitSemaphores=batch->waitCount;
			submit.waitValues=waitValues;submit.signalSemaphores=signals;
			submit.numSignalSemaphores=1u;submit.signalValues=signalValues;
		}
		if ( ops.submitExact(frozen.opsContext,
				(ralBackend_t *)frozen.executionPlan->description.backendIdentity,
				batch->physicalQueue,&submit,&executable[i],&submitted[i]) != ralSuccess
				|| !SubmissionMatchesExecutable(&submitted[i],&executable[i]) ) {
			CancelRange(&frozen,&ops,i,&cancelAttempts,&cancelFailures);
			PublishFailure(&frozen,RAL_FRAME_GRAPH_SUBMIT_FAILURE_QUEUE,i,submitted,i,
				cancelAttempts,cancelFailures,outFailure);
			return RAL_FRAME_GRAPH_SUBMIT_FAILED;
		}
	}
	{
		ralFrameGraphSubmissionReceipt_t candidate;
		memset(&candidate,0,sizeof(candidate));
		candidate.schemaVersion=RAL_FRAME_GRAPH_SUBMIT_SCHEMA_VERSION;
		candidate.backendIdentity=frozen.executionPlan->description.backendIdentity;
		candidate.executionPlanIdentity=frozen.executionPlan;
		candidate.recordingReceiptIdentity=frozen.recordingReceipt;
		candidate.opsIdentity=frozen.ops;candidate.opsContextIdentity=frozen.opsContext;
		candidate.executionGeneration=frozen.executionPlan->description.generation;
		candidate.graphGeneration=frozen.executionPlan->description.graphPlan
			->description.generation;
		candidate.recordingGeneration=frozen.recordingReceipt->recordingGeneration;
		candidate.submissionGeneration=frozen.generation;
		candidate.timelineSemaphoresSupported=frozen.timelineSemaphoresSupported;
		memcpy(candidate.queueTimelines,frozen.queueTimelines,
			sizeof(candidate.queueTimelines));
		memcpy(candidate.timelineBaseValues,
			frozen.executionPlan->description.timelineBaseValues,
			sizeof(candidate.timelineBaseValues));
		memcpy(candidate.timelineFinalValues,frozen.executionPlan->timelineFinalValues,
			sizeof(candidate.timelineFinalValues));
		candidate.submissionCount=frozen.executionPlan->submissionBatchCount;
		for ( i=0u; i<candidate.submissionCount; ++i )
			candidate.submissions[i]=submitted[i];
		candidate.ready=qtrue;
		if ( !SuccessReceiptValid(&candidate) ) return RAL_FRAME_GRAPH_SUBMIT_FAILED;
		*outComplete=candidate;
	}
	return RAL_FRAME_GRAPH_SUBMIT_COMPLETE;
}
