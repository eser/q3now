// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef CL_PING_QUEUE_H
#define CL_PING_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
	CL_PING_QUEUE_REUSE_FREE = 0,
	CL_PING_QUEUE_REUSE_EXPIRED,
	CL_PING_QUEUE_REUSE_COMPLETED,
	CL_PING_QUEUE_REUSE_OLDEST_PENDING
} clPingQueueReuseReason_t;

typedef struct {
	bool occupied;
	bool completed;
	uint32_t start;
	uint32_t timeout;
} clPingQueueEntry_t;

typedef struct {
	size_t index;
	clPingQueueReuseReason_t reason;
	uint32_t age;
} clPingQueueSelection_t;

/* Translate the engine's existing result-time encoding into the pure queue
 * descriptor. Zero is pending and every positive result, including 1..499ms,
 * is completed. A negative result is invalid and leaves output unchanged. */
bool CL_PingQueueDescribe( bool occupied, uint32_t start, uint32_t timeout,
	int resultTime, clPingQueueEntry_t *output );

/* Select one reusable slot without mutating the input. Selection priority is
 * the first free slot, then the oldest expired pending slot, then the oldest
 * completed slot, and finally the oldest live pending slot. Unsigned age
 * arithmetic intentionally preserves the engine millisecond-clock wrap rule.
 * Invalid input leaves output byte-for-byte unchanged. */
bool CL_PingQueueSelect( const clPingQueueEntry_t *entries, size_t count,
	uint32_t now, clPingQueueSelection_t *output );

#endif
