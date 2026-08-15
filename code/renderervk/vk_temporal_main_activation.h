// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_MAIN_ACTIVATION_H
#define WIRED_VK_TEMPORAL_MAIN_ACTIVATION_H

#include "vk_temporal_motion_recording.h"
#include "vk_temporal_iqm_exact3_factory.h"
#include "vk_temporal_iqm_geometry.h"
#include "vk_temporal_iqm_payload_authoring.h"
#include "vk_temporal_shader_cohort.h"

typedef enum {
	VK_TEMPORAL_MAIN_EVENT_GENERIC = 1,
	VK_TEMPORAL_MAIN_EVENT_IQM = 2
} vkTemporalMainEventKind_t;

typedef struct {
	uint64_t lane0;
	uint64_t lane1;
	uint32_t count;
	uint32_t genericCount;
	uint32_t iqmCount;
} vkTemporalMainTaggedSequence_t;

typedef struct {
	vkTemporalMotionRecordingAuthority_t authority;
	uint32_t planSerial;
	uint32_t pipelineSlot;
	uint32_t absoluteEntMatSlot;
	vkTemporalGenericPipelineReceipt_t pipelineReceipt;
	vkTemporalShaderRecipeKind_t kind;
	ralPipeline_t *pipeline;
	qboolean endOrdinary;
	qboolean beginExact3;
	qboolean clearAuxiliary;
	qboolean endExact3;
	qboolean resumeOrdinary;
} vkTemporalMainActivationPlan_t;

typedef struct {
	vkTemporalMotionRecordingAuthority_t authority;
	uint32_t planSerial;
	uint32_t ordinal;
	uint32_t recordIndex;
	uint64_t sequenceDigest;
	temporalIqmSequenceEntry_t entry;
	ralBindGroup_t *payloadGroup;
	vkTemporalShaderRecipeKind_t kind;
	ralPipeline_t *pipeline;
	vkTemporalIqmExact3FactoryReceipt_t factory;
	qboolean endOrdinary;
	qboolean beginExact3;
	qboolean clearAuxiliary;
	qboolean endExact3;
	qboolean resumeOrdinary;
} vkTemporalMainIqmActivationPlan_t;

typedef struct {
	uint32_t topologyGeneration;
	ralFormat_t sceneFormat;
	ralFormat_t depthFormat;
	qboolean reversedDepth;
	qboolean ready;
} vkTemporalMainIqmPassReceipt_t;

typedef struct {
	qboolean (*revalidateGeometry)( void *context,
		const temporalIqmDrawFacts_t *expected,
		vkTemporalIqmGeometryReceipt_t *outCurrent );
	qboolean (*revalidateBindless)( void *context,
		const temporalIqmDrawFacts_t *expected,
		vkBindlessOrdinaryReceipt_t *outCurrent );
} vkTemporalMainIqmActivationOps_t;

typedef struct {
	temporalIqmSequenceAuthority_t authority;
	uint64_t sequenceDigest;
	uint32_t drawCount;
	uint32_t entityCount;
	uint32_t prepared;
	uint32_t temporalSegments;
	uint32_t written;
	uint32_t invalidated;
	vkTemporalIqmPayloadContentReceipt_t content;
	vkTemporalIqmExact3FactoryReceipt_t factory;
	vkTemporalMainIqmPassReceipt_t pass;
	qboolean ready;
} vkTemporalMainIqmActivationReceipt_t;

typedef struct {
	vkTemporalMotionRecordingAuthority_t authority;
	uint32_t prepared;
	uint32_t temporalSegments;
	uint32_t preserved;
	uint32_t written;
	uint32_t invalidated;
	vkTemporalMotionDrawSequence_t drawSequence;
	vkTemporalMainIqmActivationReceipt_t iqm;
	vkTemporalMainTaggedSequence_t taggedSequence;
	qboolean auxiliaryCleared;
	qboolean depthStoreRequired;
	qboolean stencilStoreRequired;
	qboolean ready;
} vkTemporalMainActivationReceipt_t;

typedef struct {
	vkTemporalMotionRecordingAuthority_t authority;
	vkTemporalMainActivationPlan_t pendingPlan;
	vkTemporalMainIqmActivationPlan_t pendingIqmPlan;
	vkTemporalMainActivationReceipt_t pendingReceipt;
	vkTemporalMainActivationReceipt_t receipt;
	uint32_t nextPlanSerial;
	uint32_t prepared;
	uint32_t temporalSegments;
	uint32_t preserved;
	uint32_t written;
	uint32_t invalidated;
	vkTemporalMotionDrawSequence_t drawSequence;
	vkTemporalMainIqmActivationReceipt_t iqm;
	vkTemporalMainTaggedSequence_t taggedSequence;
	temporalIqmSequenceVerifier_t iqmVerifier;
	void *iqmRevalidateContext;
	vkTemporalMainIqmActivationOps_t iqmRevalidateOps;
	qboolean auxiliaryInitialized;
	qboolean iqmRevalidateBound;
	qboolean requireDepthStencilStore;
	qboolean active;
	qboolean pending;
	vkTemporalMainEventKind_t pendingKind;
	qboolean submissionPending;
	qboolean poisoned;
} vkTemporalMainActivationOwner_t;

typedef struct {
	qboolean (*preflight)( void *context, uint32_t pipelineSlot,
		const ralPipeline_t *ordinaryPipeline,
		vkTemporalGenericPipelineReceipt_t *outReceipt,
		ralPipeline_t *outPipelines[3] );
} vkTemporalMainActivationOps_t;

void VK_TemporalMainActivationInit(
	vkTemporalMainActivationOwner_t *owner );

// Starts a CPU-only candidate transaction for the exact current recording
// authority. No pass, bind, pipeline or draw command is emitted here.
qboolean VK_TemporalMainActivationBegin(
	vkTemporalMainActivationOwner_t *owner,
	const vkTemporalMotionRecordingAuthority_t *authority );
qboolean VK_TemporalMainActivationRequiresDepthStencilStore(
	const vkTemporalMainActivationOwner_t *owner );

// Binds a borrowed immutable, pre-draw IQM sequence, sealed current-slot
// payload and exact sibling factory before either draw family is planned. The
// caller must keep the exact sequence object alive and byte-immutable through
// FinishIqm. This records no commands and leaves generic-only transactions
// byte-compatible when omitted.
qboolean VK_TemporalMainActivationBindIqm(
	vkTemporalMainActivationOwner_t *owner,
	const temporalIqmSequence_t *sequence,
	const vkTemporalIqmPayloadContentReceipt_t *content,
	const vkTemporalIqmPayloadOwner_t *payloadOwner,
	const vkTemporalIqmExact3FactoryReceipt_t *factory,
	const vkTemporalMainIqmPassReceipt_t *pass,
	void *revalidateContext,
	const vkTemporalMainIqmActivationOps_t *revalidateOps );

// Authors one exact segment recipe. PRESERVE keeps the ordinary one-color pass
// open. WRITE/INVALIDATE require the generation-bound exact3 receipt and always
// form a bounded one-logical-draw LOAD segment (including all physical VBO
// runs): ordinary end -> exact3 begin/end -> ordinary LOAD resume. The first
// exact3 segment clears both auxiliary targets; later segments load them.
qboolean VK_TemporalMainActivationPlanDraw(
	vkTemporalMainActivationOwner_t *owner,
	temporalMotionOutcome_t outcome,
	uint32_t absoluteEntMatSlot,
	uint32_t pipelineSlot,
	const ralPipeline_t *ordinaryPipeline,
	void *preflightContext,
	const vkTemporalMainActivationOps_t *ops,
	vkTemporalMainActivationPlan_t *outPlan );

// Commits only the exact pending plan. A product adapter calls this only after
// all commands described by the plan were authored successfully.
qboolean VK_TemporalMainActivationCommitDraw(
	vkTemporalMainActivationOwner_t *owner,
	const vkTemporalMainActivationPlan_t *plan );

qboolean VK_TemporalMainActivationPlanIqmDraw(
	vkTemporalMainActivationOwner_t *owner,
	const temporalIqmDrawFacts_t *observed,
	vkTemporalMainIqmActivationPlan_t *outPlan );
qboolean VK_TemporalMainActivationCommitIqmDraw(
	vkTemporalMainActivationOwner_t *owner,
	const vkTemporalMainIqmActivationPlan_t *plan );

void VK_TemporalMainActivationPoison(
	vkTemporalMainActivationOwner_t *owner );

// Final publication is intentionally after A2c3 finishes. Candidate GPU writes
// are not authoritative unless the final CPU receipt is ready and its complete
// authority, counts and ordered absolute-slot/exact3 sequence agree.
qboolean VK_TemporalMainActivationFinish(
	vkTemporalMainActivationOwner_t *owner,
	const vkTemporalMotionRecordingReceipt_t *recordingReceipt );
qboolean VK_TemporalMainActivationFinishIqm(
	vkTemporalMainActivationOwner_t *owner,
	const vkTemporalMotionRecordingReceipt_t *recordingReceipt,
	const temporalIqmSequence_t *sequence,
	const vkTemporalIqmPayloadOwner_t *payloadOwner,
	const vkTemporalIqmExact3FactoryReceipt_t *currentFactory );
qboolean VK_TemporalMainActivationReceiptExact(
	const vkTemporalMainActivationReceipt_t *a,
	const vkTemporalMainActivationReceipt_t *b );
// Returns the authored candidate while it is waiting for the graphics submit.
// The receipt is copied so a fence-delayed consumer can persist its own ticket;
// it is not yet authoritative until ResolveSubmit succeeds.
qboolean VK_TemporalMainActivationPeekPendingReceipt(
	const vkTemporalMainActivationOwner_t *owner,
	vkTemporalMainActivationReceipt_t *outReceipt );
// Promotes an authored candidate only after the graphics submit succeeds.
// A failed/discarded submit permanently cancels this frame's candidate.
qboolean VK_TemporalMainActivationResolveSubmit(
	vkTemporalMainActivationOwner_t *owner, qboolean submitted );
qboolean VK_TemporalMainActivationGetReceipt(
	const vkTemporalMainActivationOwner_t *owner,
	vkTemporalMainActivationReceipt_t *outReceipt );

_Static_assert( sizeof( vkTemporalMainActivationReceipt_t ) <= 2048u,
	"MAIN activation receipt must not embed per-event storage" );
_Static_assert( sizeof( vkTemporalMainActivationOwner_t ) <= 8192u,
	"MAIN activation owner must stay bounded" );

#endif
