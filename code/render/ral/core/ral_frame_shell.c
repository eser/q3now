// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_shell.h"

#include <limits.h>
#include <string.h>

static qboolean BackendValid( ralBackendType_t backendType ) {
	return backendType == RAL_BACKEND_VULKAN || backendType == RAL_BACKEND_METAL
		|| backendType == RAL_BACKEND_OPENGL || backendType == RAL_BACKEND_WEBGPU
		? qtrue : qfalse;
}

static qboolean StateValid( ralFrameShellState_t state ) {
	return state >= RAL_FRAME_SHELL_READY && state <= RAL_FRAME_SHELL_SHUTDOWN
		? qtrue : qfalse;
}

static qboolean ReceiptValid( const ralFrameShellReceipt_t *receipt ) {
	return ( receipt && receipt->schemaVersion == RAL_FRAME_SHELL_SCHEMA_VERSION
		&& BackendValid( receipt->backendType )
		&& receipt->ownerGeneration != 0u && receipt->ownerGeneration != UINT64_MAX
		&& receipt->shellIdentity != (uintptr_t)0
		&& StateValid( receipt->state )
		&& ( receipt->state == RAL_FRAME_SHELL_READY
			? receipt->frameGeneration == 0u : receipt->frameGeneration != 0u
				&& receipt->frameGeneration != UINT64_MAX )
		&& ( receipt->state == RAL_FRAME_SHELL_SHUTDOWN
			? receipt->ready == qfalse : receipt->ready == qtrue ) ) ? qtrue : qfalse;
}

qboolean Ral_FrameShellReceiptExact( const ralFrameShellReceipt_t *a,
		const ralFrameShellReceipt_t *b ) {
	return ( ReceiptValid( a ) && ReceiptValid( b )
		&& a->backendType == b->backendType
		&& a->ownerGeneration == b->ownerGeneration
		&& a->frameGeneration == b->frameGeneration
		&& a->shellIdentity == b->shellIdentity
		&& a->state == b->state && a->ready == b->ready ) ? qtrue : qfalse;
}

static qboolean OwnerMatches( const ralFrameShell_t *shell,
		const ralFrameShellReceipt_t *receipt ) {
	return ( shell && receipt && receipt->shellIdentity == (uintptr_t)shell
		&& Ral_FrameShellReceiptExact( receipt, &shell->receipt ) ) ? qtrue : qfalse;
}

qboolean Ral_FrameShellInit( ralFrameShell_t *shell, ralBackendType_t backendType,
		uint64_t ownerGeneration, ralFrameShellReceipt_t *outReceipt ) {
	ralFrameShellReceipt_t receipt;
	if ( !shell || !outReceipt || !BackendValid( backendType )
			|| ownerGeneration == 0u || ownerGeneration == UINT64_MAX ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_FRAME_SHELL_SCHEMA_VERSION;
	receipt.backendType = backendType; receipt.ownerGeneration = ownerGeneration;
	receipt.shellIdentity = (uintptr_t)shell;
	receipt.state = RAL_FRAME_SHELL_READY; receipt.ready = qtrue;
	if ( !ReceiptValid( &receipt ) ) return qfalse;
	memset( shell, 0, sizeof( *shell ) ); shell->receipt = receipt;
	*outReceipt = receipt;
	return qtrue;
}

qboolean Ral_FrameShellBegin( ralFrameShell_t *shell,
		const ralFrameShellReceipt_t *currentReceipt, uint64_t frameGeneration,
		ralFrameShellReceipt_t *outRecording ) {
	ralFrameShellReceipt_t receipt;
	if ( !outRecording || !OwnerMatches( shell, currentReceipt )
			|| ( currentReceipt->state != RAL_FRAME_SHELL_READY
				&& currentReceipt->state != RAL_FRAME_SHELL_PRESENTED
				&& currentReceipt->state != RAL_FRAME_SHELL_CANCELED )
			|| frameGeneration == 0u || frameGeneration == UINT64_MAX
			|| frameGeneration <= shell->lastFrameGeneration ) return qfalse;
	receipt = *currentReceipt; receipt.frameGeneration = frameGeneration;
	receipt.state = RAL_FRAME_SHELL_RECORDING;
	if ( !ReceiptValid( &receipt ) ) return qfalse;
	shell->receipt = receipt; shell->lastFrameGeneration = frameGeneration;
	*outRecording = receipt;
	return qtrue;
}

static qboolean Finish( ralFrameShell_t *shell,
		const ralFrameShellReceipt_t *recordingReceipt,
		ralFrameShellState_t state, ralFrameShellReceipt_t *outReceipt ) {
	ralFrameShellReceipt_t receipt;
	if ( !outReceipt || !OwnerMatches( shell, recordingReceipt )
			|| recordingReceipt->state != RAL_FRAME_SHELL_RECORDING
			|| ( state != RAL_FRAME_SHELL_PRESENTED
				&& state != RAL_FRAME_SHELL_CANCELED ) ) return qfalse;
	receipt = *recordingReceipt; receipt.state = state;
	if ( !ReceiptValid( &receipt ) ) return qfalse;
	shell->receipt = receipt; *outReceipt = receipt;
	return qtrue;
}

qboolean Ral_FrameShellComplete( ralFrameShell_t *shell,
		const ralFrameShellReceipt_t *recordingReceipt,
		ralFrameShellReceipt_t *outPresented ) {
	return Finish( shell, recordingReceipt, RAL_FRAME_SHELL_PRESENTED, outPresented );
}

qboolean Ral_FrameShellCancel( ralFrameShell_t *shell,
		const ralFrameShellReceipt_t *recordingReceipt,
		ralFrameShellReceipt_t *outCanceled ) {
	return Finish( shell, recordingReceipt, RAL_FRAME_SHELL_CANCELED, outCanceled );
}

qboolean Ral_FrameShellShutdown( ralFrameShell_t *shell,
		const ralFrameShellReceipt_t *currentReceipt,
		ralFrameShellReceipt_t *outShutdown ) {
	ralFrameShellReceipt_t receipt;
	if ( !outShutdown || !OwnerMatches( shell, currentReceipt )
			|| currentReceipt->state == RAL_FRAME_SHELL_RECORDING
			|| currentReceipt->state == RAL_FRAME_SHELL_SHUTDOWN ) return qfalse;
	receipt = *currentReceipt; receipt.state = RAL_FRAME_SHELL_SHUTDOWN;
	receipt.ready = qfalse;
	if ( !ReceiptValid( &receipt ) ) return qfalse;
	shell->receipt = receipt; *outShutdown = receipt;
	return qtrue;
}
