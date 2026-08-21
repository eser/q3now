// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_shell.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

int main( void ) {
	ralFrameShell_t shell;
	ralFrameShellReceipt_t ready, recording, presented, before, exact;
	memset( &shell, 0, sizeof( shell ) );
	memset( &ready, 0x5a, sizeof( ready ) ); before = ready;
	CHECK( !Ral_FrameShellInit( &shell, (ralBackendType_t)99, 1u, &ready ) );
	CHECK( memcmp( &ready, &before, sizeof( ready ) ) == 0 );
	CHECK( Ral_FrameShellInit( &shell, RAL_BACKEND_WEBGPU, 1u, &ready ) );
	CHECK( ready.state == RAL_FRAME_SHELL_READY && ready.frameGeneration == 0u );
	exact = ready; CHECK( Ral_FrameShellReceiptExact( &ready, &exact ) );
	exact.ownerGeneration++;
	CHECK( !Ral_FrameShellReceiptExact( &ready, &exact ) );
	CHECK( !Ral_FrameShellBegin( &shell, &exact, 2u, &recording ) );
	CHECK( !Ral_FrameShellBegin( &shell, &ready, UINT64_MAX, &recording ) );
	CHECK( Ral_FrameShellBegin( &shell, &ready, 2u, &recording ) );
	CHECK( !Ral_FrameShellBegin( &shell, &recording, 3u, &presented ) );
	exact = recording; exact.frameGeneration++;
	CHECK( !Ral_FrameShellComplete( &shell, &exact, &presented ) );
	CHECK( Ral_FrameShellComplete( &shell, &recording, &presented ) );
	CHECK( presented.state == RAL_FRAME_SHELL_PRESENTED );
	CHECK( !Ral_FrameShellComplete( &shell, &recording, &exact ) );
	CHECK( Ral_FrameShellBegin( &shell, &presented, 3u, &recording ) );
	CHECK( Ral_FrameShellCancel( &shell, &recording, &presented ) );
	CHECK( presented.state == RAL_FRAME_SHELL_CANCELED );
	CHECK( Ral_FrameShellShutdown( &shell, &presented, &exact ) );
	CHECK( exact.state == RAL_FRAME_SHELL_SHUTDOWN && exact.ready == qfalse );
	CHECK( !Ral_FrameShellBegin( &shell, &exact, 4u, &recording ) );
	puts( "RAL frame shell: PASS" );
	return 0;
}
