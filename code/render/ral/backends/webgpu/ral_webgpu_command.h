// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_WEBGPU_COMMAND_H
#define WIRED_RAL_WEBGPU_COMMAND_H

#include "ral_command_lifecycle.h"
#include "ral_shader_abi.h"
#include "ral_webgpu_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_WEBGPU_COMMAND_SCHEMA_VERSION 1u

typedef struct ralWebGpuCommand_s ralWebGpuCommand_t;

typedef enum {
	RAL_WEBGPU_PASS_RENDER = 1,
	RAL_WEBGPU_PASS_COMPUTE
} ralWebGpuPassKind_t;

typedef enum {
	RAL_WEBGPU_COMMAND_OBJECT_ENCODER = 1,
	RAL_WEBGPU_COMMAND_OBJECT_PASS,
	RAL_WEBGPU_COMMAND_OBJECT_BUFFER,
	RAL_WEBGPU_COMMAND_OBJECT_SUBMISSION
} ralWebGpuCommandObjectKind_t;

typedef qboolean ( *ralWebGpuBeginEncoderFn )( void *userData,
	uintptr_t deviceIdentity, uintptr_t *outEncoderIdentity );
typedef qboolean ( *ralWebGpuBeginPassFn )( void *userData,
	uintptr_t encoderIdentity, ralWebGpuPassKind_t kind,
	uintptr_t targetIdentity, uintptr_t *outPassIdentity );
typedef qboolean ( *ralWebGpuEndPassFn )( void *userData,
	uintptr_t passIdentity );
typedef enum {
	RAL_WEBGPU_DRAW_WORLD = 1,
	RAL_WEBGPU_DRAW_ENTITY,
	RAL_WEBGPU_DRAW_EFFECT,
	RAL_WEBGPU_DRAW_UI
} ralWebGpuDrawKind_t;
typedef struct {
	ralWebGpuDrawKind_t kind;
	uintptr_t pipelineIdentity;
	uintptr_t vertexBufferIdentity;
	uintptr_t indexBufferIdentity;
	uintptr_t textureIdentity;
	uintptr_t secondaryTextureIdentity;
	uintptr_t samplerIdentity;
	qboolean textured;
	uint32_t firstIndex;
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t firstInstance;
	uint64_t contentDigest;
	uintptr_t bindGroupIdentities[RAL_SHADER_ABI_MAX_BIND_GROUPS];
	uint32_t bindGroupCount;
} ralWebGpuIndexedDraw_t;
typedef qboolean ( *ralWebGpuRecordIndexedDrawFn )( void *userData,
	uintptr_t passIdentity, const ralWebGpuIndexedDraw_t *draw );
typedef struct {
	uintptr_t pipelineIdentity;
	uintptr_t bindGroupIdentities[RAL_SHADER_ABI_MAX_BIND_GROUPS];
	uint32_t bindGroupCount;
	uint32_t groupCountX;
	uint32_t groupCountY;
	uint32_t groupCountZ;
	uint64_t contentDigest;
} ralWebGpuComputeDispatch_t;
typedef qboolean ( *ralWebGpuRecordComputeDispatchFn )( void *userData,
	uintptr_t passIdentity, const ralWebGpuComputeDispatch_t *dispatch );
typedef qboolean ( *ralWebGpuFinishEncoderFn )( void *userData,
	uintptr_t encoderIdentity, uintptr_t *outCommandBufferIdentity );
typedef qboolean ( *ralWebGpuSubmitCommandFn )( void *userData,
	uintptr_t queueIdentity, uintptr_t commandBufferIdentity,
	uint64_t submissionGeneration, uintptr_t *outSubmissionIdentity );
typedef qboolean ( *ralWebGpuPollSubmissionFn )( void *userData,
	uintptr_t submissionIdentity, uint64_t submissionGeneration,
	ralWebGpuAsyncStatus_t *outStatus );
typedef void ( *ralWebGpuReleaseCommandObjectFn )( void *userData,
	ralWebGpuCommandObjectKind_t kind, uintptr_t identity );

typedef struct {
	ralWebGpuBeginEncoderFn beginEncoder;
	ralWebGpuBeginPassFn beginPass;
	ralWebGpuRecordIndexedDrawFn recordIndexedDraw;
	ralWebGpuRecordComputeDispatchFn recordComputeDispatch;
	ralWebGpuEndPassFn endPass;
	ralWebGpuFinishEncoderFn finishEncoder;
	ralWebGpuSubmitCommandFn submit;
	ralWebGpuPollSubmissionFn pollSubmission;
	ralWebGpuReleaseCommandObjectFn releaseObject;
} ralWebGpuCommandHostOps_t;

typedef struct {
	void *userData;
	ralWebGpuCommandHostOps_t host;
} ralWebGpuCommandCreateInfo_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t backendGeneration;
	ralWebGpuPassKind_t passKind;
	uintptr_t targetIdentity;
	uint64_t operationDigest;
	uint32_t drawCount;
	uint32_t dispatchCount;
	ralCommandReceipt_t command;
	qboolean ready;
} ralWebGpuCommandReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t backendGeneration;
	uint64_t operationDigest;
	ralSubmissionReceipt_t submission;
	qboolean ready;
} ralWebGpuSubmissionReceipt_t;

qboolean RalWebGpu_CommandCreate( ralWebGpuCore_t *core,
	const ralWebGpuCoreReceipt_t *coreReceipt,
	const ralWebGpuCommandCreateInfo_t *createInfo,
	ralWebGpuCommand_t **outCommand );
void RalWebGpu_CommandDestroy( ralWebGpuCommand_t *command );
qboolean RalWebGpu_CommandBegin( ralWebGpuCommand_t *command,
	ralWebGpuPassKind_t kind, uintptr_t targetIdentity,
	ralWebGpuCommandReceipt_t *outRecording );
qboolean RalWebGpu_CommandEnd( ralWebGpuCommand_t *command,
	const ralWebGpuCommandReceipt_t *recording,
	ralWebGpuCommandReceipt_t *outExecutable );
qboolean RalWebGpu_CommandRecordIndexedDraw( ralWebGpuCommand_t *command,
	const ralWebGpuCommandReceipt_t *recording,
	const ralWebGpuIndexedDraw_t *draw,
	ralWebGpuCommandReceipt_t *outRecording );
qboolean RalWebGpu_CommandRecordComputeDispatch( ralWebGpuCommand_t *command,
	const ralWebGpuCommandReceipt_t *recording,
	const ralWebGpuComputeDispatch_t *dispatch,
	ralWebGpuCommandReceipt_t *outRecording );
qboolean RalWebGpu_CommandCancel( ralWebGpuCommand_t *command,
	const ralWebGpuCommandReceipt_t *recording );
qboolean RalWebGpu_CommandSubmit( ralWebGpuCommand_t *command,
	const ralWebGpuCommandReceipt_t *executable,
	ralWebGpuSubmissionReceipt_t *outSubmission );
ralWebGpuAsyncStatus_t RalWebGpu_CommandPoll( ralWebGpuCommand_t *command,
	const ralWebGpuSubmissionReceipt_t *submission );
qboolean RalWebGpu_CommandReceiptExact( const ralWebGpuCommandReceipt_t *a,
	const ralWebGpuCommandReceipt_t *b );
qboolean RalWebGpu_SubmissionReceiptExact(
	const ralWebGpuSubmissionReceipt_t *a,
	const ralWebGpuSubmissionReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
