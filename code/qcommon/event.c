/*
===========================================================================
event.c — Event primitive stubs (V1)

DO NOT implement real dispatch here. Stop per the spec: no async dispatch,
no attributes, no per-subsystem categories in V1. Each function emits a
once-only debug log entry and returns a no-op value.
===========================================================================
*/
#include "q_shared.h"
#include "qcommon.h"
#include "event.h"

void Event_RegisterType( event_type_t type )
{
    static qboolean warned = qfalse;
    if ( !warned ) {
        Com_Printf( "Event_RegisterType: TODO V2\n" );
        warned = qtrue;
    }
    (void)type;
}

event_subscription_t *Event_Subscribe( event_type_t type,
                                        event_handler_fn handler,
                                        void *ctx )
{
    static qboolean warned = qfalse;
    if ( !warned ) {
        Com_Printf( "Event_Subscribe: TODO V2\n" );
        warned = qtrue;
    }
    (void)type; (void)handler; (void)ctx;
    return NULL;
}

void Event_Unsubscribe( event_subscription_t *sub )
{
    static qboolean warned = qfalse;
    if ( !warned ) {
        Com_Printf( "Event_Unsubscribe: TODO V2\n" );
        warned = qtrue;
    }
    (void)sub;
}

void Event_Emit( event_type_t type, const void *data, int data_size )
{
    static qboolean warned = qfalse;
    if ( !warned ) {
        Com_Printf( "Event_Emit: TODO V2\n" );
        warned = qtrue;
    }
    (void)type; (void)data; (void)data_size;
}
