// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef VM_INTERPRET_POLICY_H
#define VM_INTERPRET_POLICY_H

#include "q_shared.h"
#include "qcommon.h"

typedef struct {
	qboolean tryNative;
	qboolean allowAot;
	vmInterpret_t wasmInterpret;
} vmInterpretPolicy_t;

// Maps the public vmInterpret_t ABI to candidate policy. Output is byte-atomic
// on rejection. Values above VMI_COMPILED retain the historical compiled-WASM
// fallback behavior; negative values remain invalid.
qboolean VM_InterpretPolicy( vmInterpret_t requested, vmInterpretPolicy_t *out );

#endif
