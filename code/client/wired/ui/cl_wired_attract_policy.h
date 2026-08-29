// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#pragma once

typedef enum {
	WIRED_ATTRACT_SUSPEND_NONE = 0,
	WIRED_ATTRACT_SUSPEND_USER,
	WIRED_ATTRACT_SUSPEND_SESSION,
	WIRED_ATTRACT_SUSPEND_PLAYLIST
} wiredAttractSuspendCause_t;

/* Only a temporary engine-owned session suspension is resumed by the
 * subsequent disconnect edge. User stop and natural playlist exhaustion are
 * durable until an explicit start/restart. */
int WiredAttract_ShouldResumeAfterDisconnect( wiredAttractSuspendCause_t cause );
