// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_command_lifecycle.h"

#include <limits.h>
#include <string.h>

static qboolean ralCommandQueueValid( ralQueueType_t queue ) {
	return queue == RAL_QUEUE_GRAPHICS || queue == RAL_QUEUE_COMPUTE
		|| queue == RAL_QUEUE_TRANSFER;
}

static qboolean ralCommandStateReceiptValid( ralCommandLifecycleState_t state ) {
	return state == RAL_COMMAND_RECORDING || state == RAL_COMMAND_EXECUTABLE
		|| state == RAL_COMMAND_SUBMITTED;
}

qboolean Ral_CommandReceiptValid( const ralCommandReceipt_t *receipt ) {
	return receipt && receipt->backendIdentity && receipt->commandIdentity
		&& receipt->generation != 0 && receipt->generation != UINT64_MAX
		&& ralCommandQueueValid( receipt->queue )
		&& ralCommandStateReceiptValid( receipt->state )
		&& receipt->ready == qtrue;
}

qboolean Ral_CommandReceiptExact( const ralCommandReceipt_t *a,
	                              const ralCommandReceipt_t *b ) {
	return Ral_CommandReceiptValid( a ) && Ral_CommandReceiptValid( b )
		&& a->backendIdentity == b->backendIdentity
		&& a->commandIdentity == b->commandIdentity
		&& a->generation == b->generation && a->queue == b->queue
		&& a->state == b->state && a->ready == b->ready;
}

static qboolean ralCommandLifecycleFresh( const ralCommandLifecycle_t *lifecycle ) {
	return lifecycle && lifecycle->backendIdentity && lifecycle->commandIdentity
		&& ralCommandQueueValid( lifecycle->queue )
		&& lifecycle->state == RAL_COMMAND_IDLE;
}

static void ralCommandMakeReceipt( const ralCommandLifecycle_t *lifecycle,
	                               ralCommandReceipt_t *receipt ) {
	memset( receipt, 0, sizeof( *receipt ) );
	receipt->backendIdentity = lifecycle->backendIdentity;
	receipt->commandIdentity = lifecycle->commandIdentity;
	receipt->generation = lifecycle->generation;
	receipt->queue = lifecycle->queue;
	receipt->state = lifecycle->state;
	receipt->ready = qtrue;
}

static qboolean ralCommandLifecycleMatches( const ralCommandLifecycle_t *lifecycle,
	                                        const ralCommandReceipt_t *receipt ) {
	ralCommandReceipt_t current;
	if ( !lifecycle || lifecycle->state == RAL_COMMAND_IDLE
	  || !Ral_CommandReceiptValid( receipt ) ) return qfalse;
	ralCommandMakeReceipt( lifecycle, &current );
	return Ral_CommandReceiptExact( &current, receipt );
}

void Ral_CommandLifecycleInit( ralCommandLifecycle_t *lifecycle,
	                           const ralBackend_t *backend,
	                           const ralCommandBuffer_t *command,
	                           ralQueueType_t queue ) {
	if ( !lifecycle ) return;
	memset( lifecycle, 0, sizeof( *lifecycle ) );
	if ( !backend || !command || !ralCommandQueueValid( queue ) ) return;
	lifecycle->backendIdentity = backend;
	lifecycle->commandIdentity = command;
	lifecycle->queue = queue;
}

ralResult_t Ral_CommandLifecycleGetReceipt( const ralCommandLifecycle_t *lifecycle,
	                                        ralCommandReceipt_t *outReceipt ) {
	ralCommandReceipt_t candidate;
	if ( !outReceipt || !lifecycle || lifecycle->state == RAL_COMMAND_IDLE )
		return ralErrorInvalidArgument;
	ralCommandMakeReceipt( lifecycle, &candidate );
	if ( !Ral_CommandReceiptValid( &candidate ) ) return ralErrorInvalidArgument;
	*outReceipt = candidate;
	return ralSuccess;
}

ralResult_t Ral_CommandLifecyclePublishBegin( ralCommandLifecycle_t *lifecycle,
	                                          ralCommandReceipt_t *outReceipt ) {
	ralCommandLifecycle_t candidate;
	ralCommandReceipt_t receipt;
	if ( !outReceipt || !ralCommandLifecycleFresh( lifecycle )
	  || lifecycle->generation >= UINT64_MAX - 1u )
		return ralErrorInvalidArgument;
	candidate = *lifecycle;
	candidate.generation++;
	candidate.state = RAL_COMMAND_RECORDING;
	ralCommandMakeReceipt( &candidate, &receipt );
	if ( !Ral_CommandReceiptValid( &receipt ) ) return ralErrorInvalidArgument;
	*lifecycle = candidate;
	*outReceipt = receipt;
	return ralSuccess;
}

ralResult_t Ral_CommandLifecyclePublishEnd( ralCommandLifecycle_t *lifecycle,
	                                        const ralCommandReceipt_t *recording,
	                                        ralCommandReceipt_t *outReceipt ) {
	ralCommandLifecycle_t candidate;
	ralCommandReceipt_t receipt;
	if ( !outReceipt || !ralCommandLifecycleMatches( lifecycle, recording )
	  || recording->state != RAL_COMMAND_RECORDING )
		return ralErrorInvalidArgument;
	candidate = *lifecycle;
	candidate.state = RAL_COMMAND_EXECUTABLE;
	ralCommandMakeReceipt( &candidate, &receipt );
	if ( !Ral_CommandReceiptValid( &receipt ) ) return ralErrorInvalidArgument;
	*lifecycle = candidate;
	*outReceipt = receipt;
	return ralSuccess;
}

ralResult_t Ral_CommandLifecycleCancel( ralCommandLifecycle_t *lifecycle,
	                                    const ralCommandReceipt_t *authority ) {
	if ( !ralCommandLifecycleMatches( lifecycle, authority )
	  || ( authority->state != RAL_COMMAND_RECORDING
	    && authority->state != RAL_COMMAND_EXECUTABLE ) )
		return ralErrorInvalidArgument;
	lifecycle->state = RAL_COMMAND_IDLE;
	return ralSuccess;
}

void Ral_SubmissionLifecycleInit( ralSubmissionLifecycle_t *lifecycle,
	                              const ralBackend_t *backend,
	                              ralQueueType_t queue ) {
	if ( !lifecycle ) return;
	memset( lifecycle, 0, sizeof( *lifecycle ) );
	if ( !backend || !ralCommandQueueValid( queue ) ) return;
	lifecycle->backendIdentity = backend;
	lifecycle->queue = queue;
}

qboolean Ral_SubmissionLifecycleCanPublish(
	const ralSubmissionLifecycle_t *submission,
	ralCommandLifecycle_t *const *commands,
	const ralCommandReceipt_t *executableReceipts,
	uint32_t commandCount ) {
	uint32_t i, j;
	if ( !submission || !submission->backendIdentity
	  || !ralCommandQueueValid( submission->queue )
	  || submission->generation >= UINT64_MAX - 1u
	  || !commands || !executableReceipts || commandCount == 0
	  || commandCount > RAL_MAX_SUBMIT_COMMAND_RECEIPTS ) return qfalse;
	for ( i = 0; i < commandCount; ++i ) {
		if ( !commands[i]
		  || !ralCommandLifecycleMatches( commands[i], &executableReceipts[i] )
		  || executableReceipts[i].state != RAL_COMMAND_EXECUTABLE
		  || executableReceipts[i].backendIdentity != submission->backendIdentity
		  || executableReceipts[i].queue != submission->queue ) return qfalse;
		for ( j = 0; j < i; ++j )
			if ( commands[j] == commands[i]
			  || executableReceipts[j].commandIdentity == executableReceipts[i].commandIdentity )
				return qfalse;
	}
	return qtrue;
}

qboolean Ral_SubmissionReceiptValid( const ralSubmissionReceipt_t *receipt ) {
	static const ralCommandReceipt_t zeroCommandReceipt;
	uint32_t i, j;
	if ( !receipt || !receipt->backendIdentity
	  || receipt->generation == 0 || receipt->generation == UINT64_MAX
	  || !ralCommandQueueValid( receipt->queue )
	  || receipt->commandCount == 0
	  || receipt->commandCount > RAL_MAX_SUBMIT_COMMAND_RECEIPTS
	  || receipt->ready != qtrue ) return qfalse;
	for ( i = 0; i < receipt->commandCount; ++i ) {
		if ( !Ral_CommandReceiptValid( &receipt->commands[i] )
		  || receipt->commands[i].backendIdentity != receipt->backendIdentity
		  || receipt->commands[i].queue != receipt->queue
		  || receipt->commands[i].state != RAL_COMMAND_SUBMITTED ) return qfalse;
		for ( j = 0; j < i; ++j )
			if ( receipt->commands[j].commandIdentity == receipt->commands[i].commandIdentity )
				return qfalse;
	}
	for ( i = receipt->commandCount; i < RAL_MAX_SUBMIT_COMMAND_RECEIPTS; ++i )
		if ( memcmp( &receipt->commands[i], &zeroCommandReceipt,
		             sizeof( zeroCommandReceipt ) ) != 0 ) return qfalse;
	return qtrue;
}

qboolean Ral_SubmissionReceiptExact( const ralSubmissionReceipt_t *a,
	                                 const ralSubmissionReceipt_t *b ) {
	uint32_t i;
	if ( !Ral_SubmissionReceiptValid( a ) || !Ral_SubmissionReceiptValid( b )
	  || a->backendIdentity != b->backendIdentity || a->generation != b->generation
	  || a->queue != b->queue || a->commandCount != b->commandCount
	  || a->ready != b->ready ) return qfalse;
	for ( i = 0; i < a->commandCount; ++i )
		if ( !Ral_CommandReceiptExact( &a->commands[i], &b->commands[i] ) ) return qfalse;
	return qtrue;
}

ralResult_t Ral_SubmissionLifecyclePublish(
	ralSubmissionLifecycle_t *submission,
	ralCommandLifecycle_t *const *commands,
	const ralCommandReceipt_t *executableReceipts,
	uint32_t commandCount,
	ralSubmissionReceipt_t *outReceipt ) {
	ralSubmissionReceipt_t candidate;
	uint32_t i;
	if ( !outReceipt || !Ral_SubmissionLifecycleCanPublish( submission, commands,
	                                                       executableReceipts, commandCount ) )
		return ralErrorInvalidArgument;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.backendIdentity = submission->backendIdentity;
	candidate.generation = submission->generation + 1u;
	candidate.queue = submission->queue;
	candidate.commandCount = commandCount;
	candidate.ready = qtrue;
	for ( i = 0; i < commandCount; ++i ) {
		candidate.commands[i] = executableReceipts[i];
		candidate.commands[i].state = RAL_COMMAND_SUBMITTED;
	}
	if ( !Ral_SubmissionReceiptValid( &candidate ) ) return ralErrorInvalidArgument;
	for ( i = 0; i < commandCount; ++i ) commands[i]->state = RAL_COMMAND_SUBMITTED;
	submission->generation = candidate.generation;
	*outReceipt = candidate;
	return ralSuccess;
}
