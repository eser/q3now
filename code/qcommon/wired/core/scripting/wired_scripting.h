// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
wired_scripting.h -- Headless LuaJIT runtime for the wired engine

Common Lua scripting layer shared by client and headless server.
Provides: LuaJIT VM, sandbox, cvar metatable bridge, print -> Com_Log,
cmd() -> Cbuf_ExecuteText, file execution via engine FS.

Subsystems register additional Lua bindings via WiredScript_RegisterBindings()
before WiredScript_PostInit() runs them against the live VM.
===========================================================================
*/

#ifndef WIRED_SCRIPTING_H
#define WIRED_SCRIPTING_H

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

/* Enumerate the sandbox global table (_G) for console tab-completion.
   Invokes callback once per string-keyed global, passing the key name, its
   lua_type() value, and the opaque ctx.  Additionally, for every global
   whose value is a table, the callback is invoked once per string-keyed
   member with a synthesized "<table>.<member>" name (one level only --
   nested tables are reported but not descended into).  Names are valid
   only for the duration of the callback -- copy them if you need to keep
   them.  No-op when Lua is not initialised. */
void WiredScript_EnumerateGlobalsAndMembers(
	void (*callback)( const char *name, int type, void *ctx ), void *ctx );

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

/* ---- Chunk compile + cache ------------------------------------------ *
 *
 * Pre-compile a Lua chunk at parse time, hold a stable handle, invoke it
 * per-frame without re-parsing. Built on luaL_loadbuffer + luaL_ref into
 * LUA_REGISTRYINDEX; the returned ref survives across frames and is
 * released by WiredScript_ReleaseChunk.
 *
 * Sentinel for "no chunk / compile failed". 0 matches memset-zero
 * initialization of struct fields, so freshly-allocated items in
 * the parser pool need no extra init step. Lua's luaL_ref never
 * returns 0 for a valid ref (positive ints only) — we treat 0 and
 * LUA_REFNIL as "no chunk".
 */
#define WIRED_CHUNK_NOREF  (0)

/* Compile `text` into a chunk; return a non-negative ref or WIRED_CHUNK_NOREF
 * on syntax error / Lua not initialised. `chunkName` is a short identifier
 * used in Lua error messages (e.g. "<panel>/<item>.bind"). On error the
 * Lua error message is forwarded through Com_Log at SEV_ERROR. */
int  WiredScript_CompileChunk    ( const char *text, const char *chunkName );

/* Release a previously compiled chunk. Safe to call with WIRED_CHUNK_NOREF
 * (no-op) or with a ref already released (no-op). */
void WiredScript_ReleaseChunk    ( int chunkRef );

/* ---- Compiled-chunk invocation helpers ------------------------------ *
 *
 * Designed so cl_wired_clay.c can call compiled chunks WITHOUT including
 * lua.h. The chunk-array state is internal to wired_scripting.
 *
 * Pattern:
 *   int n = WiredScript_CallChunkArrayLen( ref );  // call, validate dense, return N
 *   for ( i = 1; i <= n; i++ ) {
 *       char buf[256];
 *       if ( WiredScript_ChunkArrayItemAsString( i, buf, sizeof(buf) ) ) { ... }
 *   }
 *   WiredScript_ChunkArrayRelease();
 *
 * Returns -1 on call error / non-table result / non-dense table (i.e. table
 * has a hole or a non-1..N integer key). All errors logged once via
 * Com_Log; chunks that consistently fail should be released by the caller.
 */
int      WiredScript_CallChunkArrayLen        ( int chunkRef );
qboolean WiredScript_ChunkArrayItemAsString   ( int index, char *out, size_t outSize );
qboolean WiredScript_ChunkArrayItemAsNumber   ( int index, double *out );

/* table-of-tables row data. Index is 1-based row in the array;
 * field is the table key to fetch (lua_getfield equivalent). Returns
 * qfalse when the row is not a table or the field is absent / not a
 * string-convertible value. */
qboolean WiredScript_ChunkArrayItemFieldAsString( int index, const char *field,
                                                   char *out, size_t outSize );

void     WiredScript_ChunkArrayRelease        ( void );

/* Single-value chunk invocations — call chunk with no arguments, return
 * one result. Errors logged once + return default. */
qboolean WiredScript_CallChunkBool            ( int chunkRef, qboolean defaultVal );
qboolean WiredScript_CallChunkString          ( int chunkRef, char *out, size_t outSize );
qboolean WiredScript_CallChunkNumber          ( int chunkRef, double *out );

/* ---- With-arguments invocation seam ---------------------------------- *
 *
 * For callers that include lua.h directly (e.g. the procedural-crosshair
 * pipeline calling update(state) and reading a nested result table). These
 * operate on the System VM returned by WiredScript_GetState():
 *
 *   lua_State *L = WiredScript_PushChunkForArgs( ref );   // chunk on top
 *   if ( L ) {
 *       lua_newtable( L ); ... // build the argument table
 *       if ( WiredScript_PCallArgs( 1, 1, "crosshair-update" ) ) {
 *           // result table on top of L; read it, then lua_pop
 *       }
 *   }
 *
 * PushChunkForArgs returns the live lua_State with the chunk function pushed,
 * or NULL on invalid ref / Lua down. PCallArgs pcalls with `nargs` args
 * already pushed, leaving `nResults`; logs + pops the error on failure. */
lua_State *WiredScript_PushChunkForArgs       ( int chunkRef );
qboolean   WiredScript_PCallArgs              ( int nargs, int nResults, const char *callerTag );

#endif /* WIRED_SCRIPTING_H */
