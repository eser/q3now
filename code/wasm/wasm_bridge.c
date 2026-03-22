/*
===========================================================================
wasm_bridge.c — varargs adapter for WASM game modules

Compiled into WASM modules (not the engine). Provides the `syscall`
function that g_syscalls.c/cg_syscalls.c/ui_syscalls.c call via extern.

The WASM module imports env.syscall with a fixed 13-param signature.
This file bridges the variadic C calling convention to that fixed import.
===========================================================================
*/

#include <stdint.h>
#include <stdarg.h>

/* Imported from engine host — fixed 13-param i32 signature */
__attribute__((import_module("env"), import_name("syscall")))
extern int32_t __wasm_syscall( int32_t, int32_t, int32_t, int32_t,
                               int32_t, int32_t, int32_t, int32_t,
                               int32_t, int32_t, int32_t, int32_t,
                               int32_t );

/*
 * Variadic wrapper — this is the extern symbol that g_syscalls.c calls.
 * Unpacks va_args into the fixed-arity WASM import.
 */
intptr_t syscall( intptr_t callNum, ... )
{
    va_list ap;
    int32_t a[12];
    int i;

    /* Zero-init to distinguish real args from garbage */
    for ( i = 0; i < 12; i++ )
        a[i] = 0;

    va_start( ap, callNum );
    for ( i = 0; i < 12; i++ ) {
        a[i] = va_arg( ap, int );
    }
    va_end( ap );

    return (intptr_t)__wasm_syscall(
        (int32_t)callNum,
        a[0], a[1], a[2],  a[3],  a[4],  a[5],
        a[6], a[7], a[8],  a[9],  a[10], a[11] );
}

/* dllEntry no-op — WASM doesn't use dllEntry, but satisfies the linker
   since some game code references it. */
void dllEntry( intptr_t (*fn)( intptr_t, ... ) )
{
    (void)fn;
}
