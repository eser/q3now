// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef _VM_TYPED_SYSCALL_H
#define _VM_TYPED_SYSCALL_H

// Typed VM-syscall dispatch (docs/vm-typed-ipc-design.md, decision 1 = A). The
// transport is the existing opaque intptr_t args[13]; this layers a per-syscall
// descriptor over it so a handler can unmarshal with argc + per-arg type + bounds
// checks instead of hand-casting VMA(x). The wire is UNCHANGED — no new syscall id,
// no struct on the wire, so the 32-bit-offset → 64-bit-pointer VMA translation is
// reused as-is.
//
// This is a real, always-compiled engine facility (ships in Release): every handler
// migrated to the typed path uses VM_UnmarshalTyped. Syscalls are migrated in
// reviewable batches (the hot/high-traffic subset first); un-migrated handlers keep
// their hand-written VMA(x) casts and read the same flat args[] — the two coexist on
// one wire, so there is nothing to "switch over". The descriptor catalogue grows one
// batch at a time.

#include "q_shared.h"

struct vm_s;   // fwd — full vm_t lives in qcommon.h (the .c includes it)

// Per-arg type. VMPTR is a VM linear-memory offset that must be translated via
// VM_ArgPtr (the dataBase + (offset & dataMask) contract); VMPTR_SIZED is a
// (offset, length) pair that is also range-checked with VM_CheckBounds; VMPTR_COUNTED
// is a (offset, element-count, element-size) array — count comes from another wire
// arg, element-size is a compile-time sizeof in the descriptor — range-checked with
// VM_CheckBounds3 (count*elemSize byte length).
typedef enum {
	VARG_INT,           // raw intptr (int/enum)
	VARG_FLOAT,         // i32 reinterpreted as float (VMF)
	VARG_VMPTR,         // VM offset → native pointer (VMA), NUL-terminated / caller-bounded
	VARG_VMPTR_SIZED,   // VM offset + the FOLLOWING arg is its byte length (bounds-checked)
	VARG_VMPTR_COUNTED  // VM offset of an array; count from desc->counted[a].countArg,
	                    // element size from desc->counted[a].elemSize (== VM_CheckBounds3)
} vmArgType_t;

// WASM_MAX_SYSCALL_ARGS = 13 (callnum + 12 params), so 12 was the practical max — but
// one game syscall (a 13-param AAS movement predictor) reads args[1..13], one past the
// nominal 12. Size the typed arrays to that true ceiling so every existing syscall fits.
#define VM_MAX_TYPED_ARGS 13

// Per-arg extra data for VARG_VMPTR_COUNTED (a parallel array, zero for every other
// arg type → existing descriptors are zero-init backward-compatible: a non-COUNTED arg
// leaves elemSize=0/countArg=0 and this struct is never consulted).
typedef struct {
	unsigned     elemSize;   // sizeof(element) — compile-time, NOT on the wire
	int          countArg;   // 1-based wire arg index holding the element count (args[countArg])
} vmCountedSpec_t;

typedef struct {
	int              id;                       // existing G_*/CG_* id — UNCHANGED
	const char      *name;                     // diagnostics
	int              argc;                      // params after args[0]
	vmArgType_t      argt[ VM_MAX_TYPED_ARGS ];
	vmCountedSpec_t  counted[ VM_MAX_TYPED_ARGS ]; // only used by VARG_VMPTR_COUNTED args
} vmSyscallDesc_t;

// A typed, validated view of one unmarshalled arg.
typedef struct {
	vmArgType_t  type;
	intptr_t     i;     // VARG_INT
	float        f;     // VARG_FLOAT
	void        *p;     // VARG_VMPTR / VARG_VMPTR_SIZED / VARG_VMPTR_COUNTED (translated)
	unsigned     len;   // VARG_VMPTR_SIZED byte length / VARG_VMPTR_COUNTED count*elemSize
} vmTypedArg_t;

// Validate + unmarshal args[] against desc using the supplied per-VM translator
// (VM_ArgPtr) and bounds checker. Returns qtrue on success; fills out[0..argc-1].
// On a bad argc / out-of-range pointer it returns qfalse (the caller falls back
// to the opaque path or errors). O(argc), no allocation, no struct copy — the same
// work the hand-written VMA(x) sequence does (a switch over arg types), just
// centralised + checked. Hot-path safe (W-41): nothing here that the inline casts
// didn't already do.
qboolean VM_UnmarshalTyped( const struct vm_s *vm, const vmSyscallDesc_t *desc,
                            const intptr_t *args,
                            void *( *argptr )( intptr_t ),
                            vmTypedArg_t *out );

// ── ABI version handshake (decision 2 = B, exact-match) ──────────────────────
// The engine's VM ABI version is the per-VM API version (GAME_API_VERSION /
// CGAME_IMPORT_API_VERSION). A module can query it via a reserved syscall id that
// is NOT part of the G_*/CG_* enum ordering (so adding it never shifts an existing
// id). 0x7FFF0000 is far above every real syscall number (G_* < 1000, BOTLIB <
// 700), so it can never collide with the enum.
#define VM_SYSCALL_ABI_QUERY  0x7FFF0000

#endif // _VM_TYPED_SYSCALL_H
