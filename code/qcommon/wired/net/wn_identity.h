// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef WN_IDENTITY_H
#define WN_IDENTITY_H

#include <stdint.h>

/* Pure ABA guard shared by production queue drains and its host contract. */
static inline int WN_AllocationIdentityMatches( int active,
	uint64_t currentHandle, uint64_t currentAllocationId,
	uint64_t queuedHandle, uint64_t queuedAllocationId ) {
	return active && currentAllocationId != 0 && queuedAllocationId != 0
		&& currentHandle == queuedHandle
		&& currentAllocationId == queuedAllocationId;
}

static inline int WN_AcceptedAllocationIdentityMatches( int active,
	int accepted, uint64_t currentHandle, uint64_t currentAllocationId,
	uint64_t routedHandle, uint64_t routedAllocationId ) {
	return accepted && WN_AllocationIdentityMatches( active, currentHandle,
		currentAllocationId, routedHandle, routedAllocationId );
}

#endif
