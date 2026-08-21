// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_command_lifecycle.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; \
} } while ( 0 )
#define ID(type, value) ((const type *)(uintptr_t)(value))

static int RunQueue( ralQueueType_t queue, uintptr_t base ) {
	const ralBackend_t *backend = ID( ralBackend_t, base + 1u );
	const ralCommandBuffer_t *commandA = ID( ralCommandBuffer_t, base + 2u );
	const ralCommandBuffer_t *commandB = ID( ralCommandBuffer_t, base + 3u );
	ralCommandLifecycle_t commands[2], before[2];
	ralCommandLifecycle_t *commandPointers[2] = { &commands[0], &commands[1] };
	ralSubmissionLifecycle_t submission, submissionBefore;
	ralCommandReceipt_t recording[2], executable[2], stale, outputCommand;
	ralSubmissionReceipt_t receipt, output, sentinel;
	uint32_t i;

	Ral_CommandLifecycleInit( &commands[0], backend, commandA, queue );
	Ral_CommandLifecycleInit( &commands[1], backend, commandB, queue );
	Ral_SubmissionLifecycleInit( &submission, backend, queue );
	memset( &outputCommand, 0x5a, sizeof( outputCommand ) );
	stale = outputCommand;
	CHECK( Ral_CommandLifecycleGetReceipt( &commands[0], &outputCommand ) == ralErrorInvalidArgument );
	CHECK( memcmp( &outputCommand, &stale, sizeof( stale ) ) == 0 );

	for ( i = 0; i < 2; ++i ) {
		CHECK( Ral_CommandLifecyclePublishBegin( &commands[i], &recording[i] ) == ralSuccess );
		CHECK( recording[i].queue == queue && recording[i].generation == 1u );
		before[i] = commands[i];
		outputCommand = recording[i];
		stale = recording[i]; stale.generation++;
		CHECK( Ral_CommandLifecyclePublishEnd( &commands[i], &stale, &outputCommand ) == ralErrorInvalidArgument );
		CHECK( memcmp( &commands[i], &before[i], sizeof( before[i] ) ) == 0 );
		CHECK( Ral_CommandReceiptExact( &outputCommand, &recording[i] ) );
		CHECK( Ral_CommandLifecyclePublishEnd( &commands[i], &recording[i], &executable[i] ) == ralSuccess );
		CHECK( executable[i].state == RAL_COMMAND_EXECUTABLE );
	}

	CHECK( Ral_SubmissionLifecycleCanPublish( &submission, commandPointers, executable, 2 ) );
	memset( &sentinel, 0xa5, sizeof( sentinel ) ); output = sentinel;
	before[0] = commands[0]; before[1] = commands[1]; submissionBefore = submission;
	{
		ralCommandReceipt_t wrong[2] = { executable[0], executable[1] };
		wrong[1].backendIdentity = ID( ralBackend_t, base + 99u );
		CHECK( Ral_SubmissionLifecyclePublish( &submission, commandPointers, wrong, 2, &output ) == ralErrorInvalidArgument );
	}
	CHECK( memcmp( &commands[0], &before[0], sizeof( before[0] ) ) == 0 );
	CHECK( memcmp( &commands[1], &before[1], sizeof( before[1] ) ) == 0 );
	CHECK( memcmp( &submission, &submissionBefore, sizeof( submission ) ) == 0 );
	CHECK( memcmp( &output, &sentinel, sizeof( output ) ) == 0 );
	stale = executable[1]; stale.queue = (ralQueueType_t)(( queue + 1 ) % 3);
	{
		ralCommandReceipt_t wrong[2] = { executable[0], stale };
		CHECK( Ral_SubmissionLifecyclePublish( &submission, commandPointers, wrong, 2, &output ) == ralErrorInvalidArgument );
	}
	CHECK( memcmp( &commands[0], &before[0], sizeof( before[0] ) ) == 0 );
	CHECK( memcmp( &commands[1], &before[1], sizeof( before[1] ) ) == 0 );
	CHECK( memcmp( &submission, &submissionBefore, sizeof( submission ) ) == 0 );
	CHECK( memcmp( &output, &sentinel, sizeof( output ) ) == 0 );

	commandPointers[1] = &commands[0];
	CHECK( !Ral_SubmissionLifecycleCanPublish( &submission, commandPointers, executable, 2 ) );
	commandPointers[1] = &commands[1];
	CHECK( Ral_SubmissionLifecyclePublish( &submission, commandPointers, executable, 2, &receipt ) == ralSuccess );
	CHECK( Ral_SubmissionReceiptValid( &receipt ) && receipt.commandCount == 2u );
	CHECK( receipt.generation == 1u && receipt.queue == queue );
	output = receipt; output.commands[2].ready = qtrue;
	CHECK( !Ral_SubmissionReceiptValid( &output )
	    && !Ral_SubmissionReceiptExact( &output, &output ) );
	CHECK( commands[0].state == RAL_COMMAND_SUBMITTED && commands[1].state == RAL_COMMAND_SUBMITTED );
	CHECK( !Ral_SubmissionLifecycleCanPublish( &submission, commandPointers, executable, 2 ) );
	output = receipt;
	CHECK( Ral_SubmissionLifecyclePublish( &submission, commandPointers, executable, 2, &output ) == ralErrorInvalidArgument );
	CHECK( Ral_SubmissionReceiptExact( &output, &receipt ) );

	return 0;
}

static int RunWebGpuCancelShape( void ) {
	const ralBackend_t *backend = ID( ralBackend_t, 0x9001u );
	const ralCommandBuffer_t *encoder = ID( ralCommandBuffer_t, 0x9002u );
	ralCommandLifecycle_t lifecycle;
	ralCommandReceipt_t recording, executable, stale;

	Ral_CommandLifecycleInit( &lifecycle, backend, encoder, RAL_QUEUE_GRAPHICS );
	// WebGPU: createCommandEncoder -> recording receipt; finish -> executable.
	CHECK( Ral_CommandLifecyclePublishBegin( &lifecycle, &recording ) == ralSuccess );
	CHECK( Ral_CommandLifecyclePublishEnd( &lifecycle, &recording, &executable ) == ralSuccess );
	stale = recording;
	CHECK( Ral_CommandLifecycleCancel( &lifecycle, &stale ) == ralErrorInvalidArgument );
	CHECK( Ral_CommandLifecycleCancel( &lifecycle, &executable ) == ralSuccess );
	// A canceled encoder gets a new monotonic recording generation.
	CHECK( Ral_CommandLifecyclePublishBegin( &lifecycle, &recording ) == ralSuccess );
	CHECK( recording.generation == 2u );
	CHECK( Ral_CommandLifecycleCancel( &lifecycle, &recording ) == ralSuccess );
	return 0;
}

int main( void ) {
	ralCommandLifecycle_t saturated;
	ralCommandReceipt_t out, sentinel;
	CHECK( RunQueue( RAL_QUEUE_GRAPHICS, 0x1000u ) == 0 );
	CHECK( RunQueue( RAL_QUEUE_COMPUTE, 0x2000u ) == 0 );
	CHECK( RunQueue( RAL_QUEUE_TRANSFER, 0x3000u ) == 0 );
	CHECK( RunWebGpuCancelShape() == 0 );
	Ral_CommandLifecycleInit( &saturated, ID( ralBackend_t, 1u ),
	                         ID( ralCommandBuffer_t, 2u ), RAL_QUEUE_GRAPHICS );
	saturated.generation = UINT64_MAX - 1u;
	memset( &sentinel, 0x3c, sizeof( sentinel ) ); out = sentinel;
	CHECK( Ral_CommandLifecyclePublishBegin( &saturated, &out ) == ralErrorInvalidArgument );
	CHECK( memcmp( &out, &sentinel, sizeof( out ) ) == 0 );
	puts( "ral command lifecycle contract: PASS" );
	return 0;
}
