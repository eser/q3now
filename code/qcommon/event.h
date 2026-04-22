/*
===========================================================================
event.h — Event primitive stubs (V1)

Declarations only. Every function body in event.c emits a once-only debug
log and returns a no-op value.

FUTURE (V2): implement real dispatch. Note that log_sink_fn is shape-
compatible with event_handler_fn — V2 migration only needs to wire the
existing sinks as subscribers.
===========================================================================
*/
#pragma once

#include "q_shared.h"

// -------------------------------------------------------------------------
// Types
// -------------------------------------------------------------------------

typedef uint32_t event_type_t;

#define EV_NONE  ((event_type_t)0)
#define EV_LOG   ((event_type_t)1)   // emitted by Com_Log in V2

typedef struct {
    event_type_t  type;
    const void   *data;
    int           data_size;
} event_t;

// V2: event_handler_fn is shape-compatible with log_sink_fn(rec, ctx).
typedef void (*event_handler_fn)(const event_t *evt, void *ctx);

typedef struct event_subscription_s event_subscription_t;

// -------------------------------------------------------------------------
// API (stubs in V1)
// -------------------------------------------------------------------------

void                  Event_RegisterType  ( event_type_t type );
event_subscription_t *Event_Subscribe     ( event_type_t type,
                                            event_handler_fn handler,
                                            void *ctx );
void                  Event_Unsubscribe   ( event_subscription_t *sub );
void                  Event_Emit          ( event_type_t type,
                                            const void *data,
                                            int data_size );
