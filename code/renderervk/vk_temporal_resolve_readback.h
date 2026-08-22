// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_RESOLVE_READBACK_H
#define WIRED_VK_TEMPORAL_RESOLVE_READBACK_H

#include "vk_temporal_resolve.h"
#include "../renderer/ral/ral_command.h"

#define VK_TEMPORAL_RESOLVE_READBACK_MAX_FRAMES 4u
#define VK_TEMPORAL_RESOLVE_READBACK_CORE_MAX 256u
#define VK_TEMPORAL_RESOLVE_READBACK_APRON_MIN 4u
#define VK_TEMPORAL_RESOLVE_READBACK_APRON_MAX 512u
#define VK_TEMPORAL_RESOLVE_READBACK_APRON_DIVISOR 4u
#define VK_TEMPORAL_RESOLVE_READBACK_MISMATCH_SAMPLES 4u

typedef enum {
	VK_TEMPORAL_RESOLVE_DEPTH_D16 = 1,
	VK_TEMPORAL_RESOLVE_DEPTH_X8_D24,
	VK_TEMPORAL_RESOLVE_DEPTH_D24_S8,
	VK_TEMPORAL_RESOLVE_DEPTH_D32,
	VK_TEMPORAL_RESOLVE_DEPTH_D16_S8,
	VK_TEMPORAL_RESOLVE_DEPTH_D32_S8
} vkTemporalResolveReadbackDepthEncoding_t;

typedef enum {
	VK_TEMPORAL_RESOLVE_READBACK_EMPTY = 0,
	VK_TEMPORAL_RESOLVE_READBACK_READY,
	VK_TEMPORAL_RESOLVE_READBACK_RECORDED,
	VK_TEMPORAL_RESOLVE_READBACK_SUBMITTED
} vkTemporalResolveReadbackState_t;

typedef struct {
	vkTemporalResolveTicket_t resolve;
	uint32_t commandSlot;
	uint32_t captureSerial;
	uint32_t bufferAllocationGeneration;
	ralBuffer_t *buffer;
	uint32_t captureX, captureY, captureWidth, captureHeight;
	uint32_t coreX, coreY, coreWidth, coreHeight;
	vkTemporalResolveReadbackDepthEncoding_t currentDepthEncoding;
	uint64_t currentColorOffset;
	uint64_t currentDepthOffset;
	uint64_t previousColorOffset;
	uint64_t previousDepthOffset;
	uint64_t velocityOffset;
	uint64_t validityOffset;
	uint64_t resolvedOffset;
	uint64_t totalBytes;
	qboolean submitted;
} vkTemporalResolveReadbackTicket_t;

typedef struct {
	uint32_t x, y;
	uint32_t halfDistance;
	uint16_t expectedBits, actualBits, currentBits;
	uint8_t channel;
	uint8_t gpuCurrentFallback;
} vkTemporalResolveReadbackMismatchSample_t;

typedef struct {
	vkTemporalResolveReadbackTicket_t ticket;
	uint64_t currentColorHash, currentDepthHash;
	uint64_t previousColorHash, previousDepthHash;
	uint64_t velocityHash, validityHash, resolvedHash;
	uint16_t centerCurrentColor[4], centerResolvedColor[4];
	uint16_t centerVelocity[2];
	uint32_t centerDepthRaw;
	uint32_t centerX, centerY;
	uint8_t centerValidity;
	uint32_t corePixels;
	uint32_t oracleEligible;
	uint32_t accepted;
	uint32_t acceptedMatches;
	uint32_t acceptedInfluence;
	uint32_t acceptedNonzeroVelocity;
	uint32_t fallbackExpected;
	uint32_t fallbackExact;
	uint32_t invalidFallbackExpected;
	uint32_t invalidFallbackExact;
	uint32_t zeroFallbackExpected;
	uint32_t zeroFallbackExact;
	uint32_t unsupportedFootprint;
	uint32_t rejectedCurrentNonfinite;
	uint32_t rejectedValidity;
	uint32_t rejectedVelocityNonfinite;
	uint32_t rejectedOutside;
	uint32_t rejectedDepthNonfinite;
	uint32_t rejectedDepthNonpositive;
	uint32_t rejectedDepthThreshold;
	uint32_t rejectedPriorNonfinite;
	uint32_t rejectedNeighborhoodNonfinite;
	uint32_t mismatches;
	uint32_t mismatchGpuCurrentFallback;
	uint32_t mismatchBlended;
	uint32_t mismatchMaxHalfDistance;
	uint32_t mismatchSampleCount;
	vkTemporalResolveReadbackMismatchSample_t
		mismatchSamples[VK_TEMPORAL_RESOLVE_READBACK_MISMATCH_SAMPLES];
	uint32_t nonfiniteInputs;
	uint32_t validityZero;
	uint32_t validityFull;
	uint32_t validityOther;
	qboolean planesPopulated;
	qboolean fenceComplete;
	qboolean ready;
} vkTemporalResolveReadbackContentReceipt_t;

typedef struct {
	ralBuffer_t *buffer;
	uint64_t bytes;
	uint32_t width, height;
	vkTemporalResolveReadbackDepthEncoding_t depthEncoding;
	vkTemporalResolveProductView_t products;
	uint32_t allocationGeneration;
	qboolean hostReadable;
	vkTemporalResolveReadbackState_t state;
	vkTemporalResolveReadbackTicket_t ticket;
} vkTemporalResolveReadbackSlot_t;

typedef struct {
	ralBackend_t *backend;
	vkTemporalResolveReadbackSlot_t slots[VK_TEMPORAL_RESOLVE_READBACK_MAX_FRAMES];
	vkTemporalResolveReadbackContentReceipt_t latest;
	uint32_t frameCount;
	uint32_t nextCaptureSerial;
	uint32_t capturesRemaining;
	qboolean initialized;
} vkTemporalResolveReadbackOwner_t;

// Vulkan renderervk supplies the one depth-only copy operation.  The callback
// must transition the whole depth/stencil image to TRANSFER_SRC, copy only its
// depth plane, restore SHADER_READ_ONLY, and resynchronise the RAL tracker.
typedef qboolean ( *vkTemporalResolveReadbackDepthCopyFn )(
	ralCommandBuffer_t *commandBuffer, ralTexture_t *depth,
	ralBuffer_t *destination, uint64_t destinationOffset,
	uint32_t x, uint32_t y, uint32_t width, uint32_t height,
	vkTemporalResolveReadbackDepthEncoding_t depthEncoding, void *user );

void VK_TemporalResolveReadbackInit( vkTemporalResolveReadbackOwner_t *owner );
qboolean VK_TemporalResolveReadbackArm(
	vkTemporalResolveReadbackOwner_t *owner, uint32_t captureCount );
qboolean VK_TemporalResolveReadbackIsArmed(
	const vkTemporalResolveReadbackOwner_t *owner );
qboolean VK_TemporalResolveReadbackPrepareAfterFence(
	vkTemporalResolveReadbackOwner_t *owner, ralBackend_t *backend,
	uint32_t frameCount, uint32_t frameIndex, uint32_t width, uint32_t height,
	vkTemporalResolveReadbackDepthEncoding_t depthEncoding,
	const vkTemporalResolveProductView_t *protectedProducts );
qboolean VK_TemporalResolveReadbackRecord(
	vkTemporalResolveReadbackOwner_t *owner, ralCommandBuffer_t *commandBuffer,
	uint32_t frameIndex, const vkTemporalResolveTicket_t *recordedResolve,
	const vkTemporalResolveProductView_t *products,
	vkTemporalResolveReadbackDepthCopyFn depthCopy, void *depthCopyUser );
// Binds the sole post-record Store-owner provenance field after H3 routing has
// succeeded and before the exact pending history write is staged.  All other
// H3 ticket fields must remain byte/field exact; failure is output-atomic.
qboolean VK_TemporalResolveReadbackBindStoreExpected(
	vkTemporalResolveReadbackOwner_t *owner, uint32_t frameIndex,
	const vkTemporalResolveTicket_t *boundResolve );
qboolean VK_TemporalResolveReadbackResolveSubmit(
	vkTemporalResolveReadbackOwner_t *owner, uint32_t frameIndex,
	qboolean submitted, const vkTemporalResolveTicket_t *submittedResolve,
	vkTemporalResolveReadbackTicket_t *outTicket );
qboolean VK_TemporalResolveReadbackCompleteAfterFence(
	vkTemporalResolveReadbackOwner_t *owner, uint32_t frameIndex,
	qboolean fenceProven, vkTemporalResolveReadbackContentReceipt_t *outReceipt );
qboolean VK_TemporalResolveReadbackGetLatest(
	const vkTemporalResolveReadbackOwner_t *owner,
	vkTemporalResolveReadbackContentReceipt_t *outReceipt );
qboolean VK_TemporalResolveReadbackHasLive(
	const vkTemporalResolveReadbackOwner_t *owner );
qboolean VK_TemporalResolveReadbackReleaseAfterIdle(
	vkTemporalResolveReadbackOwner_t *owner, qboolean idleProven );
// Public only so the host contract can pin IEEE-754 round-to-nearest-even
// boundaries used by the RGBA16F oracle.
uint16_t VK_TemporalResolveReadbackFloatToHalfRne( float value );
qboolean VK_TemporalResolveReadbackHalfWithinOneStep(
	uint16_t actualBits, float expected );
float VK_TemporalResolveReadbackDecodeDepth(
	const void *bytes, vkTemporalResolveReadbackDepthEncoding_t encoding );
void VK_TemporalResolveReadbackPreviousTexel(
	const uint32_t pixel[2], const uint32_t extent[2], const float velocity[2],
	const float previousJitter[2], const float currentJitter[2], float out[2] );

#endif
