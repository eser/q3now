// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_presentation.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); return 1; } } while ( 0 )

typedef struct {
	uintptr_t nextIdentity;
	uint32_t created[5], released[5];
	uint32_t pendingPolls;
	uint32_t configureCount, unconfigureCount, acquireCount, presentCount;
	uint32_t pixelWidth, pixelHeight;
	uint64_t submissionGeneration;
	uintptr_t submissionIdentity;
	qboolean failFinish;
	qboolean failDraw;
	uint32_t drawCount, dispatchCount;
} fakeHost_t;

static qboolean BeginAdapter( void *user, uint64_t generation ) {
	(void)user; return generation ? qtrue : qfalse;
}
static qboolean PollAdapter( void *user, uint64_t generation,
		ralWebGpuAdapterPoll_t *out ) {
	(void)user; (void)generation; memset( out, 0, sizeof( *out ) );
	out->status = RAL_WEBGPU_REQUEST_READY; out->adapterIdentity = 0x2000u;
	out->adapterType = RAL_ADAPTER_TYPE_DISCRETE;
	out->vendorName = "Wired"; out->deviceName = "WebGPU command fixture";
	out->limits.maxColorAttachments = 8u;
	out->limits.maxTextureDimension2D = 8192u;
	out->limits.maxTextureDimension3D = 2048u;
	out->limits.maxTextureArrayLayers = 2048u;
	out->limits.maxComputeInvocationsPerWorkgroup = 256u;
	out->limits.maxSampledTexturesPerShaderStage = 32u;
	out->limits.maxBindGroups = 4u;
	out->limits.maxBindingsPerBindGroup = 16u;
	out->limits.maxStorageBufferBindingSize = UINT64_C(134217728);
	out->limits.minUniformBufferOffsetAlignment = 256u;
	out->limits.minStorageBufferOffsetAlignment = 256u;
	return qtrue;
}
static qboolean BeginDevice( void *user, uintptr_t adapter, uint64_t generation ) {
	(void)user; return adapter == 0x2000u && generation > 1u;
}
static qboolean PollDevice( void *user, uint64_t generation,
		ralWebGpuDevicePoll_t *out ) {
	(void)user; (void)generation; memset( out, 0, sizeof( *out ) );
	out->status = RAL_WEBGPU_REQUEST_READY;
	out->deviceIdentity = 0x3000u; out->queueIdentity = 0x4000u; return qtrue;
}
static void ReleaseDevice( void *user, uintptr_t device, uintptr_t queue ) {
	(void)user; (void)device; (void)queue;
}
static void ReleaseAdapter( void *user, uintptr_t adapter ) {
	(void)user; (void)adapter;
}

static uintptr_t NewIdentity( fakeHost_t *host,
		ralWebGpuCommandObjectKind_t kind ) {
	host->created[kind]++; return ++host->nextIdentity;
}
static qboolean BeginEncoder( void *user, uintptr_t device, uintptr_t *out ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( device != 0x3000u ) return qfalse;
	*out = NewIdentity( host, RAL_WEBGPU_COMMAND_OBJECT_ENCODER ); return qtrue;
}
static qboolean BeginPass( void *user, uintptr_t encoder,
		ralWebGpuPassKind_t kind, uintptr_t target, uintptr_t *out ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( !encoder || kind < RAL_WEBGPU_PASS_RENDER
			|| kind > RAL_WEBGPU_PASS_COMPUTE
			|| ( kind == RAL_WEBGPU_PASS_RENDER && !target )
			|| ( kind == RAL_WEBGPU_PASS_COMPUTE && target ) ) return qfalse;
	*out = NewIdentity( host, RAL_WEBGPU_COMMAND_OBJECT_PASS ); return qtrue;
}
static qboolean RecordDraw( void *user, uintptr_t pass,
		const ralWebGpuIndexedDraw_t *draw ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( !pass || !draw || host->failDraw ) return qfalse;
	host->drawCount++; return qtrue;
}
static qboolean RecordDispatch( void *user, uintptr_t pass,
		const ralWebGpuComputeDispatch_t *dispatch ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( !pass || !dispatch ) return qfalse;
	host->dispatchCount++; return qtrue;
}
static qboolean EndPass( void *user, uintptr_t pass ) {
	(void)user; return pass ? qtrue : qfalse;
}
static qboolean FinishEncoder( void *user, uintptr_t encoder, uintptr_t *out ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( !encoder || host->failFinish ) return qfalse;
	*out = NewIdentity( host, RAL_WEBGPU_COMMAND_OBJECT_BUFFER ); return qtrue;
}
static qboolean Submit( void *user, uintptr_t queue, uintptr_t buffer,
		uint64_t generation, uintptr_t *out ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( queue != 0x4000u || !buffer || !generation ) return qfalse;
	host->submissionGeneration = generation;
	host->submissionIdentity = NewIdentity( host,
		RAL_WEBGPU_COMMAND_OBJECT_SUBMISSION );
	*out = host->submissionIdentity; return qtrue;
}
static qboolean PollSubmission( void *user, uintptr_t submission,
		uint64_t generation, ralWebGpuAsyncStatus_t *out ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( submission != host->submissionIdentity
			|| generation != host->submissionGeneration ) return qfalse;
	if ( host->pendingPolls ) {
		host->pendingPolls--; *out = RAL_WEBGPU_ASYNC_PENDING; return qtrue;
	}
	*out = RAL_WEBGPU_ASYNC_READY; return qtrue;
}
static void ReleaseCommandObject( void *user,
		ralWebGpuCommandObjectKind_t kind, uintptr_t identity ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( kind >= RAL_WEBGPU_COMMAND_OBJECT_ENCODER
			&& kind <= RAL_WEBGPU_COMMAND_OBJECT_SUBMISSION && identity )
		host->released[kind]++;
}

static qboolean ConfigureCanvas( void *user, uintptr_t canvas, uintptr_t device,
		uint32_t width, uint32_t height, ralFormat_t format,
		ralColorSpace_t colorSpace, qboolean opaque ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( canvas != 0x7000u || device != 0x3000u || !width || !height
			|| format != RAL_FORMAT_B8G8R8A8_UNORM
			|| colorSpace != RAL_COLORSPACE_SRGB_NONLINEAR || opaque != qtrue )
		return qfalse;
	host->configureCount++; host->pixelWidth = width; host->pixelHeight = height;
	return qtrue;
}
static void UnconfigureCanvas( void *user, uintptr_t canvas ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( canvas == 0x7000u ) host->unconfigureCount++;
}
static qboolean AcquireCanvas( void *user, uintptr_t canvas,
		uint64_t generation, uintptr_t *out ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( canvas != 0x7000u || !generation ) return qfalse;
	host->acquireCount++; *out = (uintptr_t)( 0x8000u + generation ); return qtrue;
}
static qboolean PresentCanvas( void *user, uintptr_t canvas, uintptr_t texture,
		uint64_t frameGeneration, uint64_t submissionGeneration ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( canvas != 0x7000u || texture != (uintptr_t)( 0x8000u + frameGeneration )
			|| submissionGeneration != host->submissionGeneration ) return qfalse;
	host->presentCount++; return qtrue;
}

static ralWebGpuCoreCreateInfo_t CoreInfo( fakeHost_t *host ) {
	ralWebGpuCoreCreateInfo_t info;
	memset( &info, 0, sizeof( info ) ); info.generation = 7u; info.userData = host;
	info.host.beginAdapter = BeginAdapter; info.host.pollAdapter = PollAdapter;
	info.host.beginDevice = BeginDevice; info.host.pollDevice = PollDevice;
	info.host.releaseDevice = ReleaseDevice; info.host.releaseAdapter = ReleaseAdapter;
	return info;
}
static ralWebGpuCommandCreateInfo_t CommandInfo( fakeHost_t *host ) {
	ralWebGpuCommandCreateInfo_t info;
	memset( &info, 0, sizeof( info ) ); info.userData = host;
	info.host.beginEncoder = BeginEncoder; info.host.beginPass = BeginPass;
	info.host.recordIndexedDraw = RecordDraw;
	info.host.recordComputeDispatch = RecordDispatch;
	info.host.endPass = EndPass; info.host.finishEncoder = FinishEncoder;
	info.host.submit = Submit; info.host.pollSubmission = PollSubmission;
	info.host.releaseObject = ReleaseCommandObject; return info;
}
static ralWebGpuPresentationCreateInfo_t PresentationInfo( fakeHost_t *host ) {
	ralWebGpuPresentationCreateInfo_t info;
	memset( &info, 0, sizeof( info ) ); info.userData = host;
	info.canvasIdentity = 0x7000u; info.host.configure = ConfigureCanvas;
	info.host.unconfigure = UnconfigureCanvas; info.host.acquire = AcquireCanvas;
	info.host.present = PresentCanvas; return info;
}
static ralMemoryFailureEvent_t DeviceLoss( void ) {
	ralMemoryFailureEvent_t event;
	memset( &event, 0, sizeof( event ) );
	event.backendType = RAL_BACKEND_WEBGPU;
	event.cause = RAL_MEMORY_FAILURE_DEVICE_LOST;
	event.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	event.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	event.criticality = RAL_MEMORY_CRITICALITY_REQUIRED;
	event.requestedBytes = 16u; event.attempt = 1u; event.maxAttempts = 1u;
	event.liveParent = qtrue; return event;
}

int main( void ) {
	fakeHost_t host = { 0 };
	ralWebGpuCoreCreateInfo_t coreInfo;
	ralWebGpuCore_t *core = NULL;
	ralWebGpuCoreReceipt_t coreReceipt;
	ralWebGpuCommandCreateInfo_t commandInfo;
	ralWebGpuCommand_t *command = NULL;
	ralWebGpuPresentationCreateInfo_t presentationInfo;
	ralWebGpuPresentation_t *presentation = NULL;
	ralWebGpuPresentationReceipt_t configured, resized, suspended, stalePresentation;
	ralWebGpuFrameReceipt_t frame, staleFrame, unchangedFrame;
	ralWebGpuCommandReceipt_t recording, updatedRecording, unchangedRecording;
	ralWebGpuCommandReceipt_t executable, staleCommand;
	ralWebGpuIndexedDraw_t draw;
	ralWebGpuComputeDispatch_t dispatch;
	ralWebGpuSubmissionReceipt_t submission;
	ralMemoryFailureEvent_t lossEvent;
	ralMemoryFailureReceipt_t loss;
	host.nextIdentity = 0x9000u; coreInfo = CoreInfo( &host );
	CHECK( RalWebGpu_CoreBegin( &coreInfo, &core ) );
	while ( RalWebGpu_CorePoll( core, &coreReceipt ) == RAL_WEBGPU_POLL_PENDING ) {}
	commandInfo = CommandInfo( &host );
	CHECK( RalWebGpu_CommandCreate( core, &coreReceipt, &commandInfo, &command ) );
	presentationInfo = PresentationInfo( &host );
	CHECK( RalWebGpu_PresentationCreate( core, &coreReceipt, &presentationInfo,
		&presentation ) );
	CHECK( RalWebGpu_PresentationConfigure( presentation, 1280u, 720u,
		RAL_WEBGPU_DPR_ONE_Q16, RAL_FORMAT_B8G8R8A8_UNORM,
		RAL_COLORSPACE_SRGB_NONLINEAR, qtrue, &configured ) );
	CHECK( configured.pixelWidth == 1280u && configured.pixelHeight == 720u );
	CHECK( RalWebGpu_PresentationAcquire( presentation, &configured, &frame ) );
	CHECK( RalWebGpu_CommandBegin( command, RAL_WEBGPU_PASS_RENDER,
		frame.textureIdentity, &recording ) );
	CHECK( RalWebGpu_CommandReceiptExact( &recording, &recording ) );
	memset( &draw, 0, sizeof( draw ) ); draw.kind = RAL_WEBGPU_DRAW_WORLD;
	draw.pipelineIdentity = 0xa001u; draw.vertexBufferIdentity = 0xa002u;
	draw.indexBufferIdentity = 0xa003u; draw.textureIdentity = 0xa004u;
	draw.samplerIdentity = 0xa005u; draw.indexCount = 3u;
	draw.textured = qtrue;
	draw.instanceCount = 1u; draw.contentDigest = 0xa006u;
	memset( &unchangedRecording, 0xa5, sizeof( unchangedRecording ) );
	updatedRecording = unchangedRecording; host.failDraw = qtrue;
	CHECK( !RalWebGpu_CommandRecordIndexedDraw( command, &recording, &draw,
		&updatedRecording ) && !memcmp( &updatedRecording, &unchangedRecording,
			sizeof( updatedRecording ) ) );
	host.failDraw = qfalse;
	CHECK( RalWebGpu_CommandRecordIndexedDraw( command, &recording, &draw,
		&updatedRecording ) );
	CHECK( updatedRecording.drawCount == 1u && host.drawCount == 1u );
	CHECK( !RalWebGpu_CommandEnd( command, &recording, &executable ) );
	recording = updatedRecording;
	CHECK( RalWebGpu_CommandEnd( command, &recording, &executable ) );
	staleCommand = executable; staleCommand.operationDigest++;
	CHECK( !RalWebGpu_CommandSubmit( command, &staleCommand, &submission ) );
	CHECK( RalWebGpu_CommandSubmit( command, &executable, &submission ) );
	CHECK( RalWebGpu_SubmissionReceiptExact( &submission, &submission ) );
	staleFrame = frame; staleFrame.frameGeneration++;
	CHECK( !RalWebGpu_PresentationPresent( presentation, &staleFrame, &submission ) );
	CHECK( RalWebGpu_PresentationPresent( presentation, &frame, &submission ) );
	host.pendingPolls = 1u;
	CHECK( RalWebGpu_CommandPoll( command, &submission ) == RAL_WEBGPU_ASYNC_PENDING );
	CHECK( RalWebGpu_CommandPoll( command, &submission ) == RAL_WEBGPU_ASYNC_READY );

	CHECK( RalWebGpu_CommandBegin( command, RAL_WEBGPU_PASS_COMPUTE,
		0u, &recording ) );
	memset( &dispatch, 0, sizeof( dispatch ) );
	dispatch.pipelineIdentity = 0xb001u;
	dispatch.bindGroupIdentities[0] = 0xb002u;
	dispatch.bindGroupCount = 1u; dispatch.groupCountX = 4u;
	dispatch.groupCountY = 2u; dispatch.groupCountZ = 1u;
	dispatch.contentDigest = 0xb003u;
	CHECK( RalWebGpu_CommandRecordComputeDispatch( command, &recording,
		&dispatch, &updatedRecording ) );
	CHECK( updatedRecording.dispatchCount == 1u && host.dispatchCount == 1u );
	recording = updatedRecording;
	CHECK( RalWebGpu_CommandEnd( command, &recording, &executable ) );
	CHECK( RalWebGpu_CommandSubmit( command, &executable, &submission ) );
	CHECK( RalWebGpu_CommandPoll( command, &submission ) == RAL_WEBGPU_ASYNC_READY );
	CHECK( RalWebGpu_PresentationConfigure( presentation, 640u, 360u,
		2u * RAL_WEBGPU_DPR_ONE_Q16, RAL_FORMAT_B8G8R8A8_UNORM,
		RAL_COLORSPACE_SRGB_NONLINEAR, qtrue, &resized ) );
	CHECK( resized.pixelWidth == 1280u && resized.pixelHeight == 720u );
	stalePresentation = configured;
	memset( &unchangedFrame, 0xa5, sizeof( unchangedFrame ) ); frame = unchangedFrame;
	CHECK( !RalWebGpu_PresentationAcquire( presentation, &stalePresentation, &frame ) );
	CHECK( !memcmp( &frame, &unchangedFrame, sizeof( frame ) ) );
	CHECK( RalWebGpu_PresentationConfigure( presentation, 0u, 0u,
		RAL_WEBGPU_DPR_ONE_Q16, RAL_FORMAT_UNDEFINED,
		RAL_COLORSPACE_SRGB_NONLINEAR, qtrue, &suspended ) );
	CHECK( suspended.suspended == qtrue && suspended.configured == qfalse );
	CHECK( !RalWebGpu_PresentationAcquire( presentation, &suspended, &frame ) );

	CHECK( RalWebGpu_PresentationConfigure( presentation, 1280u, 720u,
		RAL_WEBGPU_DPR_ONE_Q16, RAL_FORMAT_B8G8R8A8_UNORM,
		RAL_COLORSPACE_SRGB_NONLINEAR, qtrue, &configured ) );
	lossEvent = DeviceLoss();
	CHECK( RalWebGpu_CorePublishDeviceLoss( core, &lossEvent, &loss ) );
	CHECK( loss.action == RAL_MEMORY_RECOVERY_RECREATE_BACKEND );
	CHECK( !RalWebGpu_PresentationAcquire( presentation, &configured, &frame ) );
	CHECK( !RalWebGpu_CommandBegin( command, RAL_WEBGPU_PASS_RENDER,
		0x8888u, &recording ) );
	RalWebGpu_CommandDestroy( command );
	RalWebGpu_PresentationDestroy( presentation );
	RalWebGpu_CoreDestroy( core );
	CHECK( host.presentCount == 1u && host.unconfigureCount == 2u );
	CHECK( host.created[RAL_WEBGPU_COMMAND_OBJECT_ENCODER]
		== host.released[RAL_WEBGPU_COMMAND_OBJECT_ENCODER] );
	CHECK( host.created[RAL_WEBGPU_COMMAND_OBJECT_PASS]
		== host.released[RAL_WEBGPU_COMMAND_OBJECT_PASS] );
	CHECK( host.created[RAL_WEBGPU_COMMAND_OBJECT_BUFFER]
		== host.released[RAL_WEBGPU_COMMAND_OBJECT_BUFFER] );
	CHECK( host.created[RAL_WEBGPU_COMMAND_OBJECT_SUBMISSION]
		== host.released[RAL_WEBGPU_COMMAND_OBJECT_SUBMISSION] );
	puts( "ral WebGPU command/canvas presentation contract: PASS" );
	return 0;
}
