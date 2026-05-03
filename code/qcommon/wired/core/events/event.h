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

#include "../../../q_shared.h"

// -------------------------------------------------------------------------
// Types
// -------------------------------------------------------------------------

typedef uint32_t event_type_t;

#define EVTYPE_NONE  ((event_type_t)0)
#define EVTYPE_LOG   ((event_type_t)1)   // emitted by Com_Log in V2

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

// -------------------------------------------------------------------------
// WiredCoreEvents bus
// -------------------------------------------------------------------------

/*
 * wce_event_type_t — what happened, not how it sounds or looks.
 * Consumers (bot awareness, coaching, analytics) interpret per their own model.
 *
 * Payload field conventions per event type (see wce_event_data_t):
 *
 *   WCE_PLAYER_FOOTSTEP     clientNum=stepper  entityNum=clientNum  origin=pos
 *                           param1=surfaceType(0=generic/1=metal/2=splash)
 *   WCE_PLAYER_SWIM         clientNum  entityNum=clientNum  origin
 *   WCE_PLAYER_JUMP         clientNum  entityNum=clientNum  origin
 *   WCE_PLAYER_FALL         clientNum  entityNum=clientNum  origin
 *                           param1=severity(0=short/1=medium/2=far)
 *   WCE_PLAYER_WATER_*      clientNum  entityNum=clientNum  origin
 *   WCE_PLAYER_ATTACK_*     clientNum  entityNum=clientNum  origin
 *                           param1=weapon(WP_*)
 *   WCE_PLAYER_TAKE_DAMAGE  clientNum=victim  entityNum=victim  origin
 *                           param1=amount(server-approx: 0 if unknown)
 *                           param2=attacker clientNum(-1=world/unknown)
 *   WCE_PLAYER_DEATH        clientNum=victim  entityNum=victim  origin
 *                           param1=MOD(server-approx: 0 if unknown)
 *                           param2=killer clientNum(-1=world/suicide)
 *   WCE_PLAYER_TAUNT        clientNum  entityNum=clientNum  origin
 *   WCE_PLAYER_ITEM_PICKUP  clientNum=picker  entityNum=item-entity  origin
 *                           param1=giType(server-approx: modelindex if unknown)
 *                           param2=giTag  text=classname/NULL
 *   WCE_PROJECTILE_IMPACT   clientNum=owner(-1=unowned)  entityNum=projectile
 *                           origin=impact-point  param1=weapon(WP_*)
 *                           param2=hitEntity(-1=world)
 *   WCE_WORLD_SOUND         clientNum=-1  entityNum=emitter(-1 if none)
 *                           origin  param1=0  text=description/NULL
 *
 * "server-approx" fields are reconstructed from entity/player state and may
 * be 0 when the exact value is unavailable server-side without game VM access.
 */
typedef enum {
    WCE_NONE = 0,
    /* — Framework — */
    WCE_MAP_LOADING,
    WCE_MAP_LOADED,
    WCE_MAP_SHUTDOWN,
    WCE_CLIENT_CONNECT,
    WCE_CLIENT_DISCONNECT,
    WCE_CLIENT_BEGIN,
    WCE_MATCH_START,
    WCE_MATCH_END,
    WCE_ROUND_START,
    WCE_ROUND_END,
    WCE_FRAME_START,
    WCE_FRAME_END,
    WCE_HUNK_CLEAR,
    /* — Player lifecycle (pre-existing) — */
    WCE_PLAYER_RESPAWN,
    WCE_PLAYER_SCORE,
    WCE_ITEM_RESPAWN,
    /* — Player actions — */
    WCE_PLAYER_FOOTSTEP,
    WCE_PLAYER_SWIM,
    WCE_PLAYER_JUMP,
    WCE_PLAYER_FALL,
    WCE_PLAYER_WATER_ENTER,
    WCE_PLAYER_WATER_EXIT,
    WCE_PLAYER_WATER_SUBMERGE,
    WCE_PLAYER_ATTACK_PRIMARY,
    WCE_PLAYER_ATTACK_SECONDARY,
    WCE_PLAYER_TAKE_DAMAGE,
    WCE_PLAYER_DEATH,
    WCE_PLAYER_TAUNT,
    WCE_PLAYER_ITEM_PICKUP,
    WCE_PROJECTILE_IMPACT,
    WCE_WORLD_SOUND,
    WCE_COUNT
} wce_event_type_t;

typedef enum {
    WCE_PRIORITY_HIGH = 0,
    WCE_PRIORITY_NORMAL,
    WCE_PRIORITY_LOW,
    WCE_PRIORITY_COUNT
} wce_event_priority_t;

typedef struct {
    wce_event_type_t    type;
    int                 clientNum;   /* player client index, or -1 */
    int                 entityNum;   /* relevant entity (item, projectile, etc.) */
    vec3_t              origin;      /* world position */
    int                 param1;      /* integer payload — event-specific */
    int                 param2;      /* integer payload — event-specific */
    float               fparam;      /* float payload   — event-specific */
    char                text[256];   /* string payload  — event-specific */
} wce_event_data_t;

/* — Bot peripheral awareness — processed sound events delivered to game VM per-think — */
#define MAX_BOT_SOUND_EVENTS 16

typedef struct {
    wce_event_type_t type;
    vec3_t           origin;
    int              sourceClientNum;  /* -1 if no client source */
    int              sourceEntityNum;
    int              param1;           /* weapon / damage / severity / surface — event-specific */
    float            volume;           /* after distance + BSP attenuation */
    int              time;             /* sv.time when enqueued */
} bot_sound_event_t;

typedef void (*wce_event_handler_fn)( const wce_event_data_t *data, void *userdata );

/* — Bus lifecycle and registration — */
void WiredCoreEvents_Init              ( void );
void WiredCoreEvents_Shutdown          ( void );
void WiredCoreEvents_Register          ( wce_event_type_t type, wce_event_priority_t priority,
                                         wce_event_handler_fn fn, void *userdata );
void WiredCoreEvents_Unregister        ( wce_event_type_t type, wce_event_priority_t priority,
                                         wce_event_handler_fn fn, void *userdata );
void WiredCoreEvents_Dispatch          ( const wce_event_data_t *data );
void WiredCoreEvents_DispatchSimple    ( wce_event_type_t type, int clientNum );
void WiredCoreEvents_DispatchWithOrigin( wce_event_type_t type, int clientNum, const vec3_t origin );
void WiredCoreEvents_DispatchWithText  ( wce_event_type_t type, int clientNum, const char *text );
