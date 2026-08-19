// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
user_vm.c — Untrusted Lua User VM

Single lua_State shared by bot AI, rcon scripts, and future mod code.
Custom allocator enforces a configurable cap (user_vm_memory_mb, default
50 MB).  Rcon invocations run in per-call coroutines isolated via
lua_newthread; admin-context flag prevents bot scripts from invoking
rcon-privileged bindings.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "user_vm.h"
#include "wired_scripting.h"   /* shared WIRED_CHUNK_NOREF sentinel */

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
LOG_DECLARE_CHANNEL( ch_scripting, "scripting" );

#define MAX_BINDING_REGISTRARS  16
#define DEFAULT_MEMORY_MB       50
#define DEFAULT_INSN_LIMIT      100000

/* ---- Internal types -------------------------------------------------- */

typedef struct {
    size_t  used;
    size_t  limit;
} uvm_alloc_ctx_t;

typedef struct {
    char    *output;
    int      outputLen;
    int      length;
    qboolean truncated;
} uvm_capture_t;

/* ---- Module state ---------------------------------------------------- */

static lua_State          *s_L             = NULL;
static uvm_alloc_ctx_t     s_allocCtx;
static qboolean            s_adminCtx      = qfalse;
static uvm_capture_t      *s_capture       = NULL;

static UserVM_BindingFn    s_registrars[ MAX_BINDING_REGISTRARS ];
static int                 s_numRegistrars = 0;

static cvar_t             *s_memoryMbCvar  = NULL;
static cvar_t             *s_insnLimitCvar = NULL;

/* Chunk-array API state (see chunk compile/cache section below). Declared
   here so UserVM_Shutdown can reset them on teardown. */
static int                 s_uvm_chunkArrayIdx    = 0;
static int                 s_uvm_chunkErrorWarned = 0;

/* ---- Custom allocator ------------------------------------------------ */

static void *uvm_alloc( void *ud, void *ptr, size_t osize, size_t nsize ) {
    uvm_alloc_ctx_t *ctx = (uvm_alloc_ctx_t *)ud;

    if ( nsize == 0 ) {
        if ( ptr ) {
            if ( ctx->used >= osize ) {
                ctx->used -= osize;
            } else {
                ctx->used = 0;
            }
            free( ptr );
        }
        return NULL;
    }

    if ( nsize > osize ) {
        size_t grow = nsize - osize;
        void  *np;
        if ( ctx->used + grow > ctx->limit ) {
            return NULL;
        }
        /* Commit the grow to the cap counter ONLY after realloc succeeds — a
         * NULL return (true system OOM, distinct from the cap rejection above)
         * must not permanently inflate ctx->used for memory never allocated. */
        np = realloc( ptr, nsize );
        if ( np ) {
            ctx->used += grow;
        }
        return np;
    } else {
        size_t shrink = osize - nsize;
        if ( ctx->used >= shrink ) {
            ctx->used -= shrink;
        } else {
            ctx->used = 0;
        }
    }

    return realloc( ptr, nsize );
}

/* ---- Instruction hook (set per rcon coroutine) ----------------------- */

static void uvm_hook( lua_State *L, lua_Debug *ar ) {
    (void)ar;
    luaL_error( L, "user VM instruction limit exceeded" );
}

/* ---- Capture helper -------------------------------------------------- */

static void uvm_capture_append( const char *text ) {
    int i;

    if ( !s_capture || !s_capture->output || !text ) {
        return;
    }
    for ( i = 0; text[i] != '\0'; i++ ) {
        if ( s_capture->length >= s_capture->outputLen - 1 ) {
            s_capture->truncated = qtrue;
            break;
        }
        s_capture->output[ s_capture->length++ ] = text[i];
    }
    s_capture->output[ s_capture->length ] = '\0';
}

/* ---- print override -------------------------------------------------- */

static int uvm_print( lua_State *L ) {
    int n = lua_gettop( L );
    int i;

    for ( i = 1; i <= n; i++ ) {
        const char *s;

        lua_getglobal( L, "tostring" );
        lua_pushvalue( L, i );
        lua_call( L, 1, 1 );
        s = lua_tostring( L, -1 );
        if ( !s ) s = "";

        if ( i > 1 ) {
            if ( s_capture ) uvm_capture_append( "\t" );
            else             Com_Log( SEV_INFO, LOG_CH(ch_scripting), "\t" );
        }
        if ( s_capture ) uvm_capture_append( s );
        else             Com_Log( SEV_INFO, LOG_CH(ch_scripting), "%s", s );

        lua_pop( L, 1 );
    }

    if ( s_capture ) uvm_capture_append( "\n" );
    else             Com_Log( SEV_INFO, LOG_CH(ch_scripting), "\n" );

    return 0;
}

/* ---- Lifecycle ------------------------------------------------------- */

void UserVM_Init( void ) {
    size_t memLimit;

    if ( s_L ) {
        UserVM_Shutdown();
    }

    {
        static const cvarDesc_t dm = CVAR_INT( "user_vm_memory_mb", "50", CVAR_ARCHIVE,
            "Memory budget in megabytes for the Lua scripting VM.", 1, 512 );
        s_memoryMbCvar = Cvar_Register( &dm );
    }
    {
        static const cvarDesc_t di = CVAR_INT( "user_vm_instruction_limit", "100000", CVAR_ARCHIVE,
            "Maximum Lua instructions executed per call before the VM is interrupted.", 0, 0 );
        s_insnLimitCvar = Cvar_Register( &di );
    }

    memLimit = (size_t)s_memoryMbCvar->integer * 1024u * 1024u;
    if ( memLimit == 0 ) {
        memLimit = (size_t)DEFAULT_MEMORY_MB * 1024u * 1024u;
    }

    s_allocCtx.used  = 0;
    s_allocCtx.limit = memLimit;

    s_L = lua_newstate( uvm_alloc, &s_allocCtx );
    if ( !s_L ) {
        /* Do NOT blame the budget. A NULL here almost never means the cap was
         * exhausted: nothing has been allocated yet, so uvm_alloc would have to
         * reject the very first request for that to be the cause. The old
         * message named the cap and sent debugging at a number that was never
         * involved.
         *
         * lua_newstate returns NULL for several unrelated reasons and reports
         * none of them — LuaJIT's own source says as much about the PRNG path:
         * "Can only return NULL here, so this errors with 'not enough memory'"
         * (lj_state.c). So the honest message is the list of candidates.
         *
         * OBSERVED: fails on linux/aarch64 in a container, succeeds on macOS
         * arm64, same commit. Two hypotheses were tested against that and BOTH
         * were disproved — recorded so nobody re-runs them:
         *   - "custom allocator needs low 32-bit addresses (LJ_64 && !LJ_GC64)":
         *     no. lj_arch.h sets LJ_TARGET_GC64=1 for arm64, so LJ_GC64 is
         *     always on there and that constraint never applies.
         *   - "secure PRNG seeding fails in the sandbox": no. /dev/urandom reads
         *     fine inside the container, and lj_prng.c falls back to it when
         *     SYS_getrandom is unavailable.
         * Note also that the engine's OTHER Lua state — created moments earlier
         * with LuaJIT's internal allocator — initialises fine in the same
         * process, so whatever this is, it is specific to passing a custom
         * allocator on that platform. See TASK-181. */
        Com_Terminate( TERM_UNRECOVERABLE,
            "UserVM_Init: lua_newstate returned NULL. The %zu MB budget is almost "
            "certainly NOT the cause - nothing had been allocated yet. Known to "
            "happen on linux/aarch64 while working on macOS arm64 with the same "
            "source. Root cause still open (see TASK-181); GC64 address limits "
            "and PRNG seeding have both been ruled out.",
            memLimit / ( 1024u * 1024u ) );
        return;
    }

    luaL_openlibs( s_L );

    /* Sandbox: nil out dangerous globals */
    lua_pushnil( s_L ); lua_setglobal( s_L, "io" );
    lua_pushnil( s_L ); lua_setglobal( s_L, "os" );
    lua_pushnil( s_L ); lua_setglobal( s_L, "require" );
    lua_pushnil( s_L ); lua_setglobal( s_L, "loadfile" );
    lua_pushnil( s_L ); lua_setglobal( s_L, "dofile" );
    lua_pushnil( s_L ); lua_setglobal( s_L, "load" );
    /* loadstring is LuaJIT's exact alias of load() (lib_base.c) and, like load,
     * accepts precompiled bytecode when called with no mode argument — leaving
     * it reachable is the same sandbox escape as leaving `load`, so nil it too. */
    lua_pushnil( s_L ); lua_setglobal( s_L, "loadstring" );
    lua_pushnil( s_L ); lua_setglobal( s_L, "debug" );
    lua_pushnil( s_L ); lua_setglobal( s_L, "package" );
    lua_pushnil( s_L ); lua_setglobal( s_L, "collectgarbage" );

    lua_pushcfunction( s_L, uvm_print );
    lua_setglobal( s_L, "print" );

    s_adminCtx = qfalse;
    s_capture  = NULL;

    Com_Log( SEV_INFO, LOG_CH(ch_scripting), "WiredCore/Scripting: UserVM initialized (cap %d MB, insn limit %d)\n",
                s_memoryMbCvar->integer, s_insnLimitCvar->integer );
}

void UserVM_PostInit( void ) {
    int i;

    if ( !s_L ) {
        return;
    }
    for ( i = 0; i < s_numRegistrars; i++ ) {
        s_registrars[i]( s_L );
    }
}

void UserVM_Shutdown( void ) {
    if ( s_L ) {
        lua_close( s_L );
        s_L = NULL;
    }
    s_allocCtx.used  = 0;
    s_allocCtx.limit = 0;
    s_adminCtx       = qfalse;
    s_capture        = NULL;
    s_numRegistrars  = 0;
    /* Reset chunk-array static state so a VM reload starts clean. The held
       table (if any) lived on s_L's stack, which lua_close just destroyed —
       so we only clear the index; there is nothing to lua_pop. Likewise the
       once-only warning gate must reset so the next VM logs its own warnings. */
    s_uvm_chunkArrayIdx    = 0;
    s_uvm_chunkErrorWarned = 0;
}

/* ---- VM access ------------------------------------------------------- */

lua_State *UserVM_GetState( void ) {
    return s_L;
}

/* ---- Binding registration -------------------------------------------- */

void UserVM_RegisterBindings( UserVM_BindingFn fn ) {
    if ( !fn ) {
        return;
    }
    if ( s_numRegistrars >= MAX_BINDING_REGISTRARS ) {
        COM_WARN( LOG_CH(ch_scripting), "UserVM_RegisterBindings: table full (max %d)\n",
                    MAX_BINDING_REGISTRARS );
        return;
    }
    s_registrars[ s_numRegistrars++ ] = fn;
}

/* ---- Admin context --------------------------------------------------- */

void UserVM_SetAdminContext( qboolean admin ) {
    s_adminCtx = admin;
}

qboolean UserVM_IsAdminContext( void ) {
    return s_adminCtx;
}

/* ---- Rcon execution -------------------------------------------------- */

qboolean UserVM_RconExecute( const char *code, char *outBuf, int outBufSize ) {
    lua_State    *co;
    int           co_ref;
    int           insnLimit;
    int           status;
    uvm_capture_t capture;
    char          expr[1024];

    if ( !s_L || !code || !outBuf || outBufSize <= 0 ) {
        return qfalse;
    }

    outBuf[0]         = '\0';
    capture.output    = outBuf;
    capture.outputLen = outBufSize;
    capture.length    = 0;
    capture.truncated = qfalse;
    s_capture         = &capture;

    insnLimit = s_insnLimitCvar ? s_insnLimitCvar->integer : DEFAULT_INSN_LIMIT;
    if ( insnLimit <= 0 ) {
        insnLimit = DEFAULT_INSN_LIMIT;
    }

    /* Fresh coroutine per rcon call — isolated stack, shared globals.
       Anchored in the registry so GC can't collect it during execution.
       Unrefed after pcall: becomes eligible for GC immediately. */
    co     = lua_newthread( s_L );
    co_ref = luaL_ref( s_L, LUA_REGISTRYINDEX );

    lua_sethook( co, uvm_hook, LUA_MASKCOUNT, insnLimit );

    /* Try as an expression first (return <code>), fall back to statement. */
    Com_sprintf( expr, sizeof( expr ), "return %s", code );
    status = luaL_loadstring( co, expr );
    if ( status != 0 ) {
        lua_pop( co, 1 );
        status = luaL_loadstring( co, code );
    }

    UserVM_SetAdminContext( qtrue );
    if ( status == 0 ) {
        status = lua_pcall( co, 0, LUA_MULTRET, 0 );
    }
    UserVM_SetAdminContext( qfalse );

    lua_sethook( co, NULL, 0, 0 );

    if ( status != 0 ) {
        const char *err = lua_tostring( co, -1 );
        if ( !err ) err = "unknown lua error";
        Com_sprintf( outBuf, outBufSize, "error: %s", err );
        lua_pop( co, 1 );
        luaL_unref( s_L, LUA_REGISTRYINDEX, co_ref );
        s_capture = NULL;
        return qfalse;
    }

    if ( capture.truncated ) {
        uvm_capture_append( "... (output truncated)\n" );
    }

    lua_settop( co, 0 );
    luaL_unref( s_L, LUA_REGISTRYINDEX, co_ref );
    s_capture = NULL;
    return qtrue;
}

/* ---- Chunk compile + cache ----------
 *
 * Symmetric API to WiredScript_*Chunk* (wired_scripting.c). Compiles
 * into the User VM's lua_State (s_L) registry. Refs are positive ints,
 * with 0 reserved as WIRED_CHUNK_NOREF (matches memset-zero default).
 * Each VM has its own registry, so System-VM and User-VM ref namespaces
 * are independent — callers MUST route through wuiLuaVMOps_t dispatcher
 * to use the right release/call functions for the ref's VM.
 *
 * Chunks compiled here are subject to the User VM's memory cap; out-of-
 * memory at compile time returns WIRED_CHUNK_NOREF with a logged error.
 * Execution is NOT wrapped in a coroutine (so no instruction-limit
 * isolation per-call). The per-call insn limit is rcon-coroutine-specific;
 * WiredUI bind chunks run in the main thread with the default Lua
 * dynamic memory bounds, same as the System VM.
 *
 * NOTE: this chunk compile/cache/call API is an intentional tier-separated
 * copy of WiredScript_*Chunk* in wired_scripting.c — that file is the trusted
 * System VM (LuaJIT, full engine bindings); this is the untrusted User VM
 * (memory-capped, instruction-limited, distinct lua_State). They cannot share
 * one implementation without threading the lua_State + per-VM config through
 * every public signature (these are dispatched via wuiLuaVMOps_t), so both are
 * kept separate by tier rule. The s_uvm_chunkArrayIdx / s_uvm_chunkErrorWarned
 * statics are declared in the module-state section above (so UserVM_Shutdown
 * can reset them); they are NOT redeclared here.
 */

int UserVM_CompileChunk( const char *text, const char *chunkName ) {
    int status;
    int ref;

    if ( !s_L ) return WIRED_CHUNK_NOREF;
    if ( !text || !*text ) return WIRED_CHUNK_NOREF;

    status = luaL_loadbuffer( s_L, text, strlen( text ),
                              chunkName ? chunkName : "uvm-chunk" );
    if ( status != 0 ) {
        const char *err = lua_tostring( s_L, -1 );
        Com_Log( SEV_WARN, LOG_CH(ch_scripting),
            "UserVM: compile failed for '%s': %s\n",
            chunkName ? chunkName : "?", err ? err : "(no message)" );
        lua_pop( s_L, 1 );
        return WIRED_CHUNK_NOREF;
    }

    ref = luaL_ref( s_L, LUA_REGISTRYINDEX );
    if ( ref == LUA_REFNIL || ref == LUA_NOREF || ref <= 0 ) {
        return WIRED_CHUNK_NOREF;
    }
    return ref;
}

void UserVM_ReleaseChunk( int chunkRef ) {
    if ( !s_L ) return;
    if ( chunkRef == WIRED_CHUNK_NOREF || chunkRef == LUA_NOREF || chunkRef == LUA_REFNIL || chunkRef <= 0 ) return;
    luaL_unref( s_L, LUA_REGISTRYINDEX, chunkRef );
}

static qboolean uvm_chunk_push( int chunkRef, const char *tag ) {
    if ( !s_L ) return qfalse;
    if ( chunkRef == WIRED_CHUNK_NOREF || chunkRef <= 0 ) return qfalse;
    lua_rawgeti( s_L, LUA_REGISTRYINDEX, chunkRef );
    if ( !lua_isfunction( s_L, -1 ) ) {
        lua_pop( s_L, 1 );
        if ( !s_uvm_chunkErrorWarned ) {
            Com_Log( SEV_WARN, LOG_CH(ch_scripting),
                "UserVM: chunk ref %d (%s) is not a function\n",
                chunkRef, tag ? tag : "?" );
            s_uvm_chunkErrorWarned = 1;
        }
        return qfalse;
    }
    return qtrue;
}

static qboolean uvm_chunk_pcall( int nResults, const char *tag ) {
    int status;
    if ( !s_L ) return qfalse;
    status = lua_pcall( s_L, 0, nResults, 0 );
    if ( status != 0 ) {
        const char *err = lua_tostring( s_L, -1 );
        Com_Log( SEV_WARN, LOG_CH(ch_scripting),
            "UserVM: chunk call failed (%s): %s\n",
            tag ? tag : "?", err ? err : "(no message)" );
        lua_pop( s_L, 1 );
        return qfalse;
    }
    return qtrue;
}

static int uvm_chunk_array_validate( int tableIdx ) {
    int n, i;
    if ( !lua_istable( s_L, tableIdx ) ) return -1;
    n = lua_objlen( s_L, tableIdx );
    if ( n < 0 ) return -1;
    for ( i = 1; i <= n; i++ ) {
        lua_rawgeti( s_L, tableIdx, i );
        if ( lua_isnil( s_L, -1 ) ) {
            lua_pop( s_L, 1 );
            return -1;
        }
        lua_pop( s_L, 1 );
    }
    return n;
}

int UserVM_CallChunkArrayLen( int chunkRef ) {
    int n;

    if ( s_uvm_chunkArrayIdx != 0 ) {
        UserVM_ChunkArrayRelease();
    }

    if ( !uvm_chunk_push( chunkRef, "array-chunk" ) ) return -1;
    if ( !uvm_chunk_pcall( 1, "array-chunk" ) ) return -1;

    n = uvm_chunk_array_validate( -1 );
    if ( n < 0 ) {
        lua_pop( s_L, 1 );
        if ( !s_uvm_chunkErrorWarned ) {
            Com_Log( SEV_WARN, LOG_CH(ch_scripting),
                "UserVM: chunk returned non-dense / non-array — Q-3=a\n" );
            s_uvm_chunkErrorWarned = 1;
        }
        return -1;
    }

    if ( n == 0 ) {
        /* Empty table: caller's `n <= 0` early-out won't Release, so pop the
         * table here instead of leaking the stack slot / holding s_uvm_chunkArrayIdx. */
        lua_pop( s_L, 1 );
        s_uvm_chunkArrayIdx = 0;
        return 0;
    }

    s_uvm_chunkArrayIdx = lua_gettop( s_L );
    return n;
}

qboolean UserVM_ChunkArrayItemAsString( int index, char *out, size_t outSize ) {
    const char *s;
    if ( s_uvm_chunkArrayIdx == 0 || !out || outSize == 0 ) return qfalse;
    lua_rawgeti( s_L, s_uvm_chunkArrayIdx, index );
    s = lua_tostring( s_L, -1 );
    if ( s ) Q_strncpyz( out, s, outSize );
    lua_pop( s_L, 1 );
    return s != NULL;
}

qboolean UserVM_ChunkArrayItemAsNumber( int index, double *out ) {
    int        isnum;
    lua_Number n;
    if ( s_uvm_chunkArrayIdx == 0 || !out ) return qfalse;
    lua_rawgeti( s_L, s_uvm_chunkArrayIdx, index );
    isnum = lua_isnumber( s_L, -1 );
    n = lua_tonumber( s_L, -1 );
    lua_pop( s_L, 1 );
    if ( isnum ) *out = (double) n;
    return isnum ? qtrue : qfalse;
}

qboolean UserVM_ChunkArrayItemFieldAsString( int index, const char *field,
                                              char *out, size_t outSize ) {
    const char *s;
    if ( s_uvm_chunkArrayIdx == 0 || !out || outSize == 0 || !field ) return qfalse;
    lua_rawgeti( s_L, s_uvm_chunkArrayIdx, index );
    if ( !lua_istable( s_L, -1 ) ) {
        lua_pop( s_L, 1 );
        return qfalse;
    }
    lua_getfield( s_L, -1, field );
    s = lua_tostring( s_L, -1 );
    if ( s ) Q_strncpyz( out, s, outSize );
    lua_pop( s_L, 2 );
    return s != NULL;
}

void UserVM_ChunkArrayRelease( void ) {
    if ( s_uvm_chunkArrayIdx == 0 ) return;
    lua_pop( s_L, 1 );
    s_uvm_chunkArrayIdx = 0;
}

qboolean UserVM_CallChunkBool( int chunkRef, qboolean defaultVal ) {
    qboolean result = defaultVal;
    if ( !uvm_chunk_push( chunkRef, "bool-chunk" ) ) return defaultVal;
    if ( !uvm_chunk_pcall( 1, "bool-chunk" ) ) return defaultVal;
    if ( lua_isboolean( s_L, -1 ) ) {
        result = lua_toboolean( s_L, -1 ) ? qtrue : qfalse;
    } else if ( !lua_isnil( s_L, -1 ) ) {
        result = qtrue;
    } else {
        result = qfalse;
    }
    lua_pop( s_L, 1 );
    return result;
}

qboolean UserVM_CallChunkString( int chunkRef, char *out, size_t outSize ) {
    const char *s;
    if ( !out || outSize == 0 ) return qfalse;
    if ( !uvm_chunk_push( chunkRef, "string-chunk" ) ) return qfalse;
    if ( !uvm_chunk_pcall( 1, "string-chunk" ) ) return qfalse;
    s = lua_tostring( s_L, -1 );
    if ( s ) Q_strncpyz( out, s, outSize );
    lua_pop( s_L, 1 );
    return s != NULL;
}

qboolean UserVM_CallChunkNumber( int chunkRef, double *out ) {
    int        isnum;
    lua_Number n;
    if ( !out ) return qfalse;
    if ( !uvm_chunk_push( chunkRef, "number-chunk" ) ) return qfalse;
    if ( !uvm_chunk_pcall( 1, "number-chunk" ) ) return qfalse;
    isnum = lua_isnumber( s_L, -1 );
    n = lua_tonumber( s_L, -1 );
    lua_pop( s_L, 1 );
    if ( isnum ) *out = (double) n;
    return isnum ? qtrue : qfalse;
}
