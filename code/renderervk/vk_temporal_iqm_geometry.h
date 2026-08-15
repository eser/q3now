// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_IQM_GEOMETRY_H
#define WIRED_VK_TEMPORAL_IQM_GEOMETRY_H

#include "../qcommon/q_shared.h"
#include "../renderer/ral/ral_types.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
	ralBackend_t *backend;
	void *nativeVertexBuffer;
	void *nativeIndexBuffer;
	uint64_t vertexBytes;
	uint64_t indexBytes;
	uint32_t modelAllocationGeneration;
	uint32_t geometryGeneration;
	uint64_t contentDigest;
} vkTemporalIqmGeometryKey_t;

typedef struct {
	vkTemporalIqmGeometryKey_t key;
	ralBuffer_t *vertex;
	ralBuffer_t *index;
	uint32_t allocationGeneration;
	qboolean ready;
} vkTemporalIqmGeometryReceipt_t;

typedef struct {
	vkTemporalIqmGeometryReceipt_t receipt;
} vkTemporalIqmGeometryOwner_t;

typedef struct {
	ralBuffer_t *(*adopt)( ralBackend_t *backend, void *native,
		size_t bytes, const char *debugName );
	void (*destroy)( ralBuffer_t *buffer );
	qboolean (*candidateOwned)( ralBuffer_t *candidate,
		const void *context );
	qboolean (*matchesNative)( const ralBuffer_t *candidate,
		const void *native, size_t bytes );
	const void *candidateContext;
} vkTemporalIqmGeometryOps_t;

void VK_TemporalIqmGeometryInit( vkTemporalIqmGeometryOwner_t *owner );
qboolean VK_TemporalIqmGeometryNeedsIdle(
	const vkTemporalIqmGeometryOwner_t *owner,
	const vkTemporalIqmGeometryKey_t *key,
	const vkTemporalIqmGeometryOps_t *ops );
qboolean VK_TemporalIqmGeometryEnsureAfterIdle(
	vkTemporalIqmGeometryOwner_t *owner,
	const vkTemporalIqmGeometryKey_t *key, qboolean idleProven,
	const vkTemporalIqmGeometryOps_t *ops );
qboolean VK_TemporalIqmGeometryGetReceipt(
	const vkTemporalIqmGeometryOwner_t *owner,
	vkTemporalIqmGeometryReceipt_t *outReceipt,
	const vkTemporalIqmGeometryOps_t *ops );
qboolean VK_TemporalIqmGeometryReleaseAfterIdle(
	vkTemporalIqmGeometryOwner_t *owner, qboolean idleProven,
	const vkTemporalIqmGeometryOps_t *ops );

#endif
