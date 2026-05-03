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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

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
        if ( ctx->used + grow > ctx->limit ) {
            return NULL;
        }
        ctx->used += grow;
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
            else             Com_Log( SEV_INFO, LOG_CAT_SCRIPTING, "\t" );
        }
        if ( s_capture ) uvm_capture_append( s );
        else             Com_Log( SEV_INFO, LOG_CAT_SCRIPTING, "%s", s );

        lua_pop( L, 1 );
    }

    if ( s_capture ) uvm_capture_append( "\n" );
    else             Com_Log( SEV_INFO, LOG_CAT_SCRIPTING, "\n" );

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
        Com_Terminate( TERM_UNRECOVERABLE, "UserVM_Init: lua_newstate failed (memory cap %zu MB)",
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
    lua_pushnil( s_L ); lua_setglobal( s_L, "debug" );
    lua_pushnil( s_L ); lua_setglobal( s_L, "package" );
    lua_pushnil( s_L ); lua_setglobal( s_L, "collectgarbage" );

    lua_pushcfunction( s_L, uvm_print );
    lua_setglobal( s_L, "print" );

    s_adminCtx = qfalse;
    s_capture  = NULL;

    Com_Log( SEV_INFO, LOG_CAT_SCRIPTING, "WiredCore/Scripting: UserVM initialized (cap %d MB, insn limit %d)\n",
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
        COM_WARN( LOG_CAT_SCRIPTING, "UserVM_RegisterBindings: table full (max %d)\n",
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
