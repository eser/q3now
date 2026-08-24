// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Backend-neutral typed buffer mapping. Vulkan can complete Begin immediately;
// WebGPU can publish PENDING and complete it after mapAsync.

#ifndef WIRED_RAL_BUFFER_MAP_H
#define WIRED_RAL_BUFFER_MAP_H

#include "ral_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	RAL_MAP_READ = 1,
	RAL_MAP_WRITE = 2
} ralBufferMapMode_t;

typedef enum {
	RAL_BUFFER_MAP_IDLE = 0,
	RAL_BUFFER_MAP_PENDING,
	RAL_BUFFER_MAP_READY
} ralBufferMapStatus_t;

typedef struct {
	ralBufferMapMode_t mode;
	uint64_t offset;
	uint64_t size;
} ralBufferMapRequest_t;

// Immutable authority returned by a backend map operation. `mappedRange` is
// NULL while PENDING and non-NULL when READY. The buffer and ticket must remain
// alive until exact Unmap or Cancel completes.
typedef struct {
	const ralBuffer_t *bufferIdentity;
	uint64_t generation;
	ralBufferMapRequest_t request;
	ralBufferMapStatus_t status;
	void *mappedRange;
} ralBufferMapTicket_t;

qboolean Ral_BufferMapRequestValid( const ralBufferMapRequest_t *request,
	                                 uint64_t bufferSize );
qboolean Ral_BufferMapTicketValid( const ralBufferMapTicket_t *ticket );
qboolean Ral_BufferMapTicketExact( const ralBufferMapTicket_t *a,
	                                const ralBufferMapTicket_t *b );

// Backend-owned state machine shared by Vulkan and future WebGPU lowering.
// Renderer callers use tickets; they never author this lifecycle directly.
typedef struct {
	const ralBuffer_t *bufferIdentity;
	uint64_t generation;
	ralBufferMapRequest_t request;
	ralBufferMapStatus_t status;
	void *mappedRange;
} ralBufferMapLifecycle_t;

void Ral_BufferMapLifecycleInit( ralBufferMapLifecycle_t *lifecycle );
qboolean Ral_BufferMapLifecycleGpuUseAllowed( const ralBufferMapLifecycle_t *lifecycle );
ralResult_t Ral_BufferMapLifecyclePublishBegin( ralBufferMapLifecycle_t *lifecycle,
	                                             const ralBuffer_t *buffer,
	                                             uint64_t bufferSize,
	                                             const ralBufferMapRequest_t *request,
	                                             qboolean ready,
	                                             void *mappedRange,
	                                             ralBufferMapTicket_t *outTicket );
ralResult_t Ral_BufferMapLifecyclePublishReady( ralBufferMapLifecycle_t *lifecycle,
	                                             const ralBufferMapTicket_t *pendingTicket,
	                                             void *mappedRange,
	                                             ralBufferMapTicket_t *outTicket );
ralResult_t Ral_BufferMapLifecyclePoll( const ralBufferMapLifecycle_t *lifecycle,
	                                    const ralBufferMapTicket_t *authority,
	                                    ralBufferMapTicket_t *outTicket );
ralResult_t Ral_BufferMapLifecycleUnmap( ralBufferMapLifecycle_t *lifecycle,
	                                     const ralBufferMapTicket_t *readyTicket );
ralResult_t Ral_BufferMapLifecycleCancel( ralBufferMapLifecycle_t *lifecycle,
	                                      const ralBufferMapTicket_t *pendingTicket );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_BUFFER_MAP_H
