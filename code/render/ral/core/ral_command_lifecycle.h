// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Native-free command recording and submission authority. Backends publish
// receipts only after their matching encoder/native operation succeeds.

#ifndef WIRED_RAL_COMMAND_LIFECYCLE_H
#define WIRED_RAL_COMMAND_LIFECYCLE_H

#include "ral_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_MAX_SUBMIT_COMMAND_RECEIPTS 16u

typedef enum {
	RAL_COMMAND_IDLE = 0,
	RAL_COMMAND_RECORDING,
	RAL_COMMAND_EXECUTABLE,
	RAL_COMMAND_SUBMITTED
} ralCommandLifecycleState_t;

// Immutable authority for one recording generation. Identities are opaque;
// validating a receipt never dereferences caller-carried pointers.
typedef struct {
	const ralBackend_t *backendIdentity;
	const ralCommandBuffer_t *commandIdentity;
	uint64_t generation;
	ralQueueType_t queue;
	ralCommandLifecycleState_t state;
	qboolean ready;
} ralCommandReceipt_t;

typedef struct {
	const ralBackend_t *backendIdentity;
	const ralCommandBuffer_t *commandIdentity;
	uint64_t generation;
	ralQueueType_t queue;
	ralCommandLifecycleState_t state;
} ralCommandLifecycle_t;

// Exact, bounded receipt for one successful queue submission. Command entries
// are the SUBMITTED receipts published by the same transaction.
typedef struct {
	const ralBackend_t *backendIdentity;
	uint64_t generation;
	ralQueueType_t queue;
	uint32_t commandCount;
	ralCommandReceipt_t commands[ RAL_MAX_SUBMIT_COMMAND_RECEIPTS ];
	qboolean ready;
} ralSubmissionReceipt_t;

typedef struct {
	const ralBackend_t *backendIdentity;
	uint64_t generation;
	ralQueueType_t queue;
} ralSubmissionLifecycle_t;

qboolean Ral_CommandReceiptValid( const ralCommandReceipt_t *receipt );
qboolean Ral_CommandReceiptExact( const ralCommandReceipt_t *a,
	                              const ralCommandReceipt_t *b );
qboolean Ral_SubmissionReceiptValid( const ralSubmissionReceipt_t *receipt );
qboolean Ral_SubmissionReceiptExact( const ralSubmissionReceipt_t *a,
	                                 const ralSubmissionReceipt_t *b );

void Ral_CommandLifecycleInit( ralCommandLifecycle_t *lifecycle,
	                           const ralBackend_t *backend,
	                           const ralCommandBuffer_t *command,
	                           ralQueueType_t queue );
ralResult_t Ral_CommandLifecycleGetReceipt( const ralCommandLifecycle_t *lifecycle,
	                                        ralCommandReceipt_t *outReceipt );
ralResult_t Ral_CommandLifecyclePublishBegin( ralCommandLifecycle_t *lifecycle,
	                                          ralCommandReceipt_t *outReceipt );
ralResult_t Ral_CommandLifecyclePublishEnd( ralCommandLifecycle_t *lifecycle,
	                                        const ralCommandReceipt_t *recording,
	                                        ralCommandReceipt_t *outReceipt );
ralResult_t Ral_CommandLifecycleCancel( ralCommandLifecycle_t *lifecycle,
	                                    const ralCommandReceipt_t *authority );
// Publish the submitted -> idle handoff after the caller has independently
// proven that the matching queue submission completed (normally by a fence).
// The submitted receipt prevents a stale generation from recycling a newer
// encoder. Backends perform their native reset/release before publishing this
// pure lifecycle transition.
ralResult_t Ral_CommandLifecycleRecycle( ralCommandLifecycle_t *lifecycle,
	                                     const ralCommandReceipt_t *submitted );

void Ral_SubmissionLifecycleInit( ralSubmissionLifecycle_t *lifecycle,
	                              const ralBackend_t *backend,
	                              ralQueueType_t queue );
qboolean Ral_SubmissionLifecycleCanPublish(
	const ralSubmissionLifecycle_t *submission,
	ralCommandLifecycle_t *const *commands,
	const ralCommandReceipt_t *executableReceipts,
	uint32_t commandCount );
ralResult_t Ral_SubmissionLifecyclePublish(
	ralSubmissionLifecycle_t *submission,
	ralCommandLifecycle_t *const *commands,
	const ralCommandReceipt_t *executableReceipts,
	uint32_t commandCount,
	ralSubmissionReceipt_t *outReceipt );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_COMMAND_LIFECYCLE_H
