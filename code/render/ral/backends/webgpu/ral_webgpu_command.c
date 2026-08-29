// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_command.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct ralWebGpuCommand_s {
	ralWebGpuCore_t *core;
	ralWebGpuCoreReceipt_t coreReceipt;
	void *userData;
	ralWebGpuCommandHostOps_t host;
	ralCommandLifecycle_t lifecycle;
	ralSubmissionLifecycle_t submissionLifecycle;
	ralSubmissionReceipt_t liveSubmission;
	uintptr_t encoderIdentity;
	uintptr_t passIdentity;
	uintptr_t commandBufferIdentity;
	uintptr_t submissionIdentity;
	uint64_t commandToken;
	uint64_t commandGeneration;
	uint64_t operationDigest;
	uint32_t drawCount;
	uint32_t dispatchCount;
	ralWebGpuPassKind_t passKind;
	uintptr_t targetIdentity;
	qboolean active;
};

static void ReleaseNative( ralWebGpuCommand_t *command ) {
	if ( command->submissionIdentity ) command->host.releaseObject(
		command->userData, RAL_WEBGPU_COMMAND_OBJECT_SUBMISSION,
		command->submissionIdentity );
	if ( command->commandBufferIdentity ) command->host.releaseObject(
		command->userData, RAL_WEBGPU_COMMAND_OBJECT_BUFFER,
		command->commandBufferIdentity );
	if ( command->passIdentity ) command->host.releaseObject(
		command->userData, RAL_WEBGPU_COMMAND_OBJECT_PASS,
		command->passIdentity );
	if ( command->encoderIdentity ) command->host.releaseObject(
		command->userData, RAL_WEBGPU_COMMAND_OBJECT_ENCODER,
		command->encoderIdentity );
	command->submissionIdentity = (uintptr_t)0;
	command->commandBufferIdentity = (uintptr_t)0;
	command->passIdentity = (uintptr_t)0;
	command->encoderIdentity = (uintptr_t)0;
}

static qboolean CommandValid( const ralWebGpuCommandReceipt_t *receipt ) {
	return receipt && receipt->schemaVersion == RAL_WEBGPU_COMMAND_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_WEBGPU
		&& receipt->backendGeneration && receipt->backendGeneration != UINT64_MAX
		&& receipt->passKind >= RAL_WEBGPU_PASS_RENDER
		&& receipt->passKind <= RAL_WEBGPU_PASS_COMPUTE
		&& ( receipt->passKind == RAL_WEBGPU_PASS_RENDER
			? receipt->targetIdentity != 0u : receipt->targetIdentity == 0u )
		&& receipt->operationDigest
		&& Ral_CommandReceiptValid( &receipt->command )
		&& receipt->ready == qtrue;
}

static qboolean SubmissionValid( const ralWebGpuSubmissionReceipt_t *receipt ) {
	return receipt && receipt->schemaVersion == RAL_WEBGPU_COMMAND_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_WEBGPU
		&& receipt->backendGeneration && receipt->backendGeneration != UINT64_MAX
		&& receipt->operationDigest
		&& Ral_SubmissionReceiptValid( &receipt->submission )
		&& receipt->ready == qtrue;
}

static void BuildCommandReceipt( const ralWebGpuCommand_t *command,
		const ralCommandReceipt_t *shared, ralWebGpuCommandReceipt_t *out ) {
	memset( out, 0, sizeof( *out ) );
	out->schemaVersion = RAL_WEBGPU_COMMAND_SCHEMA_VERSION;
	out->backendType = RAL_BACKEND_WEBGPU;
	out->backendGeneration = command->coreReceipt.generation;
	out->passKind = command->passKind;
	out->targetIdentity = command->targetIdentity;
	out->operationDigest = command->operationDigest;
	out->drawCount = command->drawCount;
	out->dispatchCount = command->dispatchCount;
	out->command = *shared; out->ready = qtrue;
}

qboolean RalWebGpu_CommandCreate( ralWebGpuCore_t *core,
		const ralWebGpuCoreReceipt_t *coreReceipt,
		const ralWebGpuCommandCreateInfo_t *createInfo,
		ralWebGpuCommand_t **outCommand ) {
	ralWebGpuCommand_t *command;
	if ( !core || !coreReceipt || !createInfo || !outCommand
			|| !RalWebGpu_CoreMatchesReceipt( core, coreReceipt )
			|| !createInfo->host.beginEncoder || !createInfo->host.beginPass
			|| !createInfo->host.recordIndexedDraw
			|| !createInfo->host.recordComputeDispatch
			|| !createInfo->host.endPass || !createInfo->host.finishEncoder
			|| !createInfo->host.submit || !createInfo->host.pollSubmission
			|| !createInfo->host.releaseObject ) return qfalse;
	command = (ralWebGpuCommand_t *)calloc( 1u, sizeof( *command ) );
	if ( !command ) return qfalse;
	command->core = core; command->coreReceipt = *coreReceipt;
	command->userData = createInfo->userData; command->host = createInfo->host;
	Ral_SubmissionLifecycleInit( &command->submissionLifecycle,
		(const ralBackend_t *)coreReceipt->backendIdentity, RAL_QUEUE_GRAPHICS );
	command->submissionLifecycle.generation = coreReceipt->generation;
	*outCommand = command; return qtrue;
}

void RalWebGpu_CommandDestroy( ralWebGpuCommand_t *command ) {
	if ( !command ) return;
	ReleaseNative( command ); memset( command, 0, sizeof( *command ) ); free( command );
}

qboolean RalWebGpu_CommandBegin( ralWebGpuCommand_t *command,
		ralWebGpuPassKind_t kind, uintptr_t targetIdentity,
		ralWebGpuCommandReceipt_t *outRecording ) {
	ralCommandLifecycle_t lifecycle;
	ralCommandReceipt_t recording;
	uintptr_t encoder = (uintptr_t)0, pass = (uintptr_t)0;
	if ( !command || !outRecording || command->active || command->encoderIdentity
			|| !RalWebGpu_CoreMatchesReceipt( command->core, &command->coreReceipt )
			|| kind < RAL_WEBGPU_PASS_RENDER || kind > RAL_WEBGPU_PASS_COMPUTE
			|| ( kind == RAL_WEBGPU_PASS_RENDER && !targetIdentity )
			|| ( kind == RAL_WEBGPU_PASS_COMPUTE && targetIdentity )
			|| command->commandToken == UINT64_MAX ) return qfalse;
	if ( !command->host.beginEncoder( command->userData,
			command->coreReceipt.deviceIdentity, &encoder ) || !encoder ) return qfalse;
	if ( !command->host.beginPass( command->userData, encoder, kind,
			targetIdentity, &pass ) || !pass ) {
		command->host.releaseObject( command->userData,
			RAL_WEBGPU_COMMAND_OBJECT_ENCODER, encoder ); return qfalse;
	}
	command->commandToken++;
	Ral_CommandLifecycleInit( &lifecycle,
		(const ralBackend_t *)command->coreReceipt.backendIdentity,
		(const ralCommandBuffer_t *)&command->commandToken, RAL_QUEUE_GRAPHICS );
	lifecycle.generation = command->commandGeneration;
	if ( Ral_CommandLifecyclePublishBegin( &lifecycle, &recording ) != ralSuccess ) {
		command->host.releaseObject( command->userData,
			RAL_WEBGPU_COMMAND_OBJECT_PASS, pass );
		command->host.releaseObject( command->userData,
			RAL_WEBGPU_COMMAND_OBJECT_ENCODER, encoder ); return qfalse;
	}
	command->encoderIdentity = encoder; command->passIdentity = pass;
	command->lifecycle = lifecycle; command->commandGeneration = lifecycle.generation;
	command->passKind = kind; command->targetIdentity = targetIdentity;
	command->operationDigest = UINT64_C(1469598103934665603);
	command->operationDigest ^= (uint64_t)kind;
	command->operationDigest *= UINT64_C(1099511628211);
	command->operationDigest ^= (uint64_t)targetIdentity;
	command->operationDigest *= UINT64_C(1099511628211);
	if ( !command->operationDigest ) command->operationDigest = 1u;
	command->drawCount = 0u;
	command->dispatchCount = 0u;
	command->active = qtrue;
	BuildCommandReceipt( command, &recording, outRecording ); return qtrue;
}

qboolean RalWebGpu_CommandRecordComputeDispatch( ralWebGpuCommand_t *command,
		const ralWebGpuCommandReceipt_t *recording,
		const ralWebGpuComputeDispatch_t *dispatch,
		ralWebGpuCommandReceipt_t *outRecording ) {
	uint64_t digest;
	if ( !command || !recording || !dispatch || !outRecording || !command->active
			|| command->passKind != RAL_WEBGPU_PASS_COMPUTE
			|| !RalWebGpu_CommandReceiptExact( recording, recording )
			|| recording->command.state != RAL_COMMAND_RECORDING
			|| recording->operationDigest != command->operationDigest
			|| recording->drawCount != command->drawCount
			|| recording->dispatchCount != command->dispatchCount
			|| !dispatch->pipelineIdentity || !dispatch->groupCountX
			|| !dispatch->groupCountY || !dispatch->groupCountZ
			|| !dispatch->contentDigest
			|| dispatch->bindGroupCount > RAL_SHADER_ABI_MAX_BIND_GROUPS
			|| command->dispatchCount == UINT32_MAX ) return qfalse;
	for ( uint32_t i = 0u; i < dispatch->bindGroupCount; ++i )
		if ( !dispatch->bindGroupIdentities[i] ) return qfalse;
	digest = command->operationDigest;
	digest ^= dispatch->pipelineIdentity; digest *= UINT64_C(1099511628211);
	for ( uint32_t i = 0u; i < dispatch->bindGroupCount; ++i ) {
		digest ^= dispatch->bindGroupIdentities[i];
		digest *= UINT64_C(1099511628211);
	}
	digest ^= ( (uint64_t)dispatch->groupCountX << 42u )
		^ ( (uint64_t)dispatch->groupCountY << 21u ) ^ dispatch->groupCountZ;
	digest *= UINT64_C(1099511628211); digest ^= dispatch->contentDigest;
	digest *= UINT64_C(1099511628211); digest ^= dispatch->bindGroupCount;
	if ( !digest ) digest = 1u;
	if ( !command->host.recordComputeDispatch( command->userData,
			command->passIdentity, dispatch ) ) return qfalse;
	command->operationDigest = digest; command->dispatchCount++;
	BuildCommandReceipt( command, &recording->command, outRecording );
	return qtrue;
}

qboolean RalWebGpu_CommandRecordIndexedDraw( ralWebGpuCommand_t *command,
		const ralWebGpuCommandReceipt_t *recording,
		const ralWebGpuIndexedDraw_t *draw,
		ralWebGpuCommandReceipt_t *outRecording ) {
	uint64_t digest;
	if ( !command || !recording || !draw || !outRecording || !command->active
			|| command->passKind != RAL_WEBGPU_PASS_RENDER
			|| !RalWebGpu_CommandReceiptExact( recording, recording )
			|| ( recording->command.state != RAL_COMMAND_RECORDING
				&& recording->command.state != RAL_COMMAND_EXECUTABLE )
			|| recording->operationDigest != command->operationDigest
			|| recording->drawCount != command->drawCount
			|| recording->dispatchCount != command->dispatchCount
			|| draw->kind < RAL_WEBGPU_DRAW_WORLD || draw->kind > RAL_WEBGPU_DRAW_UI
			|| !draw->pipelineIdentity || !draw->vertexBufferIdentity
			|| !draw->indexBufferIdentity
			|| ( draw->textured != qtrue && draw->textured != qfalse )
			|| ( draw->textured
				&& ( !draw->textureIdentity || !draw->samplerIdentity ) )
			|| ( !draw->textured
				&& ( draw->textureIdentity || draw->secondaryTextureIdentity
					|| draw->samplerIdentity ) )
			|| ( !draw->textured && draw->kind != RAL_WEBGPU_DRAW_EFFECT )
			|| !draw->indexCount || !draw->instanceCount
			|| draw->firstIndex > UINT32_MAX - draw->indexCount
			|| draw->bindGroupCount > RAL_SHADER_ABI_MAX_BIND_GROUPS
			|| !draw->contentDigest || command->drawCount == UINT32_MAX ) return qfalse;
	for ( uint32_t i = 0u; i < draw->bindGroupCount; ++i )
		if ( !draw->bindGroupIdentities[i] ) return qfalse;
	digest = command->operationDigest;
	digest ^= (uint64_t)draw->kind; digest *= UINT64_C(1099511628211);
	digest ^= draw->pipelineIdentity; digest *= UINT64_C(1099511628211);
	digest ^= draw->vertexBufferIdentity; digest *= UINT64_C(1099511628211);
	digest ^= draw->indexBufferIdentity; digest *= UINT64_C(1099511628211);
	digest ^= draw->textureIdentity; digest *= UINT64_C(1099511628211);
	digest ^= draw->secondaryTextureIdentity; digest *= UINT64_C(1099511628211);
	digest ^= draw->samplerIdentity; digest *= UINT64_C(1099511628211);
	digest ^= (uint64_t)draw->textured; digest *= UINT64_C(1099511628211);
	digest ^= ( (uint64_t)draw->firstIndex << 32u ) | draw->indexCount;
	digest *= UINT64_C(1099511628211); digest ^= draw->instanceCount;
	digest *= UINT64_C(1099511628211); digest ^= draw->firstInstance;
	digest *= UINT64_C(1099511628211); digest ^= draw->contentDigest;
	for ( uint32_t i = 0u; i < draw->bindGroupCount; ++i ) {
		digest *= UINT64_C(1099511628211);
		digest ^= draw->bindGroupIdentities[i];
	}
	digest *= UINT64_C(1099511628211); digest ^= draw->bindGroupCount;
	digest *= UINT64_C(1099511628211); if ( !digest ) digest = 1u;
	if ( !command->host.recordIndexedDraw( command->userData,
			command->passIdentity, draw ) ) return qfalse;
	command->operationDigest = digest; command->drawCount++;
	BuildCommandReceipt( command, &recording->command, outRecording );
	return qtrue;
}

qboolean RalWebGpu_CommandCancel( ralWebGpuCommand_t *command,
		const ralWebGpuCommandReceipt_t *recording ) {
	if ( !command || !recording || !command->active
			|| !RalWebGpu_CommandReceiptExact( recording, recording )
			|| recording->command.state != RAL_COMMAND_RECORDING
			|| recording->operationDigest != command->operationDigest
			|| recording->drawCount != command->drawCount ) return qfalse;
	if ( recording->dispatchCount != command->dispatchCount ) return qfalse;
	ReleaseNative( command ); command->active = qfalse;
	memset( &command->lifecycle, 0, sizeof( command->lifecycle ) );
	command->drawCount = 0u; command->operationDigest = 0u;
	command->dispatchCount = 0u;
	return qtrue;
}

qboolean RalWebGpu_CommandEnd( ralWebGpuCommand_t *command,
		const ralWebGpuCommandReceipt_t *recording,
		ralWebGpuCommandReceipt_t *outExecutable ) {
	ralCommandReceipt_t executable;
	uintptr_t commandBuffer = (uintptr_t)0;
	if ( !command || !recording || !outExecutable || !command->active
			|| command->commandBufferIdentity || command->submissionIdentity
			|| !RalWebGpu_CommandReceiptExact( recording, recording )
			|| recording->backendGeneration != command->coreReceipt.generation
			|| recording->operationDigest != command->operationDigest
			|| recording->drawCount != command->drawCount
			|| recording->dispatchCount != command->dispatchCount
			|| recording->command.state != RAL_COMMAND_RECORDING ) return qfalse;
	if ( !command->host.endPass( command->userData, command->passIdentity )
			|| !command->host.finishEncoder( command->userData,
				command->encoderIdentity, &commandBuffer ) || !commandBuffer ) {
		ReleaseNative( command ); command->active = qfalse; return qfalse;
	}
	if ( Ral_CommandLifecyclePublishEnd( &command->lifecycle,
			&recording->command, &executable ) != ralSuccess ) {
		command->host.releaseObject( command->userData,
			RAL_WEBGPU_COMMAND_OBJECT_BUFFER, commandBuffer );
		ReleaseNative( command ); command->active = qfalse; return qfalse;
	}
	command->commandBufferIdentity = commandBuffer;
	BuildCommandReceipt( command, &executable, outExecutable ); return qtrue;
}

qboolean RalWebGpu_CommandSubmit( ralWebGpuCommand_t *command,
		const ralWebGpuCommandReceipt_t *executable,
		ralWebGpuSubmissionReceipt_t *outSubmission ) {
	ralCommandLifecycle_t *commands[1];
	ralSubmissionReceipt_t submission;
	uintptr_t ticket = (uintptr_t)0;
	uint64_t nextSubmission;
	if ( !command || !executable || !outSubmission || !command->active
			|| command->submissionIdentity || !command->commandBufferIdentity
			|| !RalWebGpu_CommandReceiptExact( executable, executable )
			|| executable->command.state != RAL_COMMAND_EXECUTABLE
			|| executable->operationDigest != command->operationDigest
			|| command->submissionLifecycle.generation == UINT64_MAX ) return qfalse;
	nextSubmission = command->submissionLifecycle.generation + 1u;
	if ( !command->host.submit( command->userData,
			command->coreReceipt.queueIdentity, command->commandBufferIdentity,
			nextSubmission, &ticket ) || !ticket ) return qfalse;
	commands[0] = &command->lifecycle;
	if ( Ral_SubmissionLifecyclePublish( &command->submissionLifecycle,
			commands, &executable->command, 1u, &submission ) != ralSuccess
			|| submission.generation != nextSubmission ) {
		command->host.releaseObject( command->userData,
			RAL_WEBGPU_COMMAND_OBJECT_SUBMISSION, ticket ); return qfalse;
	}
	command->submissionIdentity = ticket; command->liveSubmission = submission;
	memset( outSubmission, 0, sizeof( *outSubmission ) );
	outSubmission->schemaVersion = RAL_WEBGPU_COMMAND_SCHEMA_VERSION;
	outSubmission->backendType = RAL_BACKEND_WEBGPU;
	outSubmission->backendGeneration = command->coreReceipt.generation;
	outSubmission->operationDigest = command->operationDigest;
	outSubmission->submission = submission; outSubmission->ready = qtrue;
	return qtrue;
}

ralWebGpuAsyncStatus_t RalWebGpu_CommandPoll( ralWebGpuCommand_t *command,
		const ralWebGpuSubmissionReceipt_t *submission ) {
	ralWebGpuAsyncStatus_t status = RAL_WEBGPU_ASYNC_FAILED;
	if ( !command || !submission || !command->submissionIdentity
			|| !RalWebGpu_SubmissionReceiptExact( submission, submission )
			|| !Ral_SubmissionReceiptExact( &submission->submission,
				&command->liveSubmission ) ) return RAL_WEBGPU_ASYNC_FAILED;
	if ( !RalWebGpu_CoreMatchesReceipt( command->core, &command->coreReceipt ) ) {
		ReleaseNative( command ); command->active = qfalse;
		return RAL_WEBGPU_ASYNC_DEVICE_LOST;
	}
	if ( !command->host.pollSubmission( command->userData,
			command->submissionIdentity, submission->submission.generation,
			&status ) ) status = RAL_WEBGPU_ASYNC_FAILED;
	if ( status == RAL_WEBGPU_ASYNC_PENDING ) return status;
	if ( status == RAL_WEBGPU_ASYNC_READY
			&& Ral_CommandLifecycleRecycle( &command->lifecycle,
				&submission->submission.commands[0] ) != ralSuccess )
		status = RAL_WEBGPU_ASYNC_FAILED;
	ReleaseNative( command ); memset( &command->liveSubmission, 0,
		sizeof( command->liveSubmission ) ); command->active = qfalse;
	return status;
}

qboolean RalWebGpu_CommandReceiptExact( const ralWebGpuCommandReceipt_t *a,
		const ralWebGpuCommandReceipt_t *b ) {
	return CommandValid( a ) && CommandValid( b )
		&& a->backendGeneration == b->backendGeneration
		&& a->passKind == b->passKind && a->targetIdentity == b->targetIdentity
		&& a->operationDigest == b->operationDigest
		&& a->drawCount == b->drawCount
		&& a->dispatchCount == b->dispatchCount
		&& Ral_CommandReceiptExact( &a->command, &b->command );
}

qboolean RalWebGpu_SubmissionReceiptExact(
		const ralWebGpuSubmissionReceipt_t *a,
		const ralWebGpuSubmissionReceipt_t *b ) {
	return SubmissionValid( a ) && SubmissionValid( b )
		&& a->backendGeneration == b->backendGeneration
		&& a->operationDigest == b->operationDigest
		&& Ral_SubmissionReceiptExact( &a->submission, &b->submission );
}
