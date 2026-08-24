// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_TR_TEMPORAL_IQM_MOTION_H
#define WIRED_TR_TEMPORAL_IQM_MOTION_H

#include "tr_temporal_motion.h"
#include "vk_bindless_publication.h"

#include <stddef.h>
#include <stdint.h>

#define TEMPORAL_IQM_MAX_JOINTS 128u
#define TEMPORAL_IQM_BONE_ROWS ( TEMPORAL_IQM_MAX_JOINTS * 3u )
#define TEMPORAL_IQM_MAX_RECORDS 256u
#define TEMPORAL_IQM_MAX_DRAWS 256u
#define TEMPORAL_IQM_ENTITY_INDEX_LIMIT 4095u
#define TEMPORAL_IQM_RECORD_ALIGNMENT 16u
#define TEMPORAL_IQM_CURRENT_BONES_OFFSET 0u
#define TEMPORAL_IQM_PREVIOUS_BONES_OFFSET 6144u
#define TEMPORAL_IQM_RASTER_MVP_OFFSET 12288u
#define TEMPORAL_IQM_CURRENT_MVP_OFFSET 12352u
#define TEMPORAL_IQM_PREVIOUS_MVP_OFFSET 12416u
#define TEMPORAL_IQM_RECORD_SIZE 12480u
#define TEMPORAL_IQM_VERTEX_STRIDE 68u
#define TEMPORAL_IQM_TEXTURE_SLOTS VK_BINDLESS_PUBLICATION_IMAGE_SLOTS
#define TEMPORAL_IQM_SAMPLER_SLOTS VK_BINDLESS_PUBLICATION_SAMPLER_SLOTS
#define TEMPORAL_IQM_SLOT_BYTES \
	( TEMPORAL_IQM_RECORD_SIZE * TEMPORAL_IQM_MAX_RECORDS )

#define TEMPORAL_IQM_SCALAR_UBYTE 1u
#define TEMPORAL_IQM_SCALAR_INT 4u
#define TEMPORAL_IQM_SCALAR_FLOAT 7u

typedef struct {
	float translate[3];
	float rotate[4];
	float scale[3];
} temporalIqmTransform_t;

// Immutable, loader-validated model view. The pointed-to arrays remain owned
// by the model. `contentDigest` binds their complete topology/pose content.
typedef struct {
	uintptr_t modelDataToken;
	uint64_t contentDigest;
	uint32_t topologyGeneration;
	uint32_t modelAllocationGeneration;
	uint32_t numFrames;
	uint32_t numJoints;
	uint32_t numPoses;
	const int32_t *jointParents;
	const float *bindJoints;
	const float *inverseBindJoints;
	const temporalIqmTransform_t *poses;
	qboolean validated;
} temporalIqmModelView_t;

typedef struct {
	float currentBones[TEMPORAL_IQM_BONE_ROWS][4];
	float previousBones[TEMPORAL_IQM_BONE_ROWS][4];
	float rasterMvp[16];
	float temporalCurrentMvp[16];
	float temporalPreviousMvp[16];
} temporalIqmGpuRecord_t;

qboolean R_TemporalIqmStorageRangeSupported( uint64_t maxStorageBufferRange );

qboolean R_TemporalIqmNormalizeFrameTuple( uint32_t numFrames,
	int32_t frame, int32_t oldframe, float backlerp,
	int32_t *outFrame, int32_t *outOldFrame, float *outBacklerp );

// Canonical loader primitive. IQM_INT indices are range-checked before their
// byte representation is authored. Lane x is always joint-bounded because the
// shader evaluates it unconditionally; inactive y/z/w lanes are canonical zero.
qboolean R_TemporalIqmDecodeInfluence( uint32_t indexFormat,
	const void *indexData, uint32_t weightFormat, const void *weightData,
	uint32_t vertexIndex, uint32_t numJoints, uint8_t outIndices[4],
	float outWeights[4] );

// Shared exact pose primitive. Product IQM and the temporal record adapter use
// this same implementation; unused rows are canonical zero.
qboolean R_TemporalIqmBuildPaletteTuple( const temporalIqmModelView_t *model,
	int32_t frame, int32_t oldframe, float backlerp,
	float outBones[TEMPORAL_IQM_BONE_ROWS][4] );

qboolean R_TemporalIqmBuildPalette( const temporalIqmModelView_t *model,
	const temporalEntityPose_t *entity,
	float outBones[TEMPORAL_IQM_BONE_ROWS][4] );

// Builds one output-atomic fixed ABI record. A missing exact previous pose is
// represented by copying current palette/MVP into the previous fields and
// returning previousValid=false; the eventual shader recipe must invalidate.
qboolean R_TemporalIqmBuildGpuRecord( const temporalIqmModelView_t *model,
	const temporalCameraPoseReceipt_t *camera,
	const temporalEntityPoseReceipt_t *entity,
	const float jitteredProjection[16], temporalIqmGpuRecord_t *outRecord,
	qboolean *outPreviousValid );

typedef struct {
	uint64_t token;
	uint64_t frameId;
	int32_t worldIndex;
	uint32_t commandSlot;
	uint32_t frameCount;
} temporalIqmSequenceAuthority_t;

typedef struct {
	qboolean primaryCommand;
	qboolean iqmSurface;
	int32_t entityIndex;
	int32_t worldEntityIndex;
	qboolean gpuDirect;
	qboolean ordinaryAvailable;
	qboolean h5Eligible;
	qboolean entityExact;
	qboolean modelExact;
	qboolean geometryExact;
	qboolean opaqueStage0Exact;
	qboolean bindlessExact;
} temporalIqmProductAdmissionFacts_t;

typedef enum {
	TEMPORAL_IQM_PRODUCT_NONE = 0,
	TEMPORAL_IQM_PRODUCT_ADMIT,
	TEMPORAL_IQM_PRODUCT_POISON
} temporalIqmProductAdmission_t;

// Non-IQM and non-primary surfaces are outside this candidate. Every IQM
// surface that the ordinary fixed GPU path can execute is all-or-poison: a
// partially exact draw cannot be omitted from the auxiliary command stream.
temporalIqmProductAdmission_t R_TemporalIqmClassifyProductSurface(
	const temporalIqmProductAdmissionFacts_t *facts );

typedef struct {
	uint32_t ordinal;
	uint32_t sourceDrawSurfOrdinal;
	uint32_t entityIndex;
	refEntityMotion_t identity;
	temporalEntityPose_t currentPose;
	temporalEntityPose_t previousPose;
	uint64_t modelContentDigest;
	uint32_t modelTopologyGeneration;
	uint32_t modelAllocationGeneration;
	uint32_t surfaceIndex;
	uint32_t firstIndex;
	uint32_t indexCount;
	uintptr_t rawVertexBuffer;
	uintptr_t rawIndexBuffer;
	uintptr_t ralVertexBuffer;
	uintptr_t ralIndexBuffer;
	uint64_t vertexBufferBytes;
	uint64_t indexBufferBytes;
	uintptr_t geometryBackend;
	uint32_t geometryGeneration;
	uint32_t geometryAllocationGeneration;
	uint32_t textureSlot;
	uint32_t samplerSlot;
	vkBindlessOrdinaryReceipt_t bindless;
	temporalMotionOutcome_t outcome;
	qboolean previousValid;
} temporalIqmDrawFacts_t;

qboolean R_TemporalIqmDrawFactsValid(
	const temporalIqmDrawFacts_t *facts );

typedef struct {
	temporalIqmDrawFacts_t facts;
	uint32_t recordIndex;
} temporalIqmSequenceEntry_t;

typedef struct {
	temporalIqmSequenceAuthority_t authority;
	temporalIqmSequenceEntry_t entries[TEMPORAL_IQM_MAX_DRAWS];
	uint32_t drawCount;
	uint32_t entityCount;
	uint64_t orderedDigest;
	qboolean ready;
} temporalIqmSequence_t;

typedef struct {
	const temporalIqmSequence_t *expected;
	uint64_t expectedDigest;
	uint32_t cursor;
	qboolean active;
	qboolean poisoned;
} temporalIqmSequenceVerifier_t;

_Static_assert( sizeof( temporalIqmSequenceVerifier_t ) <= 32u,
	"IQM sequence verifier must remain a compact borrowed view" );

// Builds the immutable pre-draw sequence and first-occurrence record mapping.
// Every direct IQM draw must be WRITE or INVALIDATE and consumes one entity
// record mapping; no preserve/truncation lane exists. Failure leaves
// `outSequence` byte-identical.
qboolean R_TemporalIqmSequenceBuild(
	const temporalIqmSequenceAuthority_t *authority,
	const temporalIqmDrawFacts_t *draws, uint32_t drawCount,
	temporalIqmSequence_t *outSequence );
qboolean R_TemporalIqmSequenceExact(
	const temporalIqmSequence_t *a, const temporalIqmSequence_t *b );
qboolean R_TemporalIqmSequenceEntryExact(
	const temporalIqmSequence_t *sequence, uint32_t ordinal,
	const temporalIqmSequenceEntry_t *entry );
qboolean R_TemporalIqmSequenceEntryEqual(
	const temporalIqmSequenceEntry_t *a,
	const temporalIqmSequenceEntry_t *b );

qboolean R_TemporalIqmSequenceVerifierBegin(
	temporalIqmSequenceVerifier_t *verifier,
	const temporalIqmSequence_t *expected );
qboolean R_TemporalIqmSequenceVerifierPeek(
	temporalIqmSequenceVerifier_t *verifier,
	const temporalIqmDrawFacts_t *observed,
	temporalIqmSequenceEntry_t *outExpected );
qboolean R_TemporalIqmSequenceVerifierCommit(
	temporalIqmSequenceVerifier_t *verifier,
	const temporalIqmSequenceEntry_t *expected );
qboolean R_TemporalIqmSequenceVerifierFinish(
	temporalIqmSequenceVerifier_t *verifier );

#endif
