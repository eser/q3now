// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_opengl_command.h"
#include "ral_opengl_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define RAL_GL_APIENTRY __stdcall
#else
#define RAL_GL_APIENTRY
#endif

typedef unsigned int glEnum_t;
typedef unsigned int glBitfield_t;
typedef void *glSync_t;

enum {
	GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT_VALUE = 0x00000001,
	GL_ELEMENT_ARRAY_BARRIER_BIT_VALUE = 0x00000002,
	GL_UNIFORM_BARRIER_BIT_VALUE = 0x00000004,
	GL_TEXTURE_FETCH_BARRIER_BIT_VALUE = 0x00000008,
	GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_VALUE = 0x00000020,
	GL_COMMAND_BARRIER_BIT_VALUE = 0x00000040,
	GL_PIXEL_BUFFER_BARRIER_BIT_VALUE = 0x00000080,
	GL_TEXTURE_UPDATE_BARRIER_BIT_VALUE = 0x00000100,
	GL_BUFFER_UPDATE_BARRIER_BIT_VALUE = 0x00000200,
	GL_FRAMEBUFFER_BARRIER_BIT_VALUE = 0x00000400,
	GL_SHADER_STORAGE_BARRIER_BIT_VALUE = 0x00002000,
	GL_ALL_BARRIER_BITS_VALUE = 0xffffffffu,
	GL_SYNC_GPU_COMMANDS_COMPLETE_VALUE = 0x9117,
	GL_SYNC_FLUSH_COMMANDS_BIT_VALUE = 0x00000001,
	GL_ALREADY_SIGNALED_VALUE = 0x911a,
	GL_CONDITION_SATISFIED_VALUE = 0x911c,
	GL_NO_ERROR_VALUE = 0
};

typedef void ( RAL_GL_APIENTRY *memoryBarrierFn )( glBitfield_t );
typedef glSync_t ( RAL_GL_APIENTRY *fenceSyncFn )( glEnum_t, glBitfield_t );
typedef glEnum_t ( RAL_GL_APIENTRY *clientWaitSyncFn )( glSync_t, glBitfield_t, uint64_t );
typedef void ( RAL_GL_APIENTRY *deleteSyncFn )( glSync_t );
typedef glEnum_t ( RAL_GL_APIENTRY *getErrorFn )( void );

struct ralOpenGlCommand_s {
	ralOpenGlCore_t *core;
	ralOpenGlCoreReceipt_t coreReceipt;
	memoryBarrierFn MemoryBarrier;
	fenceSyncFn FenceSync;
	clientWaitSyncFn ClientWaitSync;
	deleteSyncFn DeleteSync;
	getErrorFn GetError;
	ralCommandLifecycle_t lifecycle;
	ralSubmissionLifecycle_t submissionLifecycle;
	ralSubmissionReceipt_t liveSubmission;
	glSync_t fence;
	uint64_t commandToken;
	uint64_t commandGeneration;
	uint32_t barrierCount;
	uint64_t barrierDigest;
	qboolean active;
};

static qboolean LoadProc( const ralOpenGlCore_t *core, const char *name,
		ralOpenGlProc_t *out ) {
	ralOpenGlProc_t proc;
	if ( !out ) return qfalse;
	proc = RalOpenGl_CoreResolve( core, name );
	if ( !proc ) return qfalse;
	*out = proc;
	return qtrue;
}

static qboolean ScopeBits( ralBarrierScope_t scope, glBitfield_t *outBits ) {
	glBitfield_t bits;
	switch ( scope ) {
	case RAL_BARRIER_ALL: bits = GL_ALL_BARRIER_BITS_VALUE; break;
	case RAL_BARRIER_COMPUTE_TO_GRAPHICS:
		bits = GL_SHADER_STORAGE_BARRIER_BIT_VALUE
			| GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_VALUE
			| GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT_VALUE
			| GL_ELEMENT_ARRAY_BARRIER_BIT_VALUE
			| GL_UNIFORM_BARRIER_BIT_VALUE
			| GL_TEXTURE_FETCH_BARRIER_BIT_VALUE; break;
	case RAL_BARRIER_COMPUTE_TO_COMPUTE:
	case RAL_BARRIER_GRAPHICS_TO_COMPUTE:
		bits = GL_SHADER_STORAGE_BARRIER_BIT_VALUE
			| GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_VALUE; break;
	case RAL_BARRIER_COMPUTE_TO_TRANSFER:
		bits = GL_SHADER_STORAGE_BARRIER_BIT_VALUE
			| GL_PIXEL_BUFFER_BARRIER_BIT_VALUE
			| GL_TEXTURE_UPDATE_BARRIER_BIT_VALUE
			| GL_BUFFER_UPDATE_BARRIER_BIT_VALUE; break;
	case RAL_BARRIER_TRANSFER_TO_GRAPHICS:
		bits = GL_PIXEL_BUFFER_BARRIER_BIT_VALUE
			| GL_TEXTURE_UPDATE_BARRIER_BIT_VALUE
			| GL_BUFFER_UPDATE_BARRIER_BIT_VALUE
			| GL_TEXTURE_FETCH_BARRIER_BIT_VALUE
			| GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT_VALUE
			| GL_ELEMENT_ARRAY_BARRIER_BIT_VALUE; break;
	case RAL_BARRIER_COLOR_ATTACHMENT_TO_FRAGMENT:
		bits = GL_FRAMEBUFFER_BARRIER_BIT_VALUE
			| GL_TEXTURE_FETCH_BARRIER_BIT_VALUE; break;
	case RAL_BARRIER_INDIRECT:
		bits = GL_SHADER_STORAGE_BARRIER_BIT_VALUE
			| GL_COMMAND_BARRIER_BIT_VALUE; break;
	default: return qfalse;
	}
	*outBits = bits;
	return qtrue;
}

static qboolean CommandReceiptValid( const ralOpenGlCommandReceipt_t *receipt ) {
	return receipt
		&& receipt->schemaVersion == RAL_OPENGL_COMMAND_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_OPENGL
		&& receipt->coreGeneration != 0u
		&& receipt->coreGeneration != UINT64_MAX
		&& Ral_CommandReceiptValid( &receipt->command )
		&& receipt->barrierDigest != 0u && receipt->ready == qtrue;
}

static void BuildCommandReceipt( const ralOpenGlCommand_t *command,
		const ralCommandReceipt_t *shared, ralOpenGlCommandReceipt_t *out ) {
	memset( out, 0, sizeof( *out ) );
	out->schemaVersion = RAL_OPENGL_COMMAND_SCHEMA_VERSION;
	out->backendType = RAL_BACKEND_OPENGL;
	out->coreGeneration = command->coreReceipt.generation;
	out->command = *shared;
	out->barrierCount = command->barrierCount;
	out->barrierDigest = command->barrierDigest;
	out->ready = qtrue;
}

qboolean RalOpenGl_CommandReceiptExact( const ralOpenGlCommandReceipt_t *a,
		const ralOpenGlCommandReceipt_t *b ) {
	return CommandReceiptValid( a ) && CommandReceiptValid( b )
		&& a->coreGeneration == b->coreGeneration
		&& Ral_CommandReceiptExact( &a->command, &b->command )
		&& a->barrierCount == b->barrierCount
		&& a->barrierDigest == b->barrierDigest;
}

static qboolean SubmissionReceiptValid(
		const ralOpenGlSubmissionReceipt_t *receipt ) {
	return receipt
		&& receipt->schemaVersion == RAL_OPENGL_COMMAND_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_OPENGL
		&& receipt->coreGeneration != 0u
		&& receipt->coreGeneration != UINT64_MAX
		&& Ral_SubmissionReceiptValid( &receipt->submission )
		&& receipt->barrierDigest != 0u && receipt->ready == qtrue;
}

qboolean RalOpenGl_SubmissionReceiptExact(
		const ralOpenGlSubmissionReceipt_t *a,
		const ralOpenGlSubmissionReceipt_t *b ) {
	return SubmissionReceiptValid( a ) && SubmissionReceiptValid( b )
		&& a->coreGeneration == b->coreGeneration
		&& Ral_SubmissionReceiptExact( &a->submission, &b->submission )
		&& a->barrierCount == b->barrierCount
		&& a->barrierDigest == b->barrierDigest;
}

qboolean RalOpenGl_CommandCreate( ralOpenGlCore_t *core,
		const ralOpenGlCoreReceipt_t *coreReceipt,
		ralOpenGlCommand_t **outCommand ) {
	ralOpenGlCommand_t *candidate;
	ralOpenGlProc_t proc;
	if ( !outCommand || !RalOpenGl_CoreMatchesReceipt( core, coreReceipt ) )
		return qfalse;
	candidate = (ralOpenGlCommand_t *)calloc( 1u, sizeof( *candidate ) );
	if ( !candidate ) return qfalse;
	candidate->core = core;
	candidate->coreReceipt = *coreReceipt;
#define LOAD( member, name, type ) do { \
	if ( !LoadProc( core, name, &proc ) ) goto fail; \
	candidate->member = (type)proc; \
} while ( 0 )
	LOAD( MemoryBarrier, "glMemoryBarrier", memoryBarrierFn );
	LOAD( FenceSync, "glFenceSync", fenceSyncFn );
	LOAD( ClientWaitSync, "glClientWaitSync", clientWaitSyncFn );
	LOAD( DeleteSync, "glDeleteSync", deleteSyncFn );
	LOAD( GetError, "glGetError", getErrorFn );
#undef LOAD
	candidate->barrierDigest = UINT64_C(1469598103934665603);
	Ral_SubmissionLifecycleInit( &candidate->submissionLifecycle,
		(const ralBackend_t *)core, RAL_QUEUE_GRAPHICS );
	*outCommand = candidate;
	return qtrue;
fail:
	memset( candidate, 0, sizeof( *candidate ) );
	free( candidate );
	return qfalse;
}

void RalOpenGl_CommandDestroy( ralOpenGlCommand_t *command ) {
	if ( !command ) return;
	if ( command->fence ) command->DeleteSync( command->fence );
	memset( command, 0, sizeof( *command ) );
	free( command );
}

qboolean RalOpenGl_CommandBegin( ralOpenGlCommand_t *command,
		ralOpenGlCommandReceipt_t *outRecording ) {
	ralCommandLifecycle_t lifecycle;
	ralCommandReceipt_t recording;
	if ( !command || !outRecording || command->active || command->fence
			|| !RalOpenGl_CoreMatchesReceipt( command->core,
				&command->coreReceipt ) || command->commandToken == UINT64_MAX )
		return qfalse;
	command->commandToken++;
	Ral_CommandLifecycleInit( &lifecycle, (const ralBackend_t *)command->core,
		(const ralCommandBuffer_t *)&command->commandToken, RAL_QUEUE_GRAPHICS );
	lifecycle.generation = command->commandGeneration;
	if ( Ral_CommandLifecyclePublishBegin( &lifecycle, &recording ) != ralSuccess )
		return qfalse;
	command->lifecycle = lifecycle;
	command->commandGeneration = lifecycle.generation;
	command->barrierCount = 0u;
	command->barrierDigest = UINT64_C(1469598103934665603);
	command->active = qtrue;
	BuildCommandReceipt( command, &recording, outRecording );
	return qtrue;
}

qboolean RalOpenGl_CommandBarrier( ralOpenGlCommand_t *command,
		const ralOpenGlCommandReceipt_t *recording, ralBarrierScope_t scope,
		ralOpenGlCommandReceipt_t *outRecording ) {
	ralCommandReceipt_t current;
	glBitfield_t bits;
	uint64_t digest;
	if ( !command || !recording || !outRecording || !command->active
			|| !ScopeBits( scope, &bits )
			|| Ral_CommandLifecycleGetReceipt( &command->lifecycle, &current )
				!= ralSuccess || current.state != RAL_COMMAND_RECORDING
			|| !Ral_CommandReceiptExact( &current, &recording->command )
			|| recording->coreGeneration != command->coreReceipt.generation
			|| recording->barrierCount != command->barrierCount
			|| recording->barrierDigest != command->barrierDigest
			|| command->barrierCount == UINT32_MAX ) return qfalse;
	command->MemoryBarrier( bits );
	if ( command->GetError() != GL_NO_ERROR_VALUE ) return qfalse;
	digest = command->barrierDigest ^ (uint64_t)scope;
	digest *= UINT64_C(1099511628211);
	digest ^= (uint64_t)bits;
	digest *= UINT64_C(1099511628211);
	command->barrierCount++;
	command->barrierDigest = digest ? digest : 1u;
	BuildCommandReceipt( command, &current, outRecording );
	return qtrue;
}

qboolean RalOpenGl_CommandEnd( ralOpenGlCommand_t *command,
		const ralOpenGlCommandReceipt_t *recording,
		ralOpenGlCommandReceipt_t *outExecutable ) {
	ralCommandReceipt_t executable;
	if ( !command || !recording || !outExecutable || !command->active
			|| recording->coreGeneration != command->coreReceipt.generation
			|| recording->barrierCount != command->barrierCount
			|| recording->barrierDigest != command->barrierDigest
			|| Ral_CommandLifecyclePublishEnd( &command->lifecycle,
				&recording->command, &executable ) != ralSuccess ) return qfalse;
	BuildCommandReceipt( command, &executable, outExecutable );
	return qtrue;
}

qboolean RalOpenGl_CommandSubmit( ralOpenGlCommand_t *command,
		const ralOpenGlCommandReceipt_t *executable,
		ralOpenGlSubmissionReceipt_t *outSubmission ) {
	ralCommandLifecycle_t *commands[1];
	ralSubmissionReceipt_t submission;
	glSync_t fence;
	glEnum_t error;
	if ( !command || !executable || !outSubmission || !command->active
			|| command->fence || executable->command.state != RAL_COMMAND_EXECUTABLE
			|| executable->coreGeneration != command->coreReceipt.generation
			|| executable->barrierCount != command->barrierCount
			|| executable->barrierDigest != command->barrierDigest ) return qfalse;
	fence = command->FenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE_VALUE, 0u );
	error = command->GetError();
	if ( !fence || error != GL_NO_ERROR_VALUE ) {
		if ( fence ) command->DeleteSync( fence );
		return qfalse;
	}
	commands[0] = &command->lifecycle;
	if ( Ral_SubmissionLifecyclePublish( &command->submissionLifecycle, commands,
			&executable->command, 1u, &submission ) != ralSuccess ) {
		command->DeleteSync( fence );
		return qfalse;
	}
	command->fence = fence;
	command->liveSubmission = submission;
	memset( outSubmission, 0, sizeof( *outSubmission ) );
	outSubmission->schemaVersion = RAL_OPENGL_COMMAND_SCHEMA_VERSION;
	outSubmission->backendType = RAL_BACKEND_OPENGL;
	outSubmission->coreGeneration = command->coreReceipt.generation;
	outSubmission->submission = submission;
	outSubmission->barrierCount = command->barrierCount;
	outSubmission->barrierDigest = command->barrierDigest;
	outSubmission->ready = qtrue;
	return qtrue;
}

qboolean RalOpenGl_CommandWait( ralOpenGlCommand_t *command,
		const ralOpenGlSubmissionReceipt_t *submission, uint64_t timeoutNs,
		ralOpenGlCompletionReceipt_t *outCompletion ) {
	glEnum_t waitResult;
	const ralCommandReceipt_t *submitted;
	if ( !command || !submission || !outCompletion || !command->fence
			|| !RalOpenGl_SubmissionReceiptExact( submission, submission )
			|| !Ral_SubmissionReceiptExact( &submission->submission,
				&command->liveSubmission ) ) return qfalse;
	waitResult = command->ClientWaitSync( command->fence,
		GL_SYNC_FLUSH_COMMANDS_BIT_VALUE, timeoutNs );
	if ( waitResult != GL_ALREADY_SIGNALED_VALUE
			&& waitResult != GL_CONDITION_SATISFIED_VALUE ) return qfalse;
	submitted = &submission->submission.commands[0];
	command->DeleteSync( command->fence );
	command->fence = NULL;
	if ( Ral_CommandLifecycleRecycle( &command->lifecycle, submitted )
			!= ralSuccess ) return qfalse;
	memset( outCompletion, 0, sizeof( *outCompletion ) );
	outCompletion->schemaVersion = RAL_OPENGL_COMMAND_SCHEMA_VERSION;
	outCompletion->backendType = RAL_BACKEND_OPENGL;
	outCompletion->coreGeneration = command->coreReceipt.generation;
	outCompletion->submissionGeneration = submission->submission.generation;
	outCompletion->commandGeneration = submitted->generation;
	outCompletion->complete = qtrue;
	outCompletion->ready = qtrue;
	memset( &command->liveSubmission, 0, sizeof( command->liveSubmission ) );
	command->active = qfalse;
	return qtrue;
}
