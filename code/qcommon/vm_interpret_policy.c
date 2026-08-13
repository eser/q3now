// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "vm_interpret_policy.h"

qboolean VM_InterpretPolicy( vmInterpret_t requested, vmInterpretPolicy_t *out ) {
	vmInterpretPolicy_t policy;

	if ( !out || (int)requested < (int)VMI_NATIVE ) {
		return qfalse;
	}

	if ( requested == VMI_NATIVE ) {
		policy.tryNative = qtrue;
		policy.allowAot = qtrue;
		policy.wasmInterpret = VMI_COMPILED;
	} else if ( requested == VMI_BYTECODE ) {
		policy.tryNative = qfalse;
		policy.allowAot = qfalse;
		policy.wasmInterpret = VMI_BYTECODE;
	} else {
		// Preserve the pre-policy behavior for VMI_COMPILED and larger cvar
		// values: skip native and enter the AOT-with-WASM-fallback path.
		policy.tryNative = qfalse;
		policy.allowAot = qtrue;
		policy.wasmInterpret = VMI_COMPILED;
	}

	*out = policy;
	return qtrue;
}
