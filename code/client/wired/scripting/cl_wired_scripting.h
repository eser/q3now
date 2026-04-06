/*
===========================================================================
cl_wired_scripting.h -- Wired Scripting: LuaJIT integration

Phase 5: LuaJIT REPL console, cvar metatable bridge, store Lua API.
No Lua in the render path -- Lua runs on events only.
===========================================================================
*/

#ifndef CL_WIRED_SCRIPTING_H
#define CL_WIRED_SCRIPTING_H

#if FEAT_LUA

void WiredScript_Init( void );
void WiredScript_Shutdown( void );

/* Try to evaluate a string as Lua. Returns qtrue if it was handled as Lua,
   qfalse if it should fall through to the old command system. */
qboolean WiredScript_TryEval( const char *text );

/* Execute a .lua file from the game filesystem */
void WiredScript_ExecFile( const char *filename );

#endif /* FEAT_LUA */

#endif /* CL_WIRED_SCRIPTING_H */
