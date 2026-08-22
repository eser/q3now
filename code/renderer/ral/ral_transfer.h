// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_TRANSFER_H
#define WIRED_RAL_TRANSFER_H

#include "ral_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_TRANSFER_SCHEMA_VERSION 1u
#define RAL_BUFFER_UPLOAD_RECEIPT_SCHEMA_VERSION 1u

typedef enum { RAL_TRANSFER_UPLOAD = 1, RAL_TRANSFER_READBACK } ralTransferDirection_t;
typedef enum { RAL_TRANSFER_BUFFER = 1, RAL_TRANSFER_TEXTURE } ralTransferResourceKind_t;
typedef enum {
	RAL_TRANSFER_PREPARED = 1,
	RAL_TRANSFER_SUBMITTED,
	RAL_TRANSFER_COMPLETED,
	RAL_TRANSFER_CANCELED
} ralTransferState_t;
typedef enum {
	RAL_TRANSFER_OUTCOME_NONE = 0,
	RAL_TRANSFER_OUTCOME_SYNCHRONOUS,
	RAL_TRANSFER_OUTCOME_NATIVE_ASYNC,
	RAL_TRANSFER_OUTCOME_MANAGED_ASYNC
} ralTransferOutcome_t;

typedef struct {
	ralBackendType_t backendType;
	ralTransferDirection_t direction;
	ralTransferResourceKind_t resourceKind;
	uintptr_t resourceIdentity;
	uint64_t resourceGeneration;
	uint64_t byteOffset;
	uint64_t byteSize;
	uint64_t byteBudget;
	uint32_t mipLevel;
	uint32_t arrayLayer;
	uint32_t offsetX;
	uint32_t offsetY;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	// WebGPU-shaped buffer/texture layout. Texture readbacks always publish an
	// explicit 256-byte-aligned row pitch; buffer transfers keep both fields 0.
	// Legacy texture uploads may keep both 0 until their staging paths migrate.
	uint32_t bytesPerRow;
	uint32_t rowsPerImage;
	ralQueueType_t queue;
} ralTransferRequest_t;

typedef struct {
	uint32_t schemaVersion;
	ralTransferRequest_t request;
	ralTransferState_t state;
	ralTransferOutcome_t outcome;
	uint64_t transferGeneration;
	uint64_t submissionGeneration;
	uint64_t completionGeneration;
	qboolean ready;
} ralTransferReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	ralTransferReceipt_t transfer;
	uint64_t graphicsVisibilityGeneration;
	qboolean ready;
} ralBufferUploadReceipt_t;

qboolean Ral_TransferPrepare( const ralTransferRequest_t *request,
	uint64_t transferGeneration, ralTransferReceipt_t *out );
qboolean Ral_TransferPublish( const ralTransferReceipt_t *prepared,
	ralTransferOutcome_t outcome, uint64_t submissionGeneration,
	ralTransferReceipt_t *out );
qboolean Ral_TransferComplete( const ralTransferReceipt_t *submitted,
	uint64_t completionGeneration, qboolean fenceCompleted,
	ralTransferReceipt_t *out );
qboolean Ral_TransferCancel( const ralTransferReceipt_t *prepared,
	ralTransferReceipt_t *out );
qboolean Ral_TransferReceiptExact( const ralTransferReceipt_t *a,
	const ralTransferReceipt_t *b );
qboolean Ral_BufferUploadReceiptBuild( const ralTransferReceipt_t *completed,
	uint64_t graphicsVisibilityGeneration, ralBufferUploadReceipt_t *out );
qboolean Ral_BufferUploadReceiptExact( const ralBufferUploadReceipt_t *a,
	const ralBufferUploadReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
