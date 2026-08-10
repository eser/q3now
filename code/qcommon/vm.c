// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2012-2020 Quake3e project
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// vm.c -- virtual machine

/*


intermix code and data
symbol table

a dll has one imported function: VM_SystemCall
and one exported function: Perform


*/

#include "vm_local.h"
#include "q_feats.h"
#include "crash.h"
LOG_DECLARE_CHANNEL( ch_system, "system" );



#ifdef DEBUG
int		vm_debugLevel;
#endif

// used by Com_Error to get rid of running vm's before longjmp
static int forced_unload;

// Active-cgame-VM routing token (per-instance, both backends).
// The syscall callback (CL_DllSyscall / the WASM bridge -> CL_CgameSystemCalls)
// carries NO VM-identity across the ABI boundary, so to serve N cgame apps the
// handler must learn which app's VM is currently executing. This holds the VM
// whose vmMain is running RIGHT NOW. It is NOT a bare currentVM/lastVM global:
// it is SAVED ON THE C STACK and restored by each VM_Call frame (see VM_Call),
// so a nested engine->VM_Call(other vm)->engine->syscall chain correctly
// restores the caller's VM on return — arbitrary-depth recursion is preserved.
// Set on BOTH paths: the native branch wraps vm->entryPoint, the WASM branch
// wraps VM_CallWasm (a WASM module would otherwise leave it NULL and every
// pointer-arg translation in the handler would resolve to NULL). The
// deglobalized cgame syscall handler reads this (via VM_ActiveNativeVM()) for
// its in-syscall reads — the bare `cgvm` global is gone. Cleared in VM_Free on
// the ownership boundary so a Com_Terminate->Q_longjmp out of a syscall can't
// leave it dangling at a freed VM (single-app it equals the one cgame VM, so
// the clear is a happy-path no-op).
static vm_t *s_activeNativeVM = NULL;

vm_t *VM_ActiveNativeVM( void ) {
	return s_activeNativeVM;
}

// The per-app cgame slot index this VM was created for (its clientApps[] slot).
// Lets a cgame syscall handler resolve the EXECUTING app from VM_ActiveNativeVM()
// without including vm_local.h. 0 for the game VM / the single cgame.
int VM_CgameInstance( vm_t *vm ) {
	return vm ? vm->cgameInstance : 0;
}

// Server game VM storage. Indexed by vmIndex_t for VM_GAME; the VM_CGAME slot is
// dead (cgame VMs live in vmTable_cgame, below) and is never written or read —
// kept only so vmIndex_t bounds checks stay simple. The module load name comes
// from vmName[] regardless of which table holds the vm_t.
static struct vm_s vmTable_game[ VM_COUNT ];

// cgame VMs live in their own bounded array (one per local client app-instance)
// so N apps can each hold a cgame VM. Single-app uses slot 0 — behaviour is
// identical to the old single enum slot. Slot selection beyond 0 (owner-guarded)
// is a later step; nothing here picks > 0.
static struct vm_s vmTable_cgame[ MAX_LOCAL_CGAME_VMS ];

// Module load-identity names, parallel to vmIndex_t. "gamesv" = the server-app
// game VM, "gamecl" = the client-app game VM (the engine loads vm/<name>.{wasm,
// aot} / <name><arch> by these).
static const char *vmName[ VM_COUNT ] = {
	"gamesv",
	"gamecl"
};

// Resolve a vm index to its storage slot. VM_GAME -> the game table; VM_CGAME ->
// the cgame array (slot 0 today). cgameInstance selects the cgame app-slot.
static vm_t *VM_SlotFor( vmIndex_t index, int cgameInstance ) {
#ifndef HEADLESS
	if ( index == VM_CGAME ) {
		if ( cgameInstance < 0 || cgameInstance >= MAX_LOCAL_CGAME_VMS ) {
			Com_Terminate( TERM_CLIENT_DROP, "VM_SlotFor: bad cgame instance %i", cgameInstance );
		}
		return &vmTable_cgame[ cgameInstance ];
	}
#endif
	return &vmTable_game[ index ];
}

/*
==============
VM_LoadPath

Physical load-path of a loaded module, for the sysinfo "loaded-from" diagnostic.
VM_GAME reads the game slot; VM_CGAME reports the primary cgame app-slot (the one
sysinfo cares about). "" when the slot is not loaded.
==============
*/
const char *VM_LoadPath( vmIndex_t index ) {
#ifndef HEADLESS
	if ( index == VM_CGAME ) {
		const vm_t *vm = &vmTable_cgame[ VM_APP_SLOT_PRIMARY ];
		return ( vm->name && vm->loadPath[0] ) ? vm->loadPath : "";
	}
#endif
	if ( (unsigned)index < VM_COUNT ) {
		const vm_t *vm = &vmTable_game[ index ];
		return ( vm->name && vm->loadPath[0] ) ? vm->loadPath : "";
	}
	return "";
}

// Fill `out` with pointers to every live-capable VM storage slot: the server
// game slot + the cgame array (cgame does NOT live in vmTable_game). `out` must
// hold at least 1 + MAX_LOCAL_CGAME_VMS entries. Returns the count written.
// Slots may be inactive (vm->name == NULL); callers skip those.
static int VM_AllSlots( vm_t **out ) {
	int n = 0;
	out[ n++ ] = &vmTable_game[ VM_GAME ];
#ifndef HEADLESS
	for ( int i = 0; i < MAX_LOCAL_CGAME_VMS; i++ ) {
		out[ n++ ] = &vmTable_cgame[ i ];
	}
#endif
	return n;
}

static void VM_VmInfo_f( void );

#ifdef DEBUG
void VM_Debug( int level ) {
	vm_debugLevel = level;
}
#endif

/*
==============
VM_CheckBounds
==============
*/
void VM_CheckBounds( const vm_t *vm, unsigned int address, unsigned int length )
{
	//if ( !vm->entryPoint )
	{
		if ( (address | length) > vm->dataMask || (address + length) > vm->dataMask )
		{
			Com_Terminate( TERM_CLIENT_DROP, "program tried to bypass data segment bounds" );
		}
	}
}


/*
==============
VM_CheckBounds2
==============
*/
void VM_CheckBounds2( const vm_t *vm, unsigned int addr1, unsigned int addr2, unsigned int length )
{
	//if ( !vm->entryPoint )
	{
		if ( (addr1 | addr2 | length) > vm->dataMask || (addr1 + length) > vm->dataMask || (addr2+length) > vm->dataMask )
		{
			Com_Terminate( TERM_CLIENT_DROP, "program tried to bypass data segment bounds" );
		}
	}
}


/*
==============
VM_CheckBounds3
==============
*/
void VM_CheckBounds3( const vm_t *vm, unsigned int address, unsigned int count, unsigned int size )
{
	if ( !vm->entryPoint )
	{
		if ( (uint64_t)address + (uint64_t)count * size > vm->dataMask )
		{
			Com_Terminate( TERM_CLIENT_DROP, "program tried to bypass data segment bounds" );
		}
	}
}



#if FEAT_WASM
/*
==============
Cmd_ReloadWasm_f — force reload WASM modules
==============
*/
static void Cmd_ReloadWasm_f( void ) {
	vm_t *all[ VM_COUNT + MAX_LOCAL_CGAME_VMS ];
	int n = VM_AllSlots( all );
	for ( int i = 0; i < n; i++ ) {
		vm_t *vm = all[i];
		if ( vm->name && vm->isWasm ) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "Reloading %s...\n", vm->name );
			VM_Free( vm );
			vm->name = NULL;  // allow VM_Create to recreate
		}
	}
	Com_Log( SEV_INFO, LOG_CH(ch_system), "WASM modules unloaded. They will reload on next map.\n" );
}
#endif

/*
==============
VM_Init
==============
*/
void VM_Init( void ) {
#ifndef HEADLESS
	Cvar_Get( "vm_cgame", "2", CVAR_ARCHIVE | CVAR_PROTECTED );	// !@# SHIP WITH SET TO 2
#endif
	Cvar_Get( "vm_game", "2", CVAR_ARCHIVE | CVAR_PROTECTED );	// !@# SHIP WITH SET TO 2

	Cmd_AddCommand( "vminfo", VM_VmInfo_f );
#if FEAT_WASM
	Cmd_AddCommand( "reload_wasm", Cmd_ReloadWasm_f );
#endif

	memset( vmTable_game, 0, sizeof( vmTable_game ) );
	memset( vmTable_cgame, 0, sizeof( vmTable_cgame ) );
}


/*
===============
VM_ValueToSymbol

Assumes a program counter value
===============
*/
const char *VM_ValueToSymbol( vm_t *vm, int value ) {
	static char		text[MAX_TOKEN_CHARS];

	vmSymbol_t *sym = vm->symbols;
	if ( !sym ) {
		return "NO SYMBOLS";
	}

	// find the symbol
	while ( sym->next && sym->next->symValue <= value ) {
		sym = sym->next;
	}

	if ( value == sym->symValue ) {
		return sym->symName;
	}

	Com_sprintf( text, sizeof( text ), "%s+%i", sym->symName, value - sym->symValue );

	return text;
}


/*
===============
VM_ValueToFunctionSymbol

For profiling, find the symbol behind this value
===============
*/
vmSymbol_t *VM_ValueToFunctionSymbol( vm_t *vm, int value ) {
	static vmSymbol_t	nullSym;

	vmSymbol_t *sym = vm->symbols;
	if ( !sym ) {
		return &nullSym;
	}

	while ( sym->next && sym->next->symValue <= value ) {
		sym = sym->next;
	}

	return sym;
}


/*
===============
VM_SymbolToValue
===============
*/
int VM_SymbolToValue( vm_t *vm, const char *symbol ) {
	for ( vmSymbol_t *sym = vm->symbols ; sym ; sym = sym->next ) {
		if ( !strcmp( symbol, sym->symName ) ) {
			return sym->symValue;
		}
	}
	return 0;
}


/*
===============
ParseHex
===============
*/
static int	ParseHex( const char *text ) {
	int value = 0;
	int c;
	while ( ( c = (byte)*text++ ) != 0 ) {
		if ( c >= '0' && c <= '9' ) {
			value = value * 16 + c - '0';
			continue;
		}
		if ( c >= 'a' && c <= 'f' ) {
			value = value * 16 + 10 + c - 'a';
			continue;
		}
		if ( c >= 'A' && c <= 'F' ) {
			value = value * 16 + 10 + c - 'A';
			continue;
		}
	}

	return value;
}


#ifdef _DEBUG
/*
===============
VM_LoadSymbols
===============
*/
static void VM_LoadSymbols( vm_t *vm ) {
	union {
		char	*c;
		void	*v;
	} mapfile;

	char name[MAX_QPATH];
	COM_StripExtension(vm->name, name, sizeof(name));
	char symbols[MAX_QPATH];
	Com_sprintf( symbols, sizeof( symbols ), "vm/%s.map", name );
	FS_ReadFile( symbols, &mapfile.v );
	if ( !mapfile.c ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "Couldn't load symbol file: %s\n", symbols );
		return;
	}

	// parse the symbols
	const char *text_p = mapfile.c;
	vmSymbol_t **prev = &vm->symbols;
	int count = 0;

	ComParser parser = { 0 };
	while ( 1 ) {
		const char *token = COM_Parse( &parser, &text_p );
		if ( !token[0] ) {
			break;
		}
		int segment = ParseHex( token );
		if ( segment ) {
			COM_Parse( &parser, &text_p );
			COM_Parse( &parser, &text_p );
			continue;		// only load code segment values
		}

		token = COM_Parse( &parser, &text_p );
		if ( !token[0] ) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "WARNING: incomplete line at end of file\n" );
			break;
		}
		int value = ParseHex( token );

		token = COM_Parse( &parser, &text_p );
		if ( !token[0] ) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "WARNING: incomplete line at end of file\n" );
			break;
		}
		int chars = strlen( token );
		vmSymbol_t *sym = Hunk_Alloc( sizeof( *sym ) + chars, h_high );
		*prev = sym;
		prev = &sym->next;
		sym->next = NULL;

		sym->symValue = value;
		Q_strncpyz( sym->symName, token, chars + 1 );

		count++;
	}

	vm->numSymbols = count;
	Com_Log( SEV_INFO, LOG_CH(ch_system), "%i symbols parsed from %s\n", count, symbols );
	FS_FreeFile( mapfile.v );
}
#endif


/*
============
VM_DllSyscall

Dlls will call this directly

 rcg010206 The horror; the horror.

  The syscall mechanism relies on stack manipulation to get its args.
   This is likely due to C's inability to pass "..." parameters to
   a function in one clean chunk. On PowerPC Linux, these parameters
   are not necessarily passed on the stack, so while (&arg[0] == arg)
   is true, (&arg[1] == 2nd function parameter) is not necessarily
   accurate, as arg's value might have been stored to the stack or
   other piece of scratch memory to give it a valid address, but the
   next parameter might still be sitting in a register.

  Quake's syscall system also assumes that the stack grows downward,
   and that any needed types can be squeezed, safely, into a signed int.

  This hack below copies all needed values for an argument to a
   array in memory, so that Quake can get the correct values. This can
   also be used on systems where the stack grows upwards, as the
   presumably standard and safe stdargs.h macros are used.

  As for having enough space in a signed int for your datatypes, well,
   it might be better to wait for DOOM 3 before you start porting.  :)

  The original code, while probably still inherently dangerous, seems
   to work well enough for the platforms it already works on. Rather
   than add the performance hit for those platforms, the original code
   is still in use there.

  For speed, we just grab 15 arguments, and don't worry about exactly
   how many the syscall actually needs; the extra is thrown away.

============
*/

/*
=================
VM_Restart

Reload the data, but leave everything else in place
This allows a server to do a map_restart without changing memory allocation
=================
*/
vm_t *VM_Restart( vm_t *vm ) {
	// DLL's can't be restarted in place
	if ( vm->dllHandle ) {
		vmIndex_t index = vm->index;
		void *owner = vm->owner;
		syscall_t systemCall = vm->systemCall;
		dllSyscall_t dllSyscall = vm->dllSyscall;

		VM_Free( vm );

		// Only the game VM (DLL) restarts in place today; cgameInstance is
		// ignored for VM_GAME. (A cgame restart would need its own instance.)
		vm = VM_Create( index, VM_APP_SLOT_PRIMARY, owner, systemCall, dllSyscall, VMI_NATIVE );
		return vm;
	}

	COM_WARN( LOG_CH(ch_system), "WASM module cannot restart in place; freeing.\n" );
	VM_Free( vm );
	return NULL;
}


/*
=================
Sys_LoadDll

Used to load a development dll instead of a virtual machine

TTimo: added some verbosity in debug
=================
*/
static void * QDECL VM_LoadDll( const char *name, vmMainFunc_t *entryPoint, dllSyscall_t systemcalls, char *outPath, int outLen ) {
	char filename[ MAX_QPATH ];
	Com_sprintf( filename, sizeof( filename ), "%s" ARCH_STRING DLL_EXT, name );

	void *libHandle = FS_LoadLibrary( filename, outPath, outLen );

	if ( !libHandle ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_system), "VM_LoadDLL '%s' failed\n", filename );
		return NULL;
	}

	dllEntry_t dllEntry = /* ( dllEntry_t ) */ Sys_LoadFunction( libHandle, "dllEntry" );
	*entryPoint = /* ( dllSyscall_t ) */ Sys_LoadFunction( libHandle, "vmMain" );
	if ( !*entryPoint || !dllEntry ) {
		Sys_UnloadLibrary( libHandle );
		return NULL;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_system), "VM_LoadDll(%s): loaded, vmMain @ %p\n", name, *entryPoint );
	dllEntry( systemcalls );

	return libHandle;
}


/*
================
VM_Create

Loads a native shared library (VMI_NATIVE) or a WASM module
(VMI_BYTECODE / VMI_COMPILED) for the given VM index.
================
*/
vm_t *VM_Create( vmIndex_t index, int cgameInstance, void *owner, syscall_t systemCalls, dllSyscall_t dllSyscalls, vmInterpret_t interpret ) {
	if ( !systemCalls ) {
		Com_Terminate( TERM_UNRECOVERABLE, "VM_Create: bad parms" );
	}

	if ( (unsigned)index >= VM_COUNT ) {
		Com_Terminate( TERM_CLIENT_DROP, "VM_Create: bad vm index %i", index );
	}

	// VM_GAME uses the enum slot (cgameInstance ignored); cgame uses its bounded
	// array indexed by the per-app instance. At N=1 the only caller passes 0, so
	// the runtime resolution is unchanged (vmTable_cgame[0] == the primary slot).
	vm_t *vm = VM_SlotFor( index, cgameInstance );

	// see if we already have the VM
	if ( vm->name ) {
		if ( vm->index != index ) {
			Com_Terminate( TERM_CLIENT_DROP, "VM_Create: bad allocated vm index %i", vm->index );
			return NULL;
		}
		// A live slot handed back to a DIFFERENT owner is a lifecycle bug — the
		// slot was reused without being freed first, not a legitimate dedup hit.
		// Terminate loudly rather than silently return the wrong app's VM.
		if ( vm->owner != owner ) {
			Com_Terminate( TERM_CLIENT_DROP,
				"VM_Create: owner mismatch on live slot (lifecycle bug) for %s", vm->name );
			return NULL;
		}
		return vm;
	}

	const char *name = vmName[ index ];

	vm->name = name;
	vm->index = index;
	vm->cgameInstance = cgameInstance;	// crash-slot key (0 for game / primary cgame)
	vm->owner = owner;
	vm->systemCall = systemCalls;
	vm->dllSyscall = dllSyscalls;
	vm->privateFlag = CVAR_PRIVATE;

	// Per-VM arena: created before any backend load so the module file buffer
	// (allocated from it in the backend loader) never lives on the Hunk temp
	// stack. Whole-freed in VM_Free after backend teardown. On a failed load
	// VM_Create still calls VM_Free, so the arena is always reclaimed.
	vm->vmArena = Arena_Create( name, VM_ARENA_SIZE );

	// never allow dll loading with a demo
	if ( interpret == VMI_NATIVE ) {
		if ( Cvar_VariableIntegerValue( "fs_restrict" ) ) {
			interpret = VMI_COMPILED;
		}
	}

	if ( interpret == VMI_NATIVE ) {
		// try to load as a system dll
		Com_Log( SEV_INFO, LOG_CH(ch_system), "Loading dll file %s.\n", name );
		vm->dllHandle = VM_LoadDll( name, &vm->entryPoint, dllSyscalls, vm->loadPath, sizeof( vm->loadPath ) );
		if ( vm->dllHandle ) {
			vm->privateFlag = 0; // allow reading private cvars
			vm->dataAlloc = ~0U;
			vm->dataMask = ~0U;
			vm->dataBase = 0;
			Crash_SaveVMPointer( index, cgameInstance, vm );
			Crash_SaveVMChecksum( index, cgameInstance, 0 );
			return vm;
		}

		Com_Log( SEV_DEBUG, LOG_CH(ch_system), "Failed to load dll, falling back to WASM.\n" );
		interpret = VMI_COMPILED;
	}

#if FEAT_WASM
	if ( interpret >= VMI_COMPILED ) {
		// Auto-detect: prefer AOT (.aot), fall back to WASM interpreter (.wasm)
		if ( VM_WasmLoad( vm ) ) {
			Crash_SaveVMPointer( index, cgameInstance, vm );
			Crash_SaveVMChecksum( index, cgameInstance, 0 );
			return vm;
		}
	}
#endif

	// No WASM module found and no native DLL loaded
	COM_WARN( LOG_CH(ch_system), "No module found for %s (tried native + WASM).\n", name );
	VM_Free( vm );
	return NULL;
}


/*
==============
VM_Free
==============
*/
// Append a teardown callback (owner + cleanup fn) the VM will invoke at VM_Free.
// Each upper tier registers its own cleanup here so qcommon never calls
// client/UI teardown directly — the resource owner supplies the function pointer
// and the owner token it keys on. Clamps silently at the per-VM slot cap (the
// fixed set of cgame-side cleanups is well under the cap; an overflow would be a
// registration leak, not a crash).
void VM_RegisterTeardownCallback( vm_t *vm, void *owner, vmTeardownCallback_t cleanup ) {
	if ( !vm || !cleanup ) {
		return;
	}
	if ( vm->numTeardownCallbacks >= MAX_VM_TEARDOWN_CALLBACKS ) {
		Com_Log( SEV_WARN, LOG_CH( ch_system ),
			"VM_RegisterTeardownCallback(%s): slot cap %d reached; cleanup dropped\n",
			vm->name ? vm->name : "?", MAX_VM_TEARDOWN_CALLBACKS );
		return;
	}
	vm->teardownCallbacks[ vm->numTeardownCallbacks ].owner   = owner;
	vm->teardownCallbacks[ vm->numTeardownCallbacks ].cleanup = cleanup;
	vm->numTeardownCallbacks++;
}


void VM_Free( vm_t *vm ) {
	int i;

	if( !vm ) {
		return;
	}

	if ( vm->callLevel ) {
		if ( !forced_unload ) {
			Com_Terminate( TERM_UNRECOVERABLE, "VM_Free(%s) on running vm", vm->name );
			return;
		}
		Com_Log( SEV_INFO, LOG_CH( ch_system ), "forcefully unloading %s vm\n", vm->name );
	}

	if ( vm->destroy )
		vm->destroy( vm );

	// Invoke the registered teardown callbacks in REVERSE registration order, so
	// each upper tier's resources (commands, file handles, viewport providers) are
	// freed without relying on the caller (CL_ShutdownCGame) sweeping them in the
	// right order — a pure VM_Free is leak-free. Runs after the backend teardown
	// (the VM no longer executes) and before the arena destroy + struct wipe, so a
	// callback can key on its owner safely. Callbacks are plain cleanup functions:
	// no VM syscalls, no VM re-entry (safe even under forced_unload with
	// callLevel>0). They receive their `owner` token, never the vm_t (mid-teardown).
	for ( i = vm->numTeardownCallbacks - 1; i >= 0; i-- ) {
		if ( vm->teardownCallbacks[i].cleanup )
			vm->teardownCallbacks[i].cleanup( vm->teardownCallbacks[i].owner );
	}
	vm->numTeardownCallbacks = 0;

	// Whole-free the per-VM arena AFTER the backend teardown (vm->destroy ran
	// wasm_runtime_unload, releasing WAMR's in-place reference on the module
	// buffer). Destroying it before unload would be a use-after-free. Runs
	// unconditionally so a failed load (vm->destroy never set) still reclaims it.
	if ( vm->vmArena ) {
		Arena_Destroy( vm->vmArena );
		vm->vmArena = NULL;
	}

	if ( vm->dllHandle )
		Sys_UnloadLibrary( vm->dllHandle );

	// Clear crash-reporter state for this VM before wiping the struct.
	Crash_SaveVMPointer( vm->index, vm->cgameInstance, NULL );
	Crash_SaveVMChecksum( vm->index, vm->cgameInstance, 0 );

	// Active-cgame-VM routing token (s_activeNativeVM) is save/restored on the C
	// stack around the vmMain call in VM_Call — on BOTH the native entryPoint and
	// the WASM (VM_CallWasm) branch. A Com_Terminate inside a native OR WASM cgame
	// syscall (the latter also via a WAMR trap, which raises Com_Terminate) does
	// Q_longjmp(abortframe), unwinding past that restore and leaving the token
	// pointing at THIS slot — which the forced_unload teardown is about to memset.
	// Clear it here, at the ownership boundary, so the first handler read after
	// drop recovery never dereferences a freed VM. (callLevel leaks identically
	// and is the tolerated forced_unload pattern above; the token MUST be cleared
	// because CL_CgameSystemCalls reads it.) Every Q_longjmp escape routes through
	// CL_Disconnect -> CL_ShutdownCGame -> VM_Free BEFORE the longjmp, so this
	// clear runs first and covers both backends — the clear keys on the freed vm
	// pointer, not on how the token was set.
	if ( s_activeNativeVM == vm ) {
		s_activeNativeVM = NULL;
	}

	memset( vm, 0, sizeof( *vm ) );
}


// App-scoped level-transition teardown: free the host-wide game VM plus ONLY the
// owning app's cgame slot, leaving every other app's cgame VM intact. A mass sweep
// over every cgame slot would be fine at N=1 (one app, slot 0) but at N>1 would
// wrongly tear down a co-resident app's still-live cgame. At N=1 cgameInstance is
// 0, so this frees the game slot + slot 0 — the only two slots in use there.
void VM_ClearApp( int cgameInstance ) {
	VM_Free( &vmTable_game[ VM_GAME ] );
#ifndef HEADLESS
	if ( cgameInstance >= 0 && cgameInstance < MAX_LOCAL_CGAME_VMS )
		VM_Free( &vmTable_cgame[ cgameInstance ] );
#endif
}


void VM_Forced_Unload_Start(void) {
	forced_unload = 1;
}


void VM_Forced_Unload_Done(void) {
	forced_unload = 0;
}


/*
==============
VM_GetCallStack

Walks the interpreter/JIT program stack to build a human-readable call
trace. Used by the crash reporter and (when needed) by VM debug tooling.

Format: "vm_name: <sym1>+off1 <sym2>+off2 ...".

If the VM is a native DLL (no program stack, no symbols) we emit
"vm_name: native".

This function must be safe to call on a partially-torn-down VM: any
pointer may be NULL, and the walk stops as soon as we see a clearly
invalid stack offset.
==============
*/
void VM_GetCallStack( vm_t *vm, char *buf, int bufSize )
{
	if ( buf == NULL || bufSize <= 0 ) {
		return;
	}
	buf[ 0 ] = '\0';

	if ( vm == NULL ) {
		Q_strncpyz( buf, "(null vm)", bufSize );
		return;
	}

	Q_strncpyz( buf, vm->name ? vm->name : "?", bufSize );
	{
		qstring_t buf_qs = QS_WrapExisting( buf, bufSize );
		QS_Append( &buf_qs, ": " );

		if ( vm->entryPoint != NULL || vm->dllHandle != NULL ) {
			QS_Append( &buf_qs, "native" );
			return;
		}

#if FEAT_WASM
		QS_Appendf( &buf_qs, "wasm level=%d stack=0x%x", vm->callLevel, (unsigned)vm->programStack );
		return;
#else
		QS_Append( &buf_qs, "unknown" );
#endif
	}
}


/*
==============
VM_Call


Upon a system call, the stack will look like:

sp+32	parm1
sp+28	parm0
sp+24	return value
sp+20	return address
sp+16	local1
sp+14	local0
sp+12	arg1
sp+8	arg0
sp+4	return stack
sp		return address

An interpreted function will immediately execute
an OP_ENTER instruction, which will subtract space for
locals from sp
==============
*/

intptr_t QDECL VM_Call( vm_t *vm, int nargs, int callnum, ... )
{
	//vm_t	*oldVM;
	intptr_t r;

	if ( !vm ) {
		Com_Terminate( TERM_UNRECOVERABLE, "VM_Call with NULL vm" );
	}

	// Bound nargs in ALL builds: the WASM (wasm_args[MAX_VMMAIN_CALL_ARGS]) and
	// native arg arrays are fixed-size and the fill loop runs `nargs` times, so
	// an over-range nargs overflows them. This guard was previously under
	// `#ifdef DEBUG`, a macro the engine never defines (it uses `_DEBUG`), so it
	// was compiled out of every build. (The companion vm_debugLevel log was
	// dropped — that symbol was never declared, having only ever lived inside
	// the same never-compiled block.)
	if ( nargs >= MAX_VMMAIN_CALL_ARGS ) {
		Com_Terminate( TERM_CLIENT_DROP, "VM_Call: nargs >= MAX_VMMAIN_CALL_ARGS" );
	}

	// reset syscall counter for top-level calls to detect infinite loops
	if ( vm->callLevel == 0 ) {
		vm->syscallCount = 0;
	}

	++vm->callLevel;

	// if we have a dll loaded, call it directly
#if FEAT_WASM
	if ( vm->isWasm ) {
		int32_t wasm_args[MAX_VMMAIN_CALL_ARGS];
		va_list wasm_ap;
		int wasm_argc;
		memset( wasm_args, 0, sizeof( wasm_args ) );
		wasm_args[0] = callnum;
		va_start( wasm_ap, callnum );
		for ( int i = 0; i < nargs; i++ ) {
			wasm_args[i+1] = va_arg( wasm_ap, int32_t );
		}
		va_end( wasm_ap );
		// vmMain always expects at least 4 params: cmd, arg0, arg1, arg2
		wasm_argc = nargs + 1;
		if ( wasm_argc < 4 )
			wasm_argc = 4;

		// Route in-syscall reads to this VM for the duration of the call, the
		// same as the native branch below. The cgame syscall handler resolves
		// the executing VM (arg-pointer translation via dataBase/dataMask, the
		// private-cvar flag, bounds checks) through this token; a WASM module
		// would otherwise leave it NULL and every pointer arg would resolve to
		// NULL. Saved on the C stack and restored after, so a nested cross-VM
		// call restores the caller's VM — recursion-safe, not a bare global.
		vm_t *savedNativeVM = s_activeNativeVM;
		s_activeNativeVM = vm;

		r = VM_CallWasm( vm, wasm_argc, wasm_args );

		s_activeNativeVM = savedNativeVM;
	} else
#endif
	if ( vm->entryPoint )
	{
		//rcg010207 -  see dissertation at top of VM_DllSyscall() in this file.
		int32_t args[MAX_VMMAIN_CALL_ARGS-1];
		args[0] = args[1] = args[2] = 0;
		if ( nargs > 0 ) {
			va_list ap;
			va_start( ap, callnum );
			for ( int i = 0; i < nargs; i++ ) {
				args[i] = va_arg( ap, int32_t );
			}
			va_end( ap );
		}

		// Route native syscalls to this VM for the duration of the call. Saved
		// on the C stack and restored after, so a nested cross-VM call restores
		// the caller's VM — this is NOT a bare global (re-entrant, recursion-safe).
		vm_t *savedNativeVM = s_activeNativeVM;
		s_activeNativeVM = vm;

		// add more arguments if you're changed MAX_VMMAIN_CALL_ARGS:
		r = vm->entryPoint( callnum, args[0], args[1], args[2] );

		s_activeNativeVM = savedNativeVM;
	} else {
		Com_Terminate( TERM_CLIENT_DROP, "VM_Call: no module loaded for %s", vm->name );
		r = 0;
	}
	// NOLINTNEXTLINE(readability-misleading-indentation) — preceding `} else\n#endif\nif (...)` pattern fools the heuristic; this is correctly outside the chain
	--vm->callLevel;

	return r;
}


//=================================================================

/*
==============
VM_VmInfo_f
==============
*/
static void VM_VmInfo_f( void ) {
	vm_t *all[ VM_COUNT + MAX_LOCAL_CGAME_VMS ];
	int count = VM_AllSlots( all );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "Registered virtual machines:\n" );
	for ( int i = 0 ; i < count ; i++ ) {
		const vm_t *vm = all[i];
		if ( !vm->name ) {
			continue;
		}
		Com_Log( SEV_INFO, LOG_CH(ch_system), "%s : ", vm->name );
#if FEAT_WASM
		if ( vm->isWasm ) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "%s\n", vm->isWasmAot ? "WASM AOT" : "WASM interpreter" );
			Com_Log( SEV_INFO, LOG_CH(ch_system), "    data length : %7i\n", vm->dataMask + 1 );
			continue;
		}
#endif
		if ( vm->dllHandle ) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "native\n" );
			continue;
		}

		Com_Log( SEV_INFO, LOG_CH(ch_system), "unknown\n" );
	}
}
