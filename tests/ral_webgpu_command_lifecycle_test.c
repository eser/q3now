// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_command_lifecycle.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; \
} } while ( 0 )
#define ID(type, value) ((const type *)(uintptr_t)(value))

typedef struct {
	ralCommandLifecycle_t command;
	ralSubmissionLifecycle_t queue;
	qboolean createEncoderSucceeds;
	qboolean finishSucceeds;
	ralResult_t queueSubmitResult;
} webGpuShape_t;

static ralResult_t BeginEncoder( webGpuShape_t *shape, ralCommandReceipt_t *out ) {
	if ( !shape->createEncoderSucceeds ) return ralErrorOutOfMemory;
	return Ral_CommandLifecyclePublishBegin( &shape->command, out );
}

static ralResult_t FinishEncoder( webGpuShape_t *shape,
	                              const ralCommandReceipt_t *recording,
	                              ralCommandReceipt_t *out ) {
	if ( !shape->finishSucceeds ) return ralErrorUnknown;
	return Ral_CommandLifecyclePublishEnd( &shape->command, recording, out );
}

static ralResult_t QueueSubmit( webGpuShape_t *shape,
	                            const ralCommandReceipt_t *executable,
	                            ralSubmissionReceipt_t *out ) {
	ralCommandLifecycle_t *commands[1] = { &shape->command };
	if ( !Ral_SubmissionLifecycleCanPublish( &shape->queue, commands, executable, 1 ) )
		return ralErrorInvalidArgument;
	if ( shape->queueSubmitResult != ralSuccess ) return shape->queueSubmitResult;
	return Ral_SubmissionLifecyclePublish( &shape->queue, commands, executable, 1, out );
}

int main( void ) {
	webGpuShape_t shape;
	ralCommandReceipt_t recording, executable;
	ralSubmissionReceipt_t submitted, sentinel;

	memset( &shape, 0, sizeof( shape ) );
	Ral_CommandLifecycleInit( &shape.command, ID( ralBackend_t, 1u ),
	                         ID( ralCommandBuffer_t, 2u ), RAL_QUEUE_GRAPHICS );
	Ral_SubmissionLifecycleInit( &shape.queue, ID( ralBackend_t, 1u ), RAL_QUEUE_GRAPHICS );
	shape.createEncoderSucceeds = qtrue;
	shape.finishSucceeds = qtrue;
	shape.queueSubmitResult = ralErrorDeviceLost;
	CHECK( BeginEncoder( &shape, &recording ) == ralSuccess );
	CHECK( FinishEncoder( &shape, &recording, &executable ) == ralSuccess );
	memset( &sentinel, 0xa5, sizeof( sentinel ) ); submitted = sentinel;
	CHECK( QueueSubmit( &shape, &executable, &submitted ) == ralErrorDeviceLost );
	CHECK( memcmp( &submitted, &sentinel, sizeof( submitted ) ) == 0 );
	CHECK( shape.command.state == RAL_COMMAND_EXECUTABLE );
	CHECK( Ral_CommandLifecycleCancel( &shape.command, &executable ) == ralSuccess );

	CHECK( BeginEncoder( &shape, &recording ) == ralSuccess );
	shape.finishSucceeds = qfalse;
	CHECK( FinishEncoder( &shape, &recording, &executable ) == ralErrorUnknown );
	CHECK( shape.command.state == RAL_COMMAND_RECORDING );
	CHECK( Ral_CommandLifecycleCancel( &shape.command, &recording ) == ralSuccess );

	shape.finishSucceeds = qtrue;
	shape.queueSubmitResult = ralSuccess;
	CHECK( BeginEncoder( &shape, &recording ) == ralSuccess );
	CHECK( FinishEncoder( &shape, &recording, &executable ) == ralSuccess );
	CHECK( QueueSubmit( &shape, &executable, &submitted ) == ralSuccess );
	CHECK( Ral_SubmissionReceiptValid( &submitted ) );
	CHECK( shape.command.state == RAL_COMMAND_SUBMITTED );
	// WebGPU maps recycle to releasing the completed GPUCommandBuffer and
	// admitting a fresh encoder generation; no reusable native encoder exists.
	CHECK( Ral_CommandLifecycleRecycle( &shape.command,
		&submitted.commands[0] ) == ralSuccess );
	CHECK( shape.command.state == RAL_COMMAND_IDLE );
	CHECK( BeginEncoder( &shape, &recording ) == ralSuccess );
	CHECK( recording.generation == 4u );
	CHECK( Ral_CommandLifecycleCancel( &shape.command, &recording ) == ralSuccess );
	puts( "ral WebGPU command lifecycle contract: PASS" );
	return 0;
}
