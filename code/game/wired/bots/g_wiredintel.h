// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef G_WIREDINTEL_H
#define G_WIREDINTEL_H

/*
===========================================================================
g_wiredintel.h — WiredIntel unified bot directive system

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
    DIR_GOTO_EXIT,      /* head to the level's exit trigger (forward progress) */
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
    WI_ACK_YES = 0,           /* "Yes!" / affirmative ack                 */
    WI_STATUS_DEFENSE,        /* "On defense."                            */
    WI_STATUS_INPOSITION,     /* "In position."                           */
    WI_STATUS_GETFLAG,        /* "Going for the flag!"                    */
    WI_STATUS_RETURNFLAG,     /* "Returning the flag."                    */
    WI_STATUS_OFFENSE,        /* "On offense."                            */
    WI_STATUS_STARTLEADER,    /* "I'm leading."                           */
    WI_TAUNT_GENERIC,         /* generic taunt (level start / random)     */
    WI_TAUNT_PRAISE,          /* "Good game!" on level end / winning      */
    WI_TAUNT_DEATH,           /* taunt on death in teamplay               */
    WI_TAUNT_KILL,            /* taunt on kill in teamplay                */
    WI_ACK_UNKNOWN_TARGET,    /* "Who is X?" — unresolvable target name   */
    WI_ANNOUNCE_NUM_TYPES
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
    float           lastAnnounceTime[WI_ANNOUNCE_NUM_TYPES];
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
    /* ── entity activation (navigate-to-a-point then interact) ──────────────
       A closed, target-opened door (e.g. a func_door driven by a button/counter
       chain) is a nav-blocked gate: the bot cannot route past it until it presses
       the button(s) that open it. The queued buttons themselves — origin +
       entitynum — now live in the per-brain belief store as interactable records
       (bs->beliefStore.interactables[], addressed through the belief API); these
       fields are the CURSOR over that queue: which button is being approached, the
       gate being opened, and the detection rate-limit. The queued count is the
       belief store's interactable count (Belief_InteractableCount). First member of
       a general "go to a point and interact with it" family — a future capability
       (elevators, stairs) plugs into the same origin-goal + arrive-to-interact
       shape and the same per-brain BotCanActivate() gate. */
    int             activateIndex;      /* index of the button currently being approached     */
    int             activateDoor;       /* entitynum of the gate being opened (-1 = none)     */
    /* qtrue while this gate's queue is actuated by a jump-touch SEQ rather than by
       walking onto the button.  Latched when the jump-touch is armed, cleared when the
       queue retires.  It is how the queue learns its actuation plan DIED: the SEQ abort
       paths (watchdog, attempts exhausted, mover stall) clear the sequence but cannot
       reach the queue, so without this latch the queue outlives its own plan and pins
       the bot to a walk goal it can never press.  See the abort-retirement in
       WiredIntel_TrySetActivationGoal. */
    qboolean        activateByJumpTouch;/* qtrue → this gate's plan is a jump-touch SEQ       */
    /* level.time at which the queued target FIRST read unreachable from the bot's
       current position; 0 while it reads reachable.  Drives the held-goal retire:
       a single degenerate corridor is indistinguishable from a shut gate ahead, so
       only SUSTAINED unreachability retires the queue.  See the retire block in
       WiredIntel_TrySetActivationGoal. */
    int             activateUnreachSince;
    float           activateRetryTime;  /* FloatTime() before which detection is skipped       */
    float           rideRetryTime;      /* elevator-ride detection rate-limit (same shape)     */
    /* Board-reachability persistence for the ride producer.  PER CANDIDATE, not a
       single slot: the producer re-scans EVERY rideable mover on every call, so N
       candidates can be simultaneously unroutable, and each must accumulate its
       own elapsed time.  One shared slot cannot express that — rival candidates
       evict each other on every pass, no clock ever advances, and the reject can
       never fire.  (The held-goal retire above uses one slot correctly because it
       tracks exactly ONE activation target.  Same threshold derivation, different
       arity, so a different state shape.)
       Entity-indexed, and deliberately so: the tracked set is whichever movers a
       map author placed, so any cap smaller than the entity space would be a
       number taken from the maps that happen to have been measured rather than
       from a mechanism — and a map exceeding it would silently stop being timed.
       Each entry holds the level.time at which that entity's BOARD point first
       read unroutable, and 0 while it reads routable or is untracked (the
       zero-initialised struct therefore starts fully untracked).
       See the acceptance loop in WiredIntel_FindRideMover. */
    int             rideBoardUnreachSince[MAX_GENTITIES];
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
                                         const vec3_t area_origin );

/* chat parsing — call at end of BotMatchMessage for unhandled messages */
void        BotDirective_ParseChatOrder( struct bot_state_s *bs,
                                          int talker,
                                          const char *msg );

/* chat response — acknowledge a received order */
void        BotDirective_RespondTeamChat( struct bot_state_s *bs,
                                           int talker,
                                           const char *msg );

/* announcement — team-chat status message + optional voice */
void        WiredIntel_Announce( struct bot_state_s *bs,
                                 wbAnnounceType_t type,
                                 const char *context );

/* console: "bot_order <botname> <order>" */
void        BotDirective_ConsoleOrder( const char *bot_name,
                                        const char *order );

/* console: "bot_teleport <botname> <x> <y> <z> [yaw]" — deterministic
   placement of a named bot at a world origin (harness spawn-realism). */
void        BotDirective_ConsoleTeleport( const char *bot_name,
                                          vec3_t origin, float yaw );

/* configstring sync — call after any directive state change */
void        BotDirective_UpdateConfigstring( struct bot_state_s *bs );

/* exit-seeking fallback — when a bot has nothing better to do (no item goal,
   no order, no enemy), point it at the level's exit trigger for forward
   progress. Returns qtrue if an exit was found and the goal was set; qfalse on
   a map with no changelevel trigger (caller falls back to normal roam). Sets an
   UNLOCKED long-term goal, so combat (BotFindEnemy) still preempts it. */
qboolean    WiredIntel_TrySetExitGoal( struct bot_state_s *bs );

/* Instrument only (no behavioural effect): counters for the exit/door/button
   progression spine.  Counts are unconditional; only the report's PRINT is gated
   (on nav_botdebug).  which: 0 = buttons queued, 1 = queued button fired,
   2 = target-opened door left its closed rest. */
void        WiredIntel_ExitChainCount( int which );
void        WiredIntel_ExitChainReport( void );

/* Instrument only: PAIRED candidate-utility sample.  Reports each non-item
   candidate's would-be score beside the item score the chain actually acted on, at
   the same evaluation.  Decides nothing; feeds no comparison. */
void        WiredIntel_CandReport( float itemScore, qboolean itemIsLtg,
                                   const vec_t *probeOrigin );

/* STEP 4: the exit candidate's utility on the shipped [0,100] item scale, or < 0
   when the exit is ABSENT — no changelevel on this map, or no sweep weight set.
   Both parameters come from sweep cvars with NO shipped default, so an unswept
   build scores the exit absent and behaves byte-identically to before step 4. */
float       WiredIntel_ExitCandidateScore( const struct bot_state_s *bs );

/* activation goal — when a closed, target-opened door blocks the way to
   'goalOrigin', queue the button(s) that open it and drive the bot to each in
   turn (the bot presses a button by walking onto it; the door fires open once
   the chain completes). Returns qtrue if an activation goal is active this frame,
   qfalse if there is nothing to activate (caller falls through to exit/normal AI).
   Only a client-brain activates (BotCanActivate); a byte-identical no-op on maps
   with no target-opened door. Sets an UNLOCKED goal, so combat still preempts. */
qboolean    WiredIntel_TrySetActivationGoal( struct bot_state_s *bs,
                                            const vec3_t goalOrigin );

/* Ballistic run-jump arc validator. Given a launch foot and a target brush AABB,
   sweep run speeds x flight times for a player-box arc that overlaps the brush;
   returns qtrue and writes the working run speed to *speedOut if one exists. Used
   both by the jump-touch producer (feasibility at install) and by the SEQ executor
   (re-solve the speed for the bot's ACTUAL settle position at fire time, since the
   nav follower stands one agent-radius in from the computed launch edge). */
qboolean    WiredIntel_JumpTouchArcHits( const vec3_t launch, const vec3_t bmins,
                                         const vec3_t bmaxs, float *speedOut );

/* elevator-ride producer self-test (shipped-dormant, bot_debug >= 3, once per
   process): proves the format-neutral ride-mover classifier + the board ->
   wait-for-mover-state -> step-off sequence builder + the substrate walk gated on
   MOVER_POS2, on scratch state with no live map. No new cvar; a no-op below
   bot_debug 3. */
void        WiredIntel_RideProducerSelfTestOnce( void );

/* goal / item scoring hooks */
void        BotDirective_OverrideGoal( struct bot_state_s *bs,
                                        struct bot_goal_s *goal );
qboolean    BotDirective_ScoreItem( struct bot_state_s *bs,
                                     struct bot_goal_s *goal,
                                     float *score_multiplier );

/* defensive combat during directive lock — fires at visible threats without chasing */
void        WiredIntel_DefensiveCombat( struct bot_state_s *bs );

/* ── @ mention parse result (returned to G_Say) ─────────────────────── */
typedef struct {
    qboolean    hasMentions;
    char        recipientMention[MAX_NETNAME];
    char        targetMention[MAX_NETNAME];
    int         recipientClients[MAX_CLIENTS];
    int         numRecipients;
} wbParseResult_t;

/* @ mention system — called from G_Say before text relay */
void        WiredIntel_ProcessChat( int senderClient, const char *message,
                                    wbParseResult_t *result );
void        WiredIntel_ColorizeMentions( const char *input, char *output, int outputSize,
                                         const char *recipientMention,
                                         const char *targetMention );

#endif /* G_WIREDINTEL_H */
