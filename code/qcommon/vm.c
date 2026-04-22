/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2012-2020 Quake3e project

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
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



#ifdef DEBUG
int		vm_debugLevel;
#endif

// used by Com_Error to get rid of running vm's before longjmp
static int forced_unload;

static struct vm_s vmTable[ VM_COUNT ];

static const char *vmName[ VM_COUNT ] = {
	"qagame",
	"cgame"
};

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
			Com_Error( ERR_DROP, "program tried to bypass data segment bounds" );
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
			Com_Error( ERR_DROP, "program tried to bypass data segment bounds" );
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
			Com_Error( ERR_DROP, "program tried to bypass data segment bounds" );
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
	for ( int i = 0; i < VM_COUNT; i++ ) {
		vm_t *vm = &vmTable[i];
		if ( vm->name && vm->isWasm ) {
			Com_Printf( "Reloading %s...\n", vm->name );
			VM_Free( vm );
			vm->name = NULL;  // allow VM_Create to recreate
		}
	}
	Com_Printf( "WASM modules unloaded. They will reload on next map.\n" );
}
#endif

/*
==============
VM_Init
==============
*/
void VM_Init( void ) {
#ifndef DEDICATED
	Cvar_Get( "vm_cgame", "2", CVAR_ARCHIVE | CVAR_PROTECTED );	// !@# SHIP WITH SET TO 2
#endif
	Cvar_Get( "vm_game", "2", CVAR_ARCHIVE | CVAR_PROTECTED );	// !@# SHIP WITH SET TO 2

	Cmd_AddCommand( "vminfo", VM_VmInfo_f );
#if FEAT_WASM
	Cmd_AddCommand( "reload_wasm", Cmd_ReloadWasm_f );
#endif

	memset( vmTable, 0, sizeof( vmTable ) );
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
	while ( ( c = *text++ ) != 0 ) {
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

	// don't load symbols if not developer
	if ( !com_developer->integer ) {
		return;
	}

	char name[MAX_QPATH];
	COM_StripExtension(vm->name, name, sizeof(name));
	char symbols[MAX_QPATH];
	Com_sprintf( symbols, sizeof( symbols ), "vm/%s.map", name );
	FS_ReadFile( symbols, &mapfile.v );
	if ( !mapfile.c ) {
		Com_Printf( "Couldn't load symbol file: %s\n", symbols );
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
			Com_Printf( "WARNING: incomplete line at end of file\n" );
			break;
		}
		int value = ParseHex( token );

		token = COM_Parse( &parser, &text_p );
		if ( !token[0] ) {
			Com_Printf( "WARNING: incomplete line at end of file\n" );
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
	Com_Printf( "%i symbols parsed from %s\n", count, symbols );
	FS_FreeFile( mapfile.v );
}


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
		syscall_t systemCall = vm->systemCall;
		dllSyscall_t dllSyscall = vm->dllSyscall;

		VM_Free( vm );

		vm = VM_Create( index, systemCall, dllSyscall, VMI_NATIVE );
		return vm;
	}

	Com_Printf( S_COLOR_YELLOW "WASM module cannot restart in place; freeing.\n" );
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
static void * QDECL VM_LoadDll( const char *name, vmMainFunc_t *entryPoint, dllSyscall_t systemcalls ) {
	char filename[ MAX_QPATH ];
	Com_sprintf( filename, sizeof( filename ), "%s" ARCH_STRING DLL_EXT, name );

	void *libHandle = FS_LoadLibrary( filename );

	if ( !libHandle ) {
		Com_DPrintf( "VM_LoadDLL '%s' failed\n", filename );
		return NULL;
	}

	dllEntry_t dllEntry = /* ( dllEntry_t ) */ Sys_LoadFunction( libHandle, "dllEntry" );
	*entryPoint = /* ( dllSyscall_t ) */ Sys_LoadFunction( libHandle, "vmMain" );
	if ( !*entryPoint || !dllEntry ) {
		Sys_UnloadLibrary( libHandle );
		return NULL;
	}

	Com_Printf( "VM_LoadDll(%s): loaded, vmMain @ %p\n", name, *entryPoint );
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
vm_t *VM_Create( vmIndex_t index, syscall_t systemCalls, dllSyscall_t dllSyscalls, vmInterpret_t interpret ) {
	if ( !systemCalls ) {
		Com_Error( ERR_FATAL, "VM_Create: bad parms" );
	}

	if ( (unsigned)index >= VM_COUNT ) {
		Com_Error( ERR_DROP, "VM_Create: bad vm index %i", index );
	}

	int remaining = Hunk_MemoryRemaining();

	vm_t *vm = &vmTable[ index ];

	// see if we already have the VM
	if ( vm->name ) {
		if ( vm->index != index ) {
			Com_Error( ERR_DROP, "VM_Create: bad allocated vm index %i", vm->index );
			return NULL;
		}
		return vm;
	}

	const char *name = vmName[ index ];

	vm->name = name;
	vm->index = index;
	vm->systemCall = systemCalls;
	vm->dllSyscall = dllSyscalls;
	vm->privateFlag = CVAR_PRIVATE;

	// never allow dll loading with a demo
	if ( interpret == VMI_NATIVE ) {
		if ( Cvar_VariableIntegerValue( "fs_restrict" ) ) {
			interpret = VMI_COMPILED;
		}
	}

	if ( interpret == VMI_NATIVE ) {
		// try to load as a system dll
		Com_Printf( "Loading dll file %s.\n", name );
		vm->dllHandle = VM_LoadDll( name, &vm->entryPoint, dllSyscalls );
		if ( vm->dllHandle ) {
			vm->privateFlag = 0; // allow reading private cvars
			vm->dataAlloc = ~0U;
			vm->dataMask = ~0U;
			vm->dataBase = 0;
			Crash_SaveVMPointer( index, vm );
			Crash_SaveVMChecksum( index, 0 );
			return vm;
		}

		Com_DPrintf( "Failed to load dll, falling back to WASM.\n" );
		interpret = VMI_COMPILED;
	}

#if FEAT_WASM
	if ( interpret >= VMI_COMPILED ) {
		// Auto-detect: prefer AOT (.aot), fall back to WASM interpreter (.wasm)
		if ( VM_WasmLoad( vm ) ) {
			Crash_SaveVMPointer( index, vm );
			Crash_SaveVMChecksum( index, 0 );
			return vm;
		}
	}
#endif

	// No WASM module found and no native DLL loaded
	Com_Printf( S_COLOR_YELLOW "No module found for %s (tried native + WASM).\n", name );
	VM_Free( vm );
	return NULL;
}


/*
==============
VM_Free
==============
*/
void VM_Free( vm_t *vm ) {

	if( !vm ) {
		return;
	}

	if ( vm->callLevel ) {
		if ( !forced_unload ) {
			Com_Error( ERR_FATAL, "VM_Free(%s) on running vm", vm->name );
			return;
		} else {
			Com_Printf( "forcefully unloading %s vm\n", vm->name );
		}
	}

	if ( vm->destroy )
		vm->destroy( vm );

	if ( vm->dllHandle )
		Sys_UnloadLibrary( vm->dllHandle );

	// Clear crash-reporter state for this VM before wiping the struct.
	Crash_SaveVMPointer( vm->index, NULL );
	Crash_SaveVMChecksum( vm->index, 0 );

	memset( vm, 0, sizeof( *vm ) );
}


void VM_Clear( void ) {
	for ( int i = 0; i < VM_COUNT; i++ ) {
		VM_Free( &vmTable[ i ] );
	}
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
		Com_Error( ERR_FATAL, "VM_Call with NULL vm" );
	}

#ifdef DEBUG
	if ( vm_debugLevel ) {
	  Com_Printf( "VM_Call( %d )\n", callnum );
	}

	if ( nargs >= MAX_VMMAIN_CALL_ARGS ) {
		Com_Error( ERR_DROP, "VM_Call: nargs >= MAX_VMMAIN_CALL_ARGS" );
	}
#endif

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
		r = VM_CallWasm( vm, wasm_argc, wasm_args );
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

		// add more arguments if you're changed MAX_VMMAIN_CALL_ARGS:
		r = vm->entryPoint( callnum, args[0], args[1], args[2] );
	} else {
		Com_Error( ERR_DROP, "VM_Call: no module loaded for %s", vm->name );
		r = 0;
	}
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
	Com_Printf( "Registered virtual machines:\n" );
	for ( int i = 0 ; i < VM_COUNT ; i++ ) {
		const vm_t *vm = &vmTable[i];
		if ( !vm->name ) {
			continue;
		}
		Com_Printf( "%s : ", vm->name );
#if FEAT_WASM
		if ( vm->isWasm ) {
			Com_Printf( "%s\n", vm->isWasmAot ? "WASM AOT" : "WASM interpreter" );
			Com_Printf( "    data length : %7i\n", vm->dataMask + 1 );
			continue;
		}
#endif
		if ( vm->dllHandle ) {
			Com_Printf( "native\n" );
			continue;
		}

		Com_Printf( "unknown\n" );
	}
}
