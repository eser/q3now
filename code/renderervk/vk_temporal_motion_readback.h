// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_MOTION_READBACK_H
#define WIRED_VK_TEMPORAL_MOTION_READBACK_H

#include "vk_temporal_main_activation.h"
#include "vk_temporal_motion_materialization.h"
#include "../renderer/ral/ral_command.h"

#define VK_TEMPORAL_READBACK_MAX_FRAMES TEMPORAL_MOTION_PAYLOAD_MAX_FRAMES
#define VK_TEMPORAL_READBACK_MAX_ROI 256u

typedef enum {
	VK_TEMPORAL_READBACK_EMPTY = 0,
	VK_TEMPORAL_READBACK_READY,
	VK_TEMPORAL_READBACK_RECORDED,
	VK_TEMPORAL_READBACK_SUBMITTED
} vkTemporalMotionReadbackState_t;

typedef struct {
	vkTemporalMainActivationReceipt_t activation;
	uint32_t commandSlot;
	uint32_t captureSerial;
	uint32_t bufferAllocationGeneration;
	uint32_t roiX, roiY, roiWidth, roiHeight;
	uint64_t velocityBytes;
	uint64_t validityOffset;
	uint64_t totalBytes;
	uint64_t iqmCurrentPaletteHash;
	uint64_t iqmPreviousPaletteHash;
	uint32_t iqmRasterMvpBits[16];
	uint32_t iqmRecordCount;
	qboolean submitted;
} vkTemporalMotionReadbackTicket_t;

typedef struct {
	vkTemporalMotionReadbackTicket_t ticket;
	uint64_t velocityHash;
	uint64_t validityHash;
	uint32_t pixels;
	uint32_t validityZero;
	uint32_t validityFull;
	uint32_t validityOther;
	uint32_t finiteVelocity;
	uint32_t nonfiniteVelocity;
	uint32_t nonzeroValidVelocity;
	uint32_t nonzeroInvalidVelocity;
	qboolean fenceComplete;
	qboolean ready;
} vkTemporalMotionReadbackContentReceipt_t;

typedef struct {
	ralBuffer_t *buffer;
	uint64_t bytes;
	uint32_t width, height;
	uint32_t allocationGeneration;
	qboolean hostReadable;
	vkTemporalMotionReadbackState_t state;
	vkTemporalMotionReadbackTicket_t ticket;
} vkTemporalMotionReadbackSlot_t;

typedef struct {
	ralBackend_t *backend;
	vkTemporalMotionReadbackSlot_t slots[VK_TEMPORAL_READBACK_MAX_FRAMES];
	vkTemporalMotionReadbackContentReceipt_t latest;
	uint32_t frameCount;
	uint32_t nextCaptureSerial;
	uint32_t capturesRemaining;
	qboolean initialized;
} vkTemporalMotionReadbackOwner_t;

void VK_TemporalMotionReadbackInit( vkTemporalMotionReadbackOwner_t *owner );

// Diagnostic-only CPU arm. It allocates nothing and records no GPU command.
// Captures are consumed only by successfully submitted exact temporal frames.
qboolean VK_TemporalMotionReadbackArm(
	vkTemporalMotionReadbackOwner_t *owner, uint32_t captureCount );
qboolean VK_TemporalMotionReadbackIsArmed(
	const vkTemporalMotionReadbackOwner_t *owner );

// Called at the completed command-slot fence boundary. Prepare owns one
// MAP_READ-capable TRANSFER_DST buffer for the centered bounded ROI. Mapping is
// bounded to CompleteAfterFence and is never live while GPU commands use it.
qboolean VK_TemporalMotionReadbackPrepareAfterFence(
	vkTemporalMotionReadbackOwner_t *owner, ralBackend_t *backend,
	uint32_t frameCount, uint32_t frameIndex, uint32_t width, uint32_t height );

// Records only copies/transitions at the final no-rendering-scope seam.
qboolean VK_TemporalMotionReadbackRecord(
	vkTemporalMotionReadbackOwner_t *owner, ralCommandBuffer_t *commandBuffer,
	uint32_t frameIndex,
	const vkTemporalMainActivationReceipt_t *pendingActivation,
	const vkTemporalMotionMaterializationProductView_t *view );

// Must be called immediately after activation submit resolution. A successful
// exact receipt promotes the durable per-slot ticket; failure cancels it.
qboolean VK_TemporalMotionReadbackResolveSubmit(
	vkTemporalMotionReadbackOwner_t *owner, uint32_t frameIndex,
	qboolean submitted,
	const vkTemporalMainActivationReceipt_t *resolvedActivation,
	vkTemporalMotionReadbackTicket_t *outTicket );

// Parses mapped bytes only with an explicit proof that the matching
// command-slot fence (or a global-idle boundary) completed.
qboolean VK_TemporalMotionReadbackCompleteAfterFence(
	vkTemporalMotionReadbackOwner_t *owner, uint32_t frameIndex,
	qboolean fenceProven,
	vkTemporalMotionReadbackContentReceipt_t *outReceipt );

qboolean VK_TemporalMotionReadbackGetLatest(
	const vkTemporalMotionReadbackOwner_t *owner,
	vkTemporalMotionReadbackContentReceipt_t *outReceipt );
qboolean VK_TemporalMotionReadbackHasLive(
	const vkTemporalMotionReadbackOwner_t *owner );
qboolean VK_TemporalMotionReadbackReleaseAfterIdle(
	vkTemporalMotionReadbackOwner_t *owner, qboolean idleProven );

#endif
