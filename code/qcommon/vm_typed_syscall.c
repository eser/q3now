// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// Typed VM-syscall unmarshalling (see vm_typed_syscall.h +
// docs/vm-typed-ipc-design.md). Validates the opaque args[] against a per-syscall
// descriptor and produces translated, bounds-checked typed values. The wire is
// the existing flat intptr_t args[13]; this only adds the engine-side typing.

#include "q_shared.h"
#include "qcommon.h"
#include "vm_typed_syscall.h"

qboolean VM_UnmarshalTyped( const struct vm_s *vm, const vmSyscallDesc_t *desc,
                            const intptr_t *args,
                            void *( *argptr )( intptr_t ),
                            vmTypedArg_t *out )
{
	int a;

	if ( !desc || !args || !out || !argptr )
		return qfalse;
	if ( desc->argc < 0 || desc->argc > VM_MAX_TYPED_ARGS )
		return qfalse;

	for ( a = 0; a < desc->argc; a++ ) {
		const intptr_t raw = args[ a + 1 ];   // args[0] is the call number
		out[a].type = desc->argt[a];
		out[a].i    = 0;
		out[a].f    = 0.0f;
		out[a].p    = NULL;
		out[a].len  = 0;

		switch ( desc->argt[a] ) {
		case VARG_INT:
			out[a].i = raw;
			break;
		case VARG_FLOAT: {
			floatint_t fi;
			fi.i = (int)raw;
			out[a].f = fi.f;
			break;
		}
		case VARG_VMPTR:
			// Translate the VM offset to a native pointer via the per-VM
			// translator (VM_ArgPtr: dataBase + (offset & dataMask), or raw for
			// a native DLL). NULL is allowed (the handler decides if that's ok).
			out[a].p = argptr( raw );
			break;
		case VARG_VMPTR_SIZED: {
			// The next arg is the byte length; range-check the offset+len against
			// the VM's data window before translating (the VMPTR's own arg is the
			// offset, the following arg is the length).
			int lenRaw;
			if ( a + 1 >= desc->argc )
				return qfalse;             // sized ptr must be followed by a length arg
			// The length is an `int` on the wire. Truncate to int before the sign +
			// bounds checks — a native-DLL syscall passes a 32-bit int in a 64-bit
			// intptr_t slot WITHOUT clearing the high 32 bits (varargs leaves them as
			// stack garbage), so the full intptr_t can read negative even when the
			// real length (low 32 bits) is positive. (See the COUNTED case below;
			// for QVM/WASM the value is already a clean 32-bit, so this is a no-op.)
			lenRaw = (int)args[ a + 2 ];
			if ( lenRaw < 0 )
				return qfalse;
			if ( vm )
				VM_CheckBounds( vm, (unsigned)raw, (unsigned)lenRaw );
			out[a].p   = argptr( raw );
			out[a].len = (unsigned)lenRaw;
			break;
		}
		case VARG_VMPTR_COUNTED: {
			// An array: count comes from another wire arg (desc->counted[a].countArg,
			// 1-based), element size is a compile-time sizeof in the descriptor. Range-
			// check (offset, count, elemSize) with VM_CheckBounds3 — the exact count×size
			// primitive the hand-written handlers used (and a no-op for a native DLL,
			// which VM_CheckBounds3 gates on vm->entryPoint internally).
			const int      countArg  = desc->counted[a].countArg;
			const unsigned elemSize  = desc->counted[a].elemSize;
			int            countRaw;
			if ( countArg < 1 || countArg > desc->argc || elemSize == 0 )
				return qfalse;             // mis-authored descriptor
			// The count is an `int` on the wire. A native-DLL syscall passes a 32-bit int
			// in a 64-bit intptr_t slot WITHOUT clearing the high 32 bits (varargs leaves
			// them as stack garbage), so the full intptr_t can read negative even when the
			// real count (low 32 bits) is positive. Truncate to int — matching how every
			// handler consumes the count (e.g. `(int)t[3].i`) — before the sign + bounds
			// checks. (QVM/WASM already deliver a clean 32-bit value; the truncation is a
			// no-op there and the correct read for a native DLL.)
			countRaw = (int)args[ countArg ];
			if ( countRaw < 0 )
				return qfalse;
			if ( vm )
				VM_CheckBounds3( vm, (unsigned)raw, (unsigned)countRaw, elemSize );
			out[a].p   = argptr( raw );
			out[a].len = (unsigned)countRaw * elemSize;   // byte length == count × elemSize
			break;
		}
		default:
			return qfalse;
		}
	}

	return qtrue;
}
