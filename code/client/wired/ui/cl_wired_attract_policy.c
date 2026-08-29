// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "cl_wired_attract_policy.h"

int WiredAttract_ShouldResumeAfterDisconnect( wiredAttractSuspendCause_t cause ) {
	return cause == WIRED_ATTRACT_SUSPEND_SESSION;
}
