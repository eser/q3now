// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "cl_wired_attract_policy.h"
#include <stdio.h>

static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#expr); failures++; } } while (0)

int main( void ) {
	CHECK( !WiredAttract_ShouldResumeAfterDisconnect( WIRED_ATTRACT_SUSPEND_NONE ) );
	CHECK( !WiredAttract_ShouldResumeAfterDisconnect( WIRED_ATTRACT_SUSPEND_USER ) );
	CHECK( WiredAttract_ShouldResumeAfterDisconnect( WIRED_ATTRACT_SUSPEND_SESSION ) );
	CHECK( !WiredAttract_ShouldResumeAfterDisconnect( WIRED_ATTRACT_SUSPEND_PLAYLIST ) );
	return failures ? 1 : 0;
}
