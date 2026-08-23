// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Backend-neutral, generation-bound readback ownership.

#ifndef WIRED_RAL_READBACK_H
#define WIRED_RAL_READBACK_H

#include "ral_allocation.h"
#include "ral_buffer_map.h"
#include "ral_transfer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_READBACK_SCHEMA_VERSION 1u

typedef enum {
	RAL_READBACK_ROLE_STAGING = 1,
	RAL_READBACK_ROLE_SUBMISSION
} ralReadbackOwnedRole_t;

typedef struct {
	uint32_t schemaVersion;
	ralTransferReceipt_t transfer;
	ralAllocationReceipt_t stagingAllocation;
	uintptr_t stagingIdentity;
	uintptr_t submissionIdentity;
	qboolean ready;
} ralReadbackReceipt_t;

typedef struct ralReadbackOwner_s ralReadbackOwner_t;

// Every callback is output-atomic. A false submit result means no GPU work was
// published and no submission identity needs retirement. `candidateAllowed`
// is the typed ownership oracle for newly-created staging/submission objects.
typedef struct {
	qboolean (*createStaging)( void *context, uint64_t bytes,
		ralBuffer_t **outBuffer, ralAllocationReceipt_t *outAllocation );
	qboolean (*submitCopy)( void *context, const ralTransferRequest_t *request,
		ralBuffer_t *staging, ralTransferOutcome_t *outOutcome,
		uintptr_t *outSubmissionIdentity, uint64_t *outSubmissionGeneration );
	qboolean (*submissionCompleted)( void *context, uintptr_t submissionIdentity,
		qboolean *outCompleted );
	qboolean (*submissionWait)( void *context, uintptr_t submissionIdentity );
	ralResult_t (*mapBegin)( void *context, ralBuffer_t *staging,
		const ralBufferMapRequest_t *request, ralBufferMapTicket_t *outTicket );
	ralResult_t (*mapPoll)( void *context, ralBuffer_t *staging,
		const ralBufferMapTicket_t *authority, ralBufferMapTicket_t *outTicket );
	ralResult_t (*mapUnmap)( void *context, ralBuffer_t *staging,
		const ralBufferMapTicket_t *readyTicket );
	ralResult_t (*mapCancel)( void *context, ralBuffer_t *staging,
		const ralBufferMapTicket_t *pendingTicket );
	qboolean (*candidateAllowed)( void *context, ralReadbackOwnedRole_t role,
		uintptr_t identity );
	void (*retireStaging)( void *context, ralBuffer_t *staging );
	void (*retireSubmission)( void *context, uintptr_t submissionIdentity );
	void (*destroyContext)( void *context );
} ralReadbackOps_t;

typedef struct {
	ralTransferRequest_t transfer;
	uint64_t transferGeneration;
	void *context;
	const ralReadbackOps_t *ops;
} ralReadbackCreateInfo_t;

qboolean Ral_ReadbackReceiptExact( const ralReadbackReceipt_t *a,
	const ralReadbackReceipt_t *b );
qboolean Ral_ReadbackCreate( const ralReadbackCreateInfo_t *createInfo,
	ralReadbackOwner_t **outOwner );
qboolean Ral_ReadbackGetReceipt( const ralReadbackOwner_t *owner,
	ralReadbackReceipt_t *outReceipt );
qboolean Ral_ReadbackComplete( ralReadbackOwner_t *owner );
qboolean Ral_ReadbackWait( ralReadbackOwner_t *owner );
ralResult_t Ral_ReadbackMapBegin( ralReadbackOwner_t *owner,
	ralBufferMapTicket_t *outTicket );
ralResult_t Ral_ReadbackMapPoll( ralReadbackOwner_t *owner,
	const ralBufferMapTicket_t *authority, ralBufferMapTicket_t *outTicket );
ralResult_t Ral_ReadbackMapUnmap( ralReadbackOwner_t *owner,
	const ralBufferMapTicket_t *readyTicket );
ralResult_t Ral_ReadbackMapCancel( ralReadbackOwner_t *owner,
	const ralBufferMapTicket_t *pendingTicket );
qboolean Ral_ReadbackRelease( ralReadbackOwner_t **owner );

typedef struct {
	uint32_t mipLevel;
	uint32_t arrayLayer;
	uint32_t z;
	uint32_t aspects; // one explicit RAL_TEXTURE_ASPECT_* plane, or zero for legacy inference
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
} ralTextureReadbackRegion_t;

// Backend adapters. Both begin functions publish an asynchronous owner or
// leave `outOwner` unchanged. The source remains caller-owned throughout.
qboolean Ral_BufferReadbackBegin( ralBuffer_t *source, uint64_t offset,
	uint64_t size, ralReadbackOwner_t **outOwner );
qboolean Ral_TextureReadbackBegin( ralTexture_t *source,
	const ralTextureReadbackRegion_t *region, ralReadbackOwner_t **outOwner );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_READBACK_H
