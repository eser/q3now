// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "cl_ping_queue.h"

bool CL_PingQueueDescribe( bool occupied, uint32_t start, uint32_t timeout,
	int resultTime, clPingQueueEntry_t *output ) {
	clPingQueueEntry_t entry;

	if ( resultTime < 0 || !output ) return false;
	entry.occupied = occupied;
	entry.completed = resultTime > 0;
	entry.start = start;
	entry.timeout = timeout;
	*output = entry;
	return true;
}

static bool CL_PingQueueOldest( const clPingQueueEntry_t *entries, size_t count,
	uint32_t now, bool completed, bool expiredOnly,
	clPingQueueReuseReason_t reason, clPingQueueSelection_t *selection ) {
	bool found = false;
	clPingQueueSelection_t candidate = { 0, reason, 0 };

	for ( size_t i = 0; i < count; i++ ) {
		uint32_t age;
		if ( !entries[i].occupied || entries[i].completed != completed ) continue;
		age = now - entries[i].start;
		if ( expiredOnly && age < entries[i].timeout ) continue;
		if ( !found || age > candidate.age ) {
			candidate.index = i;
			candidate.age = age;
			found = true;
		}
	}
	if ( found ) *selection = candidate;
	return found;
}

bool CL_PingQueueSelect( const clPingQueueEntry_t *entries, size_t count,
	uint32_t now, clPingQueueSelection_t *output ) {
	clPingQueueSelection_t selection;

	if ( !entries || count == 0 || !output ) return false;
	for ( size_t i = 0; i < count; i++ ) {
		if ( !entries[i].occupied ) {
			selection.index = i;
			selection.reason = CL_PING_QUEUE_REUSE_FREE;
			selection.age = 0;
			*output = selection;
			return true;
		}
	}
	if ( CL_PingQueueOldest( entries, count, now, false, true,
		CL_PING_QUEUE_REUSE_EXPIRED, &selection )
	  || CL_PingQueueOldest( entries, count, now, true, false,
		CL_PING_QUEUE_REUSE_COMPLETED, &selection )
	  || CL_PingQueueOldest( entries, count, now, false, false,
		CL_PING_QUEUE_REUSE_OLDEST_PENDING, &selection ) ) {
		*output = selection;
		return true;
	}
	return false;
}
