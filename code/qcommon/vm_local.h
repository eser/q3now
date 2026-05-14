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

typedef struct vmSymbol_s {
	struct vmSymbol_s	*next;
	int		symValue;
	int		profileCount;
	char	symName[1];		// variable sized
} vmSymbol_t;

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

	// for dynamic linked modules
	void		*dllHandle;
	vmMainFunc_t entryPoint;
	dllSyscall_t dllSyscall;
	void (*destroy)(vm_t* self);

#if FEAT_WASM
	// for WASM modules (WAMR)
	void		*wasmModule;		// wasm_module_t*
	void		*wasmModuleInst;	// wasm_module_inst_t*
	void		*wasmExecEnv;		// wasm_exec_env_t*
	void		*wasmFuncVmMain;	// wasm_function_inst_t*
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
qboolean VM_WasmLoad( vm_t *vm );
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
