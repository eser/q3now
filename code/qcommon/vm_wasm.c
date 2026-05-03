/*
===========================================================================
Copyright (C) 2024-2026 q3now contributors

This file is part of q3now source code.

q3now source code is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as published
by the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

q3now source code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.
===========================================================================
*/

/*
 * vm_wasm.c — WASM VM backend via WAMR (WebAssembly Micro Runtime)
 *
 * Sole non-native VM backend (QVM removed):
 *   VM_WasmLoad()    — load .aot or .wasm module
 *   VM_CallWasm()    — call vmMain
 *   VM_WasmDestroy() — cleanup
 *
 * Syscall bridge:
 *   WASM module imports env.syscall(i32 × 13) → i32
 *   Bridge widens to intptr_t and calls vm->systemCall(args)
 *   vm_t* retrieved via wasm_runtime_get_user_data(exec_env)
 */

#include "q_feats.h"

#if FEAT_WASM

#include "vm_local.h"
#include <wasm_export.h>

/* ────────────────────────────────────────────────────────────────────── */
/*  Constants                                                            */
/* ────────────────────────────────────────────────────────────────────── */

#define WASM_STACK_SIZE       (256 * 1024)   /* 256 KB exec stack         */
#define WASM_HEAP_SIZE        (0)            /* no managed heap needed    */
#define WASM_MAX_SYSCALL_ARGS 13             /* callNum + 12 params       */
#define WASM_ERROR_BUF_SIZE   256
#define Q3NOW_WASM_API_VERSION 1
#define WASM_MAX_EXEC_ENVS    4              /* max reentrant call depth  */

/* ────────────────────────────────────────────────────────────────────── */
/*  WAMR runtime singleton (lazy-initialized)                            */
/* ────────────────────────────────────────────────────────────────────── */

static qboolean wamr_initialized = qfalse;

/*
 * Syscall bridge — called by WASM module via imported env.syscall.
 *
 * Uses WAMR's "raw" calling convention: all WASM i32 params are passed
 * in a uint64 argv[] array (each i32 zero-extended to uint64).
 * This avoids the invokeNative_aarch64.s float/int register split
 * which mis-routes i32 params into d0-d7 float registers on arm64.
 */
static void
wasm_syscall_bridge_raw( wasm_exec_env_t exec_env, uint64_t *argv )
{
	vm_t *vm = (vm_t *)wasm_runtime_get_user_data( exec_env );
	intptr_t args[WASM_MAX_SYSCALL_ARGS];

	for ( int i = 0; i < WASM_MAX_SYSCALL_ARGS; i++ ) {
		args[i] = (intptr_t)(uint32_t)argv[i];
	}

	/* Return value goes in argv[0] */
	argv[0] = (uint64_t)(uint32_t)vm->systemCall( args );
}

/* Native symbol registered with WAMR via register_natives_raw */
static NativeSymbol wasm_native_symbols_raw[] = {
	{
		"syscall",                         /* symbol name (import field)  */
		(void *)wasm_syscall_bridge_raw,   /* function pointer            */
		"(iiiiiiiiiiiii)i",                /* signature: 13 i32 → i32    */
		NULL                               /* attachment                  */
	}
};

static void VM_WasmInitRuntime( void )
{
	RuntimeInitArgs init_args;

	if ( wamr_initialized )
		return;

	memset( &init_args, 0, sizeof( init_args ) );
	init_args.mem_alloc_type = Alloc_With_System_Allocator;

	if ( !wasm_runtime_full_init( &init_args ) ) {
		COM_ERROR( LOG_CAT_SYSTEM, "WASM: failed to initialize WAMR runtime\n" );
		return;
	}

	/* Register syscall using raw calling convention —
	 * avoids arm64 float/int register split issue with 13+ params */
	if ( !wasm_runtime_register_natives_raw( "env",
	         wasm_native_symbols_raw,
	         sizeof( wasm_native_symbols_raw ) / sizeof( NativeSymbol ) ) ) {
		COM_ERROR( LOG_CAT_SYSTEM, "WASM: failed to register native syscall\n" );
		wasm_runtime_destroy();
		return;
	}

	wamr_initialized = qtrue;
	Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "WASM: WAMR runtime initialized\n" );
}

/* ────────────────────────────────────────────────────────────────────── */
/*  VM_WasmLoad                                                          */
/* ────────────────────────────────────────────────────────────────────── */

qboolean VM_WasmLoad( vm_t *vm )
{
	VM_WasmInitRuntime();
	if ( !wamr_initialized )
		return qfalse;

	int startTime = Sys_Milliseconds();
	qboolean isAot = qfalse;

	/* ── Try .aot first (AOT-compiled, near-native speed) ────────── */
	char filename[MAX_QPATH];
	Com_sprintf( filename, sizeof( filename ), "vm/%s.aot", vm->name );
	byte *buf = NULL;
	int fileLen = FS_ReadFile( filename, (void **)&buf );
	if ( fileLen > 0 && buf ) {
		isAot = qtrue;
	} else {
		/* ── Fall back to .wasm (interpreter) ────────────────────── */
		Com_sprintf( filename, sizeof( filename ), "vm/%s.wasm", vm->name );
		fileLen = FS_ReadFile( filename, (void **)&buf );
		if ( fileLen <= 0 || !buf ) {
			return qfalse;  /* neither found — caller handles fallback */
		}
	}

	/* ── Load module ─────────────────────────────────────────────── */
	char errorBuf[WASM_ERROR_BUF_SIZE];
	errorBuf[0] = '\0';
	wasm_module_t module = wasm_runtime_load( buf, (uint32_t)fileLen, errorBuf, sizeof( errorBuf ) );
	if ( !module ) {
		COM_WARN( LOG_CAT_SYSTEM, "WASM: failed to load %s: %s\n", filename, errorBuf );
		FS_FreeFile( buf );
		/* If .aot failed (wrong platform?), try .wasm fallback */
		if ( isAot ) {
			Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "WASM: .aot load failed, trying .wasm interpreter\n" );
			Com_sprintf( filename, sizeof( filename ), "vm/%s.wasm", vm->name );
			fileLen = FS_ReadFile( filename, (void **)&buf );
			if ( fileLen > 0 && buf ) {
				isAot = qfalse;
				module = wasm_runtime_load( buf, (uint32_t)fileLen, errorBuf, sizeof( errorBuf ) );
			}
		}
		if ( !module ) {
			if ( buf ) FS_FreeFile( buf );
			return qfalse;
		}
	}

	/* ── API version check via custom section ────────────────────── */
	{
		uint32_t sectionLen = 0;
		const uint8_t *section = wasm_runtime_get_custom_section( module, "q3now_api", &sectionLen );
		if ( section && sectionLen > 0 ) {
			char verStr[16];
			int ver;
			int copyLen = sectionLen < sizeof( verStr ) - 1 ? (int)sectionLen : (int)sizeof( verStr ) - 1;
			memcpy( verStr, section, copyLen );
			verStr[copyLen] = '\0';
			ver = atoi( verStr );
			if ( ver > Q3NOW_WASM_API_VERSION ) {
				COM_WARN( LOG_CAT_SYSTEM, "WASM: %s requires API v%d, engine has v%d\n",
				            vm->name, ver, Q3NOW_WASM_API_VERSION );
				wasm_runtime_unload( module );
				FS_FreeFile( buf );
				return qfalse;
			}
		}
	}

	/* ── Instantiate ─────────────────────────────────────────────── */
	errorBuf[0] = '\0';
	wasm_module_inst_t moduleInst = wasm_runtime_instantiate( module, WASM_STACK_SIZE, WASM_HEAP_SIZE,
	                                       errorBuf, sizeof( errorBuf ) );
	if ( !moduleInst ) {
		COM_WARN( LOG_CAT_SYSTEM, "WASM: failed to instantiate %s: %s\n", filename, errorBuf );
		wasm_runtime_unload( module );
		FS_FreeFile( buf );
		return qfalse;
	}

	/* ── Look up vmMain export ───────────────────────────────────── */
	wasm_function_inst_t funcVmMain = wasm_runtime_lookup_function( moduleInst, "vmMain" );
	if ( !funcVmMain ) {
		COM_WARN( LOG_CAT_SYSTEM, "WASM: %s does not export vmMain\n", filename );
		wasm_runtime_deinstantiate( moduleInst );
		wasm_runtime_unload( module );
		FS_FreeFile( buf );
		return qfalse;
	}

	/* ── Create execution environment ────────────────────────────── */
	wasm_exec_env_t execEnv = wasm_runtime_create_exec_env( moduleInst, WASM_STACK_SIZE );
	if ( !execEnv ) {
		COM_WARN( LOG_CAT_SYSTEM, "WASM: failed to create exec env for %s\n", filename );
		wasm_runtime_deinstantiate( moduleInst );
		wasm_runtime_unload( module );
		FS_FreeFile( buf );
		return qfalse;
	}

	/* Set vm_t* as user data so the syscall bridge can reach it */
	wasm_runtime_set_user_data( execEnv, vm );

	/* ── Set up linear memory for VMA() translation ──────────────── */
	wasm_memory_inst_t memoryInst = wasm_runtime_get_default_memory( moduleInst );
	uint32_t memSize = 0;
	if ( memoryInst ) {
		uint64_t memPages = wasm_memory_get_cur_page_count( memoryInst );
		memSize = (uint32_t)( memPages * 65536 );
		vm->dataBase = (byte *)wasm_runtime_addr_app_to_native( moduleInst, 0 );
		vm->dataMask = memSize - 1;
		vm->dataLength = memSize;
		vm->exactDataLength = memSize;
		vm->dataAlloc = memSize;
	} else {
		/* Module has no memory — unusual but not fatal */
		vm->dataBase = NULL;
		vm->dataMask = 0;
		vm->dataLength = 0;
		vm->dataAlloc = 0;
	}

	/* ── Store WASM state in vm_t ────────────────────────────────── */
	vm->wasmModule      = module;
	vm->wasmModuleInst  = moduleInst;
	vm->wasmExecEnv     = execEnv;
	vm->wasmFuncVmMain  = funcVmMain;
	vm->isWasm          = qtrue;
	vm->isWasmAot       = isAot;
	vm->destroy         = VM_WasmDestroy;

	/* Note: we intentionally do NOT free buf here.
	 * WAMR requires the buffer to remain valid until wasm_runtime_unload.
	 * It will be freed in VM_WasmDestroy. We store nothing extra —
	 * WAMR holds the reference internally. */

	Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "%s loaded as WASM %s (%u KB memory, %d ms)\n",
	            filename,
	            isAot ? "AOT" : "interpreter",
	            memSize / 1024,
	            Sys_Milliseconds() - startTime );

	return qtrue;
}

/* ────────────────────────────────────────────────────────────────────── */
/*  VM_CallWasm                                                          */
/* ────────────────────────────────────────────────────────────────────── */

int32_t VM_CallWasm( vm_t *vm, int nargs, int32_t *args )
{
	wasm_exec_env_t execEnv = (wasm_exec_env_t)vm->wasmExecEnv;
	wasm_function_inst_t func = (wasm_function_inst_t)vm->wasmFuncVmMain;
	wasm_module_inst_t inst = (wasm_module_inst_t)vm->wasmModuleInst;

	/* Guard: module not loaded or destroyed */
	if ( !execEnv || !func || !inst ) {
		COM_WARN( LOG_CAT_SYSTEM, "WASM: %s call skipped (module not loaded)\n", vm->name );
		return 0;
	}

	/* Clear any lingering exception */
	wasm_runtime_clear_exception( inst );

	uint32_t argv[MAX_VMMAIN_CALL_ARGS];
	for ( int i = 0; i < nargs && i < MAX_VMMAIN_CALL_ARGS; i++ ) {
		argv[i] = (uint32_t)args[i];
	}

	if ( !wasm_runtime_call_wasm( execEnv, func, nargs, argv ) ) {
		const char *exception = wasm_runtime_get_exception( inst );
		COM_ERROR( LOG_CAT_SYSTEM, "WASM: %s trap: %s\n",
		            vm->name, exception ? exception : "unknown error" );
		wasm_runtime_clear_exception( inst );
		vm->wasmExecEnv = NULL;
		Com_Terminate( TERM_CLIENT_DROP, "WASM: %s trap", vm->name );
		return 0;
	}

	/* Return value is in argv[0] after the call */
	return (int32_t)argv[0];
}

/* ────────────────────────────────────────────────────────────────────── */
/*  VM_WasmDestroy                                                       */
/* ────────────────────────────────────────────────────────────────────── */

void VM_WasmDestroy( vm_t *vm )
{
	if ( vm->wasmExecEnv ) {
		wasm_runtime_destroy_exec_env( (wasm_exec_env_t)vm->wasmExecEnv );
		vm->wasmExecEnv = NULL;
	}

	if ( vm->wasmModuleInst ) {
		wasm_runtime_deinstantiate( (wasm_module_inst_t)vm->wasmModuleInst );
		vm->wasmModuleInst = NULL;
	}

	if ( vm->wasmModule ) {
		wasm_runtime_unload( (wasm_module_t)vm->wasmModule );
		vm->wasmModule = NULL;
	}

	vm->wasmFuncVmMain = NULL;
	vm->isWasm = qfalse;
	vm->isWasmAot = qfalse;
}

#endif /* FEAT_WASM */
