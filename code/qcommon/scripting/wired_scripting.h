/*
===========================================================================
wired_scripting.h -- Headless LuaJIT runtime for q3now

Common Lua scripting layer shared by client and dedicated server.
Provides: LuaJIT VM, sandbox, cvar metatable bridge, print -> Com_Printf,
cmd() -> Cbuf_ExecuteText, file execution via engine FS.

Subsystems register additional Lua bindings via WiredScript_RegisterBindings()
before WiredScript_PostInit() runs them against the live VM.
===========================================================================
*/

#ifndef WIRED_SCRIPTING_H
#define WIRED_SCRIPTING_H

#if FEAT_LUA

/* Forward-declare lua_State so subsystems can extend the VM without
   pulling in the full Lua headers. */
typedef struct lua_State lua_State;

/* ---- Lifecycle ------------------------------------------------------- */

void WiredScript_Init( void );
void WiredScript_PostInit( void );
void WiredScript_Shutdown( void );

/* ---- Console integration --------------------------------------------- */

/* Try to evaluate a string as Lua.  Returns qtrue if it was handled as Lua,
   qfalse if it should fall through to the old command system. */
qboolean WiredScript_TryEval( const char *text );

/* Execute a .lua file from the game filesystem. */
void WiredScript_ExecFile( const char *filename );

/* ---- Binding registration -------------------------------------------- */

/* Callback signature for subsystem binding registrars.
   Called with the active Lua state during WiredScript_PostInit(). */
typedef void (*WiredScript_BindingFn)( lua_State *L );

/* Queue a binding registrar to run when WiredScript_PostInit() is called.
   Call this from subsystem init code (e.g. CL_Init) before PostInit runs. */
void WiredScript_RegisterBindings( WiredScript_BindingFn fn );

/* ---- Extension point for subsystems ---------------------------------- */

/* Return the active Lua state so subsystems can register
   additional bindings.  Returns NULL when Lua is not initialised. */
lua_State *WiredScript_GetState( void );

#endif /* FEAT_LUA */

#endif /* WIRED_SCRIPTING_H */
