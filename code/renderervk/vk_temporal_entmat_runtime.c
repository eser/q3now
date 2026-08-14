// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_entmat_runtime.h"
#include "../renderer/ral_vulkan/ral_vulkan_bridge.h"

#include <string.h>

static ralBuffer_t *RuntimeAdopt( void *userData, ralBackend_t *backend,
		void *nativeBuffer, size_t size, const char *debugName ) {
	ralBuffer_t *adopted;
	(void)userData;
	adopted = Ral_AdoptBuffer( backend, nativeBuffer, size, debugName );
	if ( adopted && Ral_GetBufferHandle( adopted ) != nativeBuffer ) {
		Ral_DestroyBuffer( adopted );
		return NULL;
	}
	return adopted;
}

static void RuntimeDestroy( void *userData, ralBuffer_t *adopted ) {
	(void)userData;
	Ral_DestroyBuffer( adopted );
}

static qboolean RuntimeRebind( void *userData, uint32_t frameIndex,
		const ralBuffer_t *adopted, uint32_t allocationGeneration ) {
	vkTemporalEntMatRuntime_t *runtime =
		(vkTemporalEntMatRuntime_t *)userData;
	return R_TemporalMotionPayloadEnsure( &runtime->payload,
		runtime->requestedBackend, runtime->adoption.frameCount, frameIndex,
		runtime->requestedCapacity, adopted, allocationGeneration );
}

static qboolean RuntimeDetach( void *userData, uint32_t frameIndex,
		const ralBuffer_t *adopted, uint32_t allocationGeneration ) {
	vkTemporalEntMatRuntime_t *runtime =
		(vkTemporalEntMatRuntime_t *)userData;
	return R_TemporalMotionPayloadDetachEntityBuffer( &runtime->payload,
		frameIndex, adopted, allocationGeneration );
}

static vkTemporalEntMatAdoptionOps_t RuntimeOps(
		vkTemporalEntMatRuntime_t *runtime ) {
	vkTemporalEntMatAdoptionOps_t ops;
	memset( &ops, 0, sizeof( ops ) );
	ops.adopt = RuntimeAdopt;
	ops.destroy = RuntimeDestroy;
	ops.consumerRebind = RuntimeRebind;
	ops.consumerDetach = RuntimeDetach;
	ops.userData = runtime;
	return ops;
}

void VK_TemporalEntMatRuntimeInit( vkTemporalEntMatRuntime_t *runtime ) {
	if ( !runtime ) return;
	memset( runtime, 0, sizeof( *runtime ) );
	VK_TemporalEntMatAdoptionInit( &runtime->adoption );
	R_TemporalMotionPayloadInit( &runtime->payload );
	runtime->initialized = qtrue;
}

qboolean VK_TemporalEntMatRuntimeEnsureAfterFence(
		vkTemporalEntMatRuntime_t *runtime, ralBackend_t *backend,
		uint32_t frameCount, uint32_t frameIndex, void *nativeBuffer,
		size_t size, uint32_t allocationGeneration, uint32_t capacity ) {
	vkTemporalEntMatAdoptionOps_t ops;
	qboolean payloadWasReady;
	if ( !runtime || !runtime->initialized || !backend || !nativeBuffer
			|| !size || !allocationGeneration
			|| allocationGeneration == UINT32_MAX || !capacity
			|| capacity > TEMPORAL_MOTION_PAYLOAD_MAX_SLOTS
			|| frameCount == 0 || frameCount > VK_TEMPORAL_ENTMAT_MAX_SLOTS
			|| frameCount > TEMPORAL_MOTION_PAYLOAD_MAX_FRAMES
			|| frameIndex >= frameCount ) return qfalse;
	payloadWasReady = runtime->payload.ready
		&& frameIndex < runtime->payload.frameCount
		&& runtime->payload.frames[frameIndex].ready;
	if ( !VK_TemporalEntMatAdoptionResetAfterFence( &runtime->adoption,
			frameCount, frameIndex ) ) return qfalse;
	if ( payloadWasReady && !R_TemporalMotionPayloadResetAfterFence(
			&runtime->payload, frameIndex ) ) return qfalse;
	runtime->requestedBackend = backend;
	runtime->requestedCapacity = capacity;
	ops = RuntimeOps( runtime );
	if ( !VK_TemporalEntMatAdoptionEnsure( &runtime->adoption, backend,
			frameIndex, nativeBuffer, size, allocationGeneration, &ops ) ) {
		runtime->requestedBackend = NULL;
		runtime->requestedCapacity = 0;
		return qfalse;
	}
	runtime->requestedBackend = NULL;
	runtime->requestedCapacity = 0;
	return qtrue;
}

qboolean VK_TemporalEntMatRuntimeHasLive(
		const vkTemporalEntMatRuntime_t *runtime ) {
	uint32_t i;
	if ( !runtime || !runtime->initialized ) return qfalse;
	if ( runtime->payload.ready ) return qtrue;
	for ( i = 0; i < runtime->adoption.frameCount; ++i ) {
		if ( runtime->adoption.slots[i].ready
				&& runtime->adoption.slots[i].adopted ) return qtrue;
	}
	return qfalse;
}

qboolean VK_TemporalEntMatRuntimeReleaseAfterIdle(
		vkTemporalEntMatRuntime_t *runtime, qboolean idleProven ) {
	vkTemporalEntMatAdoptionOps_t ops;
	uint32_t i;
	if ( !runtime || !runtime->initialized || !idleProven ) return qfalse;
	if ( !runtime->adoption.configured ) return qtrue;
	for ( i = 0; i < runtime->payload.frameCount; ++i ) {
		if ( runtime->payload.frames[i].ready
				&& !R_TemporalMotionPayloadResetAfterFence(
					&runtime->payload, i ) ) return qfalse;
	}
	ops = RuntimeOps( runtime );
	if ( !VK_TemporalEntMatAdoptionReleaseAfterIdle( &runtime->adoption,
			qtrue, &ops ) ) return qfalse;
	R_TemporalMotionPayloadRelease( &runtime->payload );
	runtime->requestedBackend = NULL;
	runtime->requestedCapacity = 0;
	return qtrue;
}
