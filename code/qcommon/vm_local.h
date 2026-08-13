// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef VM_LOCAL_H
#define VM_LOCAL_H

#include "q_shared.h"
#include "qcommon.h"
#include "q_feats.h"

// guard space at end of VM data segment — covers small syscall argument structs
// referenced by VMA() pointer translation in syscall handlers (e.g. sv_game.c)
#define VM_DATA_GUARD_SIZE 1024

// Fixed capacity of the per-VM arena (vmArena). The dominant occupant is the VM
// module file buffer; a .wasm module is sub-1 MB today and a future .aot variant
// is a few times larger, so this is sized with wide headroom. Arena_Alloc fails
// loudly (Com_Terminate) if exceeded, so the ceiling is generous on purpose.
#define VM_ARENA_SIZE ( 8 * 1024 * 1024 )

// Local cgame-VM capacity: how many client app-instances can coexist in this
// process, each owning one cgame VM. This is the LOCAL app-instance count, NOT
// the protocol MAX_CLIENTS (server player limit) — deliberately a separate,
// small bound. API capacity, not a forced count; single-app uses slot 0.
#define MAX_LOCAL_CGAME_VMS 4

// The single-app primary cgame slot. Today every VM_Create(VM_CGAME) routes
// here; the per-app spawn path will pass a real instance index instead. Named
// so that flip happens at the call site without re-touching VM_Create.
#define VM_APP_SLOT_PRIMARY 0

typedef struct vmSymbol_s {
	struct vmSymbol_s	*next;
	int		symValue;
	int		profileCount;
	char	symName[1];		// variable sized
} vmSymbol_t;

// VM teardown callbacks: each upper tier (client commands, file handles, UI
// viewport providers) registers its own cleanup at VM create / cgame init time;
// VM_Free invokes them in reverse registration order. So a pure VM_Free (map
// restart, client re-init) frees every resource the VM accreted without relying
// on the caller's cleanup order — and qcommon never calls client/UI cleanup
// directly (it only invokes the registered callback, the resource owner provides
// its own teardown function). `owner` is the opaque token the cleanup keys on
// (the vm handle, or a tier-specific context); the callback must NOT dereference
// the vm_t (it may be mid-teardown). MAX_VM_TEARDOWN_CALLBACKS bounds the array.
// vmTeardownCallback_t is declared in qcommon.h (the register call is cross-tier).
#define MAX_VM_TEARDOWN_CALLBACKS 8
typedef struct {
	void					*owner;
	vmTeardownCallback_t	cleanup;
} vmTeardownSlot_t;

struct vm_s {

	syscall_t	systemCall;
	byte		*dataBase;
	int32_t		*opStack;			// pointer to local function stack
	int32_t		*opStackTop;

	int32_t		programStack;		// the vm may be recursively entered
	int32_t		stackBottom;		// if programStack < stackBottom, error

	//------------------------------------

	const char	*name;				// module should be bare: "cgame", not "cgame.dll" or "vm/cgame.wasm"
	vmIndex_t	index;
	int			cgameInstance;		// per-app cgame slot (in-process-queue L7); 0 for VM_GAME
									// and for the single-app primary cgame. Identifies the
									// crash-reporter slot so N cgame VMs don't collapse onto one.
	void		*owner;				// app-instance handle for the multi-client per-app VM model;
									// consumed since 5.2.2.3 by the owner-guarded dedup in VM_Create
									// (zero-initialized by the VM_Create/VM_Free memset path).
	vmInterpret_t requestedInterpret;	// caller policy at VM_Create entry
	vmInterpret_t effectiveInterpret;	// live backend policy; restart authority

	// for dynamic linked modules
	void		*dllHandle;
	vmMainFunc_t entryPoint;
	dllSyscall_t dllSyscall;
	void (*destroy)(vm_t* self);

	// Physical path the module was loaded from, captured engine-side at load time
	// (the module has no idea where it lives). Native DLL → the loose OS path;
	// WASM → the loose OS path, or "<pak> :: <qpath>" when served from a pak.
	// Engine-private diagnostic (sysinfo "loaded-from"); never crosses the VM ABI.
	char		loadPath[ MAX_OSPATH ];

	// Per-VM arena, owned for the VM's whole lifetime: created in VM_Create,
	// whole-freed in VM_Free after the backend teardown. Backend-agnostic (the
	// VM owns it regardless of WASM / native / QVM). The module file buffer is
	// allocated here so a long-lived buffer never lives on the LIFO Hunk temp
	// stack, where module instantiation's temp churn would clobber it.
	arena_t		*vmArena;

	// Teardown-callback registry (see vmTeardownSlot_t above). Appended by
	// VM_RegisterTeardownCallback at create / cgame-init time; invoked in reverse
	// order by VM_Free after the backend teardown, before the arena + token clear.
	vmTeardownSlot_t	teardownCallbacks[ MAX_VM_TEARDOWN_CALLBACKS ];
	int					numTeardownCallbacks;

#if FEAT_WASM
	// for WASM modules (WAMR)
	void		*wasmModule;		// wasm_module_t*
	void		*wasmModuleInst;	// wasm_module_inst_t*
	void		*wasmExecEnv;		// wasm_exec_env_t*
	void		*wasmFuncVmMain;	// wasm_function_inst_t*
	byte		*wasmModuleBuf;		// module file bytes, allocated from vmArena; WAMR (esp. AOT)
									// references it in place until wasm_runtime_unload. Freed
									// wholesale by Arena_Destroy in VM_Free, not individually.
	qboolean	isWasm;
	qboolean	isWasmAot;			// loaded from .aot (near-native speed)
#endif

	qboolean	forceDataMask;

	// for WASM / native VMA pointer translation
	uint32_t	dataMask;
	uint32_t	dataLength;			// data segment length
	uint32_t	exactDataLength;	// from qvm header / wasm memory
	uint32_t	dataAlloc;			// actually allocated

	int			numSymbols;
	vmSymbol_t	*symbols;

	int			callLevel;			// counts recursive VM_Call
	int			breakFunction;		// increment breakCount on function entry to this
	int			breakCount;

	int			syscallCount;		// syscall counter for current VM_Call invocation

	int			privateFlag;
};


#if FEAT_WASM
qboolean VM_WasmLoad( vm_t *vm, qboolean allowAot );
int32_t VM_CallWasm( vm_t *vm, int nargs, int32_t *args );
void VM_WasmDestroy( vm_t *vm );
#endif

vmSymbol_t *VM_ValueToFunctionSymbol( vm_t *vm, int value );
int VM_SymbolToValue( vm_t *vm, const char *symbol );
const char *VM_ValueToSymbol( vm_t *vm, int value );

// Walks the WASM / native stack and writes a space-separated hex representation
// into buf. Format: "vm_name: 0xaddr 0xaddr ...".
// Safe to call from crash handlers — never allocates, never calls printf.
#define MAX_VM_CALLSTACK_DEPTH 64
void VM_GetCallStack( vm_t *vm, char *buf, int bufSize );

#endif // VM_LOCAL_H
