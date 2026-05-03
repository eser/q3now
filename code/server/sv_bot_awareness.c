// sv_bot_awareness.c — bot peripheral awareness: WCE subscriber + per-bot sound ring

#include "server.h"
#include "sv_bot_awareness.h"

// ── Internal state ────────────────────────────────────────────────────────────

typedef struct {
    bot_sound_event_t events[MAX_BOT_SOUND_EVENTS];
    int               count;         // monotonic enqueue counter
    int               lastReadCount; // game VM consumed up to here
    int               lastLogSvTime; // sv.time of last debug print for this bot (rate-limit)
} bot_awareness_state_t;

static bot_awareness_state_t sv_botAwareness[MAX_CLIENTS];

/*
 * bot_debug — unified bot/AI debug cvar.
 *   0  = off (default)
 *   1  = high-level state transitions (enemy acquired/lost, awareness trigger fired)
 *   2  = detailed trace (event enqueue, per-entity FOV results, aim target resolution)
 *
 * Register once; Cvar_Get is idempotent across subsystems that reference it.
 */
static cvar_t *bot_debug;

// ── Event handler ─────────────────────────────────────────────────────────────

static void SV_BotAwareness_OnEvent( const wce_event_data_t *ev, void *userdata ) {
    float baseVolume, maxRange;
    int   i;

    (void)userdata;

    switch ( ev->type ) {
        case WCE_PLAYER_FOOTSTEP:
            baseVolume = (ev->param1 == 1) ? 0.5f : 0.3f;
            maxRange   = (ev->param1 == 1) ? 700.0f : 400.0f;
            break;
        case WCE_PLAYER_JUMP:
            baseVolume = 0.4f; maxRange = 400.0f; break;
        case WCE_PLAYER_FALL:
            baseVolume = 0.4f + ev->param1 * 0.2f;
            if ( baseVolume > 0.9f ) baseVolume = 0.9f;
            maxRange = 400.0f + ev->param1 * 250.0f;
            break;
        case WCE_PLAYER_SWIM:
            baseVolume = 0.2f; maxRange = 300.0f; break;
        case WCE_PLAYER_WATER_ENTER:
        case WCE_PLAYER_WATER_EXIT:
            baseVolume = 0.3f; maxRange = 400.0f; break;
        case WCE_PLAYER_WATER_SUBMERGE:
            baseVolume = 0.2f; maxRange = 300.0f; break;
        case WCE_PLAYER_ATTACK_PRIMARY:
        case WCE_PLAYER_ATTACK_SECONDARY: {
            int w = ev->param1;
            switch ( w ) {
                case WP_GAUNTLET:
                    baseVolume = 0.3f; maxRange = 600.0f;   break;
                case WP_MACHINEGUN:
                    baseVolume = 0.6f; maxRange = 1500.0f;  break;
                case WP_SHOTGUN:
                    baseVolume = 0.8f; maxRange = 2000.0f;  break;
                case WP_GRENADE_LAUNCHER:
                    baseVolume = 0.7f; maxRange = 1800.0f;  break;
                case WP_ROCKET_LAUNCHER:
                    baseVolume = 0.9f; maxRange = 2200.0f;  break;
                case WP_LIGHTNING_GUN:
                    baseVolume = 0.7f; maxRange = 1800.0f;  break;
                case WP_RAILGUN:
                    baseVolume = 0.9f; maxRange = 2200.0f;  break;
                case WP_PLASMA_RIFLE:
                    baseVolume = 0.6f; maxRange = 1500.0f;  break;
                default:
                    baseVolume = 0.7f; maxRange = 1800.0f;  break;
            }
            break;
        }
        case WCE_PLAYER_TAKE_DAMAGE:
            baseVolume = 0.4f + (ev->param1 / 100.0f) * 0.3f;
            if ( baseVolume > 0.9f ) baseVolume = 0.9f;
            maxRange = 800.0f;
            break;
        case WCE_PLAYER_DEATH:
            baseVolume = 0.9f; maxRange = 1500.0f; break;
        case WCE_PLAYER_TAUNT:
            baseVolume = 0.7f; maxRange = 1200.0f; break;
        case WCE_PLAYER_ITEM_PICKUP:
            baseVolume = 0.4f; maxRange = 500.0f; break;
        case WCE_PROJECTILE_IMPACT: {
            int w = ev->param1;
            switch ( w ) {
                case PROJ_ROCKET:
                    baseVolume = 1.0f; maxRange = 2500.0f;  break;
                case PROJ_GRENADE:
                case PROJ_LAVABALL:
                    baseVolume = 0.9f; maxRange = 2000.0f;  break;
                case PROJ_PLASMA:
                    baseVolume = 0.5f; maxRange = 1200.0f;  break;
                default:
                    baseVolume = 0.7f; maxRange = 1500.0f;  break;
            }
            break;
        }
        case WCE_WORLD_SOUND:
            baseVolume = 0.5f; maxRange = 1000.0f; break;
        default:
            return;
    }

    for ( i = 0; i < sv_maxclients->integer; i++ ) {
        client_t *cl = &svs.clients[i];
        vec3_t    botOrigin, delta;
        float     dist, volume;
        trace_t   tr;

        if ( cl->state != CS_ACTIVE ) continue;
        if ( !cl->gentity || !(cl->gentity->r.svFlags & SVF_BOT) ) continue;
        if ( i == ev->clientNum ) continue;  // bot doesn't hear itself

        VectorCopy( cl->gentity->r.currentOrigin, botOrigin );
        VectorSubtract( ev->origin, botOrigin, delta );
        dist = VectorLength( delta );
        if ( dist >= maxRange ) continue;

        volume = baseVolume * (1.0f - dist / maxRange);

        // BSP trace — wall between source and bot attenuates but doesn't zero
        SV_Trace( &tr, ev->origin, NULL, NULL, botOrigin, -1, MASK_OPAQUE, qfalse );
        if ( tr.fraction < 1.0f ) {
            volume *= 0.4f;
        }

        if ( volume < 0.05f ) continue;

        {
            bot_awareness_state_t *st  = &sv_botAwareness[i];
            int                    slot = st->count % MAX_BOT_SOUND_EVENTS;
            bot_sound_event_t     *e   = &st->events[slot];

            e->type            = ev->type;
            VectorCopy( ev->origin, e->origin );
            e->sourceClientNum = ev->clientNum;
            e->sourceEntityNum = ev->entityNum;
            e->param1          = ev->param1;
            e->volume          = volume;
            e->time            = sv.time;
            st->count++;

            // state-change log: at most one line per bot per 2 seconds
            if ( bot_debug->integer >= 1 && sv.time - st->lastLogSvTime > 2000 ) {
                Com_Log( SEV_INFO, LOG_CAT_SERVER, "BotAwareness: client=%d heard ev=%d src=%d dist=%.0f vol=%.2f\n",
                    i, (int)ev->type, ev->clientNum, dist, volume );
                st->lastLogSvTime = sv.time;
            }
        }
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void SV_BotAwareness_Init( void ) {
    static const wce_event_type_t subscribed[] = {
        WCE_PLAYER_FOOTSTEP,
        WCE_PLAYER_JUMP,
        WCE_PLAYER_FALL,
        WCE_PLAYER_SWIM,
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
    };
    int n = (int)( sizeof(subscribed) / sizeof(subscribed[0]) );
    int i;

    memset( sv_botAwareness, 0, sizeof(sv_botAwareness) );

    bot_debug = Cvar_Get( "bot_debug", "0", CVAR_CHEAT );

    for ( i = 0; i < n; i++ ) {
        WiredCoreEvents_Register( subscribed[i], WCE_PRIORITY_NORMAL,
                                  SV_BotAwareness_OnEvent, NULL );
    }
}

void SV_BotAwareness_Shutdown( void ) {
    static const wce_event_type_t subscribed[] = {
        WCE_PLAYER_FOOTSTEP,
        WCE_PLAYER_JUMP,
        WCE_PLAYER_FALL,
        WCE_PLAYER_SWIM,
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
    };
    int n = (int)( sizeof(subscribed) / sizeof(subscribed[0]) );
    int i;

    for ( i = 0; i < n; i++ ) {
        WiredCoreEvents_Unregister( subscribed[i], WCE_PRIORITY_NORMAL,
                                    SV_BotAwareness_OnEvent, NULL );
    }

    memset( sv_botAwareness, 0, sizeof(sv_botAwareness) );
}

int SV_BotAwareness_GetEvents( int clientNum, bot_sound_event_t *out, int maxOut ) {
    bot_awareness_state_t *st;
    int                    written = 0;

    if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) return 0;
    if ( !out || maxOut <= 0 ) return 0;

    st = &sv_botAwareness[clientNum];

    while ( st->lastReadCount < st->count && written < maxOut ) {
        int slot = st->lastReadCount % MAX_BOT_SOUND_EVENTS;
        out[written++] = st->events[slot];
        st->lastReadCount++;
    }

    return written;
}
