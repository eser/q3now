// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_FRAME_SHELL_H
#define WIRED_RAL_FRAME_SHELL_H

#include "ral_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_FRAME_SHELL_SCHEMA_VERSION 1u

typedef enum {
	RAL_FRAME_SHELL_READY = 1,
	RAL_FRAME_SHELL_RECORDING,
	RAL_FRAME_SHELL_PRESENTED,
	RAL_FRAME_SHELL_CANCELED,
	RAL_FRAME_SHELL_SHUTDOWN
} ralFrameShellState_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t ownerGeneration;
	uint64_t frameGeneration;
	uintptr_t shellIdentity;
	ralFrameShellState_t state;
	qboolean ready;
} ralFrameShellReceipt_t;

typedef struct {
	ralFrameShellReceipt_t receipt;
	uint64_t lastFrameGeneration;
} ralFrameShell_t;

qboolean Ral_FrameShellInit( ralFrameShell_t *shell, ralBackendType_t backendType,
	uint64_t ownerGeneration, ralFrameShellReceipt_t *outReceipt );
qboolean Ral_FrameShellBegin( ralFrameShell_t *shell,
	const ralFrameShellReceipt_t *currentReceipt, uint64_t frameGeneration,
	ralFrameShellReceipt_t *outRecording );
qboolean Ral_FrameShellComplete( ralFrameShell_t *shell,
	const ralFrameShellReceipt_t *recordingReceipt,
	ralFrameShellReceipt_t *outPresented );
qboolean Ral_FrameShellCancel( ralFrameShell_t *shell,
	const ralFrameShellReceipt_t *recordingReceipt,
	ralFrameShellReceipt_t *outCanceled );
qboolean Ral_FrameShellShutdown( ralFrameShell_t *shell,
	const ralFrameShellReceipt_t *currentReceipt,
	ralFrameShellReceipt_t *outShutdown );
qboolean Ral_FrameShellReceiptExact( const ralFrameShellReceipt_t *a,
	const ralFrameShellReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
