// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_OPENGL_COMMAND_H
#define WIRED_RAL_OPENGL_COMMAND_H

#include "ral_opengl_core.h"
#include "ral_command.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_OPENGL_COMMAND_SCHEMA_VERSION 1u

typedef struct ralOpenGlCommand_s ralOpenGlCommand_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t coreGeneration;
	ralCommandReceipt_t command;
	uint32_t barrierCount;
	uint64_t barrierDigest;
	qboolean ready;
} ralOpenGlCommandReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t coreGeneration;
	ralSubmissionReceipt_t submission;
	uint32_t barrierCount;
	uint64_t barrierDigest;
	qboolean ready;
} ralOpenGlSubmissionReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t coreGeneration;
	uint64_t submissionGeneration;
	uint64_t commandGeneration;
	qboolean complete;
	qboolean ready;
} ralOpenGlCompletionReceipt_t;

qboolean RalOpenGl_CommandCreate( ralOpenGlCore_t *core,
	const ralOpenGlCoreReceipt_t *coreReceipt,
	ralOpenGlCommand_t **outCommand );
void RalOpenGl_CommandDestroy( ralOpenGlCommand_t *command );
qboolean RalOpenGl_CommandBegin( ralOpenGlCommand_t *command,
	ralOpenGlCommandReceipt_t *outRecording );
qboolean RalOpenGl_CommandBarrier( ralOpenGlCommand_t *command,
	const ralOpenGlCommandReceipt_t *recording, ralBarrierScope_t scope,
	ralOpenGlCommandReceipt_t *outRecording );
qboolean RalOpenGl_CommandEnd( ralOpenGlCommand_t *command,
	const ralOpenGlCommandReceipt_t *recording,
	ralOpenGlCommandReceipt_t *outExecutable );
qboolean RalOpenGl_CommandSubmit( ralOpenGlCommand_t *command,
	const ralOpenGlCommandReceipt_t *executable,
	ralOpenGlSubmissionReceipt_t *outSubmission );
qboolean RalOpenGl_CommandWait( ralOpenGlCommand_t *command,
	const ralOpenGlSubmissionReceipt_t *submission, uint64_t timeoutNs,
	ralOpenGlCompletionReceipt_t *outCompletion );
qboolean RalOpenGl_CommandReceiptExact( const ralOpenGlCommandReceipt_t *a,
	const ralOpenGlCommandReceipt_t *b );
qboolean RalOpenGl_SubmissionReceiptExact(
	const ralOpenGlSubmissionReceipt_t *a,
	const ralOpenGlSubmissionReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
