#ifndef G_WIREDBOTS_H
#define G_WIREDBOTS_H

/*
===========================================================================
g_wiredbots.h — WiredBots unified bot directive system

All bot orders flow through one channel: chat messages parsed into
botDirective_t values.  BotDirective_FrameUpdate() translates the active
directive back into bs->ltgtype / bs->teamgoal every frame so that the
existing AINode_Seek_LTG navigation engine in ai_dmnet.c keeps working
without modification.

Requires: Callers must include g_local.h (for q_shared, bg_public) before
          this header so that vec3_t, qboolean, bot_goal_t are visible.
===========================================================================
*/

#include "../../../qcommon/q_feats.h"
#include "../../../qcommon/q_shared.h"

/* forward-declare opaque types so this header compiles standalone */
struct bot_state_s;
struct bot_goal_s;

/* ── directive types ─────────────────────────────────────────────────── */
typedef enum {
    DIR_NONE = 0,       /* no active directive; AI runs freely            */
    DIR_FOLLOW,         /* follow a specific teammate                      */
    DIR_DEFEND_AREA,    /* hold / defend an area origin                    */
    DIR_CAMP_SPOT,      /* camp a fixed position                           */
    DIR_PATROL,         /* patrol waypoints (uses bs->patrolpoints)        */
    DIR_SEEK_ITEM,      /* go fetch a named item or "flag"                 */
    DIR_RUSH_BASE,      /* CTF: rush to our base                           */
    DIR_RETURN_FLAG,    /* CTF: bring flag back to base                    */
    DIR_ATTACK_BASE,    /* CTF / general: assault enemy base               */
    DIR_KILL_TARGET,    /* kill a specific named player                    */
    DIR_HARVEST,        /* harvester mode: collect skulls                  */
    DIR_ROAM,           /* cancel all orders; free roam                   */
    DIR_SET_SUBTEAM,    /* join / leave a named subteam                    */
    DIR_MAX
} directiveType_t;

/* ── combat tactic modifiers ─────────────────────────────────────────── */
typedef enum {
    TACTIC_NONE = 0,
    TACTIC_HUNT,        /* prioritize killing tactic_target               */
    TACTIC_AVOID,       /* evade tactic_target                            */
    TACTIC_RUSH,        /* move fast, do not stop to pick up items        */
    TACTIC_CAMP,        /* stay in place; react only to nearby threats    */
    TACTIC_RETREAT,     /* fall back toward own base                      */
    TACTIC_AMBUSH,      /* wait at a choke point for enemies to pass      */
    TACTIC_MAX
} botTactic_t;

/* ── announcement types (team-chat status messages + taunts) ─────────── */
typedef enum {
    WB_ACK_YES = 0,           /* "Yes!" / affirmative ack                 */
    WB_STATUS_DEFENSE,        /* "On defense."                            */
    WB_STATUS_INPOSITION,     /* "In position."                           */
    WB_STATUS_GETFLAG,        /* "Going for the flag!"                    */
    WB_STATUS_RETURNFLAG,     /* "Returning the flag."                    */
    WB_STATUS_OFFENSE,        /* "On offense."                            */
    WB_STATUS_STARTLEADER,    /* "I'm leading."                           */
    WB_TAUNT_GENERIC,         /* generic taunt (level start / random)     */
    WB_TAUNT_PRAISE,          /* "Good game!" on level end / winning      */
    WB_TAUNT_DEATH,           /* taunt on death in teamplay               */
    WB_TAUNT_KILL,            /* taunt on kill in teamplay                */
    WB_ACK_UNKNOWN_TARGET,    /* "Who is X?" — unresolvable target name   */
    WB_ANNOUNCE_NUM_TYPES
} wbAnnounceType_t;

/* ── single issued directive ─────────────────────────────────────────── */
typedef struct {
    directiveType_t type;
    int             source_client;          /* issuer client num; -1 = console   */
    float           issue_time;             /* FloatTime() when issued            */
    float           expire_time;            /* 0 = never expires                  */
    char            target_name[MAX_NETNAME];        /* target player name (KILL/FOLLOW)   */
    int             target_client;          /* resolved client num; -1 = none     */
    vec3_t          area_origin;            /* for DEFEND/CAMP                    */
    float           area_radius;            /* influence radius (0 = point)       */
    char            item_classname[64];     /* for SEEK_ITEM; "flag" is special   */
} botDirective_t;

/* ── per-bot directive state (embedded in bot_state_t) ──────────────── */
typedef struct {
    botDirective_t  tactical;       /* active tactical directive                 */
    botTactic_t     tactic;         /* active combat tactic modifier             */
    int             tactic_target;  /* client num for HUNT/AVOID; -1 = none      */
    qboolean        tactic_active;  /* qtrue when a tactic is in force           */
    /* ── team identity fields (migrated from bot_state_t) ───────────── */
    char            teamleader[MAX_NETNAME];          /* netname of accepted team leader   */
    char            subteam[MAX_NETNAME];             /* active subteam name; "" = none    */
    int             preference;              /* TEAMTP_ATTACKER/DEFENDER bitmask  */
    /* ── announcement cooldown tracking ────────────────────────────── */
    float           lastAnnounceTime[WB_ANNOUNCE_NUM_TYPES];
    float           lastAnnounceAnyTime;     /* global: min gap between any two announcements */
    /* ── directive lock (persistent goal enforcement) ──────────────────── */
    qboolean        directiveLocked;    /* qtrue → AI cannot override goal with enemy sight */
    int             armorAtStart;       /* STAT_ARMOR snapshot when directive was issued     */
    int             healthAtStart;      /* STAT_HEALTH snapshot when directive was issued    */
    qboolean        needsBaselineReset; /* qtrue → re-snapshot stats on first alive frame    */
    /* ── item seek state (position cache + raw-movement fallback) ───────── */
    vec3_t          seekPosition;       /* world position of the target item entity          */
    qboolean        useRawPosition;     /* qtrue → AAS unreachable; use direct EA movement   */
    float           rawMoveStartTime;   /* FloatTime() when raw position movement began      */
    /* ── stuck detection ────────────────────────────────────────────────── */
    int             lastProgressCheck;  /* level.time (ms) when distance was last sampled    */
    float           lastProgressDist;   /* distance to teamgoal.origin at last check         */
    /* ── near-spawn patrol ──────────────────────────────────────────────── */
    int             nextPatrolTime;     /* level.time when next patrol waypoint is picked    */
} botDirectiveState_t;

/* ── API ─────────────────────────────────────────────────────────────── */

/* lifecycle */
void        BotDirective_Init( botDirectiveState_t *ds );
void        BotDirective_ClearAll( struct bot_state_s *bs );
void        BotDirective_OnItemPickup( struct bot_state_s *bs, int item_entity );
void        BotDirective_OnDeath( struct bot_state_s *bs );

/* think-loop integration — call before BotDeathmatchAI() each frame */
void        BotDirective_FrameUpdate( struct bot_state_s *bs );

/* authorization */
qboolean    BotAuthorizeOrder( struct bot_state_s *bs, int issuer_client );

/* directive delivery */
void        BotReceiveDirective( struct bot_state_s *bs,
                                  int issuer_client,
                                  const char *order );

void        BotDirective_IssueToDirect( struct bot_state_s *bs,
                                         int issuer_client,
                                         directiveType_t type,
                                         int target_client,
                                         vec3_t area_origin );

/* chat parsing — call at end of BotMatchMessage for unhandled messages */
void        BotDirective_ParseChatOrder( struct bot_state_s *bs,
                                          int talker,
                                          const char *msg );

/* chat response — acknowledge a received order */
void        BotDirective_RespondTeamChat( struct bot_state_s *bs,
                                           int talker,
                                           const char *msg );

/* announcement — team-chat status message + optional voice */
void        WiredBots_Announce( struct bot_state_s *bs,
                                 wbAnnounceType_t type,
                                 const char *context );

/* console: "bot_order <botname> <order>" */
void        BotDirective_ConsoleOrder( const char *bot_name,
                                        const char *order );

/* configstring sync — call after any directive state change */
void        BotDirective_UpdateConfigstring( struct bot_state_s *bs );

/* goal / item scoring hooks */
void        BotDirective_OverrideGoal( struct bot_state_s *bs,
                                        struct bot_goal_s *goal );
qboolean    BotDirective_ScoreItem( struct bot_state_s *bs,
                                     struct bot_goal_s *goal,
                                     float *score_multiplier );

/* defensive combat during directive lock — fires at visible threats without chasing */
void        WiredBots_DefensiveCombat( struct bot_state_s *bs );

/* ── @ mention parse result (returned to G_Say) ─────────────────────── */
typedef struct {
    qboolean    hasMentions;
    char        recipientMention[MAX_NETNAME];
    char        targetMention[MAX_NETNAME];
    int         recipientClients[MAX_CLIENTS];
    int         numRecipients;
} wbParseResult_t;

/* @ mention system — called from G_Say before text relay */
void        WiredBots_ProcessChat( int senderClient, const char *message,
                                    wbParseResult_t *result );
void        WiredBots_ColorizeMentions( const char *input, char *output, int outputSize,
                                         const char *recipientMention,
                                         const char *targetMention );

#endif /* G_WIREDBOTS_H */
