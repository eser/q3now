// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
user_vm.h — Untrusted Lua User VM (wired engine scripting layer)

Sandbox for rcon scripts, bot AI, and future mod code.  One shared
lua_State with a configurable allocator cap and a per-call coroutine
instruction limit.  System VM (wired_scripting.c) remains engine-trusted
with full access.

Memory cap:   user_vm_memory_mb cvar   (default 50 MB)
Insn limit:   user_vm_instruction_limit cvar (default 100 000 per rcon call)
===========================================================================
*/

// q_shared.h must be included by the translation unit before this header.

#ifndef USER_VM_H
#define USER_VM_H

typedef struct lua_State lua_State;

/* ---- Lifecycle ------------------------------------------------------- */

void UserVM_Init     ( void );
void UserVM_PostInit ( void );   /* fires all queued binding registrars */
void UserVM_Shutdown ( void );

/* ---- VM access ------------------------------------------------------- */

/* Returns NULL when the VM is not initialised. */
lua_State *UserVM_GetState( void );

/* ---- Binding registration -------------------------------------------- */

/* Callback signature for subsystem binding registrars.
   Called with the active Lua state during UserVM_PostInit(). */
typedef void (*UserVM_BindingFn)( lua_State *L );

/* Queue a binding registrar to run when UserVM_PostInit() is called.
   Must be called after UserVM_Init() and before UserVM_PostInit(). */
void UserVM_RegisterBindings( UserVM_BindingFn fn );

/* ---- Admin context (rcon privilege flag) ----------------------------- */

/* Set/clear the C-side admin flag around rcon coroutine execution.
   Privileged rcon bindings call UserVM_IsAdminContext() to verify they
   are not being invoked from bot scripts. */
void     UserVM_SetAdminContext( qboolean admin );
qboolean UserVM_IsAdminContext ( void );

/* ---- Rcon execution -------------------------------------------------- */

/* Execute a Lua snippet in a fresh coroutine with admin context set.
   Output is written to outBuf[0..outBufSize-1] (NUL-terminated).
   Returns qtrue on success, qfalse on Lua error (error string in outBuf). */
qboolean UserVM_RconExecute( const char *code, char *outBuf, int outBufSize );

#endif /* USER_VM_H */
