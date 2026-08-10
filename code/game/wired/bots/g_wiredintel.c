// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
g_wiredintel.c — WiredIntel unified bot directive system

All bot orders flow through one channel: chat messages parsed into
botDirective_t values stored in bs->directives.

BotDirective_FrameUpdate() is called before BotDeathmatchAI() each frame
and translates the active directive into bs->ltgtype / bs->teamgoal so the
legacy AINode_Seek_LTG navigation code keeps driving movement unchanged.
===========================================================================
*/

#include "g_local.h"
#include "../botlib/botlib.h"
#include "../botlib/be_aas.h"
#include "../botlib/be_ea.h"
#include "../botlib/be_ai_char.h"
#include "../botlib/be_ai_chat.h"
#include "../botlib/be_ai_gen.h"
#include "../botlib/be_ai_goal.h"
#include "../botlib/be_ai_move.h"
#include "../botlib/be_ai_weap.h"
#include "ai_main.h"
#include "ai_dmnet.h"
#include "ai_dmq3.h"
#include "g_wiredintel.h"
#include "g_belief.h"
#include "g_sequenced_goal.h"
LOG_DECLARE_CHANNEL( ch_game, "game" );
LOG_DECLARE_CHANNEL( ch_botai, "botlib.ai" );

/* ── botstates array (defined in ai_main.c) ─────────────────────────── */
extern bot_state_t *botstates[MAX_CLIENTS];

/* ── forward declarations ────────────────────────────────────────────── */
static int      ResolveClientByName( const char *name );
static qboolean BotCanActivate( const bot_state_t *bs );   /* defined below; used by the ride producer */
#if FEAT_RECAST_NAVMESH
static qboolean WiredIntel_RestSurfaceIsRoutable( const vec3_t cand );  /* defined below; used by the touch-spot resolver */
#endif

/* The shared vertical-plat touch handler, installed by BOTH map formats' plat
   spawns (SP_q1_func_plat and SP_q3_func_plat) via ent->touch. Its address is a
   stable, format-neutral marker that an entity is a rideable elevator platform —
   the game-VM equivalent of the extractMovers plat classification, with NO
   classname string compare and NO format branch. Declared here exactly as
   g_mover_q1.c forward-declares it (it lives in g_mover_q3.c, external linkage,
   no shared header). Only its ADDRESS is used, never called. */
void Q3_Touch_Plat( gentity_t *ent, gentity_t *other, trace_t *trace );

/* =========================================================================
   LIFECYCLE
   ========================================================================= */

/*
==================
BotDirective_Init
==================
Zeroes the directive state and sets sentinel values.
Called once during BotAISetupClient().
*/
void BotDirective_Init( botDirectiveState_t *ds ) {
    memset( ds, 0, sizeof( *ds ) );
    ds->tactical.type          = DIR_NONE;
    ds->tactical.target_client = -1;
    ds->tactical.source_client = -1;
    ds->tactic_target          = -1;
    ds->directiveLocked        = qfalse;
    ds->armorAtStart           = 0;
    ds->healthAtStart          = 0;
}

/*
==================
WiredIntel_ArmDirective
==================
Lock the bot's stored directive and snapshot the baseline its progress and
pickup checks read. Extracted from BotReceiveDirective so the human-order path
and the engine's own autonomous issuers arm a directive through ONE mechanism
rather than two copies that can drift apart.

A locked directive makes the bot pursue its objective and fight only
defensively: Seek_LTG's locked branch aims and fires at a visible enemy but does
NOT abandon the goal — it falls through to the nav path, so the bot keeps
advancing while shooting (fight-while-advance).

CALLER-SIDE RE-ARM GUARD. Arming resets lastProgressCheck, which is the sample
the stall watchdog (the DIR_GOTO_EXIT case in WiredIntel_FrameUpdate) measures
from. An issuer that re-arms an already-armed identical directive every frame
would restart that sample forever and silently disable the abort path, so a
caller on a hot path must arm only on the TRANSITION into the directive.
*/
static void WiredIntel_ArmDirective( bot_state_t *bs ) {
    if ( !bs || bs->directives.tactical.type == DIR_NONE ) {
        return;
    }
    bs->directives.directiveLocked   = qtrue;
    bs->directives.armorAtStart      = bs->cur_ps.stats[STAT_ARMOR];
    bs->directives.healthAtStart     = bs->cur_ps.stats[STAT_HEALTH];
    bs->directives.lastProgressCheck = 0;   /* FrameUpdate takes snapshot on first frame */
    bs->directives.lastProgressDist  = 0.0f;
}

/*
==================
BotDirective_ClearAll
==================
Resets all directives to idle.  Keeps the bot alive and controllable by AI
but removes any current orders.
*/
void BotDirective_ClearAll( bot_state_t *bs ) {
    BotDirective_Init( &bs->directives );
    bs->ordered        = qfalse;
    bs->ltgtype        = 0;
    bs->teamgoal_time  = 0;
}

/*
==================
BotDirective_OnItemPickup
==================
If the bot just picked up the item it was sent to fetch, clear the
seek-item directive so it resumes normal AI.
*/
void BotDirective_OnItemPickup( bot_state_t *bs, int item_entity ) {
    botDirective_t *d = &bs->directives.tactical;

    if ( d->type != DIR_SEEK_ITEM ) {
        return;
    }

    /* We only know the entity number here.  If the directive had a specific
       entity target set (target_client repurposed for entity num), compare;
       otherwise clear unconditionally after any pickup. */
    if ( d->target_client < 0 || d->target_client == item_entity ) {
        d->type = DIR_NONE;
        bs->directives.directiveLocked = qfalse;
        bs->ordered       = qfalse;
        bs->ltgtype       = 0;
        bs->teamgoal_time = 0;
        BotDirective_UpdateConfigstring( bs );
    }
}

/*
==================
BotDirective_OnDeath
==================
Directive persists across death — bot resumes pursuing the same goal on
respawn without needing a new coach order.  Only three events clear a
directive: completion, a replacement order, or TTL expiry.

On death we reset the TTL clock so the bot gets a fresh window after
respawn, and zero the stat baselines so pickup detection works correctly
with the new post-respawn health/armor values.

Tactics (HUNT/AVOID personality modifiers) persist across deaths unchanged.
*/
void BotDirective_OnDeath( bot_state_t *bs ) {
    botDirective_t *d = &bs->directives.tactical;

    if ( bs->directives.directiveLocked && d->type != DIR_NONE ) {
        /* Restart the TTL from now so bot gets a full window after respawn */
        float ttl = d->expire_time - d->issue_time;
        if ( ttl > 0.0f ) {
            d->issue_time  = FloatTime();
            d->expire_time = d->issue_time + ttl;
        }
        /* Defer baseline re-snapshot until the first alive frame post-respawn.
           Zeroing here causes false "Got it!" because respawn health (100) > 0. */
        bs->directives.needsBaselineReset = qtrue;
        /* Reset raw movement — re-evaluate AAS reachability on respawn */
        bs->directives.useRawPosition = qfalse;
    }

    /* Reset movement state so the nav system picks up the goal cleanly */
    bs->ordered       = qfalse;
    bs->ltgtype       = 0;
    bs->teamgoal_time = 0;
    BotDirective_UpdateConfigstring( bs );

    /* An in-flight sequenced goal does NOT persist across death: its cursor is
       tied to a concrete in-world position/step that a respawn invalidates, so
       carrying it forward would leave a stale cursor driving a dead plan. Reset
       to idle here (the death/respawn hook) so no stale cursor survives a life —
       the shipped consumer re-installs a fresh sequence after respawn if needed. */
    WiredIntel_SequencedGoalReset( &bs->sequencedGoal );
}

/*
==================
BotDirective_UpdateConfigstring
==================
Pushes the current directive to the CS_BOTDIRECTIVES+clientNum configstring
so cgame can render it as 3D text above the bot's head, and so out-of-process
readers can see what state a bot is in.

Format: "type\displayname\ltgtype\seqactive\locked"

  type         directiveType_t integer value
  displayname  target/item name, "" when the type carries neither
  ltgtype      bs->ltgtype — the long-term goal the engine is actually driving
  seqactive    1 while a sequenced goal owns the goal, else 0
  locked       1 while the directive is locked, else 0

The first two fields keep their position and meaning, so a reader that parses
only "type" and the first separator is unaffected by the later fields.

Deliberately NOT carried here: the nav cursor (path index / point count). Those
change every frame while a bot walks, and a configstring write per frame per bot
would turn a state channel into a per-frame broadcast — the engine deduplicates
identical writes, which is only cheap because this string changes rarely. The
fields above change on directive and sequence transitions, which is the cadence
this channel is built for.

Called after every directive state change; engine deduplicates identical writes.
*/
void BotDirective_UpdateConfigstring( bot_state_t *bs ) {
    char            cs[128];
    botDirective_t *d = &bs->directives.tactical;
    const char     *displayName;

    if ( !bs->directives.directiveLocked || d->type == DIR_NONE ) {
        trap_SetConfigstring( CS_BOTDIRECTIVES + bs->client, "" );
        return;
    }

    switch ( d->type ) {
        case DIR_FOLLOW:
        case DIR_KILL_TARGET:
            displayName = d->target_name;
            break;
        case DIR_SEEK_ITEM:
            displayName = d->item_classname;
            break;
        default:
            displayName = "";
            break;
    }

    Com_sprintf( cs, sizeof( cs ), "%d\\%s\\%d\\%d\\%d",
                 (int)d->type, displayName,
                 (int)bs->ltgtype,
                 bs->sequencedGoal.active ? 1 : 0,
                 bs->directives.directiveLocked ? 1 : 0 );
    trap_SetConfigstring( CS_BOTDIRECTIVES + bs->client, cs );
}

/* =========================================================================
   THINK-LOOP INTEGRATION
   ========================================================================= */

/*
==================
BotResolveItemName (local)
==================
Resolves a user-supplied string to the pickup_name that botlib's
trap_BotGetLevelItemGoal expects.  Accepts either:
  • entity classname  (e.g. "item_armor_body")
  • pickup_name       (e.g. "Heavy Armor")
Returns a pointer into bg_itemlist on success, NULL if unknown.
*/

/*
==================
BotPickupToClassname
==================
Maps a user-visible pickup_name (e.g. "Heavy Armor") to the primary entity
classname (e.g. "item_armor_body") that trap_BotGetLevelItemGoal indexes by.
Also accepts classnames directly — returns them unchanged.
*/
static const char *BotPickupToClassname( const char *pickup_name ) {
    int i;
    for ( i = 1; i < bg_numItems; i++ ) {
        if ( bg_itemlist[i].pickup_name &&
            Q_stricmp( bg_itemlist[i].pickup_name, pickup_name ) == 0 )
            return bg_itemlist[i].classname;
    }
    return pickup_name; /* fallback: may already be a classname */
}

static const char *BotResolveItemName( const char *input )
{
    gitem_t *it;
    int      j;

    if ( !input || !input[0] )
        return NULL;

    /* Special alias used by coach commands */
    if ( Q_stricmp( input, "flag" ) == 0 )
        return "flag";

    for ( it = bg_itemlist + 1; it->classname; it++ ) {
        /* Match by pickup_name first (exact, case-insensitive) */
        if ( it->pickup_name && Q_stricmp( it->pickup_name, input ) == 0 )
            return it->pickup_name;
        /* Match by any classname alias */
        if ( Q_stricmp( it->classname, input ) == 0 )
            return it->pickup_name;
    }
    return NULL;
}

/*
==================
BotDirective_Abort
==================
Full reset after a directive fails for any reason other than success.
Clears all directive state and returns the bot to normal free-roam AI
immediately — the bot will never stand idle after a failed directive.
*/
static void BotDirective_Abort( bot_state_t *bs ) {
    botDirectiveState_t *ds = &bs->directives;
    ds->tactical.type     = DIR_NONE;
    ds->directiveLocked   = qfalse;
    ds->useRawPosition    = qfalse;
    ds->lastProgressCheck = 0;
    ds->lastProgressDist  = 0.0f;
    bs->ltgtype           = 0;
    bs->nbg_time          = 0;
    trap_BotEmptyGoalStack( bs->gs );
    AIEnter_Seek_LTG( bs, "directive aborted" );
}

/*
==================
BotSetOriginGoal
==================
Point a bot at a fixed world origin as a navigate-then-hold long-term goal.
Sets bs->ltgtype = LTG_DEFENDKEYAREA and fills bs->teamgoal so the existing
AINode_Seek_LTG engine walks the bot to the point via the nav mesh. Shared by
the DEFEND/CAMP/GOTO_EXIT directive cases and the idle exit fallback so there is
one origin-goal path, not several copies. 'range' is the hold radius once there.
*/
static void BotSetOriginGoal( bot_state_t *bs, const vec3_t origin, float range ) {
    VectorCopy( origin, bs->teamgoal.origin );
    bs->teamgoal.areanum   = BotPointAreaNum( (float *)origin );
    bs->teamgoal.entitynum = -1;
    bs->teamgoal.flags     = 0;
    bs->ltgtype            = LTG_DEFENDKEYAREA;
    bs->teamgoal_time      = FloatTime() + 1.0f;   /* refreshed each frame */
    bs->defendaway_time    = 0;
    bs->defendaway_range   = range > 0.0f ? range : 200.0f;
}

/* Player standing box (bg_public.h): half-width and z extents.  Used by the
   button touch-spot search to verify a candidate stand point is collision-
   standable and within touch range of the brush. */
#define WI_PLAYER_HALF_W   15.0f    /* NAV_WALKABLE_RADIUS = player half-width */
#define WI_PLAYER_MINS_Z  (-24.0f)  /* bg_public.h MINS_Z */
#define WI_PLAYER_MAXS_Z    32.0f   /* bg_public.h MAXS_Z */
/* Max horizontal reach from a button face to search for a standable touch-spot —
   the same envelope the jump-touch launch search uses (WI_JUMPTOUCH_MAX_HORIZ). */
#define WI_TOUCHSPOT_MAX_HORIZ 200.0f

/* Agent size for path probes: 0 is the player-size nav mesh, the same the bot's
   own follower uses — so a probe path matches the routes the bot actually walks.
   Shared by the rest-surface snap probe and WiredIntel_PathReaches. */
#define WI_NAV_AGENT_PLAYER 0

#if FEAT_RECAST_NAVMESH
/*
==================
WiredIntel_RestSurfaceIsRoutable
==================
CLASS RULE: a stand-point resolver may only accept a rest surface a route can be
planned against.  A rest on a mover, a button brush, or an actor bbox is not floor.

THE DISCRIMINATOR IS SNAP DISTANCE, NOT SURFACE IDENTITY — and that is an honest
correction of the "reject rest surfaces that are not world collision" framing this
started as.  Surface identity CANNOT separate the cases: the drop trace that
produces a candidate rests on the button's OWN brush for every candidate the
resolver generates, genuine and phantom alike, so rejecting on "the trace hit an
entity" (hitEnt) would discard every real candidate along with the impostor.  What
actually separates them is how far the candidate is from the navmesh: a candidate
resting on real, routable floor snaps a short way onto the mesh, while a candidate
resting on a surface no route exists against (a plate hanging over a void, a mover
lid, an actor) has to be dragged a long way to find any poly at all.

So this defect IS the coverage check, more precisely stated than before: the old
test asked only whether SOME poly existed inside a 64/64/96 search box, which a
phantom satisfies by snapping to the far corner of that box.  Asking HOW FAR the
snap had to travel is what distinguishes them.

MEASURING THE SNAP WITH NO NEW SYSCALL.  trap_Nav_FindNearestPoly returns only a
ref, never the snapped position, so the distance is not observable through it.
But Nav_FindPath's first waypoint IS the snapped start: Detour's findStraightPath
clamps the raw start onto the start polygon (closestPointOnPolyBoundary) and emits
that clamped point as the DT_STRAIGHTPATH_START waypoint.  Pathing from the
candidate to itself therefore returns the candidate's own snapped position as
positions[0], and |cand - positions[0]| is exactly the snap distance.  Existing
trap, no new interface, no new tuning number.

THE TOLERANCE IS NOT SPECIMEN-DERIVED.  It is the resolver's own player half-width
(WI_PLAYER_HALF_W = NAV_WALKABLE_RADIUS = 15u), doubled: 2R is the erosion inset
Recast applies when it carves the walkable surface back from every wall, so a
stand point on genuine floor is at most one full erosion inset from the mesh by
construction.  Anything beyond that is not floor the mesh was eroded from — it is
a surface the mesh never covered.  The same 2R identity the bake uses, read back
at query time; no constant chosen to fit any measurement.

THE BOUND IS HORIZONTAL BECAUSE THE IDENTITY IS HORIZONTAL.  The 2R erosion inset
is carved in the XY plane: Recast erodes the walkable area laterally away from
every wall, so it bounds how far a genuine stand point can lie HORIZONTALLY from
the mesh.  It says nothing about vertical offset.  Comparing it against a 3D
VectorLength was the defect: 'cand' is a player ORIGIN resting on the floor, so
its z sits |WI_PLAYER_MINS_Z| = 24u ABOVE the surface the poly was baked on,
plus up to one NAV_CH voxel of bake quantisation.  A FLAWLESS candidate therefore
carries ~24u of purely STRUCTURAL vertical offset before any horizontal error at
all, which alone very nearly exhausts a 30u 3D budget — so the 3D form rejected
correct inputs by construction, on every map, and the resolver silently fell
through to the center-snap it exists to replace.  Mixing a structural vertical
offset into a distance compared against a horizontal bound is the class error;
the fix is to measure the quantity the identity actually bounds.

VERTICAL TERM — REQUIRED, and now MEASURED.  The earlier revision of this comment
predicted no vertical term would be needed and flagged itself UNVERIFIED because
the discriminating population had never been enumerated.  It has now been, and the
prediction was wrong: the horizontal metric alone does NOT separate the population.

Measured (2026-07-31, e1m1, two independent buttons in different rooms): a
candidate the collision trace found standing on a real floor was bound by the mesh
to a DIFFERENT floor 63u below, on BOTH buttons — identical value, so systematic,
not geometry.  Both passed this gate, because their horizontal snap was inside the
2R inset while the vertical snap was not measured at all.

  63u total = 24u structural (|WI_PLAYER_MINS_Z|: the player ORIGIN rests that far
              above the baked surface — real, expected, not error)
            + 39u excess

39u exceeds NAV_WALKABLE_CLIMB (18u), and that is what makes it nameable: two
surfaces more than one walk-climb apart are, by the bake's own joining rule,
DIFFERENT floors.  So an excess beyond a walk-climb is not snap error — it is
proof the mesh bound the candidate to a floor other than the one it stands on.

The bound is therefore |WI_PLAYER_MINS_Z| + NAV_WALKABLE_CLIMB = 24 + 18 = 42u:
the structural offset that is always present, plus the largest step the bake will
join.  Both terms are identities already in scope — no fitted constant.

SIGN: bounded on ABSOLUTE value, not downward only.  The measured class is downward
(the mesh binds to a lower floor), but an upward displacement >42u is equally a
different-floor binding — it would mean the nearest poly is a ledge above the
candidate — and nothing in the query prevents it.  Bounding one direction would
leave the mirror case admitted for no reason.  The horizontal metric below is
UNCHANGED: this adds a second axis, it does not replace the first.

🔴 KNOWN LIMITATION — THIS WHOLE MEASUREMENT READS ZERO.  Verified 2026-07-31 by
per-candidate instrumentation: BOTH terms below are identically 0 for every
candidate, so NEITHER the horizontal bound nor the vertical one has ever rejected
anything.  The reason is that positions[0] does not carry the snap:
Nav_FindPath fills it from findStraightPath(rOrigin, ...) (nav_impl.cpp), and
Detour's straight path BEGINS AT THE RAW rOrigin — the snapped nearPt from
findNearestPoly is used to pick startRef and then discarded.  So positions[0] is
the input point echoed back, and (positions[0] - cand) is structurally zero.
The 63u displacement this gate is supposed to catch is real (measured via
nav_findpath, which reports its own A/B snap), but it is NOT observable through
this probe.  Fixing it requires a query that returns the snapped point — a
resolver change, not a bound change.  Do not cite this gate as evidence that a
candidate is on the surface the mesh bound it to; today it proves only that
SOME path query from the candidate succeeded.

WHICH QUERY THE BOUND IS STATED AGAINST — this matters, and an unstated extent
mismatch is exactly how the defect got in.  Two different nav queries are in play:
  (a) the COVERAGE gate in the caller, trap_Nav_FindNearestPoly with {64,64,96} —
      answers only "does SOME poly exist near here", and a phantom satisfies it by
      finding a poly at the far corner of that box;
  (b) THIS measurement, which is taken against Nav_FindPath's own snap, and
      Nav_FindPath does NOT use any caller extent — it hardcodes kDefaultExtents
      {280,280,480} (nav_impl.cpp) for its findNearestPoly start snap.
The bound below is stated against (b).  The {280,280,480} envelope is what makes
the measurement meaningful: it is wide enough that a far-off-mesh candidate still
resolves to SOME start poly and therefore reports a large, honest snap distance,
instead of failing the query and being indistinguishable from "no mesh at all".
The bound is NOT stated against the {64,64,96} coverage box.
*/
#define WI_RESTSNAP_MAX_HORIZ (2.0f * WI_PLAYER_HALF_W)   /* 2 x NAV_WALKABLE_RADIUS: the bake's own horizontal erosion inset */
/* Walk-climb mirror, same convention as WI_PLAYER_HALF_W above (nav_impl.cpp's
   NAV_WALKABLE_CLIMB is file-static there, so game-side code mirrors the value;
   it is itself STEPSIZE from bg_local.h:9, which is the shared source of truth). */
#define WI_WALK_CLIMB      18.0f    /* NAV_WALKABLE_CLIMB == STEPSIZE (bg_local.h) */
/* Structural stand height + one walk-climb: beyond this the snap has bound the
   candidate to a different floor, not the one it stands on.  See the block above. */
#define WI_RESTSNAP_MAX_VERT  ( -(WI_PLAYER_MINS_Z) + WI_WALK_CLIMB )   /* 24 + 18 = 42u */

static qboolean WiredIntel_RestSurfaceIsRoutable( const vec3_t cand ) {
    static navPath_t s_snapProbe;   /* ~7 KB — file-static, single-threaded think */
    vec3_t  self;
    float   dx, dy, dz;
    int     n;

    VectorCopy( cand, self );
    n = trap_Nav_FindPath( self, self, WI_NAV_AGENT_PLAYER, &s_snapProbe );
    if ( NAV_FINDPATH_COUNT( n ) <= 0 )
        return qfalse;              /* no poly reachable from here at all */

    /* positions[0] is the candidate clamped onto the start polygon — the snap.
       TWO axes, each bounding a different quantity:
         HORIZONTAL — the 2R erosion inset bounds how far a genuine floor candidate
           can sit laterally from the baked mesh.
         VERTICAL — the structural 24u stand offset plus one walk-climb.  Measured
           on the discriminating population: a snap exceeding this bound the point
           to a floor other than the one the collision trace found (63u observed).
           Absolute value: an upward binding to a ledge is the same defect.
       The horizontal test is unchanged; the vertical one is the added axis. */
    dx = s_snapProbe.positions[0][0] - cand[0];
    dy = s_snapProbe.positions[0][1] - cand[1];
    dz = s_snapProbe.positions[0][2] - cand[2];
    if ( ( dx*dx + dy*dy ) > ( WI_RESTSNAP_MAX_HORIZ * WI_RESTSNAP_MAX_HORIZ ) )
        return qfalse;
    if ( fabsf( dz ) > WI_RESTSNAP_MAX_VERT )
        return qfalse;              /* bound to a different floor, not this one */
    return qtrue;
}
#endif

/*
==================
WiredIntel_ButtonTouchOrigin
==================
Resolve a func_button to the point a player STANDS at to press it — the standable,
nav-reachable floor in FRONT of the button's face, within touch range of the brush —
NOT its bbox center.  For a wall-mounted button the center is embedded in the wall,
so the naive center-snap (WiredIntel_EntityNavOrigin) lands on whatever poly is
nearest the center: routinely a poly on the wrong side of the wall, and — because the
navmesh can mark a poly standable that collision calls solid — a point the player box
cannot actually stand on.  The follower then drives the bot into the wall toward a
solid target and wedges.

A Q1 func_button is pressed by the client BBOX contacting the brush (Q1_Button_Touch,
no grounded requirement) — so its GOAL is the floor spot a player STANDS on where the
standing box actually OVERLAPS the button AABB (the touch predicate).  The center-snap
misses this: for a wall/ledge button the center is inside solid and its nearest-poly
snap lands on the wrong side.  This walks outward from EACH of the four horizontal
faces, drops the standing box from ABOVE the button top (so the drop is never started
inside the brush — the artifact that made an earlier center-height drop report the whole
front as solid), and returns the first floor spot that is (a) collision-standable,
(b) TRUE touch range — the standing box at that rest floor overlaps the button AABB on
all three axes, so walking onto it fires Q1_Button_Touch, and (c) navmesh-covered.
Closest-to-the-face candidates are tried first, all four faces, so a button pressed
from any side (a wall plate, a shaft-bottom control) resolves to its real walk-in spot.
Face-normal + touch-envelope only — no map/button constant.

FALLBACK: if no candidate passes every gate but some candidate was collision-standable
AND in true touch range, the closest such candidate is returned rather than qfalse.
For a button the bbox centre is never a valid goal — measured, centre-standable was 0
of 40 buttons across all 9 maps — so letting the caller fall back to the centre puts
the goal inside the wall.  A standable in-touch-range spot that merely lacks navmesh
coverage is strictly better than that.  Returns qfalse ONLY when not even standability
and touch range were satisfied on any face; the caller then behaves as it does today.
*/
static qboolean WiredIntel_ButtonTouchOrigin( const gentity_t *button, vec3_t out ) {
#if FEAT_RECAST_NAVMESH
    vec3_t bcen, half;
    float  dropTop;
    /* Four horizontal outward faces, closest tried first. */
    static const float FACES[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
    int fi;

    bcen[0] = 0.5f*(button->r.absmin[0]+button->r.absmax[0]);
    bcen[1] = 0.5f*(button->r.absmin[1]+button->r.absmax[1]);
    bcen[2] = 0.5f*(button->r.absmin[2]+button->r.absmax[2]);
    VectorSubtract( button->r.absmax, button->r.absmin, half );
    VectorScale( half, 0.5f, half );
    /* Start the drop ABOVE the button top: a drop begun at button-centre height starts
       INSIDE the brush for a button whose z-band spans the stand height, reporting a
       false startsolid across the whole front (the earlier resolver's blind spot). */
    dropTop = button->r.absmax[2] + 40.0f;

    /* Best-rejected fallback (part 2).  A candidate that is collision-standable AND
       in true touch range, but fails the navmesh coverage/routability gate, is still
       strictly better than the bbox centre: the centre is inside the wall for every
       wall-mounted button (measured: centre-standable was 0 of 40 buttons across all
       9 maps, so for a button the centre is NEVER the right answer).  Remember the
       FIRST such candidate and return it only if all four faces are exhausted.
       ORDERING: first-found == closest-to-the-face, because the d loop ascends from
       faceHalf+2 and faces are tried in a fixed order — the same closest-first
       ordering the primary search already uses, so no new criterion is introduced.
       If not even standability+touch-range is ever satisfied we still return qfalse
       and let the caller do what it does today — we do not invent a goal. */
    qboolean haveFallback = qfalse;
    vec3_t   fallbackCand;

    for ( fi = 0; fi < 4; fi++ ) {
        float fx = FACES[fi][0], fy = FACES[fi][1];
        float faceHalf = fabsf(fx)*half[0] + fabsf(fy)*half[1];
        float d;
        for ( d = faceHalf + 2.0f; d <= WI_TOUCHSPOT_MAX_HORIZ; d += 6.0f ) {
            vec3_t mins, maxs, ds, de, cand;
            trace_t dr;
            float   px = bcen[0] + fx*d, py = bcen[1] + fy*d, rz;

            /* (a) collision-standable: drop the standing box from above the button to a
               rest floor; require it to FIT (no startsolid → standing headroom). */
            VectorSet( mins, -WI_PLAYER_HALF_W, -WI_PLAYER_HALF_W, WI_PLAYER_MINS_Z );
            VectorSet( maxs,  WI_PLAYER_HALF_W,  WI_PLAYER_HALF_W, WI_PLAYER_MAXS_Z );
            VectorSet( ds, px, py, dropTop );
            VectorSet( de, px, py, button->r.absmin[2] - 300.0f );
            trap_Trace( &dr, ds, mins, maxs, de, ENTITYNUM_NONE, MASK_PLAYERSOLID );
            if ( dr.startsolid || dr.allsolid || dr.fraction >= 1.0f )
                continue;   /* no clearance / no floor within reach → not standable */
            rz = dr.endpos[2];                        /* player ORIGIN resting on the floor */
            VectorSet( cand, px, py, rz );

            /* (b) TRUE touch range: the standing box at the rest floor must overlap the
               button AABB on ALL three axes, so walking onto it contacts the brush and
               fires Q1_Button_Touch (the same box-vs-AABB the engine touch dispatch uses). */
            if ( px + WI_PLAYER_HALF_W < button->r.absmin[0] || px - WI_PLAYER_HALF_W > button->r.absmax[0] ) continue;
            if ( py + WI_PLAYER_HALF_W < button->r.absmin[1] || py - WI_PLAYER_HALF_W > button->r.absmax[1] ) continue;
            if ( rz + WI_PLAYER_MAXS_Z < button->r.absmin[2] || rz + WI_PLAYER_MINS_Z > button->r.absmax[2] ) continue;

            /* (c) navmesh-covered AND the rest surface is one a route can be planned
               against.  Mere existence of a poly inside the search box is not enough:
               that box is 64/64/96, so a candidate resting on a surface the mesh never
               covered (a plate over a void, a mover lid, an actor bbox) still finds a
               poly at the far corner and passes.  Requiring the SNAP to be short —
               within the bake's own erosion inset — keeps candidates standing on real
               floor and drops the ones that only borrow a distant poly. */
            /* Standable + in touch range: eligible as the least-bad fallback even if
               the coverage/routability gates below reject it (closest-first order). */
            if ( !haveFallback ) {
                VectorCopy( cand, fallbackCand );
                haveFallback = qtrue;
            }

            {
                vec3_t ext = { 64.0f, 64.0f, 96.0f };
                if ( trap_Nav_FindNearestPoly( cand, ext ) == 0 )
                    continue;
                if ( !WiredIntel_RestSurfaceIsRoutable( cand ) )
                    continue;
            }

            VectorCopy( cand, out );
            return qtrue;   /* standable + touch-range + covered walk-in spot */
        }
    }
    if ( haveFallback ) {
        /* No fully-gated spot on any face, but a standable in-touch-range one exists.
           Prefer it over the caller's centre fallback — the centre is inside the wall. */
        VectorCopy( fallbackCand, out );
        return qtrue;
    }
    return qfalse;   /* not even a standable, in-touch-range spot on any face */
#else
    (void)button; (void)out;
    return qfalse;
#endif
}

/*
==================
WiredIntel_EntityNavOrigin
==================
Reduce an entity to a reachable nav destination: the center of its bounding box,
verified to be near the walkable mesh. A brush volume (trigger, button, door) is
navigated to by its bbox center; the nav follower snaps that point down to the
walkable floor via its own findNearestPoly. Under Recast, AAS areanum is always 0,
so it cannot serve as the reachability signal — this poly snap-test does. Returns
qtrue and fills 'out' when the center is on/near the mesh; qfalse for an off-mesh
entity (floating/embedded) that must not be set as an unreachable goal. Shared by
the exit finder and the button finder so there is one reachability path.

For a func_button whose bbox center does not itself sit on a standable floor — the
wall-mounted case, where the center is inside the wall and its naive nearest-poly snap
lands on the wrong side (a solid-embedded poly the follower cannot reach) — resolve to
the real touch-spot in FRONT of the face (WiredIntel_ButtonTouchOrigin) instead.  A
floor-level button whose center IS standable, and the exit / plat / any non-button
entity, keep the center-snap unchanged (no regression).
*/
static qboolean WiredIntel_EntityNavOrigin( const gentity_t *ent, vec3_t out ) {
    vec3_t center;
    VectorAdd( ent->r.absmin, ent->r.absmax, center );
    VectorScale( center, 0.5f, center );

#if FEAT_RECAST_NAVMESH
    /* Wall-button touch-spot projection: only for a func_button whose center is NOT
       itself standable (the wall-mounted case).  A plain floor-button's center drops
       to a standable floor and keeps today's center-snap; the exit / plat / triggers
       are not buttons and never take this path. */
    if ( ent->classname && G_ClassnameIs( (gentity_t *)ent, "func_button" ) ) {
        trace_t dr;
        vec3_t  mins = { -WI_PLAYER_HALF_W, -WI_PLAYER_HALF_W, WI_PLAYER_MINS_Z };
        vec3_t  maxs = {  WI_PLAYER_HALF_W,  WI_PLAYER_HALF_W, WI_PLAYER_MAXS_Z };
        vec3_t  ds   = { center[0], center[1], center[2] + 8.0f };
        vec3_t  de   = { center[0], center[1], center[2] - 200.0f };
        qboolean centerStandable;
        trap_Trace( &dr, ds, mins, maxs, de, ENTITYNUM_NONE, MASK_PLAYERSOLID );
        /* Center standable = the standing box drops onto a rest floor near the center
           and fits there (no startsolid).  A wall button's center is inside solid, so
           startsolid/allsolid → not standable → take the touch-spot path. */
        centerStandable = ( !dr.startsolid && !dr.allsolid && dr.fraction < 1.0f );
        if ( !centerStandable ) {
            vec3_t touch;
            if ( WiredIntel_ButtonTouchOrigin( ent, touch ) ) {
                VectorCopy( touch, out );
                return qtrue;
            }
        }
    }
#endif

#if FEAT_RECAST_NAVMESH
    {
        /* Quake-space extents (Z is up); the trap converts to Recast. Wide
           horizontal + tall vertical so a center that floats above the floor
           (or sits inside a brush) still snaps down to the walkable poly below.
           ref 0 → too far from any walkable poly to route to. Matches the
           follower's generous extents. */
        vec3_t extents = { 280.0f, 280.0f, 480.0f };
        if ( trap_Nav_FindNearestPoly( center, extents ) == 0 ) {
            return qfalse;
        }
    }
#endif
    VectorCopy( center, out );
    return qtrue;
}

/*
==================
WiredIntel_FindExitOrigin
==================
Locate the level's exit — the brush trigger that changes the map when touched
(classname "q1_trigger_changelevel") — and return the center of its bounding
box, a reachable nav destination. If several exist, the nearest to 'from' wins.
Cached per level (the exit does not move); the cache is keyed on level.startTime
so it re-resolves after a map change. Returns qtrue and fills 'out' when an exit
exists, qfalse on a map with no changelevel trigger (e.g. an arena) — callers
must treat qfalse as "no exit goal" and fall back to normal AI.
*/
static qboolean WiredIntel_FindExitOrigin( const vec3_t from, vec3_t out ) {
    static int    s_cachedStartTime = -1;
    static qboolean s_haveExit      = qfalse;
    static vec3_t s_exitOrigin;

    if ( s_cachedStartTime != level.startTime ) {
        /* First query this level — scan for the nearest reachable changelevel
           trigger. */
        int i;
        float bestDistSq = 0.0f;
        s_cachedStartTime = level.startTime;
        s_haveExit        = qfalse;
        for ( i = MAX_CLIENTS; i < level.num_entities; i++ ) {
            gentity_t *ent = &g_entities[i];
            vec3_t center, delta;
            float distSq;
            if ( !ent->inuse || !ent->classname ) continue;
            if ( Q_stricmp( ent->classname, "q1_trigger_changelevel" ) != 0 ) continue;
            if ( !WiredIntel_EntityNavOrigin( ent, center ) ) {
                continue;   /* this exit is off the mesh — skip it */
            }
            VectorSubtract( center, from, delta );
            distSq = VectorLengthSquared( delta );
            if ( !s_haveExit || distSq < bestDistSq ) {
                s_haveExit = qtrue;
                bestDistSq = distSq;
                VectorCopy( center, s_exitOrigin );
            }
        }
    }

    if ( !s_haveExit ) {
        return qfalse;
    }
    VectorCopy( s_exitOrigin, out );
    return qtrue;
}

/* =========================================================================
   ELEVATOR-RIDE PRODUCER — the first live consumer of the sequenced-goal
   substrate. When the bot's objective lies across a mover it must ride, this
   installs a board -> wait-for-mover-state -> step-off sequence into the
   brain's botSequencedGoal_t; the landed WiredIntel_StepSequencedGoal driver
   then walks it (GoTo onto the boarding surface starts a touch-activated plat,
   WAIT_MOVERSTATE yields until the car reaches its far stop, GoTo steps off).

   Format-agnostic by construction: a rideable mover is recognized by the
   shared plat touch handler (ent->touch == Q3_Touch_Plat, installed by BOTH
   map formats) plus a format-neutral geometry test on pos1/pos2 — never a
   classname and never a format branch. The boarding/step-off points are
   DERIVED from the mover's own geometry (pos1 = lower/boarding stop, pos2 =
   upper/delivery stop — the game-VM equivalent of navMoverDesc_t's shaftFloor/
   topLedge) snapped to the nearest walkable poly, never hardcoded coordinates.
   ========================================================================= */

/* A rideable mover's pos1->pos2 travel must be primarily vertical for a ride to
   make sense (an elevator lifts the rider; a horizontally-sliding mover is a
   door the bot walks through, handled by the activation path). "Primarily
   vertical" = the |z| component of the travel dominates the horizontal, by this
   factor. Kept format-neutral: it reads only pos1/pos2, no classname. */
#define WI_RIDE_VERTICAL_RATIO   1.5f
/* A ride only matters if the car actually travels a meaningful height — below
   this a "plat" is a step the bot walks up, not something to board and wait on.
   In Quake world units; well under a real elevator shaft, above nav jitter. */
#define WI_RIDE_MIN_TRAVEL       48.0f
/* Hold radius for the boarding/step-off GoTo steps. Wide enough that "arrived on
   the car" is satisfied by standing on the platform surface (the boarding trigger
   spans the shaft), matching the plat trigger's own generous extents. */
#define WI_RIDE_GOTO_RANGE       64.0f
/* The ride detector's own re-arm period, in milliseconds — the single statement of
   the cadence that WiredIntel_TrySetRideGoal rate-limits detection with. The board
   -reachability persistence window in WiredIntel_FindRideMover is derived from
   THIS (two full cycles), so the two cannot drift apart: changing the detector's
   cadence moves the persistence window with it, by construction. */
#define WI_RIDE_RETRY_PERIOD_MS  1000.0f
/* Non-zero sequence identity stamped on an installed ride sequence ('RIDE'). The
   substrate uses sequenceId only as an identity/idle sentinel (0 = idle); any
   non-zero value works. */
#define WI_RIDE_SEQUENCE_ID      0x52494445
/* Sequence identity for an installed jump-touch ('JMPT'). */
#define WI_JUMPTOUCH_SEQUENCE_ID 0x4A4D5054

/*
==================
WiredIntel_MoverIsRideable
==================
Format-neutral test: is 'ent' a rideable vertical elevator platform? True when
the entity carries the shared plat touch handler (installed by both q1 and q3
plat spawns — the game-VM analog of the extractMovers plat classification) AND
its pos1->pos2 travel is primarily vertical and tall enough to be a ride rather
than a step. Reads only format-neutral fields (ent->touch address, pos1, pos2);
no classname string compare, no q1_/format branch.
*/
static qboolean WiredIntel_MoverIsRideable( const gentity_t *ent ) {
    vec3_t travel;
    float  horiz, vert;

    if ( !ent->inuse ) {
        return qfalse;
    }
    /* The plat touch handler address is the format-neutral "this is a rideable
       platform" marker (SP_q1_func_plat / SP_q3_func_plat both set it). A door,
       button, train, or trigger carries a different (or no) touch handler. */
    if ( ent->touch != Q3_Touch_Plat ) {
        return qfalse;
    }
    /* Travel geometry: pos1 = boarding stop, pos2 = delivery stop. Require a
       predominantly vertical, tall-enough displacement. */
    VectorSubtract( ent->pos2, ent->pos1, travel );
    horiz = sqrtf( travel[0] * travel[0] + travel[1] * travel[1] );
    vert  = fabsf( travel[2] );
    if ( vert < WI_RIDE_MIN_TRAVEL ) {
        return qfalse;   /* barely moves vertically — a step, not a ride */
    }
    if ( vert < horiz * WI_RIDE_VERTICAL_RATIO ) {
        return qfalse;   /* travel is not primarily vertical — not an elevator */
    }
    return qtrue;
}

/*
==================
WiredIntel_MoverRideEndpoints
==================
Derive the boarding point and step-off point from the mover's own geometry, each
snapped to the nearest walkable poly — NOT hardcoded. Boarding = pos1 (the lower
stop, where a shaft-trigger plat rests and its trigger sits; the game-VM analog
of navMoverDesc_t.shaftFloor). Step-off = pos2 (the upper stop / delivery; the
analog of topLedge). Each is centered horizontally on the platform brush (pos*
is the brush origin; the bbox center over it is the walkable surface) and snapped
down onto the mesh with the same trap_Nav_FindNearestPoly the OMC endpoint-snap
and WiredIntel_EntityNavOrigin already use. Returns qfalse (→ caller HALTS this
mover, never guesses) if either endpoint cannot be snapped to a walkable poly.
*/
static qboolean WiredIntel_MoverRideEndpoints( const gentity_t *ent,
                                               vec3_t boardOut, vec3_t stepOffOut ) {
    vec3_t halfSpan, board, stepOff;

    /* Horizontal center of the platform brush over its two stops. pos1/pos2 are
       the brush origin at each stop; the walkable surface is the brush's top,
       centered in x/y. Use the bbox half-span in x/y to center; z stays at the
       stop (the snap searches downward onto the surface). */
    VectorSubtract( ent->r.maxs, ent->r.mins, halfSpan );
    VectorScale( halfSpan, 0.5f, halfSpan );

    VectorCopy( ent->pos1, board );
    board[0] += ent->r.mins[0] + halfSpan[0];
    board[1] += ent->r.mins[1] + halfSpan[1];

    VectorCopy( ent->pos2, stepOff );
    stepOff[0] += ent->r.mins[0] + halfSpan[0];
    stepOff[1] += ent->r.mins[1] + halfSpan[1];

#if FEAT_RECAST_NAVMESH
    {
        /* Same generous extents WiredIntel_EntityNavOrigin uses: wide horizontal,
           tall vertical, so a stop that floats above/below the adjacent floor
           still snaps onto the walkable poly. ref 0 → not near the mesh → cannot
           derive this endpoint → caller HALTS (no guessed coordinate). */
        vec3_t extents = { 280.0f, 280.0f, 480.0f };
        if ( trap_Nav_FindNearestPoly( board, extents ) == 0 ) {
            return qfalse;
        }
        if ( trap_Nav_FindNearestPoly( stepOff, extents ) == 0 ) {
            return qfalse;
        }
    }
#endif

    VectorCopy( board,   boardOut );
    VectorCopy( stepOff, stepOffOut );
    return qtrue;
}

/*
==================
WiredIntel_InstallRideSequence
==================
Pure builder (no detection, no nav queries): install the canonical elevator-ride
sequence into 'sg' from already-derived points and the mover's entity number.
  step 0  SEQ_STEP_GOTO(boardPt)             — drive onto the boarding surface;
                                                for a touch-activated plat, arriving
                                                on the trigger volume starts the ride.
  step 1  SEQ_STEP_WAIT_MOVERSTATE(entnum)   — yield until the car reaches POS2
                                                (delivery), with the landed embedded
                                                stall-inference (give up on a wedged
                                                car → normal goal selection).
  step 2  SEQ_STEP_GOTO(stepOffPt)           — drive off onto the delivery surface.
Isolated from the detector so the self-test can exercise the exact installed shape
(step kinds, points, mover entnum) on scratch state with no live map.
*/
static void WiredIntel_InstallRideSequence( botSequencedGoal_t *sg, int moverEntitynum,
                                            const vec3_t boardPt, const vec3_t stepOffPt ) {
    WiredIntel_SequencedGoalReset( sg );

    sg->steps[0].type  = SEQ_STEP_GOTO;
    VectorCopy( boardPt, sg->steps[0].point );
    sg->steps[0].range = WI_RIDE_GOTO_RANGE;

    sg->steps[1].type            = SEQ_STEP_WAIT_MOVERSTATE;
    sg->steps[1].moverEntitynum  = moverEntitynum;

    sg->steps[2].type  = SEQ_STEP_GOTO;
    VectorCopy( stepOffPt, sg->steps[2].point );
    sg->steps[2].range = WI_RIDE_GOTO_RANGE;

    sg->stepCount  = 3;
    sg->cursor     = 0;
    sg->changeTime = level.time;
    sg->sequenceId = WI_RIDE_SEQUENCE_ID;
    sg->active     = qtrue;
}

/* The ride DETECTOR + PRODUCER (WiredIntel_FindRideMover / WiredIntel_TrySetRideGoal)
   are defined later in the file, after the nav-probe helpers they use
   (WiredIntel_PathReaches / WiredIntel_PathFrontier and the WI_NAV_PROGRESS_*
   macros) and BotCanActivate. Forward-declare the entry point here so the earlier
   WiredIntel_TrySetExitGoal can call it. */
static qboolean WiredIntel_TrySetRideGoal( bot_state_t *bs, const vec3_t goalOrigin );

/*
==================
WiredIntel_TrySetExitGoal
==================
Idle fallback: point an otherwise-goalless bot at the level's exit so it makes
forward progress instead of random-roaming. This is the low-priority "nothing
better to do" hint (no directive was issued), so it stays UNLOCKED — a visible
enemy still diverts the bot to full combat via AINode_Seek_LTG. A no-op
returning qfalse on a map with no exit trigger. Still opens a blocking gate first
via the activation goal when one is in the way.

An issued exit order (DIR_GOTO_EXIT) produces the SAME unlocked
LTG_DEFENDKEYAREA goal this path does — it is not a separate "locked,
fight-while-advance" path, which is what this comment used to claim. Measured:
bs->ordered is written and read only by the LTG_TEAMACCOMPANY and CTF logic in
ai_dmq3.c; neither item selection nor exit selection ever reads it. The one
WiredIntel write (WiredIntel_InstallJumpTouch) sets it to hold the directive
lock for the JUMP-TOUCH SEQUENCE, not for the ordered-exit path. So the only
behavioural difference between issued and idle exit goals is the directive lock
that BotReceiveDirective sets, not anything keyed on bs->ordered.
*/
/* PROGRESSION-SPINE INSTRUMENT (measurement only; no behavioural effect).
 *
 * This function is the sole entry to the exit/door/button progression, so where a
 * run stops inside it is the whole diagnosis.  Every counter here increments
 * UNCONDITIONALLY — before any debug cvar and outside every throttle — because the
 * shipped FOUND/NONE instrument next door demonstrates the failure mode: it shares
 * one 2 s/client throttle between two call paths, so the rarer path is almost never
 * printed and its scarcity reads as a low call rate rather than as throttling.
 * Only WiredIntel_ExitChainReport's PRINT is gated.  Gate the print, never the count. */
/* STEP 3 — candidate-utility telemetry (measurement only; changes NO decision).
 *
 * Door and ride relevance are decided today by thresholding a real magnitude and
 * then discarding it: DoorCutsRoute computes closedGap-openGap ("how much does
 * opening this door advance the route") and keeps only the boolean; FindRideMover
 * computes curFrontier-deliverFrontier and does the same.  Those are exactly the
 * bases a utility model needs, so they are recorded where they are already
 * computed.  Nothing here feeds a comparison in this pass — the fallback chain is
 * untouched.  It only lets the would-be candidate scores be measured against the
 * item score the chain actually acted on.
 *
 * Frontier values are SQUARED world units (compared against WI_NAV_PROGRESS_GAIN
 * squared), so the normaliser is that same squared shipped constant and the result
 * is a dimensionless route-gain ratio.  No new constant. */
static float s_candDoorGain     = -1.0f;   /* < 0 = not evaluated yet */
static float s_candRideGain     = -1.0f;
static float s_candExitFrontier = -1.0f;   /* RAW only — exit base weight UNRATIFIED */
static float s_candRouteFrontier = -1.0f;  /* bot's own route frontier toward the exit */

/* Defined below, with WiredIntel_PathFrontier and the WI_NAV_PROGRESS_* constants
 * it needs.  Fills the ride/exit magnitudes read-only; door is excluded on purpose. */
static void WiredIntel_CandProbeRideExit( const vec_t *probeOrigin );

static int s_exitEntered, s_exitNoOrigin, s_exitActivation, s_exitRide, s_exitPlain;
static int s_exitArmed, s_btnQueued, s_btnPressed, s_doorsOpened;

void WiredIntel_ExitChainCount( int which ) {
    switch ( which ) {
    case 0: s_btnQueued++;   break;   /* a button was queued for a gate      */
    case 1: s_btnPressed++;  break;   /* a queued button was confirmed press */
    case 2: s_doorsOpened++; break;   /* a target-opened door left POS1      */
    default: break;
    }
}

void WiredIntel_ExitChainReport( void ) {
    if ( !trap_Cvar_VariableIntegerValue( "nav_botdebug" ) ) return;
    Com_Log( SEV_INFO, LOG_CH(ch_botai),
        "[EXITCHAIN] entered=%d noOrigin=%d activation=%d ride=%d plain=%d armed=%d"
        " | btnQueued=%d btnPressed=%d doorsOpened=%d\n",
        s_exitEntered, s_exitNoOrigin, s_exitActivation, s_exitRide, s_exitPlain,
        s_exitArmed, s_btnQueued, s_btnPressed, s_doorsOpened );
}

/* STEP 3 emit — the PAIRED sample.  Reports each candidate's would-be utility
 * beside the item score the chain actually acted on, at the same evaluation.
 *
 * doorU / rideU use the shipped [0,100] item scale and the shipped item falloff
 * 1/(1+tt/220) so every candidate decays identically; their base is the route-gain
 * ratio already computed by the relevance tests.  exitDist / exitFall are RAW: the
 * exit's base weight does not exist in the codebase and is not invented here. */
void WiredIntel_CandReport( float itemScore, qboolean itemIsLtg,
                            const vec_t *probeOrigin ) {
    float doorU = -1.0f, rideU = -1.0f, exitFall = -1.0f, routeF = -1.0f;
    if ( !trap_Cvar_VariableIntegerValue( "nav_botdebug" ) ) return;

    /* FORCED EVALUATION — ride + exit ONLY (measurement; enters no goal path).
     *
     * The magnitudes above are normally computed inside the exit chain, and that
     * chain is never entered, so every sample read -1: the "pairs" were the
     * coupling measuring itself rather than a comparison.  Computing them here
     * makes the samples real.
     *
     * DOOR IS DELIBERATELY EXCLUDED and stays -1.  Its magnitude needs `openGap`,
     * which only exists inside a probe that MUTATES door poly flags
     * (SetPolyFlagsForDoor around a PathFrontier read, :2223-2225).  That probe has
     * a documented corruption history where a balanced toggle around a wrong
     * baseline left 8 of 14 doors stuck at BLOCKED while every pairing audit passed
     * (nav_local.h:138-147).  Forcing it here would move the highest-risk mutation
     * in this subsystem onto the hottest path in the bot.  So the door magnitude is
     * STRUCTURALLY UNMEASURABLE without mutation, and that is reported as a finding.
     *
     * Ride and exit are pure reads: WiredIntel_PathFrontier is a trap_Nav_FindPath
     * into a file-static buffer with no flag writes, and the exit term is vector
     * arithmetic.  Nothing here sets a goal, queues a button, or touches a door. */
    /* Reset here: TrySetExitGoal (which owns the other reset) is never entered, so
     * without this a value would persist across evaluations and be reported stale. */
    s_candExitFrontier  = -1.0f;
    s_candRouteFrontier = -1.0f;
    WiredIntel_CandProbeRideExit( probeOrigin );
    if ( s_candDoorGain >= 0.0f ) {
        doorU = s_candDoorGain * 100.0f;
        if ( doorU > 100.0f ) doorU = 100.0f;
    }
    if ( s_candRideGain >= 0.0f ) {
        rideU = s_candRideGain * 100.0f;
        if ( rideU > 100.0f ) rideU = 100.0f;
    }
    if ( s_candExitFrontier >= 0.0f ) {
        /* the shipped item falloff, applied to the exit's own straight-line proxy */
        float tt = s_candExitFrontier * 1000.0f / 300.0f + 1.0f;
        exitFall = 1.0f / ( 1.0f + tt / 220.0f );
    }
    if ( s_candRouteFrontier >= 0.0f ) {
        routeF = s_candRouteFrontier;
    }
    Com_Log( SEV_INFO, LOG_CH(ch_botai),
        "[CANDSCORE] item=%.2f ltg=%d | doorU=%.2f rideU=%.2f routeF=%.3f"
        " | exitDist=%.0f exitFall=%.4f\n",
        itemScore, itemIsLtg ? 1 : 0, doorU, rideU, routeF,
        s_candExitFrontier, exitFall );
}

qboolean WiredIntel_TrySetExitGoal( bot_state_t *bs ) {
    vec3_t exitOrigin;

    if ( !bs || !bs->wiredIntelActive ) {
        return qfalse;
    }
    s_exitEntered++;   /* unconditional: gated on nothing */
    /* Instrument only: reset the per-evaluation candidate magnitudes so a stale
     * value from an earlier frame can never be reported as this frame's. */
    s_candDoorGain     = -1.0f;
    s_candRideGain     = -1.0f;
    s_candExitFrontier = -1.0f;
    if ( !WiredIntel_FindExitOrigin( bs->origin, exitOrigin ) ) {
        /* No changelevel trigger on this map (e.g. an arena), or the exit is off
           the nav mesh — either way there is no exit goal to set. */
        s_exitNoOrigin++;
        return qfalse;
    }

    /* Before heading straight for the exit, check whether a closed target-opened
       door blocks the way there. If so, the activation goal takes priority: it
       drives the bot to press the button(s) that open the gate first. On a map
       with no such gate this is a no-op and we fall through to the exit goal. */
    if ( WiredIntel_TrySetActivationGoal( bs, exitOrigin ) ) {
        s_exitActivation++;
        return qtrue;
    }

    /* Next, check whether a rideable elevator lies on the route to the exit that
       the bot must board and ride (a mover the nav corridor cannot cross on foot).
       If so, install the board -> wait-for-delivery -> step-off sequence; the
       sequenced-goal substrate then walks it. A byte-identical no-op on a map with
       no ride-requiring mover (or when the exit is already reachable) and we fall
       through to the plain exit goal. Ordered after activation because the button
       that clears the path to the elevator may itself be behind a gate. */
    if ( WiredIntel_TrySetRideGoal( bs, exitOrigin ) ) {
        s_exitRide++;
        return qtrue;
    }

    /* WiredIntel_FindExitOrigin already verified the center is near the nav mesh,
       so hand the origin to the shared goal path. The nav follower routes by
       origin (AAS areanum is always 0 under Recast, so it is not a reachability
       signal here). */
    s_exitPlain++;
    /* Instrument only: the exit's RAW distance magnitude and the shipped falloff
     * that every other candidate decays by.  Deliberately NOT combined into a
     * utility — the exit's base weight is unratified, and inventing one is exactly
     * what this pass refuses to do.  Publishing the raw terms is what lets the
     * weight be derived from data later. */
    {
        vec3_t d;
        VectorSubtract( exitOrigin, bs->origin, d );
        s_candExitFrontier = VectorLength( d );
    }
    BotSetOriginGoal( bs, exitOrigin, 96.0f );

    /* Issue the exit as a real directive rather than a bare goal, so it arms the
       same lock a human "botorder exit" arms. Without this the goal is UNLOCKED
       and a visible enemy diverts the bot to full combat via AINode_Seek_LTG —
       which on a monster-dense campaign level means the exit is re-selected and
       re-abandoned indefinitely and never reached. That failure is precisely what
       the lock was written for (see the comment above the arming block in
       BotReceiveDirective), but nothing in the engine ever issued the directive:
       BotReceiveDirective's only callers are human chat/console, so the ordered
       path never fired on its own.

       Type/persistence match the human "exit" order exactly (DIR_GOTO_EXIT,
       expire_time 0 = persist until reached) — the two paths are now the same
       mechanism, not two similar ones.

       ARM ONLY ON THE TRANSITION. This function runs from WiredIntel_ChooseLTGItem
       on a hot path. WiredIntel_ArmDirective resets lastProgressCheck, which is
       the baseline the stall watchdog measures from, so re-arming an
       already-armed DIR_GOTO_EXIT every call would restart that sample forever
       and silently disable the abort path. Re-issuing an identical, already-locked
       directive is therefore a no-op. */
    {
        botDirective_t *d = &bs->directives.tactical;
        if ( d->type != DIR_GOTO_EXIT || !bs->directives.directiveLocked ) {
            d->type        = DIR_GOTO_EXIT;
            d->expire_time = 0;   /* persist until the exit is reached */
            WiredIntel_ArmDirective( bs );
            s_exitArmed++;

            if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
                vec3_t delta;
                VectorSubtract( exitOrigin, bs->origin, delta );
                Com_Log( SEV_INFO, LOG_CH(ch_botai),
                    "cl=%d ordered -> exit goal (%.0f %.0f %.0f) dist=%.0f\n",
                    bs->client, exitOrigin[0], exitOrigin[1], exitOrigin[2],
                    VectorLength( delta ) );
            }
        }
    }

    if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
        static float s_exitLog[MAX_CLIENTS];
        if ( FloatTime() - s_exitLog[bs->client] > 2.0f ) {
            vec3_t delta;
            s_exitLog[bs->client] = FloatTime();
            VectorSubtract( exitOrigin, bs->origin, delta );
            Com_Log( SEV_INFO, LOG_CH(ch_botai),
                "cl=%d idle -> exit goal (%.0f %.0f %.0f) dist=%.0f\n",
                bs->client, exitOrigin[0], exitOrigin[1], exitOrigin[2],
                VectorLength( delta ) );
        }
    }
    return qtrue;
}

/* =========================================================================
   ENTITY ACTIVATION — navigate to a point, then interact by arrival

   A target-opened door (a func_door with a targetname, opened by a button or
   counter chain rather than by walking up to it) is a nav-blocked gate: while
   it is closed its polys carry NAVPOLY_BLOCKED, so a route past it fails. To
   pass, the bot must press the button(s) that fire the door open. A Q1 button
   fires on client touch (Q1_Button_Touch requires other->client), so a bot that
   walks its bbox onto the button presses it — the same touch primitive the exit
   goal already relies on. This subsystem detects such a gate, resolves its
   button(s) through the target chain, and queues their reachable origins for the
   bot to visit in turn.

   Only a client-brain (a bot) drives this: the button touch is client-gated in
   the engine, and monsters never run this goal path at all. BotCanActivate() is
   the single per-brain capability gate — the seam a future monster-activatable
   capability (or a different interact family, e.g. elevators) would extend.
   ========================================================================= */

/*
==================
BotCanActivate
==================
Per-entity capability gate for the "navigate-and-interact" family, driven by the
character manifest's `can_activate` flag (read via the same char:<name>:<field>
trap the monster spawn uses for bbox/movement/attack). A character that declares
`can_activate = false` (e.g. a dog) is denied; a character that declares it true,
or omits it, may activate — so a plain bot with no such declaration presses
buttons by default. The button-press primitive is additionally client-gated in the
engine (Q1_Button_Touch ignores non-clients), so this data flag layers a per-brain
policy on top of that physical gate. The manifest read is cached per client for
the life of the map (the character does not change mid-session). This is the seam
a future monster-activatable capability extends without touching the callers.
*/
static qboolean BotCanActivate( const bot_state_t *bs ) {
    static int      s_cachedStartTime[MAX_CLIENTS];
    static qboolean s_cachedValue[MAX_CLIENTS];
    static qboolean s_haveCached[MAX_CLIENTS];
    int cl;

    if ( !bs || !bs->wiredIntelActive ) {
        return qfalse;
    }
    cl = bs->client;
    if ( cl < 0 || cl >= MAX_CLIENTS ) {
        return qfalse;
    }

    /* Re-resolve once per map (cache keyed on level.startTime). */
    if ( !s_haveCached[cl] || s_cachedStartTime[cl] != level.startTime ) {
        char cs[MAX_INFO_STRING];
        const char *charName;
        char value[16];
        qboolean can = qtrue;   /* default: a bot with no declaration can activate */

        trap_GetConfigstring( CS_PLAYERS + cl, cs, sizeof( cs ) );
        charName = Info_ValueForKey( cs, "char" );
        if ( charName && charName[0] ) {
            char key[MAX_QPATH];
            Com_sprintf( key, sizeof( key ), "char:%s:can_activate", charName );
            /* trap returns qfalse when the manifest omits the key → keep the
               default; only an explicit "0" denies. */
            if ( trap_GetValue( value, sizeof( value ), key ) && value[0] ) {
                can = ( atoi( value ) != 0 ) ? qtrue : qfalse;
            }
        }
        s_cachedValue[cl]     = can;
        s_cachedStartTime[cl] = level.startTime;
        s_haveCached[cl]      = qtrue;
    }
    return s_cachedValue[cl];
}

/*
==================
WiredIntel_DoorIsClosedGate
==================
True when 'ent' is a target-opened door that is currently a nav-blocking gate:
a func_door that opens only via a target chain (it has a targetname, so it got no
auto-spawned proximity trigger — see SP_q1_func_door) and is at or moving toward
its closed position. Such a door marks its polys NAVPOLY_BLOCKED while closed, so
the bot cannot route past it until the chain fires it open. An auto-door (no
targetname) is never a gate here — the bot opens it just by walking up. Matches the
door in either map format (Q3 "func_door" or Q1-prefixed "q1_func_door").
*/
static qboolean WiredIntel_DoorIsClosedGate( const gentity_t *ent ) {
    if ( !ent->inuse || !ent->classname ) return qfalse;
    if ( !G_ClassnameIs( ent, "func_door" ) ) return qfalse;
    if ( !ent->targetname || !ent->targetname[0] ) return qfalse;   /* auto-door, not a gate */
    /* Closed (POS1) or closing (2TO1) — treat both as blocking; an opening/open
       door clears its polys and needs no activation. */
    return ( ent->moverState == MOVER_POS1 || ent->moverState == MOVER_2TO1 ) ? qtrue : qfalse;
}

/* ── GATE-SATISFIED MEMORY ────────────────────────────────────────────────────
 *
 * CLASS RULE: a gate the agent has already caused to open must not re-arm as a
 * blocking gate while it remains SATISFIED. Detection must be coupled to the gate's
 * ACHIEVED state, not only to its instantaneous closed-state.
 *
 * Why the instantaneous state is not enough. A target-opened door with a finite
 * `wait` is closed for most of its duty cycle: it opens, holds, and closes again.
 * WiredIntel_DoorIsClosedGate reads only the mover's CURRENT state, so the moment the
 * leaf begins closing the door is a "closed gate" again — indistinguishable from one
 * that was never opened. The bot, which is by then walking toward the opening it just
 * bought, re-detects the same gate, re-queues the same button, and walks back to
 * press it. It spends its own open-window re-earning a gate it already earned. The
 * closed-state is a fact about the leaf; "do I still need to act on this gate" is a
 * fact about the agent's PROGRESS against it, and only the second should drive
 * detection.
 *
 * The memory is one bit per gate, keyed on the door entity, set when that door is
 * observed to leave its closed position (the chain fired — the gate is achieved).
 * While the bit is set the door is not offered as a detection candidate, so the bot
 * keeps the goal it has instead of re-committing to the actuation.
 *
 * 🔴 RETIREMENT — why this bit cannot latch forever.
 *
 * A satisfied bit that never clears is strictly worse than no bit: a bot that fails
 * to cross before the leaf shuts would be permanently blind to a gate that really is
 * blocking it, and would never press the button again. So the bit is retired by the
 * one condition that means "this gate is cutting me off again": the door has returned
 * to a fully closed rest state (MOVER_POS1) AND the agent is still on the near side —
 * i.e. it did not get through, and the closed leaf is once more between it and where
 * it is going.
 *
 * That second clause is what makes the retirement CORRECT rather than merely
 * eventual. The door returning to POS1 alone is not evidence the gate matters again:
 * a bot that crossed successfully also sees the door shut behind it, and re-arming
 * there would send it back through the gate it just cleared. The route test is what
 * separates the two: after a genuine crossing the door no longer cuts the corridor to
 * the goal, so it is not re-armed; after a failed crossing it does, so it is. Both
 * are read from the same predicate the detector already uses to decide relevance
 * (WiredIntel_RouteCrossesDoor), so the retirement asks exactly the question the
 * detection asks, and cannot disagree with it.
 *
 * The bit therefore survives exactly one open-cycle's worth of committed progress,
 * and no longer: it is set by the gate opening, and cleared the moment the gate is
 * both shut and once again on the route. A bot that never crosses gets its gate back
 * on the first close; a bot that crosses keeps its forward goal. Neither can latch,
 * because neither branch is reachable without a live re-read of the mover state and
 * the current route.
 *
 * Per-gate (a world fact) rather than per-bot: the door's achieved state is the same
 * for every agent. Sized to the entity array's gate range and reset per map with the
 * level's own generation, so a map change cannot carry a stale bit across.
 */
static byte s_gateSatisfied[MAX_GENTITIES];

/* Instrument-only counters for the exit/door/button progression; defined with
 * WiredIntel_TrySetExitGoal below.  Declared here because the gate-memory frame and
 * the press-confirm loop both feed them.  No behavioural effect. */
void WiredIntel_ExitChainCount( int which );
void WiredIntel_ExitChainReport( void );
static int  s_gateSatisfiedStartTime = -1;

#if FEAT_RECAST_NAVMESH
/* Defined below with the route-relevance tests; forward-declared because the
   retirement condition asks the same crossing question the detector asks. */
static qboolean WiredIntel_RouteCrossesDoor( const gentity_t *door, const vec3_t from,
                                             const vec3_t goalOrigin, qboolean *reachesOut );
#endif

/*
==================
WiredIntel_GateMemoryFrame

Per-frame maintenance of the gate-satisfied memory: SET on a gate that has left its
closed position, RETIRE on one that is shut again AND still cutting the route. Called
once per detection tick with the agent's current goal so the route test is asked
against the goal the agent is actually pursuing.
==================
*/
static void WiredIntel_GateMemoryFrame( bot_state_t *bs, const vec3_t goalOrigin ) {
    int i;

    /* Map-change reset: the bits describe THIS level's movers. level.startTime is
       the same generation key the reachability cache above uses. */
    if ( s_gateSatisfiedStartTime != level.startTime ) {
        memset( s_gateSatisfied, 0, sizeof( s_gateSatisfied ) );
        s_gateSatisfiedStartTime = level.startTime;
    }

    for ( i = MAX_CLIENTS; i < level.num_entities; i++ ) {
        gentity_t *ent = &g_entities[i];

        if ( !ent->inuse || !ent->classname ) continue;
        if ( !G_ClassnameIs( ent, "func_door" ) ) continue;
        if ( !ent->targetname || !ent->targetname[0] ) continue;   /* auto-door, never a gate */

        /* SET: the leaf has left its closed rest position — the chain fired and the
           gate is achieved. Covers both the opening transit and the held-open dwell. */
        if ( ent->moverState == MOVER_1TO2 || ent->moverState == MOVER_POS2 ) {
            /* Instrument only: count the 0->1 EDGE, so a held-open door is counted
             * once rather than once per think.  Reads state already computed here. */
            if ( !s_gateSatisfied[i] ) WiredIntel_ExitChainCount( 2 );
            s_gateSatisfied[i] = 1;
            continue;
        }

        /* RETIRE: fully shut again (POS1) AND still cutting the route to the goal.
           A door mid-close (2TO1) is not yet retired — the opening it bought may
           still be passable, and retiring there would re-arm the gate while the bot
           is in the act of using it. */
        if ( s_gateSatisfied[i] && ent->moverState == MOVER_POS1 ) {
#if FEAT_RECAST_NAVMESH
            qboolean reaches = qfalse;
            if ( WiredIntel_RouteCrossesDoor( ent, bs->origin, goalOrigin, &reaches ) ) {
                /* Shut, and the corridor to the goal runs through it again: the bot
                   did NOT get through. The gate is blocking once more — give it back
                   to detection. This is the branch that makes the bit un-latchable. */
                s_gateSatisfied[i] = 0;
                if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
                    Com_Log( SEV_INFO, LOG_CH(ch_botai),
                        "cl=%d gate door=%d re-cuts route while shut — satisfied bit retired\n",
                        bs->client, i );
                }
            }
            /* Shut but NOT on the route: the bot is through (or routed elsewhere).
               Keep the bit so the cleared gate is not re-detected behind it. */
#else
            /* No navmesh: no route test is available, so the bit cannot be justified
               past the close. Retire on the close alone — the conservative side, and
               identical to the pre-memory behaviour. */
            (void)goalOrigin;
            s_gateSatisfied[i] = 0;
#endif
        }
    }
}

/* Q1 door/button key spawnflags (mirrors g_mover_q1.c's file-static defines; kept
   here as named constants for the button-reachability filter). A keyed button
   rejects a client that lacks the key, so the bot would overlap it forever — such
   a button is only queued when the bot already holds the matching key. */
#define WI_DOOR_GOLD_KEY    8
#define WI_DOOR_SILVER_KEY  16

/*
==================
WiredIntel_ButtonIsPressable
==================
True when a bot can actually press 'button' by walking onto it. A Q1 func_button
is touch-activated only when it has no health (SP_q1_func_button installs the touch
handler only for health==0; a health>0 button is shoot-to-activate and has NO touch
handler, so walking onto it does nothing). A keyed button (gold/silver spawnflag)
rejects a client without the key, so it is pressable only when the bot holds it.
Filtering here keeps the queue to buttons the bot can complete, so it never strands
itself on a button it can't fire.
*/
static qboolean WiredIntel_ButtonIsPressable( const bot_state_t *bs, const gentity_t *button ) {
    if ( button->health > 0 ) {
        return qfalse;   /* shoot-to-activate — no touch handler, walking on it is inert */
    }
    if ( button->spawnflags & WI_DOOR_GOLD_KEY ) {
        if ( !( bs->cur_ps.stats[STAT_HOLDABLE_BITS] & BG_HOLDABLE_BIT( HI_KEY_GOLD ) ) ) {
            return qfalse;
        }
    }
    if ( button->spawnflags & WI_DOOR_SILVER_KEY ) {
        if ( !( bs->cur_ps.stats[STAT_HOLDABLE_BITS] & BG_HOLDABLE_BIT( HI_KEY_SILVER ) ) ) {
            return qfalse;
        }
    }
    return qtrue;
}

/*
==================
WiredIntel_QueueButtonsForTarget
==================
Append every reachable, pressable func_button whose target is 'targetname' to the
bot's activation queue. This is one hop of the door-opening chain: a button may
target the door directly, or target an intermediate (a q1_trigger_counter / relay)
that targets the door. Non-button entities matching 'targetname' are followed one
hop deeper (recursively) so door<-counter<-buttons and door<-button both resolve.
A button that the bot can't actually press (shoot-to-activate, or keyed without the
key) is skipped so the bot never queues an unfirable button. Depth is bounded to
guard against a cyclic target graph. Records each button's entitynum so the press
is later confirmed from the button's own mover state. Returns the number appended.
*/
static int WiredIntel_QueueButtonsForTarget( bot_state_t *bs, const char *targetname, int depth ) {
    gentity_t *t = NULL;
    int added = 0;

    if ( !targetname || !targetname[0] || depth <= 0 ) {
        return 0;
    }

    while ( ( t = G_Find( t, FOFS( target ), targetname ) ) != NULL ) {
        /* An ACTUATOR: something the bot fires by ARRIVING at it. Two kinds, one
           queue. G_ClassnameIs compares the BARE classname (it strips the "q1_"
           format prefix), so each test covers both map formats at once.
             PRESS — func_button: walk onto/at it; its touch handler fires.
             TOUCH — a brush trigger volume targeting the gate: walking the player
                     bbox into it IS the actuation, so arrival is the whole
                     interaction and nothing is pressed.
           Matched by class family, not by one map's entity: trigger_once fires its
           target once and trigger_multiple re-fires, but from the bot's side both
           open the gate identically. Without the TOUCH kind a trigger matched
           neither arm here (it carries no targetname either), so a gate opened by
           a trigger produced an EMPTY queue and was skipped entirely. */
        const qboolean isButton  = G_ClassnameIs( t, "func_button" );
        const qboolean isTouched = ( G_ClassnameIs( t, "trigger_once" ) ||
                                     G_ClassnameIs( t, "trigger_multiple" ) );
        if ( isButton || isTouched ) {
            vec3_t origin;
            if ( Belief_InteractableCount( bs ) >= BELIEF_MAX_INTERACTABLES ) {
                break;   /* queue full — take what we have */
            }
            /* Pressability is a BUTTON predicate (shoot-to-activate, key-locked
               spawnflags). A trigger volume has none of those gates — it fires on
               touch for anyone whose bbox enters it — so it is queueable as-is. */
            if ( isButton && !WiredIntel_ButtonIsPressable( bs, t ) ) {
                continue;   /* can't fire this button by touch — don't queue it */
            }
            /* EntityNavOrigin already handles a brush entity: it takes the
               r.absmin/r.absmax midpoint and snaps it to the nearest walkable
               poly, and its wall-button touch-spot projection is gated on
               func_button, so a trigger takes the plain center-snap path. */
            if ( WiredIntel_EntityNavOrigin( t, origin ) ) {
                /* Record the discovered actuator in the belief store — this is the
                   interactables producer; the door association is stamped once the
                   gate is committed (Belief_InteractablesSetDoor below). */
                if ( Belief_InteractableAdd( bs, origin, (int)( t - g_entities ), -1,
                                             isTouched ? BELIEF_ACT_TOUCH
                                                       : BELIEF_ACT_PRESS ) >= 0 ) {
                    added++;
                }
            }
        } else if ( t->targetname && t->targetname[0] ) {
            /* An intermediate link (counter/relay): follow the chain one hop
               deeper toward the buttons that feed it. */
            added += WiredIntel_QueueButtonsForTarget( bs, t->targetname, depth - 1 );
        }
    }
    return added;
}

/* A findPath result "reaches" the goal when its last waypoint lands within this of
   the goal HORIZONTALLY. Detour returns a PARTIAL corridor (count>0) to the nearest
   reachable poly when the goal is disconnected by the excluded NAVPOLY_BLOCKED door
   polys, so a positive count alone does not mean reachable. ~64u is the agent/goal-
   poly radius. Only the horizontal component is checked against this: Nav_FindPath
   snaps the goal DOWN onto the walkable floor before pathing, so the corridor end
   lands at floor height while the raw goal 'to' can float far above it (a brush
   entity's bbox center). A raw 3D compare therefore wrongly rejects a reachable
   off-floor goal as blocked. */
#define WI_NAV_REACH_EPS 64.0f
/* Vertical slack allowed between the corridor end (on the floor) and the raw goal
   center (possibly well above the floor). Matches the follower's goal snap extents
   (WiredIntel_EntityNavOrigin uses a 480u Z search), so a goal up to a tall room's
   height above its floor still reads as reached. The horizontal check above is what
   actually distinguishes reached-vs-blocked; this only keeps the vertical snap from
   producing a false negative. */
#define WI_NAV_REACH_Z_SLACK 512.0f

/*
==================
WiredIntel_PathReaches
==================
True when a nav path from 'from' actually reaches 'to' (not merely a partial
corridor toward it). Uses a file-static scratch path (navPath_t is large) — not
reentrant, which is fine on the single-threaded game think. Under Recast the
excluded-flag filter drops NAVPOLY_BLOCKED door polys, so a closed gate between
the two points makes this return qfalse — the signal the gate detector keys on.

Two "did not reach" signals are combined so a false positive from either alone
cannot mislead the detector:
  1. The engine's authoritative blocked status: Nav_FindPath encodes
     NAV_FINDPATH_PARTIAL when Detour's corridor did not reach the goal poly because
     the goal is DISCONNECTED (a closed NAVPOLY_BLOCKED gate cuts the route). A
     reachable path merely truncated by the path-point cap on a huge map is NOT
     flagged partial (the engine separates the two DT_PARTIAL_RESULT causes), so a
     truncated-but-reachable path does not read as blocked.
  2. Endpoint proximity, compared HORIZONTALLY (+ a generous Z slack): the goal is
     snapped to the floor before pathing, so a raw 3D compare against the unsnapped
     goal wrongly rejects a reachable off-floor goal (fix for the unsnapped-goal
     vertical offset). XY within the reach epsilon and Z within the snap slack means
     the corridor stands under/at the goal.
Reachable requires BOTH: the engine did not flag the goal disconnected, AND the
corridor end sits under/at the goal. A closed gate fails both (partial flagged, and
the corridor dead-ends at the door, far from the goal).
*/
/*
==================
WiredIntel_OriginIsDegenerate
==================
Is the query ORIGIN itself unable to produce evidence about any target?

WiredIntel_PathReaches answers "can the agent route from THIS EXACT POSE to the
target". That is the right question only while the pose is one the mesh can route
FROM. An origin that snaps onto an isolated poly — a mesh fragment the collision
world connects to its surroundings but the navmesh does not — yields a corridor of
a single poly toward EVERY target in the world, so "unreachable" from such a pose
is a fact about the pose, not about the target. Measured on the e1m1 specimen: the
agent walks across such a fragment repeatedly (3 exits, 2 returns) while 9 of 10
sampled poses on the same trajectory route normally; a gate evaluated during one of
those crossings was rejected as unactuatable even though it is reachable from
almost everywhere else the agent stands.

🔴 THE TEST MUST BE TARGET-INDEPENDENT, and that is the whole difficulty.  The
question is "can this pose route ANYWHERE", not "can it route to THAT target" — a
pose-only property, whose verdict must not change when the caller's target does.
Two earlier forms both failed that requirement in opposite directions:

  - `count <= 1` ("did the route leave the origin's own poly") had the right SHAPE
    — a fragment returns a stub toward any goal — but was too narrow.  An isolated
    fragment is not always a SINGLE poly: measured on the e1m1 specimen, the pose
    the agent occupies most produced a SEVEN-waypoint corridor while being exactly
    as unroutable as its one-waypoint neighbours (all three comp=-1).  The search
    wandered inside the cluster and the test passed it — 6 catches out of 32.

  - Reading NAV_FINDPATH_PARTIAL had the wrong shape: it INHERITS the target.  On
    the VM side that bit already encodes "the corridor did not terminate at the
    goal poly", which is precisely WiredIntel_PathReaches's first rejection arm, so
    the guard became a copy of the test it was meant to qualify — every unreachable
    target was simultaneously reported as a degenerate pose.  Vacuous: ordinary
    floor flagged 44 times in one run.

The form below asks the question directly.  Probe from the pose toward points the
mesh chooses ITSELF (trap_Nav_GetRandomPoint), not toward anything the caller
supplied: a pose on connected floor reaches a random mesh point readily, while a
pose on a severed fragment reaches none of them however many are drawn.  The
verdict therefore depends on the pose alone and is identical for every caller and
every goal.  No threshold: it is not "how many" or "how far" but "did ANY probe
arrive", and a single arrival is proof the pose can route.

Callers must treat a degenerate origin as NO VERDICT — never as "reachable", which
would resurrect the false-positive class the route-relevance pass exists to kill.
==================
*/
static qboolean WiredIntel_OriginIsDegenerate( const vec3_t from, const vec3_t to ) {
    static navPath_t s_degen;   /* ~7 KB — file-static, single-threaded think */
    int  i;

    (void)to;   /* deliberately unused: the verdict is a property of 'from' alone */

    /* Draw a handful of mesh-chosen probe targets.  Several rather than one because
       a single draw could land on another severed fragment and slander a healthy
       pose; the loop exits on the FIRST arrival, so the common (healthy) case pays
       one query.  The bound is a loop shape, not a tuning value — it does not
       encode a distance, an area, or a map dimension, and changing it cannot change
       the verdict for a pose that can route at all. */
    for ( i = 0; i < 4; i++ ) {
        vec3_t probe;
        int    n;
        if ( !trap_Nav_GetRandomPoint( NAVPOLY_WALKABLE, probe ) ) {
            continue;
        }
        n = trap_Nav_FindPath( (vec_t *)from, probe, WI_NAV_AGENT_PLAYER, &s_degen );
        if ( n <= 0 || NAV_FINDPATH_COUNT( n ) <= 0 ) {
            continue;
        }
        if ( !( n & NAV_FINDPATH_PARTIAL ) ) {
            return qfalse;   /* arrived somewhere: this pose can route */
        }
    }
    return qtrue;   /* nothing on the mesh was reachable from here */
}

static qboolean WiredIntel_PathReaches( const vec3_t from, const vec3_t to ) {
    static navPath_t s_probe;   /* ~7 KB — file-static, single-threaded think */
    int n, count;
    float dx, dy, dz, horizSq;

    n = trap_Nav_FindPath( (vec_t *)from, (vec_t *)to, WI_NAV_AGENT_PLAYER, &s_probe );
    if ( n <= 0 ) {
        return qfalse;   /* no corridor at all */
    }
    if ( n & NAV_FINDPATH_PARTIAL ) {
        return qfalse;   /* engine: goal poly disconnected → genuinely blocked */
    }
    count = NAV_FINDPATH_COUNT( n );
    if ( count <= 0 ) {
        return qfalse;
    }

    /* Horizontal endpoint proximity to the (unsnapped) goal, with vertical slack for
       the floor snap: the corridor end lands on the floor, the raw goal can float
       above it, so only the XY distance is checked against the reach epsilon. */
    dx = s_probe.positions[count - 1][0] - to[0];
    dy = s_probe.positions[count - 1][1] - to[1];
    dz = s_probe.positions[count - 1][2] - to[2];
    horizSq = dx * dx + dy * dy;

    if ( horizSq > WI_NAV_REACH_EPS * WI_NAV_REACH_EPS ) {
        return qfalse;   /* corridor ends away from the goal horizontally — blocked */
    }
    if ( dz < -WI_NAV_REACH_Z_SLACK || dz > WI_NAV_REACH_Z_SLACK ) {
        return qfalse;   /* goal is beyond the floor-snap slack — not the same place */
    }
    return qtrue;
}

/*
==================
WiredIntel_StandPointTouchesButton
==================
Touch-envelope predicate: standing at 'standOrigin' (a resolved approach/touch-spot),
does the player's standing box overlap the button's brush AABB — i.e. would walking
onto that spot fire Q1_Button_Touch?  This is the SAME box-vs-AABB overlap the engine's
touch dispatch uses (Q1_Button_Touch has no grounded requirement — any client bbox that
contacts the brush presses it), so it is the correct test for "walk-pressable from here".

A recessed WALL button (t1a's case) fails this from every navmesh-reachable stand point:
the nearest standable+covered floor is ~85u out from the thin plate, so the standing box
never reaches the brush — walk-pressable is FALSE, and the caller must arm the jump-touch
instead.  A FLOOR button passes (the stand point sits on the button footprint), so it
keeps the walk goal.  No map/button constant — purely the runtime brush AABB + player box.
*/
static qboolean WiredIntel_StandPointTouchesButton( const vec3_t standOrigin, const gentity_t *button ) {
    vec3_t pmin, pmax;
    if ( !button ) {
        return qfalse;
    }
    /* Player standing box at the stand origin (bg_public.h: mins/maxs). */
    pmin[0] = standOrigin[0] - WI_PLAYER_HALF_W; pmax[0] = standOrigin[0] + WI_PLAYER_HALF_W;
    pmin[1] = standOrigin[1] - WI_PLAYER_HALF_W; pmax[1] = standOrigin[1] + WI_PLAYER_HALF_W;
    pmin[2] = standOrigin[2] + WI_PLAYER_MINS_Z; pmax[2] = standOrigin[2] + WI_PLAYER_MAXS_Z;
    /* AABB overlap on all three axes = the bboxes touch = Q1_Button_Touch would fire. */
    if ( pmax[0] < button->r.absmin[0] || pmin[0] > button->r.absmax[0] ) return qfalse;
    if ( pmax[1] < button->r.absmin[1] || pmin[1] > button->r.absmax[1] ) return qfalse;
    if ( pmax[2] < button->r.absmin[2] || pmin[2] > button->r.absmax[2] ) return qfalse;
    return qtrue;
}

/*
==================
WiredIntel_ButtonBelowClosedLeaf
==================
Collision veto for the walk-preferrer's nav reachability: is this opener button
UNDER the closed gate's own solid leaf, so that reaching it means descending
through the leaf the button is meant to open?  Such a button (e.g. e1m1's *19 at
the shaft bottom, below hatch t1's 238u closed leaf) reads as nav-reachable only
because the closed door's OPENABLE_CLOSED off-mesh connection is a traversable link
in the path filter (the navmesh-vs-collision grounding-lie): the follower "plans"
the drop through the shut hatch, but the CONTENTS_SOLID leaf physically caps it.
Walking to that button is impossible until the door opens — so the walk-preferrer
must NOT pick it; rejecting it lets the OR-gate fall to a reachable opener (a
top-side plate pressed by walk or the jump-touch fallback) that opens the door
FIRST, after which the drop becomes real.

Two conditions, both from the door's OWN closed brush (no coordinate, no map
constant — generalizes to any below-a-closed-leaf opener):
  1. The button's approach z sits below the closed leaf top (door->r.absmax[2]) —
     reaching it requires getting past the leaf's height.
  2. A downward collision trace CONFINED to the leaf's own z-band at the leaf
     footprint centre is solid AND that solid is the DOOR itself (hit-attribution
     trace.entityNum == door slot).  The trace starts at the leaf top MINUS a small
     inset (inside the leaf's own top surface, BELOW where any entity could stand on
     the lid) so a monster resting on the closed hatch cannot shadow the leaf and
     false-clear the veto — the segment lies entirely within the leaf's solid, and
     MASK_PLAYERSOLID attributes the contact to the door brush.

Only fires for a CLOSED gate leaf under a below-leaf button; a normal floor button
(z at/above the leaf top → condition 1 false) is untouched, and a button whose
column the door does not cap (condition 2's attribution fails) is untouched — so
arena/test floor-button gates stay byte-identical.
*/
static qboolean WiredIntel_ButtonBelowClosedLeaf( const gentity_t *door, const vec3_t buttonApproach ) {
    trace_t tr;
    vec3_t  zero = { 0, 0, 0 };
    vec3_t  ls, le;
    float   cx, cy;
    int     doorSlot;

    if ( !door ) {
        return qfalse;
    }
    /* Condition 1: the button is below the closed leaf's solid top. */
    if ( buttonApproach[2] >= door->r.absmax[2] ) {
        return qfalse;
    }
    /* Condition 2: a segment CONFINED to the leaf's own z-band at its footprint
       centre is solid and attributed to this door.  Start just under the leaf top
       (inside its top surface, below any entity standing on the lid) and end just
       above the leaf bottom, so the whole segment lies within the leaf's solid. */
    cx = 0.5f * ( door->r.absmin[0] + door->r.absmax[0] );
    cy = 0.5f * ( door->r.absmin[1] + door->r.absmax[1] );
    VectorSet( ls, cx, cy, door->r.absmax[2] - 4.0f );
    VectorSet( le, cx, cy, door->r.absmin[2] + 4.0f );
    trap_Trace( &tr, ls, zero, zero, le, ENTITYNUM_NONE, MASK_PLAYERSOLID );
    doorSlot = (int)( door - g_entities );
    /* Solid within the leaf band (startsolid or a hit) attributed to the door brush
       → the closed leaf caps this column: the below-leaf button is unreachable while
       the door is shut. */
    if ( ( tr.startsolid || tr.fraction < 1.0f ) && tr.entityNum == doorSlot ) {
        return qtrue;
    }
    return qfalse;
}

/*
==================
WiredIntel_PathFrontier
==================
Run a path probe and report the reachable frontier toward the goal: the squared
horizontal gap from the corridor's END waypoint to the goal (smaller = the corridor
reaches nearer), and, in 'endOut', the world position of that END waypoint (where the
route currently dead-ends). Returns WI_NAV_PROGRESS_NONE for the gap when there is no
corridor at all. Only the horizontal component of the gap is used, for the same floor-
snap reason as WiredIntel_PathReaches (the corridor end lands on the mesh while the goal
can float above it). The end position is the point where the bot's route stops — the
location the blocking obstacle sits at.
*/
#define WI_NAV_PROGRESS_NONE 1.0e9f
static float WiredIntel_PathFrontier( const vec3_t from, const vec3_t to, vec3_t endOut ) {
    static navPath_t s_prog;   /* ~7 KB — file-static, single-threaded think */
    int n, count;
    float dx, dy;

    if ( endOut ) { VectorClear( endOut ); }
    n = trap_Nav_FindPath( (vec_t *)from, (vec_t *)to, WI_NAV_AGENT_PLAYER, &s_prog );
    if ( n <= 0 ) {
        return WI_NAV_PROGRESS_NONE;   /* no corridor — no frontier toward the goal */
    }
    count = NAV_FINDPATH_COUNT( n );
    if ( count <= 0 ) {
        return WI_NAV_PROGRESS_NONE;
    }
    if ( endOut ) { VectorCopy( s_prog.positions[count - 1], endOut ); }
    dx = s_prog.positions[count - 1][0] - to[0];
    dy = s_prog.positions[count - 1][1] - to[1];
    return dx * dx + dy * dy;   /* squared horizontal gap to the goal */
}

/*
==================
WiredIntel_SweptSegmentHitsBox
==================
Does the AGENT, walking the corridor leg p0->p1, overlap the world AABB
[bmin,bmax]?  The agent is a box, so this is a swept-box-vs-box test, done as the
standard Minkowski reduction: expand the target AABB by the agent's own half
extents and ask whether the segment of the agent's ORIGIN enters the expanded box
(slab clip, Liang-Barsky form).  The expansion comes entirely from the shipped
player box (WI_PLAYER_HALF_W / WI_PLAYER_MINS_Z / WI_PLAYER_MAXS_Z, the same
bg_public.h numbers WiredIntel_StandPointTouchesButton uses) plus the target's own
extents — no route/gate/map tuning value enters here.

The corridor waypoints are the agent's ORIGIN path (Nav_FindPath snaps to the
standing-origin mesh), which is why expanding by the origin-relative box is the
correct reduction: mins_z is negative, maxs_z positive, so the z band is
[bmin.z - MAXS_Z, bmax.z - MINS_Z] in origin space.
==================
*/
static qboolean WiredIntel_SweptSegmentHitsBox( const vec3_t p0, const vec3_t p1,
                                                const vec3_t bmin, const vec3_t bmax ) {
    vec3_t emin, emax, d;
    float  t0 = 0.0f, t1 = 1.0f;
    int    a;

    /* Minkowski expansion of the target box by the agent's origin-relative box. */
    emin[0] = bmin[0] - WI_PLAYER_HALF_W;  emax[0] = bmax[0] + WI_PLAYER_HALF_W;
    emin[1] = bmin[1] - WI_PLAYER_HALF_W;  emax[1] = bmax[1] + WI_PLAYER_HALF_W;
    emin[2] = bmin[2] - WI_PLAYER_MAXS_Z;  emax[2] = bmax[2] - WI_PLAYER_MINS_Z;

    VectorSubtract( p1, p0, d );

    for ( a = 0; a < 3; a++ ) {
        if ( d[a] > -1.0e-6f && d[a] < 1.0e-6f ) {
            /* Parallel to this slab: the whole segment must already lie inside it. */
            if ( p0[a] < emin[a] || p0[a] > emax[a] ) {
                return qfalse;
            }
            continue;
        }
        {
            float inv = 1.0f / d[a];
            float ta  = ( emin[a] - p0[a] ) * inv;
            float tb  = ( emax[a] - p0[a] ) * inv;
            if ( ta > tb ) { float s = ta; ta = tb; tb = s; }
            if ( ta > t0 ) t0 = ta;
            if ( tb < t1 ) t1 = tb;
            if ( t0 > t1 ) {
                return qfalse;   /* slabs disjoint along the segment */
            }
        }
    }
    return qtrue;
}

/*
==================
WiredIntel_RouteCrossesDoor
==================
🔴 THE ROUTE-CONTENTS PREDICATE.  Does the bot's corridor toward 'goalOrigin' PASS
THROUGH 'door'?  This asks what the route CONTAINS, never whether the route exists.

Why this predicate and not reachability: reachability and passability are different
questions.  The Detour query filter excludes only NAVPOLY_BLOCKED, and a closed door
that the mesh never marked blocked — or whose threshold poly survived the bake —
yields a COMPLETE corridor straight through its solid leaf.  A detector predicated on
route FAILURE is therefore blind to exactly the gate class that matters, and gets
BLINDER as the mesh improves: a better mesh means fewer broken routes, hence fewer
detections.  Reading the route's CONTENTS inverts that — mesh quality now helps,
because a crossing is only visible on a corridor that exists.

Mechanism, entirely from data already in hand:
  1. Run the path query the detector already runs (same trap, same agent type) and
     keep the whole waypoint list, not just its end.
  2. Walk the corridor leg by leg and sweep the AGENT BOX along each leg against the
     door's own runtime AABB (r.absmin/r.absmax).  A leg whose swept agent overlaps
     the leaf is a crossing: either the corridor threads the doorway (a door-gap OMC
     or a surviving threshold poly) or it drives into the closed leaf.
  3. Corroborate with the mesh's own permanent NAVPOLY_DOOR tag: a waypoint tagged
     NAVPOLY_DOOR whose position falls inside the expanded leaf is the same crossing
     seen from the mesh side.  Either signal alone is sufficient — the geometric sweep
     covers doors the bake never tagged, the tag covers a leaf whose polys sit at the
     threshold rather than under the brush.

Tolerance provenance: the door's OWN extents plus the shipped player box
(WI_PLAYER_HALF_W / WI_PLAYER_MINS_Z / WI_PLAYER_MAXS_Z).  No new constant, no
coordinate, no per-map value — the predicate is scale-free and holds for any door
entity on any map.

Returns qtrue when the corridor crosses the door.  'reachesOut' (optional) reports
whether that same corridor actually REACHED the goal, so a caller needing both facts
pays for one query.
==================
*/
static qboolean WiredIntel_RouteCrossesDoor( const gentity_t *door, const vec3_t from,
                                             const vec3_t goalOrigin, qboolean *reachesOut ) {
    static navPath_t s_route;   /* ~7 KB — file-static, single-threaded think */
    int      n, count, i;
    qboolean crosses = qfalse;

    if ( reachesOut ) { *reachesOut = qfalse; }
    if ( !door ) {
        return qfalse;
    }

    n = trap_Nav_FindPath( (vec_t *)from, (vec_t *)goalOrigin, WI_NAV_AGENT_PLAYER, &s_route );
    if ( n <= 0 ) {
        return qfalse;   /* no corridor at all — it has no contents to read */
    }
    count = NAV_FINDPATH_COUNT( n );
    if ( count <= 0 ) {
        return qfalse;
    }
    if ( reachesOut && !( n & NAV_FINDPATH_PARTIAL ) ) {
        /* Same endpoint proximity WiredIntel_PathReaches applies (horizontal + floor-
           snap z slack), so the two agree on "reached" without a second query. */
        float dx = s_route.positions[count - 1][0] - goalOrigin[0];
        float dy = s_route.positions[count - 1][1] - goalOrigin[1];
        float dz = s_route.positions[count - 1][2] - goalOrigin[2];
        if ( dx * dx + dy * dy <= WI_NAV_REACH_EPS * WI_NAV_REACH_EPS &&
             dz >= -WI_NAV_REACH_Z_SLACK && dz <= WI_NAV_REACH_Z_SLACK ) {
            *reachesOut = qtrue;
        }
    }

    /* Signal A — swept-agent crossing on any corridor leg. */
    for ( i = 0; i + 1 < count && !crosses; i++ ) {
        if ( WiredIntel_SweptSegmentHitsBox( s_route.positions[i], s_route.positions[i + 1],
                                             door->r.absmin, door->r.absmax ) ) {
            crosses = qtrue;
        }
    }
    /* A single-waypoint corridor has no leg; test the point itself. */
    if ( !crosses && count == 1 &&
         WiredIntel_SweptSegmentHitsBox( s_route.positions[0], s_route.positions[0],
                                         door->r.absmin, door->r.absmax ) ) {
        crosses = qtrue;
    }

#if FEAT_RECAST_NAVMESH
    /* Signal B — the mesh's own door tag, for the leaf whose polys sit AT the
       threshold rather than under the brush.  A waypoint carrying the permanent
       NAVPOLY_DOOR tag is a door crossing by the bake's own attribution; the only
       open question is WHICH door, and that is answered by attributing it to the
       nearest leaf.  The attribution envelope is the door's OWN half-span (the same
       self-scaled measure the old traversable test used), so a wide leaf accepts a
       threshold waypoint at its own scale and a narrow one does not — no absolute
       distance and no map value.  Signal A cannot see this case: the threshold poly
       lies just outside the leaf, so no swept leg overlaps the brush. */
    for ( i = 0; i < count && !crosses; i++ ) {
        vec3_t span, emin, emax;
        float  half;
        if ( !( trap_Nav_GetPolyAreaFlags( s_route.polyrefs[i] ) & NAVPOLY_DOOR ) ) {
            continue;
        }
        VectorSubtract( door->r.absmax, door->r.absmin, span );
        half = 0.5f * VectorLength( span );
        VectorSet( emin, door->r.absmin[0] - half, door->r.absmin[1] - half,
                         door->r.absmin[2] - half );
        VectorSet( emax, door->r.absmax[0] + half, door->r.absmax[1] + half,
                         door->r.absmax[2] + half );
        if ( WiredIntel_SweptSegmentHitsBox( s_route.positions[i], s_route.positions[i],
                                             emin, emax ) ) {
            crosses = qtrue;
        }
    }
#endif

    return crosses;
}

/*
==================
WiredIntel_DoorCutsRoute
==================
Route-relevance test for a candidate gate: is THIS specific closed door on the bot's
route to its goal, or is it an unrelated closed door elsewhere on the map? The route-
cut probe (WiredIntel_PathReaches) already proved the bot is blocked, but not WHICH door
blocks it — ranking purely by distance-to-bot sends the bot to the nearest closed door
even when a different door is the one on its route (the compositional false-positive).

Two signals mark a door as on the route; a door qualifies if EITHER fires:

  1. Poly-toggle advance (definitive when the door's nav polys carry NAVPOLY_BLOCKED):
     open the door in the nav mesh only (clear its blocked polys), re-measure the
     reachable frontier, restore the flag. If opening it pulls the frontier meaningfully
     closer to the goal, the door was cutting the route. This is NOT "opening it fully
     reaches the goal" — on a multi-gate map, opening the first gate only extends the
     route to the NEXT obstacle, so a full-reach test would reject every gate. An
     unrelated door changes the frontier by nothing, so it is skipped.

  2. Frontier proximity (works when the door's polys are NOT nav-blocked — e.g. a Q1
     door the navmesh tags by a different key, so the toggle is a no-op, yet the door
     still physically blocks the bot): the partial corridor toward the goal dead-ends
     at the obstacle. A door whose bbox is near that dead-end is the one the bot is
     stuck against; a door far from it is not on this route. Distance is measured to the
     door's bbox (expanded by a margin), so a wide door still matches when the dead-end
     is at its threshold.

Signal 1 excludes an unrelated poly-blocked door; signal 2 excludes an unrelated door
away from the dead-end. Together they replace the removed relevance test without
depending on the door being nav-poly-tagged, so both the arena (poly-blocked) and Q1
(geometry-blocked) gates resolve to the correct door.

The toggle is confined to this single-threaded think and the flag is restored before
returning, so no other consumer observes the transient state (G_Nav_ReconcileDoors
re-derives it from the still-closed mover state regardless). Only a named door (already
guaranteed by WiredIntel_DoorIsClosedGate) is toggled, since the trap keys on the name.
*/
/* Signal 1: opening the gate must pull the reachable frontier at least this much closer
   to the goal (Q3 units) to count as on the route. Above nav jitter, below a real gate's
   corridor advance; squared for the squared gaps compared below. */
#define WI_NAV_PROGRESS_GAIN 128.0f
/* Signal 2: the corridor dead-end must land within this of the door's bbox (Q3 units)
   for the door to be the obstacle the bot is stuck against. Roughly a doorway's width
   plus the agent radius, so the threshold poly in front of a wide door still matches. */
#define WI_NAV_DEADEND_NEAR 96.0f

/* STEP 4 — the exit candidate's utility, on the shipped [0,100] item scale.
 *
 *   utility = exitWeight * 1/(1 + tt/divisor),  tt = dist*1000/300 (shipped proxy)
 *
 * NEITHER PARAMETER IS INVENTED.  Both come from sweep cvars with NO shipped
 * default: `wi_exitWeight` unset (<= 0) makes this return -1, so the exit candidate
 * is ABSENT and WiredIntel_ChooseLTGItem behaves byte-identically to before step 4.
 * The landed build therefore changes nothing until a sweep value is supplied on the
 * command line.
 *
 * The divisor is swept too, and that is a MEASUREMENT rather than a new tunable.
 * `220.0f` is a bare literal at exactly two sites, never named, and it is the only
 * distance-sensitivity in the goal system.  It was tuned for arena pickup ranges —
 * an item 66 units away already scores at half strength — while the e1m1 exit sits
 * ~974 units away, where that falloff yields ~0.063.  Asking one constant to serve
 * two goal classes with different natural scales is a modelling question, so the
 * sweep measures what the exit would REQUIRE.  The item path keeps 220 untouched.
 *
 * ARENA: FindExitOrigin failing returns -1 (ABSENT), never 0 (present-but-worthless). */
float WiredIntel_ExitCandidateScore( const bot_state_t *bs ) {
#if FEAT_RECAST_NAVMESH
    vec3_t exitOrigin, d;
    float  w, divisor, tt;
    if ( !bs || !bs->wiredIntelActive ) {
        return -1.0f;
    }
    w = (float)trap_Cvar_VariableIntegerValue( "wi_exitWeight" );
    if ( w <= 0.0f ) {
        return -1.0f;   /* unswept -> ABSENT -> pre-step-4 behaviour exactly */
    }
    if ( !WiredIntel_FindExitOrigin( bs->origin, exitOrigin ) ) {
        return -1.0f;   /* arena / no changelevel -> ABSENT, not zero-scoring */
    }
    divisor = (float)trap_Cvar_VariableIntegerValue( "wi_exitDivisor" );
    if ( divisor <= 0.0f ) {
        divisor = 220.0f;   /* the shipped item falloff, used unchanged when unswept */
    }
    VectorSubtract( exitOrigin, bs->origin, d );
    tt = VectorLength( d ) * 1000.0f / 300.0f + 1.0f;
    return w * ( 1.0f / ( 1.0f + tt / divisor ) );
#else
    (void)bs;
    return -1.0f;
#endif
}

/* Instrument only (see WiredIntel_CandReport): fill the RIDE and EXIT magnitudes.
 *
 * Both are pure reads — WiredIntel_PathFrontier is a trap_Nav_FindPath into a
 * file-static buffer with no flag writes, and the exit term is vector arithmetic.
 * The DOOR magnitude is deliberately NOT computed here: it needs `openGap`, which
 * only exists inside a probe that mutates NAVPOLY_BLOCKED, and that probe has a
 * documented corruption history whose pairing audit is provably blind
 * (nav_local.h:138-147).  Forcing it would put the highest-risk mutation in this
 * subsystem on the hottest path in the bot, so door stays structurally
 * unmeasurable without mutation and is reported as -1.
 *
 * Sets no goal, queues no button, touches no door. */
static void WiredIntel_CandProbeRideExit( const vec_t *probeOrigin ) {
#if FEAT_RECAST_NAVMESH
    vec3_t exitOrigin, d;
    float  cur;
    if ( !probeOrigin ) {
        return;
    }
    if ( !WiredIntel_FindExitOrigin( probeOrigin, exitOrigin ) ) {
        return;   /* no changelevel on this map (arena): exit stays ABSENT, not zero */
    }
    VectorSubtract( exitOrigin, probeOrigin, d );
    s_candExitFrontier = VectorLength( d );
    /* The bot's own route FRONTIER toward the exit — the squared horizontal gap the
     * corridor still leaves.  This is the quantity FindRideMover subtracts a mover's
     * delivery frontier FROM, so it is the ride reference frame, not a ride gain. */
    cur = WiredIntel_PathFrontier( probeOrigin, exitOrigin, NULL );
    if ( cur < WI_NAV_PROGRESS_NONE ) {
        s_candRouteFrontier = cur / ( WI_NAV_PROGRESS_GAIN * WI_NAV_PROGRESS_GAIN );
    }
#else
    (void)probeOrigin;
#endif
}

/*
==================
WiredIntel_DeriveGateFarSide

Derive a point on the FAR side of 'door' along the corridor the bot would take to
'goalOrigin' — the point the bot must reach for the gate to have been *used*
rather than merely opened.

Derivation, not authorship. The query must be asked as if the gate were already
open: at install time the leaf is still SHUT, and Detour will not plan through a
closed leaf, so a plain query returns a corridor that goes AROUND the door and
contains no crossing to name (measured: 0 of 4 derivations on the e1m1 specimen).
So this reuses the neutralised-probe idiom the on-route gate finder already
relies on — clear NAVPOLY_OPENABLE_CLOSED for this door, which trips the
area-cost revert so the leaf plans through at normal cost, run the probe, and
restore the flag before returning on EVERY path. The NAVPOLY_DOOR tag is
permanent and survives the toggle, so the door waypoint is still identifiable on
the returned corridor.

The waypoint carrying NAVPOLY_DOOR that resolves to THIS door is the crossing;
the waypoint immediately AFTER it is on the far side by construction, and is
already proven walkable because the corridor contains it. Nothing is authored:
no coordinate, no offset, no axis assumption, no map value — the point comes from
the mesh's own route. Door waypoints are matched to the door entity with the same
expanded-bbox technique the gate finder uses, not a second matcher.

Returns qtrue and fills 'farOut' when such a waypoint exists; qfalse otherwise,
in which case the caller installs no traverse steps and behaves as before.
==================
*/
#if FEAT_RECAST_NAVMESH
static qboolean WiredIntel_DeriveGateFarSide( const gentity_t *door, const vec3_t from,
                                              const vec3_t goalOrigin, vec3_t farOut ) {
    static navPath_t s_far;   /* ~7 KB — file-static, single-threaded think */
    int      n, count, i;
    qboolean found = qfalse;

    if ( !door || !farOut ) {
        return qfalse;
    }

    /* Neutralise THIS door's closed cost so the probe plans through it, exactly as
       WiredIntel_FindFirstOnRouteGate does for its candidate set. */
    trap_Nav_SetPolyFlagsForDoor( door->targetname, 0, NAVPOLY_OPENABLE_CLOSED );

    n     = trap_Nav_FindPath( (vec_t *)from, (vec_t *)goalOrigin, WI_NAV_AGENT_PLAYER, &s_far );
    count = ( n > 0 ) ? NAV_FINDPATH_COUNT( n ) : 0;

    /* First NAVPOLY_DOOR waypoint that resolves to THIS door; the NEXT waypoint is
       past the leaf.  A door waypoint in the last slot has no following waypoint,
       so there is no far side to name and the derivation fails. */
    for ( i = 0; i + 1 < count && !found; i++ ) {
        vec3_t nearest, delta;
        int    a;
        if ( !( trap_Nav_GetPolyAreaFlags( s_far.polyrefs[i] ) & NAVPOLY_DOOR ) ) {
            continue;
        }
        for ( a = 0; a < 3; a++ ) {
            nearest[a] = s_far.positions[i][a];
            if ( nearest[a] < door->r.absmin[a] - WI_NAV_DEADEND_NEAR )
                nearest[a] = door->r.absmin[a] - WI_NAV_DEADEND_NEAR;
            if ( nearest[a] > door->r.absmax[a] + WI_NAV_DEADEND_NEAR )
                nearest[a] = door->r.absmax[a] + WI_NAV_DEADEND_NEAR;
        }
        VectorSubtract( s_far.positions[i], nearest, delta );
        if ( VectorLengthSquared( delta ) <= WI_NAV_DEADEND_NEAR * WI_NAV_DEADEND_NEAR ) {
            VectorCopy( s_far.positions[i + 1], farOut );
            found = qtrue;
        }
    }

    /* Restore unconditionally, before any return: a door left permanently cheap
       would silently bias every later route query. Single-threaded think, so no
       consumer observes the transient. */
    trap_Nav_SetPolyFlagsForDoor( door->targetname, NAVPOLY_OPENABLE_CLOSED, 0 );
    return found;
}
#endif

/*
==================
WiredIntel_PathReachesThroughGate

Can the agent reach 'to' ONCE 'door' IS OPEN — the state the plan is FOR?

WiredIntel_PathReaches is correct about the navmesh state it is given, and this does
not weaken it: it gives it the RIGHT state and asks the same question.

Why the plain query is unanswerable for a whole class of gate. A gate's opener may
lie BEYOND the gate's own leaf, so the route to it crosses the very door it would
open. At planning time that leaf is still shut, and while a closed OPENABLE door is
still INCLUDED by the filter, its polys — and any crossing OMC tagged to it
(nav_impl.cpp promotes the crossing with setPolyArea(oref, NAVAREA_DOOR)) — carry the
10x door area cost, so the cost-directed search terminates short of the goal and
Detour reports DT_PARTIAL_RESULT. Measured on the e1m1 specimen: 70 probes from 70
different poses, every one a partial, every corridor 84-95 polys traversing an OMC,
and every one terminating at the SAME point regardless of where it started — a
frontier, not a routing accident. The answer "no" was correct and would stay "no"
forever, because the question presupposed a state the plan intends to change.

A plan to open a door is not invalidated by the door being closed at planning time.
So ask under the state that WILL exist: neutralise THIS door's closed cost, probe,
restore. That is not a new mechanism — it is the idiom WiredIntel_DeriveGateFarSide
and WiredIntel_FindFirstOnRouteGate already rely on, hoisted so a third consumer can
share it instead of open-coding a second copy.

🔴 The restore is structural, not a discipline. The caller's scan has breaks,
continues and an early return; if the neutralise/restore pair straddled those exits a
leak would be one edit away, and a door left permanently cheap silently biases EVERY
later route query in the run — a fault that would first look like an improvement.
Here both flag writes and the probe live in one function with a single return, so
every path through the caller restores by construction.

Neutralises ONLY the gate under evaluation, never the candidate set and never
globally: the question is per-door and stays per-door. Nothing here keys on a door
id, a targetname, a map, or a distance — any gate whose opener lies beyond its own
leaf is served by the same code.
==================
*/
/* Unguarded, matching WiredIntel_PathReaches: its three call sites in the activation
   scan are not inside a FEAT_RECAST_NAVMESH block either, and trap_Nav_SetPolyFlagsForDoor
   is a no-op when the nav layer is absent. */
static qboolean WiredIntel_PathReachesThroughGate( const gentity_t *door,
                                                   const vec3_t from, const vec3_t to ) {
    qboolean reaches;

    if ( !door || !door->targetname ) {
        /* No door to neutralise — the plain question is already the right one. */
        return WiredIntel_PathReaches( from, to );
    }

    trap_Nav_SetPolyFlagsForDoor( door->targetname, 0, NAVPOLY_OPENABLE_CLOSED );
    reaches = WiredIntel_PathReaches( from, to );
    /* Restore unconditionally, before the single return: single-threaded think, so no
       consumer observes the transient. */
    trap_Nav_SetPolyFlagsForDoor( door->targetname, NAVPOLY_OPENABLE_CLOSED, 0 );

    return reaches;
}

/*
==================
WiredIntel_DoorCrossingAlreadyTraversable

The gate-divert test (activation-goal 2a): does this door genuinely NOT OBSTRUCT the
route, so arming its opener would be pointless?  Only then may the candidate be
vetoed.

🔴 What this used to be, and why it was wrong.  The old signal was "the corridor's
frontier end lies PAST the door's own position ⇒ the bot already crossed it without
opening it ⇒ veto".  That is the same reachability/passability conflation as the
global short-circuit, applied per candidate.  A corridor that runs BEYOND a closed
leaf is not evidence the leaf is open — the query filter excludes only
NAVPOLY_BLOCKED, so a closed door the bake never marked blocked is planned straight
through, and the corridor continues past it happily while the physical leaf still
caps the bot.  "The corridor goes past this door" is in fact the STRONGEST available
evidence that the door is ON THE ROUTE, and the old test spent it as a reason to
ignore the door.  Measured: it vetoed 1440 of 1440 candidates.

The corrected predicate reads the route's CONTENTS instead:

  - If the corridor CROSSES this door (WiredIntel_RouteCrossesDoor), the door is on
    the route.  NEVER veto — regardless of how far beyond the leaf the corridor runs.
  - If the corridor does NOT cross this door and it REACHES the goal, the route
    genuinely avoids this door: opening it changes nothing about getting to the goal,
    so the veto is correct and fires.
  - If the corridor does not cross it and does not reach either, the door is neither
    proven on-route nor proven irrelevant; leave the decision to the relevance test
    (WiredIntel_DoorCutsRoute) rather than vetoing on no evidence.

This is not the old test renamed: the old one keyed on the frontier's DISTANCE
relative to the door and vetoed on "past"; this one keys on set MEMBERSHIP (is the
leaf among the things the corridor passes through) and treats "past" as inclusion.
The two disagree on exactly the class this arc is about — a corridor-complete closed
gate — where the old form vetoed and this one detects.
==================
*/
static qboolean WiredIntel_DoorCrossingAlreadyTraversable( const gentity_t *door,
                                                           const vec3_t from,
                                                           const vec3_t goalOrigin ) {
#if FEAT_RECAST_NAVMESH
    qboolean reaches = qfalse;

    if ( WiredIntel_RouteCrossesDoor( door, from, goalOrigin, &reaches ) ) {
        return qfalse;   /* on the route — the crossing is the gate, not a bypass */
    }
    /* Not crossed.  A corridor that reaches the goal without touching this leaf is a
       genuine bypass: opening the door is moot.  Anything short of that is no
       evidence either way, so do not veto. */
    return reaches ? qtrue : qfalse;
#else
    (void)door; (void)from; (void)goalOrigin;
    return qfalse;
#endif
}

static qboolean WiredIntel_DoorCutsRoute( const gentity_t *door, const vec3_t from,
                                         const vec3_t goalOrigin ) {
#if FEAT_RECAST_NAVMESH
    float  closedGap, openGap;
    vec3_t closedEnd, nearest;
    int    a;

    if ( !door->targetname || !door->targetname[0] ) {
        return qfalse;
    }

    /* Frontier + dead-end with the door closed (the current, blocked state). */
    closedGap = WiredIntel_PathFrontier( from, goalOrigin, closedEnd );

    /* Signal 1 — poly-toggle advance. A no-op when the door's polys are not blocked
       (openGap == closedGap), which is exactly when signal 2 must decide. */
    trap_Nav_SetPolyFlagsForDoor( door->targetname, 0, NAVPOLY_BLOCKED );
    openGap = WiredIntel_PathFrontier( from, goalOrigin, NULL );
    trap_Nav_SetPolyFlagsForDoor( door->targetname, NAVPOLY_BLOCKED, 0 );
    /* Instrument only: publish the route-gain this test is about to reduce to a
     * boolean.  Reads values already computed above; alters nothing. */
    if ( openGap < WI_NAV_PROGRESS_NONE && closedGap < WI_NAV_PROGRESS_NONE ) {
        s_candDoorGain = ( closedGap - openGap ) /
                         ( WI_NAV_PROGRESS_GAIN * WI_NAV_PROGRESS_GAIN );
    }
    if ( openGap < WI_NAV_PROGRESS_NONE &&
         openGap + WI_NAV_PROGRESS_GAIN * WI_NAV_PROGRESS_GAIN <= closedGap ) {
        return qtrue;   /* opening it advances the route → this door was the cut */
    }

    /* Signal 2 — dead-end proximity. Only meaningful when the closed-door probe
       actually produced a corridor that dead-ends short of the goal. */
    if ( closedGap >= WI_NAV_PROGRESS_NONE ) {
        return qfalse;
    }
    /* Nearest point on the door's bbox to the corridor dead-end. */
    for ( a = 0; a < 3; a++ ) {
        nearest[a] = closedEnd[a];
        if ( nearest[a] < door->r.absmin[a] ) nearest[a] = door->r.absmin[a];
        if ( nearest[a] > door->r.absmax[a] ) nearest[a] = door->r.absmax[a];
    }
    {
        vec3_t d;
        VectorSubtract( closedEnd, nearest, d );
        if ( VectorLengthSquared( d ) <= WI_NAV_DEADEND_NEAR * WI_NAV_DEADEND_NEAR ) {
            return qtrue;   /* the route stops right at this door → it is the gate */
        }
    }
    return qfalse;
#else
    (void)door; (void)from; (void)goalOrigin;
    return qtrue;   /* no runtime nav mesh (AAS build) — fall back to distance rank */
#endif
}

/*
==================
WiredIntel_FindRideMover
==================
Scan for a rideable elevator that the bot must board to reach 'goalOrigin', and
if found derive its boarding/step-off points. Returns the mover entitynum (>=0)
and fills board/stepOff on success, or -1 when there is no ride to install.

Ride-required vs bypassable (mirrors the door-gate producer's logic exactly):
 - If the bot can ALREADY reach the goal on the current mesh, nothing blocks it —
   no ride. (WiredIntel_PathReaches; byte-identical no-op on maps with no ride.)
 - Otherwise, among rideable movers, keep only one whose UPPER stop (delivery) is
   reachable toward the goal AND materially closer to the goal than the bot's
   current reachable frontier — i.e. riding it up actually advances the route.
   An unrelated elevator elsewhere on the map does not, so it is skipped. This is
   the ride analog of WiredIntel_DoorCutsRoute's frontier-advance signal.
 - The mover must not already be delivering the bot (bot already at/above the top),
   and its endpoints must be derivable (else that mover is skipped, never guessed).
*/
static int WiredIntel_FindRideMover( bot_state_t *bs, const vec3_t goalOrigin,
                                     vec3_t boardOut, vec3_t stepOffOut ) {
#if FEAT_RECAST_NAVMESH
    int    i, best = -1;
    float  curFrontier, bestGain = 0.0f;
    vec3_t curEnd;

    if ( !trap_Nav_IsReady() ) {
        return -1;   /* navmesh still baking — no reliable probe */
    }
    /* Already reachable → nothing to ride (the bypass check). */
    if ( WiredIntel_PathReaches( bs->origin, goalOrigin ) ) {
        return -1;
    }
    /* The bot's current reachable frontier toward the goal (how close its route
       gets before dead-ending). A ride is worth installing only if boarding an
       elevator gets the route materially closer than this. */
    curFrontier = WiredIntel_PathFrontier( bs->origin, goalOrigin, curEnd );

    for ( i = MAX_CLIENTS; i < level.num_entities; i++ ) {
        gentity_t *ent = &g_entities[i];
        vec3_t     board, stepOff;
        float      deliverFrontier;

        if ( !WiredIntel_MoverIsRideable( ent ) ) {
            continue;
        }
        if ( !WiredIntel_MoverRideEndpoints( ent, board, stepOff ) ) {
            continue;   /* endpoints not derivable from geometry+nav — skip (no guess) */
        }
        /* Skip a mover the bot is already on top of / above its delivery: riding
           it would not advance the route (the bot is past it). */
        if ( bs->origin[2] >= stepOff[2] - WI_RIDE_GOTO_RANGE &&
             WiredIntel_PathReaches( bs->origin, stepOff ) ) {
            continue;
        }
        /* Route-advance signal: how close to the goal is the route FROM this
           elevator's delivery point? If that frontier is meaningfully nearer the
           goal than the bot's current frontier, boarding this car advances the
           route → it is on the way. An unrelated elevator's delivery is no closer,
           so it contributes no gain and is skipped. */
        deliverFrontier = WiredIntel_PathFrontier( stepOff, goalOrigin, NULL );
        if ( deliverFrontier >= WI_NAV_PROGRESS_NONE ) {
            continue;   /* the goal is not reachable even from its delivery — not this ride */
        }
        /* BOARD REACHABILITY.  The frontier test above asks "does the route advance
           once I step OFF this car?" — it says nothing about whether the bot can get
           ON.  A delivery point deep on the far side of the map scores an enormous
           gain while its boarding point has no corridor at all, so the candidate is
           accepted, the sequence takes the goal under its directive lock, the bot
           cannot make progress toward the board, and the sequence watchdog releases
           it seconds later — then the same candidate is selected again.  The route
           the bot must WALK is the boarding point, so that endpoint carries the same
           routability requirement the delivery endpoint already carries.  This is the
           counterpart to the PathReaches call on stepOff above, on the other end.
           SUSTAINED, NEVER SINGLE-SAMPLE.  A car in motion can put its boarding point
           briefly out of corridor (mid-travel, or occluded at the far stop), and a
           one-shot reject would starve a legitimate ride for a transient condition.
           So unreachability must persist: the threshold is 2x the ride detector's own
           re-arm period (WI_RIDE_RETRY_PERIOD_MS, the single statement of the cadence
           WiredIntel_TrySetRideGoal rate-limits with), i.e. two full detector cycles
           — the shortest window in which a transient block would have cleared and
           re-armed.  Derived from that identity rather than fitted to any specimen,
           the same way the held-goal retire's threshold is stated from the gate
           detector's cadence.
           Tracked PER CANDIDATE.  The scan revisits every rideable mover on every
           call, so several can read unroutable at once and each needs its own
           clock — one shared slot would let rivals evict each other every pass so
           that no clock ever advances and the reject never fires.  What the
           per-candidate form guarantees is exactly this and no more: a candidate is
           skipped only once ITS OWN board has read unroutable continuously for the
           window, and any single routable reading clears that candidate's clock.
           It does not assert the board is permanently unreachable — only that the
           condition has outlasted any transient the detector's own period could
           hide. */
        if ( !WiredIntel_PathReaches( bs->origin, board ) ) {
            int *since = &bs->directives.rideBoardUnreachSince[i];
            if ( !*since ) {
                *since = level.time;   /* first unroutable reading — start its clock */
            } else if ( level.time - *since >=
                            (int)( 2.0f * WI_RIDE_RETRY_PERIOD_MS ) ) {
                continue;   /* board unroutable for two detector cycles — not this ride */
            }
        } else {
            bs->directives.rideBoardUnreachSince[i] = 0;   /* routable — clear its clock */
        }
        /* Instrument only: publish the ride route-gain before the threshold below
         * reduces it to a selection.  Same normaliser as the door gain. */
        if ( deliverFrontier < WI_NAV_PROGRESS_NONE &&
             curFrontier < WI_NAV_PROGRESS_NONE ) {
            float g = ( curFrontier - deliverFrontier ) /
                      ( WI_NAV_PROGRESS_GAIN * WI_NAV_PROGRESS_GAIN );
            if ( g > s_candRideGain ) s_candRideGain = g;
        }
        if ( curFrontier - deliverFrontier > bestGain &&
             curFrontier - deliverFrontier >
                 WI_NAV_PROGRESS_GAIN * WI_NAV_PROGRESS_GAIN ) {
            bestGain = curFrontier - deliverFrontier;
            best     = i;
            VectorCopy( board,   boardOut );
            VectorCopy( stepOff, stepOffOut );
        }
    }
    return best;
#else
    (void)bs; (void)goalOrigin; (void)boardOut; (void)stepOffOut;
    return -1;   /* no runtime nav mesh — no ride detection */
#endif
}

/*
==================
WiredIntel_TrySetRideGoal
==================
When a rideable elevator lies on the bot's route to 'goalOrigin', install a
board -> wait-for-delivery -> step-off sequence into the brain's sequenced-goal
cursor and return qtrue; the substrate driver then walks it across frames. A
byte-identical no-op (returns qfalse without touching the brain) when there is no
ride-requiring mover in reach, when the goal is already reachable, or when the
brain already has a ride sequence in flight. Only a client-brain rides (the same
BotCanActivate capability gate as the door-activation family; the plat trigger is
client-gated in the engine, so a monster could never start the ride anyway).
Rate-limited like the activation detector so a ride-free map does not rescan every
think.
*/
static qboolean WiredIntel_TrySetRideGoal( bot_state_t *bs, const vec3_t goalOrigin ) {
    botDirectiveState_t *ds;
    vec3_t board, stepOff;
    int    mover;

    if ( !BotCanActivate( bs ) ) {
        return qfalse;
    }
    /* A ride already in flight owns the goal — let the substrate finish it. */
    if ( bs->sequencedGoal.active && bs->sequencedGoal.sequenceId == WI_RIDE_SEQUENCE_ID ) {
        return qtrue;
    }
    /* Do not stomp some other in-flight sequence (defensive; today rides are the
       only producer, but this keeps the producer well-behaved if others land). */
    if ( bs->sequencedGoal.active ) {
        return qfalse;
    }
    ds = &bs->directives;
    if ( FloatTime() < ds->rideRetryTime ) {
        return qfalse;
    }
    /* One statement of the detector cadence (WI_RIDE_RETRY_PERIOD_MS, in seconds
       here) — the board-reachability persistence window is derived from the same
       constant, so the two stay locked together if this is ever retuned. */
    ds->rideRetryTime = FloatTime() + ( WI_RIDE_RETRY_PERIOD_MS / 1000.0f );

    mover = WiredIntel_FindRideMover( bs, goalOrigin, board, stepOff );
    if ( mover < 0 ) {
        return qfalse;   /* no ride-requiring mover in reach → caller falls through */
    }

    WiredIntel_InstallRideSequence( &bs->sequencedGoal, mover, board, stepOff );

    if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
        Com_Log( SEV_INFO, LOG_CH(ch_botai),
            "cl=%d ride mover=%d board=(%.0f %.0f %.0f) stepoff=(%.0f %.0f %.0f)\n",
            bs->client, mover, board[0], board[1], board[2],
            stepOff[0], stepOff[1], stepOff[2] );
    }
    return qtrue;
}

/* =========================================================================
   ELEVATOR-RIDE PRODUCER SELF-TEST — out of the shipped path (bot_debug >= 3,
   once per process). Same removable-witness pattern as the sequenced-goal
   substrate + primitive self-tests: it proves, on scratch state with no live
   map, that

     (1) WiredIntel_MoverIsRideable classifies a vertical plat (touch handler +
         vertical pos1/pos2 travel) as rideable and rejects a horizontal /
         non-plat mover — the format-neutral detector recognizes a ride mover;
     (2) WiredIntel_InstallRideSequence builds exactly
           GoTo(board) -> WAIT_MOVERSTATE(mover) -> GoTo(stepOff)
         with the right step kinds, points, and mover entnum;
     (3) driving that sequence through the landed substrate walks the cursor with
         mover-state gating: it parks on the WAIT step until a fake plat flips to
         MOVER_POS2, then advances to the step-off GoTo.

   The FULL detector (WiredIntel_FindRideMover) needs a live nav mesh
   (trap_Nav_FindNearestPoly / trap_Nav_FindPath return no result off-map), so it
   is NOT exercised here — the honest offline proof is the classifier + builder +
   substrate-walk. The producer's route decision is proven live only on a map with
   a ride-requiring mover (see handoff). The endpoints given to the builder here
   are the fake mover's own pos1/pos2 (derived, not invented), matching what the
   live derivation feeds after its nav snap.

   Gated on bot_debug >= 3 so it is dormant at the shipped default and at the
   debug levels normal play uses (0/1/2). Uses the existing bot_debug cvar; no new
   cvar. Borrows one entity slot for the fake plat and restores it — byte-neutral.
   ========================================================================= */

/* Scratch entity slot the ride self-test borrows for its fake plat. Any index
   < MAX_GENTITIES works; the test snapshots and restores this slot (and
   level.num_entities / level.time) so it is byte-neutral. Distinct from the
   substrate self-test's slot so the two never interfere if both run. */
#define WI_RIDE_TEST_MOVER_ENT (MAX_CLIENTS + 2)

static qboolean WiredIntel_RideProducerSelfTest( void ) {
    const int   ent = WI_RIDE_TEST_MOVER_ENT;
    gentity_t   savedEnt      = g_entities[ent];
    int         savedNumEnts  = level.num_entities;
    int         savedTime     = level.time;
    gentity_t  *mover         = &g_entities[ent];
    qboolean    pass          = qtrue;
    bot_state_t scratch;
    botSequencedGoal_t *sg;
    const vec3_t board   = { 200.0f, 100.0f,   0.0f };   /* stand-in derived boarding pt  */
    const vec3_t stepOff = { 200.0f, 100.0f, 128.0f };   /* stand-in derived step-off pt  */

    if ( level.num_entities <= ent ) {
        level.num_entities = ent + 1;
    }

    /* ── (1) classifier: a vertical plat is rideable ─────────────────────── */
    memset( mover, 0, sizeof( *mover ) );
    mover->inuse = qtrue;
    mover->touch = Q3_Touch_Plat;                 /* the format-neutral plat marker */
    VectorSet( mover->r.mins, -48.0f, -48.0f, -8.0f );
    VectorSet( mover->r.maxs,  48.0f,  48.0f,  8.0f );
    VectorSet( mover->pos1, 200.0f, 100.0f,   0.0f );   /* boarding stop (bottom) */
    VectorSet( mover->pos2, 200.0f, 100.0f, 128.0f );   /* delivery stop (top)    */
    mover->moverState = MOVER_POS1;
    if ( !WiredIntel_MoverIsRideable( mover ) ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "RideProducer-selftest: vertical plat not classified rideable\n" );
    }

    /* a non-plat mover (no plat touch handler) is NOT rideable */
    mover->touch = NULL;
    if ( WiredIntel_MoverIsRideable( mover ) ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "RideProducer-selftest: non-plat (no touch handler) wrongly rideable\n" );
    }
    /* a horizontally-travelling plat is NOT a ride (a sliding door, not an elevator) */
    mover->touch = Q3_Touch_Plat;
    VectorSet( mover->pos2, 400.0f, 100.0f, 4.0f );   /* mostly horizontal travel */
    if ( WiredIntel_MoverIsRideable( mover ) ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "RideProducer-selftest: horizontal mover wrongly classified as a ride\n" );
    }

    /* ── (2) builder: the installed sequence has the exact shape ──────────── */
    memset( &scratch, 0, sizeof( scratch ) );
    scratch.client = 0;
    sg = &scratch.sequencedGoal;
    WiredIntel_InstallRideSequence( sg, ent, board, stepOff );

    if ( sg->stepCount != 3 || !sg->active || sg->sequenceId != WI_RIDE_SEQUENCE_ID ||
         sg->cursor != 0 ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "RideProducer-selftest: install shape wrong (stepCount=%d active=%d id=%d cursor=%d)\n",
            sg->stepCount, sg->active, sg->sequenceId, sg->cursor );
    }
    if ( sg->steps[0].type != SEQ_STEP_GOTO ||
         Distance( sg->steps[0].point, (float *)board ) > 0.5f ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "RideProducer-selftest: step0 is not GoTo(board)\n" );
    }
    if ( sg->steps[1].type != SEQ_STEP_WAIT_MOVERSTATE || sg->steps[1].moverEntitynum != ent ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "RideProducer-selftest: step1 is not WAIT_MOVERSTATE(mover ent=%d), got type=%d ent=%d\n",
            ent, sg->steps[1].type, sg->steps[1].moverEntitynum );
    }
    if ( sg->steps[2].type != SEQ_STEP_GOTO ||
         Distance( sg->steps[2].point, (float *)stepOff ) > 0.5f ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "RideProducer-selftest: step2 is not GoTo(stepOff)\n" );
    }

    /* ── (3) substrate walk with mover-state gating ──────────────────────── */
    /* Fresh fake plat at POS1 (boarding, stationary). */
    memset( mover, 0, sizeof( *mover ) );
    mover->inuse        = qtrue;
    mover->touch        = Q3_Touch_Plat;
    mover->moverState   = MOVER_POS1;
    mover->s.pos.trType = TR_STATIONARY;
    VectorCopy( board, mover->s.pos.trBase );

    /* Bot standing on the boarding point → GoTo(board) completes at once and the
       cursor lands on the WAIT_MOVERSTATE step. */
    level.time = 1000;
    VectorCopy( board, scratch.origin );
    WiredIntel_StepSequencedGoal( &scratch );
    if ( sg->cursor != 1 || !sg->active ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "RideProducer-selftest: expected park on WAIT_MOVERSTATE (cursor 1), got cursor=%d active=%d\n",
            sg->cursor, sg->active );
    }

    /* Car still at POS1 → the WAIT step yields, cursor holds. */
    level.time += 200;
    WiredIntel_StepSequencedGoal( &scratch );
    if ( sg->cursor != 1 ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "RideProducer-selftest: WAIT should hold at cursor 1 while car below POS2, got %d\n",
            sg->cursor );
    }

    /* Car reaches POS2 (delivered) → WAIT completes, cursor advances to the
       step-off GoTo. Bot not yet at the top, so it parks at cursor 2. */
    mover->moverState = MOVER_POS2;
    WiredIntel_StepSequencedGoal( &scratch );
    if ( sg->cursor != 2 || !sg->active ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "RideProducer-selftest: on POS2 expected advance to step-off GoTo (cursor 2), got cursor=%d active=%d\n",
            sg->cursor, sg->active );
    }

    /* Bot steps off at the delivery point → GoTo(stepOff) completes → sequence
       runs off the end → idle, directive lock released. */
    VectorCopy( stepOff, scratch.origin );
    WiredIntel_StepSequencedGoal( &scratch );
    if ( sg->active || scratch.directives.directiveLocked ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "RideProducer-selftest: after step-off expected idle + unlocked, got active=%d locked=%d\n",
            sg->active, scratch.directives.directiveLocked );
    }

    /* Restore the borrowed slot + counters — byte-neutral exit. */
    g_entities[ent]    = savedEnt;
    level.num_entities = savedNumEnts;
    level.time         = savedTime;

    BotAI_Print( PRT_MESSAGE, "RideProducer-selftest: classify-rideable + build(GoTo(board) -> WAIT_MOVERSTATE -> GoTo(stepOff)) + substrate-walk-gated-on-POS2: %s\n",
        pass ? "ALL PASS" : "FAILURE" );
    return pass;
}

void WiredIntel_RideProducerSelfTestOnce( void ) {
    static qboolean s_ran = qfalse;
    if ( s_ran ) return;
    if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) < 3 ) return;
    s_ran = qtrue;
    WiredIntel_RideProducerSelfTest();
}

/*
==================
WiredIntel_FindFirstOnRouteGate
==================
Select the first closed gate the bot must open on the REAL route to a goal it can
only reach as a PARTIAL corridor.

Why a special path: a closed openable door is planned-THROUGH at 10x cost (the
NAVAREA_DOOR area cost), so on a route that crosses several closed doors the
accumulated door cost can make a shorter cost-cheaper DEAD-END (e.g. a func_plat
up-lift the bot cannot actually progress past) win the A* search — the follower is
handed a partial corridor that dead-ends at the cheap decoy, nowhere near the real
gates.  DoorCutsRoute cannot see those gates: its dead-end-proximity signal keys on
the (decoy) dead-end, and its poly-toggle signal flips NAVPOLY_BLOCKED which these
OPENABLE_CLOSED doors do not carry.  And no per-door advance test fires, because
opening any single door does not lower the real route's cost below the decoy until
enough of them are open (the cost threshold).

Mechanism: neutralise ALL closed gates at once — clearing NAVPOLY_OPENABLE_CLOSED
reverts each door's area from NAVAREA_DOOR back to its original (the landed door-cost
revert), dropping the 10x — so a fresh probe yields the genuine reachable route.
Walk that route's waypoints in order (findStraightPath emits them origin-outward) and
take the FIRST one still flagged NAVPOLY_DOOR (a permanent per-poly tag the toggle
never clears): that waypoint sits at the nearest on-route door, the one to open
first.  Map it back to a closed-gate entity spatially (the same door-bbox technique
DoorCutsRoute / DoorCrossingAlreadyTraversable already use — the poly->entity table is
engine-side).  Restore every neutralised door before returning (single-threaded think,
no consumer observes the transient).  Returns the chosen door's entitynum, or -1.

General: keys on the PARTIAL signal + the NAVPOLY_DOOR tag + door bboxes — no map,
plat, or gate-name constant.  A map whose exit is a plain single-gate cut never
reaches here (that door's own DoorCutsRoute fires in the normal path); this handles
only the cost-diverted multi-gate PARTIAL case.
*/
#if FEAT_RECAST_NAVMESH
static int WiredIntel_FindFirstOnRouteGate( bot_state_t *bs, const vec3_t goalOrigin,
                                            const int *cand, int nCand ) {
    static navPath_t s_real;   /* file-static: navPath_t is large; single-threaded think */
    int   i, n, count, chosen = -1;

    /* Neutralise every collected closed gate: clearing OPENABLE_CLOSED trips the
       area-cost revert so the door plans through at normal (1x) cost. */
    for ( i = 0; i < nCand; i++ ) {
        if ( cand[i] < 0 ) continue;
        trap_Nav_SetPolyFlagsForDoor( g_entities[cand[i]].targetname, 0, NAVPOLY_OPENABLE_CLOSED );
    }

    /* Probe the genuine route with door costs neutralised. */
    n = trap_Nav_FindPath( (vec_t *)bs->origin, (vec_t *)goalOrigin, WI_NAV_AGENT_PLAYER, &s_real );
    count = ( n > 0 ) ? NAV_FINDPATH_COUNT( n ) : 0;

    /* First on-route door waypoint (permanent NAVPOLY_DOOR tag survives the toggle). */
    for ( i = 0; i < count && chosen < 0; i++ ) {
        if ( !( trap_Nav_GetPolyAreaFlags( s_real.polyrefs[i] ) & NAVPOLY_DOOR ) ) continue;
        /* Map this door waypoint to the nearest collected closed-gate entity (the
           poly->door table is engine-side; match spatially by bbox, expanded a little
           so a threshold/door-gap waypoint just outside the leaf still resolves). */
        {
            int   s, best = -1;
            float bestDsq = 0.0f;
            for ( s = 0; s < nCand; s++ ) {
                gentity_t *d;
                vec3_t nearest, delta;
                int a;
                if ( cand[s] < 0 ) continue;
                d = &g_entities[cand[s]];
                for ( a = 0; a < 3; a++ ) {
                    nearest[a] = s_real.positions[i][a];
                    if ( nearest[a] < d->r.absmin[a] - WI_NAV_DEADEND_NEAR ) nearest[a] = d->r.absmin[a] - WI_NAV_DEADEND_NEAR;
                    if ( nearest[a] > d->r.absmax[a] + WI_NAV_DEADEND_NEAR ) nearest[a] = d->r.absmax[a] + WI_NAV_DEADEND_NEAR;
                }
                VectorSubtract( s_real.positions[i], nearest, delta );
                {
                    float dsq = VectorLengthSquared( delta );
                    if ( best < 0 || dsq < bestDsq ) { best = cand[s]; bestDsq = dsq; }
                }
            }
            /* Only accept a spatial match that actually lands on/near a gate's bbox,
               so an unrelated door-tagged poly far from every candidate is ignored. */
            if ( best >= 0 && bestDsq <= WI_NAV_DEADEND_NEAR * WI_NAV_DEADEND_NEAR ) {
                chosen = best;
            }
        }
    }

    /* Restore every door we neutralised (re-set OPENABLE_CLOSED -> re-promote the
       NAVAREA_DOOR cost).  Unconditional, before any return, so no consumer observes
       the transient state. */
    for ( i = 0; i < nCand; i++ ) {
        if ( cand[i] < 0 ) continue;
        trap_Nav_SetPolyFlagsForDoor( g_entities[cand[i]].targetname, NAVPOLY_OPENABLE_CLOSED, 0 );
    }
    return chosen;
}
#endif

/*
==================
WiredIntel_FindBlockingGateButtons
==================
When the bot's route to its goal is cut by a closed target-opened door, find that
gate and queue the button(s) that open it. The "am I actually blocked" test is a
nav path probe: if the bot can already reach the goal, there is no gate to open
(byte-identical no-op on ungated maps and once a gate has been opened). When the
route IS cut, rank the closed gates nearest-to-the-bot first, then for each apply a
route-relevance test (WiredIntel_DoorCutsRoute) — only a door whose opening restores
the route to the goal is a blocking gate. This rejects a nearby-but-unrelated closed
door that a pure distance rank would wrongly pick. Take the first candidate that
(a) actually cuts the route and (b) resolves to >=1 pressable button. Iterating
candidates — rather than committing to a single nearest door — means a nearby door
that is either irrelevant to the route or has no reachable button does not abort the
whole detection. Populates the queue and records the gate's entitynum; returns qtrue
on success.
*/
/*
==================
WiredIntel_JumpTouchArcHits

Ballistic-arc feasibility for a jump-touch: does a run-jump from 'launch' (a
standable foot) toward the button brush AABB [bmins,bmaxs] have the swept player
box OVERLAP the brush mid-flight?  Pure kinematics from the shipped movement
constants — JUMP_VELOCITY (up impulse), DEFAULT_GRAVITY, DEFAULT_MOVESPEED_PLAYER
(horizontal run) — the same numbers PM_ apply; no map/button-specific constant.
The player box is (MINS_Z..MAXS_Z) tall and NAV agent-radius wide; the arc is
stepped and, at each sample, the box (expanded by the brush's own extents so a
point-in-AABB test stands in for box-vs-box) is tested against the brush AABB.
Returns qtrue and reports the ground launch speed if any sample overlaps.
==================
*/
#define WI_JUMPTOUCH_MAX_HORIZ 200.0f   /* run-jump reaches ~216u; cap candidates a touch under */
#define WI_JUMPTOUCH_AGENT_R   15.0f    /* NAV_WALKABLE_RADIUS — player half-width */
qboolean WiredIntel_JumpTouchArcHits( const vec3_t launch, const vec3_t bmins,
                                      const vec3_t bmaxs, float *speedOut ) {
    /* Aim the horizontal run at the button-brush centre. */
    vec3_t bcen;
    bcen[0] = 0.5f*(bmins[0]+bmaxs[0]);
    bcen[1] = 0.5f*(bmins[1]+bmaxs[1]);
    bcen[2] = 0.5f*(bmins[2]+bmaxs[2]);

    float dx = bcen[0]-launch[0], dy = bcen[1]-launch[1];
    float horiz = sqrtf( dx*dx + dy*dy );
    if ( horiz < 1.0f || horiz > WI_JUMPTOUCH_MAX_HORIZ )
        return qfalse;
    float ux = dx/horiz, uy = dy/horiz;

    const float g   = DEFAULT_GRAVITY;
    const float vz0 = JUMP_VELOCITY;
    /* Player-box half extents to expand the brush AABB by (box-vs-AABB overlap =
       point-in-expanded-AABB of the box centre).  The box origin is the foot +
       |MINS_Z|; sample the ORIGIN path and expand vertically by the box's own
       mins/maxs so any part of the body touching the brush counts. */
    const float boxR = WI_JUMPTOUCH_AGENT_R;

    /* Sweep a range of run speeds (slow..full): a slower run spends more time in
       flight (a higher, nearer arc), a faster run reaches farther — trying several
       finds an overlapping arc if one exists.  Deterministic fixed steps. */
    const int   NSP = 6;
    for ( int si = 0; si < NSP; si++ ) {
        float speed = DEFAULT_MOVESPEED_PLAYER * ( 0.4f + 0.6f * (float)si/(float)(NSP-1) );
        float tflight = ( 2.0f * vz0 ) / g;            /* up-and-back-to-launch-z time */
        /* Extend a little past tflight so an arc landing lower still samples. */
        float tmax = tflight * 1.3f;
        const int NT = 24;
        for ( int ti = 0; ti <= NT; ti++ ) {
            float t = tmax * (float)ti/(float)NT;
            float px = launch[0] + ux*speed*t;
            float py = launch[1] + uy*speed*t;
            float footZ = launch[2] + vz0*t - 0.5f*g*t*t;   /* foot height along the arc */
            /* Only advance while within horizontal reach of the brush centre. */
            float ddx = bcen[0]-px, ddy = bcen[1]-py;
            if ( sqrtf(ddx*ddx+ddy*ddy) > horiz + boxR ) continue;
            /* Box-vs-AABB: player box [px±R, py±R, footZ+MINS_Z .. footZ+MAXS_Z]
               overlaps brush [bmins,bmaxs]? */
            float lo_z = footZ + MINS_Z, hi_z = footZ + MAXS_Z;
            if ( px + boxR < bmins[0] || px - boxR > bmaxs[0] ) continue;
            if ( py + boxR < bmins[1] || py - boxR > bmaxs[1] ) continue;
            if ( hi_z      < bmins[2] || lo_z      > bmaxs[2] ) continue;
            if ( speedOut ) *speedOut = speed;
            return qtrue;   /* the swept box overlaps the brush at this arc sample */
        }
    }
    return qfalse;
}

/*
==================
WiredIntel_TryInstallJumpTouch

For a button the bot cannot walk its bbox onto (PathReaches failed) but whose brush
a run-jump arc CAN overlap: find the launch foot (the reachable frontier toward the
button — where the bot's route currently dead-ends, a standable ledge), validate the
arc, and install a SEQ_STEP_JUMP_TOUCH sequence that opens 'door'.  Returns qtrue if
a feasible jump-touch was installed (the gate is then committed by the caller).
==================
*/
static qboolean WiredIntel_TryInstallJumpTouch( bot_state_t *bs, gentity_t *button,
                                                gentity_t *door, const vec3_t buttonApproach ) {
    vec3_t launch, bcen, speedDir;
    float  speed = 0.0f;
    (void)buttonApproach;

    if ( !button || !door ) return qfalse;

    /* Launch foot = the nearest REACHABLE, STANDABLE floor in FRONT of the button
       face from which a run-jump arc overlaps the brush.  The pressable side is the
       button's face — a Q1 button moves along 'movedir' when pushed, so its face
       normal is -movedir (the player presses from the side the plate faces).  Walk
       outward along that normal from the brush face, and for each candidate drop the
       standing box to a rest floor, require the standing box to FIT there (a headroom
       gate — a launch with no standing+jump clearance is geometrically impossible,
       so it is rejected here instead of relying on the watchdog), require the floor
       be spawn-reachable, and validate the ballistic arc.  The first candidate that
       passes all three is the launch.  This replaces the old EntityNavOrigin snap,
       which returned the nearest nav poly to the brush CENTRE (a low-ceiling ledge
       145u off to the side, not the floor a player actually stands on to press it). */
    bcen[0] = 0.5f*(button->r.absmin[0]+button->r.absmax[0]);
    bcen[1] = 0.5f*(button->r.absmin[1]+button->r.absmax[1]);
    bcen[2] = 0.5f*(button->r.absmin[2]+button->r.absmax[2]);
    {
        vec3_t face;   /* outward face normal = -movedir (unit or axis) */
        face[0] = -button->movedir[0]; face[1] = -button->movedir[1]; face[2] = -button->movedir[2];
        face[2] = 0.0f;   /* horizontal approach only */
        if ( VectorNormalize( face ) < 0.1f ) {
            /* No movedir (shouldn't happen for a real button) — aim from the bot. */
            VectorSubtract( bs->origin, bcen, face ); face[2] = 0.0f;
            if ( VectorNormalize( face ) < 0.1f ) return qfalse;
        }
        /* Half-extent of the brush along the face normal, so the first sample clears
           the brush itself. */
        vec3_t half; VectorSubtract( button->r.absmax, button->r.absmin, half );
        VectorScale( half, 0.5f, half );
        float faceHalf = fabsf(face[0])*half[0] + fabsf(face[1])*half[1];
        qboolean found = qfalse;
        for ( float d = faceHalf + 8.0f; d <= WI_JUMPTOUCH_MAX_HORIZ && !found; d += 12.0f ) {
            vec3_t probe;
            probe[0] = bcen[0] + face[0]*d;
            probe[1] = bcen[1] + face[1]*d;
            probe[2] = bcen[2];   /* start the drop from button height */
            /* Drop the STANDING box to a rest floor; require it FIT (no startsolid)
               so there is standing+jump headroom (the launch-site headroom gate). */
            trace_t dr;
            vec3_t mins = { -15, -15, MINS_Z }, maxs = { 15, 15, MAXS_Z };
            vec3_t ds = { probe[0], probe[1], probe[2] + 8.0f };
            vec3_t de = { probe[0], probe[1], probe[2] - 200.0f };
            trap_Trace( &dr, ds, mins, maxs, de, ENTITYNUM_NONE, MASK_PLAYERSOLID );
            if ( dr.startsolid || dr.allsolid || dr.fraction >= 1.0f )
                continue;   /* no clearance / no floor within reach → skip */
            vec3_t cand = { probe[0], probe[1], dr.endpos[2] };
            /* The collision floor may extend PAST the NAV floor edge (the nav mesh is
               eroded ~one agent radius in from the real wall/lip).  The bot can only
               walk to the nav-reachable edge, so take the route's actual dead-end
               toward the candidate as the launch — a point the follower can truly
               stand on — not the raw collision point it cannot path onto. */
            vec3_t navLaunch;
            float gap = WiredIntel_PathFrontier( bs->origin, cand, navLaunch );
            if ( gap >= WI_NAV_PROGRESS_NONE ) continue;   /* no corridor toward it */
            /* Validate the ballistic arc from the NAV-reachable launch to the brush. */
            if ( !WiredIntel_JumpTouchArcHits( navLaunch, button->r.absmin, button->r.absmax, &speed ) )
                continue;
            VectorCopy( navLaunch, launch );
            found = qtrue;
        }
        if ( !found ) return qfalse;
    }

    /* Install the one-step jump-touch sequence: launch → run-jump → confirm door. */
    {
        botSequencedGoal_t *sg = &bs->sequencedGoal;
        WiredIntel_SequencedGoalReset( sg );
        bcen[0] = 0.5f*(button->r.absmin[0]+button->r.absmax[0]);
        bcen[1] = 0.5f*(button->r.absmin[1]+button->r.absmax[1]);
        bcen[2] = 0.5f*(button->r.absmin[2]+button->r.absmax[2]);
        (void)speedDir;
        sg->steps[0].type            = SEQ_STEP_JUMP_TOUCH;
        VectorCopy( launch, sg->steps[0].point );
        VectorCopy( bcen,   sg->steps[0].target );
        VectorCopy( button->r.absmin, sg->steps[0].buttonMins );
        VectorCopy( button->r.absmax, sg->steps[0].buttonMaxs );
        sg->steps[0].jumpSpeed       = speed;
        sg->steps[0].moverEntitynum  = (int)( door - g_entities );
        sg->steps[0].doorClosedState = (int)door->moverState;   /* rest = closed */
        sg->stepCount  = 1;

        /* POLICY: an actuation sequence completes PAST the gate it actuated, not
           at the actuator.
           Pressing the button is not the goal; getting through the door the button
           opens is. Ending at the actuator leaves the bot standing beside an open
           door with no reason to use it: the lock releases, ordinary goal selection
           resumes, the leaf auto-closes unused, the satisfied bit is correctly
           retired because the route still crosses it, and the same gate is
           detected and actuated again — measured on the e1m1 specimen as four
           actuations of one door with the bot never closer than 395u while it
           stood open.
           This generalises the RIDE sequence, which already has the right shape:
           GoTo(board) -> WAIT_MOVERSTATE(car) -> GoTo(stepOff). It ends beyond the
           obstacle it used. Both step types and the { GoTo, WAIT_MOVERSTATE, GoTo }
           composition already ship and are covered by the substrate's self-tests,
           including the stall-abandon path — so a door that never opens cannot
           wedge the sequence, it is given up exactly as a wedged elevator is.
           The far-side point is DERIVED from the mesh's own corridor (see
           WiredIntel_DeriveGateFarSide), never authored. When no far side can be
           derived the traverse steps are simply not appended and the sequence
           behaves exactly as it did before. */
#if FEAT_RECAST_NAVMESH
        {
            /* Asked against the goal the bot is actually pursuing — the same goal
               the satisfied-memory's own route test uses, so "past the gate" means
               past it with respect to the route that made it a gate. */
            vec3_t farSide;
            if ( WiredIntel_DeriveGateFarSide( door, bs->origin,
                                               bs->teamgoal.origin, farSide ) ) {
                sg->steps[1].type           = SEQ_STEP_WAIT_MOVERSTATE;
                sg->steps[1].moverEntitynum = (int)( door - g_entities );
                sg->steps[2].type           = SEQ_STEP_GOTO;
                VectorCopy( farSide, sg->steps[2].point );
                sg->steps[2].range          = 0.0f;   /* substrate default */
                sg->stepCount               = 3;
            }
        }
#endif
        sg->cursor     = 0;
        sg->changeTime = level.time;
        sg->sequenceId = WI_JUMPTOUCH_SEQUENCE_ID;
        sg->active     = qtrue;
        /* Lock the directive from THIS frame so the bot does not enter Battle_Fight
           before the sequence's own driver runs next frame: AINode_Seek_LTG routes a
           directive-locked bot to defensive-combat-only (it keeps pursuing the goal
           and never leaves for a full fight), which is what keeps the SEQ authority
           reachable.  Without this, an enemy found on the install frame sends the bot
           to Battle_Fight, where BotGetLongTermGoal (the authority gate) is never
           called and the jump-touch goal is abandoned. */
        bs->ordered                    = qtrue;
        bs->directives.directiveLocked = qtrue;
    }

    if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
        Com_Log( SEV_INFO, LOG_CH(ch_botai),
            "cl=%d jump-touch INSTALLED: launch=(%.0f %.0f %.0f) speed=%.0f button=%d door=%d\n",
            bs->client, launch[0], launch[1], launch[2], speed,
            (int)( button - g_entities ), (int)( door - g_entities ) );
    }
    return qtrue;
}

#define WI_MAX_GATE_CANDIDATES 64
static qboolean WiredIntel_FindBlockingGateButtons( bot_state_t *bs, const vec3_t goalOrigin ) {
    botDirectiveState_t *ds = &bs->directives;
    int   i, nCand = 0;
    int   cand[WI_MAX_GATE_CANDIDATES];
    float candDistSq[WI_MAX_GATE_CANDIDATES];
    vec3_t toGoal;

    VectorSubtract( goalOrigin, bs->origin, toGoal );
    if ( VectorLength( toGoal ) < 1.0f ) {
        return qfalse;   /* already at the goal — no gate to open */
    }

#if FEAT_RECAST_NAVMESH
    if ( !trap_Nav_IsReady() ) {
        return qfalse;   /* navmesh still baking — no reliable probe */
    }
#endif

    /* Refresh the gate-satisfied memory before collecting candidates: set the bit on
       any gate that has left its closed rest position, and retire it on any that is
       shut again AND once more cutting the route. Run here, at the detection tick, so
       the memory is always as fresh as the decision it feeds. */
    WiredIntel_GateMemoryFrame( bs, goalOrigin );

    /* Collect closed target-opened gates and their squared distance to the bot.
       Far more closed doors than the cap on one map is implausible; if it happens,
       the nearest WI_MAX_GATE_CANDIDATES are considered (the bot's own gate is
       near it, so it is in this set). */
    for ( i = MAX_CLIENTS; i < level.num_entities && nCand < WI_MAX_GATE_CANDIDATES; i++ ) {
        gentity_t *ent = &g_entities[i];
        vec3_t doorCenter, toDoor;
        if ( !WiredIntel_DoorIsClosedGate( ent ) ) continue;
        /* Already satisfied: this gate was opened by the agent's own action and has
           not been shown to re-cut the route since. Do not offer it as a candidate —
           re-detecting it here is exactly how the bot spends its open-window walking
           back to a button it already pressed. The bit is retired the moment the gate
           genuinely blocks again (WiredIntel_GateMemoryFrame). */
        if ( s_gateSatisfied[i] ) continue;
        VectorAdd( ent->r.absmin, ent->r.absmax, doorCenter );
        VectorScale( doorCenter, 0.5f, doorCenter );
        VectorSubtract( doorCenter, bs->origin, toDoor );
        cand[nCand]       = i;
        candDistSq[nCand] = VectorLengthSquared( toDoor );
        nCand++;
    }
    if ( nCand == 0 ) {
        return qfalse;   /* no closed gate exists — do not activate (off-mesh goal, etc.) */
    }

    /* 🔴 ROUTE-CONTENTS SHORT-CIRCUIT (replaces the route-FAILURE short-circuit).
       The old form was `if (PathReaches(...)) return qfalse` — "a route exists, so
       nothing blocks me".  That conflates reachability with passability: the query
       filter excludes only NAVPOLY_BLOCKED, so a closed leaf the bake never marked
       blocked yields a COMPLETE corridor straight through solid.  The bot walked that
       corridor into the door and concluded there was no gate to open — and the better
       the mesh got, the more often the corridor completed, so detection got BLINDER as
       the mesh improved.

       The replacement asks what the route CONTAINS.  Bail out only when BOTH hold:
       the corridor reaches the goal AND it crosses none of the closed gates.  A
       complete corridor that crosses a closed gate now falls through to detection —
       which is the entire behavioural change of this landing.  On a genuinely ungated
       route (arena maps, and any map once its gates are open) the loop finds no
       crossing and the early return fires exactly as before, so those paths are
       unchanged. */
    {
        qboolean reaches = qfalse, crossesAny = qfalse;
        for ( i = 0; i < nCand && !crossesAny; i++ ) {
            qboolean thisReaches = qfalse;
            if ( WiredIntel_RouteCrossesDoor( &g_entities[cand[i]], bs->origin, goalOrigin,
                                              &thisReaches ) ) {
                crossesAny = qtrue;
            }
            if ( thisReaches ) {
                reaches = qtrue;
            }
        }
        if ( reaches && !crossesAny ) {
            return qfalse;   /* route complete AND passes through no closed gate */
        }
    }

    /* PARTIAL-route gate selection (the cost-diverted multi-gate case).  When the
       route to the goal is only a PARTIAL corridor — findpath dead-ended before the
       goal because the cheaper decoy (a dead-end plat / lift the door cost made
       preferable) won A* — the normal per-door relevance test cannot find the on-route
       gates: the corridor dead-ends at the decoy, not at any door.  Reveal the genuine
       route by neutralising every closed gate at once and take the first door on it.
       Force that door as the sole candidate; it is confirmed on-route by construction,
       so pass 0's DoorCutsRoute (which cannot see it) is bypassed for it.  On a single-
       gate map the exit probe is not PARTIAL (the one gate's own crossing is the cut,
       not a decoy), so this branch is inert there and the normal path runs. */
    qboolean partialForced = qfalse;
#if FEAT_RECAST_NAVMESH
    {
        static navPath_t s_partialProbe;   /* file-static via fn-static: navPath_t is large */
        int pn = trap_Nav_FindPath( (vec_t *)bs->origin, (vec_t *)goalOrigin,
                                    WI_NAV_AGENT_PLAYER, &s_partialProbe );
        if ( ( pn & NAV_FINDPATH_PARTIAL ) != 0 ) {
            int firstGate = WiredIntel_FindFirstOnRouteGate( bs, goalOrigin, cand, nCand );
            if ( firstGate >= 0 ) {
                cand[0]       = firstGate;
                candDistSq[0] = 0.0f;
                nCand         = 1;
                partialForced = qtrue;
                if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
                    Com_Log( SEV_INFO, LOG_CH(ch_botai),
                        "cl=%d PARTIAL exit route -> first on-route gate door=%d\n",
                        bs->client, firstGate );
                }
            }
        }
    }
#endif

    /* Route-relevance pass. Prefer a door that the nav layer confirms is on the route
       (WiredIntel_DoorCutsRoute), taking the nearest such door. This is the fix for the
       compositional false-positive: a spurious "blocked" plus a nearby UNRELATED closed
       door must not send the bot to the wrong button. But the relevance test can only
       decide when the door's nav polys carry the block: on a map whose doors the mesh
       does not poly-tag (e.g. a Q1 level tagged by a different key), opening a door in
       the mesh changes nothing and no door can be confirmed on-route. In that case the
       nav layer cannot attribute the cut to any door, so we fall back to the prior
       nearest-pressable heuristic rather than refusing to open any gate — the block is
       real (the reach probe already failed), and the nearest reachable gate is the best
       available pick. Two passes over the candidate set (a handful of doors): first
       nav-confirmed-relevant, then any pressable. */
    {
        int pass;
        for ( pass = 0; pass < 2; pass++ ) {
            int consumed[WI_MAX_GATE_CANDIDATES];
            int s;
            for ( s = 0; s < nCand; s++ ) consumed[s] = 0;

            for ( ;; ) {
                int   bestSlot = -1;
                float bestDistSq = 0.0f;
                gentity_t *door;

                for ( s = 0; s < nCand; s++ ) {
                    if ( cand[s] < 0 || consumed[s] ) continue;
                    if ( bestSlot < 0 || candDistSq[s] < bestDistSq ) {
                        bestSlot   = s;
                        bestDistSq = candDistSq[s];
                    }
                }
                if ( bestSlot < 0 ) {
                    break;   /* this pass exhausted its candidates */
                }
                consumed[bestSlot] = 1;
                door = &g_entities[cand[bestSlot]];

                /* Pass 0 keeps only doors the nav layer confirms cut the route; pass 1
                   accepts any door (the fallback for maps where nav cannot attribute
                   the block to a specific door).  A door already selected as the first
                   on-route gate of a PARTIAL route (partialForced) is on-route by
                   construction — its relevance was proven by the neutralised-route
                   probe, which DoorCutsRoute cannot reproduce (the closed corridor
                   dead-ends at the decoy) — so skip the DoorCutsRoute gate for it. */
                if ( pass == 0 && !partialForced &&
                     !WiredIntel_DoorCutsRoute( door, bs->origin, goalOrigin ) ) {
                    continue;   /* not confirmed on-route — try the next nearest */
                }

                /* Activation-goal divert (2a): a door whose crossing is ALREADY
                   traversable in its current state — the reachable corridor has gone
                   PAST it without opening it (e1m1's hatch: the bot drops through the
                   closed leaf; its OR-gate button is moot for getting past) — must not
                   arm a button sub-goal.  Latching an opener the bot may not even be
                   able to reach (an unreachable wall plate) freezes it at the frontier
                   forever; the already-traversable route is the answer.  This single
                   guard covers both passes, so the fallback pass no longer commits a
                   gate the nav layer has effectively shown to be behind the bot's
                   reach.

                   EXCEPTION — a PARTIAL-forced gate.  The "already traversable" the
                   guard sees is the bot passing this door via the DECOY branch (the
                   cheap dead-end that made the exit a PARTIAL corridor), not via the
                   route to the goal.  The neutralised-route probe has already proven
                   this is the first door on the GENUINE route — so its closed-state
                   pass-through is exactly the decoy this whole branch exists to reject.
                   Opening it (not dropping through it) is what puts the bot on the real
                   route, so do not let the decoy pass-through veto the commit. */
                if ( !partialForced &&
                     WiredIntel_DoorCrossingAlreadyTraversable( door, bs->origin, goalOrigin ) ) {
                    continue;   /* route already crosses this door — do not open it */
                }

                /* Resolve this gate's pressable buttons. Reset the queue first so a
                   re-detection rebuilds it cleanly. Chain depth 4 covers
                   door<-counter<-button and door<-relay<-...<-button with margin. The
                   queued buttons are belief-store interactable records; the cursor is
                   reset here. */
                Belief_InteractablesClear( bs );
                ds->activateIndex       = 0;
                ds->activateDoor        = -1;
                ds->activateByJumpTouch = qfalse;
                WiredIntel_QueueButtonsForTarget( bs, door->targetname, 4 );
                /* Instrument only: record how many buttons this gate resolved to.
                 * Reads the count the line above already produced; changes nothing. */
                WiredIntel_ExitChainCount( 0 );

                if ( Belief_InteractableCount( bs ) <= 0 ) {
                    ds->activateIndex       = 0;
                    ds->activateDoor        = -1;
                    ds->activateByJumpTouch = qfalse;
                    continue;   /* no pressable button for this gate — try the next nearest */
                }

                /* Reachability guard: every queued button must be pathable under the
                   CURRENT (closed-door) navmesh, or this gate's chain cannot be opened
                   by the flat single-goal model — its button sits behind the very door
                   it opens (that door's NAVPOLY_BLOCKED threshold cuts the route to it).
                   Committing anyway latches the queue forever: the door never opens
                   (button unpressed) so the queue never retires, and the bot mills at the
                   corridor frontier. Skip such a gate here so the queue stays empty and
                   the caller falls through to the exit goal.

                   The probe target is each button's APPROACH ORIGIN — the walkable poly
                   WiredIntel_EntityNavOrigin already snapped the bbox center down onto —
                   NOT the raw button brush center. An earlier per-button probe against
                   the raw center was removed because it false-rejected reachable buttons:
                   the center sits on/inside the button brush and a tight 3D reach epsilon
                   failed even when the bot could walk there. This one reuses
                   WiredIntel_PathReaches, whose horizontal-only + Z-slack tolerance
                   (WI_NAV_REACH_EPS / WI_NAV_REACH_Z_SLACK) is built for exactly that
                   floor-snap. A reachable button yields a full corridor ending at its
                   approach origin within tolerance (commit, works as today); an
                   unreachable one yields a partial/short corridor that dead-ends at the
                   blocking door — the same signal that made the GATE detectable — so the
                   guard distinguishes commit from fall-through without the old brush-snap
                   false-negative. */
                {
                    int  bi, nbtn;
                    int  walkIdx = -1;             /* first WALK-pressable button's index */
                    qboolean allReach = qtrue;
                    qboolean jumpTouchArmed = qfalse;
                    nbtn = Belief_InteractableCount( bs );

                    /* A func_door opens on ANY of its buttons (SUB_UseTargets fires the whole
                       target set), so this is an OR-gate: ONE button the bot can WALK to and
                       press is enough.  Prefer walking — scan for the first WALK-PRESSABLE
                       button: a reachable stand point WITHIN touch range of the plate (the
                       goal-snap resolver returns the real walk-in spot on the button's face;
                       StandPointTouchesButton confirms the standing box there overlaps the
                       brush, so walking onto it fires Q1_Button_Touch).  This is the canonical
                       press for e1m1's elevator controls — a walk, not a jump. */
                    /* POSE-ROBUSTNESS. Every reach test below queries from bs->origin,
                       so a pose on an isolated mesh fragment answers "unreachable" for
                       every actuator at once — a fact about where the agent happens to
                       stand, not about the gate. Detect that structurally ONCE for this
                       scan and, when it holds, DECLINE TO DECIDE: leave the queue as
                       detection built it and let a later tick — from a pose the mesh can
                       route from — return the verdict. Deliberately not defaulting to
                       "reachable": that would resurrect the false-positive class the
                       route-relevance pass exists to kill. */
                    qboolean poseDegenerate = qfalse;
                    if ( nbtn > 0 ) {
                        const beliefInteractable_t *ir0 = Belief_Interactable( bs, 0 );
                        if ( ir0 && WiredIntel_OriginIsDegenerate( bs->origin, ir0->origin ) ) {
                            poseDegenerate = qtrue;
                            if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
                                static float s_degenLog[MAX_CLIENTS];
                                if ( FloatTime() - s_degenLog[bs->client] > 2.0f ) {
                                    s_degenLog[bs->client] = FloatTime();
                                    Com_Log( SEV_INFO, LOG_CH(ch_botai),
                                        "cl=%d gate door=%d reach test DEFERRED — origin "
                                        "(%.0f %.0f %.0f) is on an isolated mesh fragment\n",
                                        bs->client, (int)( door - g_entities ),
                                        bs->origin[0], bs->origin[1], bs->origin[2] );
                                }
                            }
                        }
                    }
                    for ( bi = 0; !poseDegenerate && bi < nbtn; bi++ ) {
                        const beliefInteractable_t *ir = Belief_Interactable( bs, bi );
                        gentity_t *btn = ( ir && ir->entitynum >= 0 && ir->entitynum < level.num_entities )
                                       ? &g_entities[ir->entitynum] : NULL;
                        if ( !ir ) continue;
                        /* A TOUCH actuator is walk-actuated BY DEFINITION: the volume
                           fires when the player bbox enters it, so arriving at its
                           snapped approach point IS the press. The remaining button
                           predicates do not apply to it — StandPointTouchesButton asks
                           whether the standing box overlaps a BUTTON BRUSH, and
                           ButtonBelowClosedLeaf guards a button tucked under the very
                           leaf it opens, which is a button-placement hazard. Both would
                           reject a trigger volume for failing to be a button. */
                        if ( ir->kind == BELIEF_ACT_TOUCH ) {
                            if ( WiredIntel_PathReachesThroughGate( door, bs->origin, ir->origin ) ) {
                                walkIdx = bi;
                                break;
                            }
                            /* A TOUCH actuator's target is the VOLUME, not a point.
                               The trigger fires wherever the player bbox enters its
                               AABB, so a single snapped approach point that happens
                               to be unroutable does not make the actuator unusable —
                               it makes that one point a bad representative of it.
                               Re-resolve against the volume: probe points spread
                               across the AABB and take the first the agent can
                               actually route to. Sampling is derived from the
                               entity's own extents (corners, edge midpoints and the
                               centre of its footprint at its own mid-height), so it
                               is scale-free and introduces no coordinate or tuning
                               value. Button actuators are untouched: their approach
                               point is a real touch spot on a brush face, not a
                               region, and StandPointTouchesButton still governs them. */
                            if ( btn ) {
                                static const float sx[9] = { 0.5f, 0.1f, 0.9f, 0.1f, 0.9f, 0.5f, 0.5f, 0.1f, 0.9f };
                                static const float sy[9] = { 0.5f, 0.1f, 0.1f, 0.9f, 0.9f, 0.1f, 0.9f, 0.5f, 0.5f };
                                int si;
                                for ( si = 0; si < 9; si++ ) {
                                    vec3_t cand;
                                    cand[0] = btn->r.absmin[0] + sx[si] * ( btn->r.absmax[0] - btn->r.absmin[0] );
                                    cand[1] = btn->r.absmin[1] + sy[si] * ( btn->r.absmax[1] - btn->r.absmin[1] );
                                    cand[2] = 0.5f * ( btn->r.absmin[2] + btn->r.absmax[2] );
                                    if ( WiredIntel_PathReachesThroughGate( door, bs->origin, cand ) ) {
                                        Belief_InteractableSetOrigin( bs, bi, cand );
                                        walkIdx = bi;
                                        break;
                                    }
                                }
                                if ( walkIdx >= 0 ) break;
                            }
                            continue;
                        }
                        /* Same open-state question for a BUTTON actuator: a plate whose
                           approach lies beyond the leaf it opens is the same class of gate
                           as a trigger volume beyond it.  The two remaining predicates are
                           unchanged and still govern: ButtonBelowClosedLeaf and
                           StandPointTouchesButton ask about GEOMETRY, not routing, so the
                           navmesh state does not enter them. */
                        /* ACC5PROBE-BEGIN — TEMPORARY. Names WHICH predicate rejects a
                           button actuator, so "still rejected" can be stated by mechanism
                           rather than by outcome. Read-only: it re-evaluates the same three
                           predicates the line below evaluates, in the same tick; each is
                           side-effect-free with respect to control flow, and the reach test
                           restores its own neutralisation internally. */
                        if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
                            static float s_a5Log;
                            if ( FloatTime() - s_a5Log > 0.5f ) {
                                qboolean pr = WiredIntel_PathReachesThroughGate( door, bs->origin, ir->origin );
                                qboolean bc = WiredIntel_ButtonBelowClosedLeaf( door, ir->origin );
                                qboolean st = WiredIntel_StandPointTouchesButton( ir->origin, btn );
                                s_a5Log = FloatTime();
                                Com_Log( SEV_INFO, LOG_CH(ch_botai),
                                    "ACC5PROBE door=%d btn=%d reachThroughGate=%d belowClosedLeaf=%d "
                                    "standTouches=%d rejectedBy=%s\n",
                                    (int)( door - g_entities ), bi, pr ? 1 : 0, bc ? 1 : 0, st ? 1 : 0,
                                    !pr ? "PathReachesThroughGate"
                                        : ( bc ? "ButtonBelowClosedLeaf"
                                               : ( !st ? "StandPointTouchesButton" : "(none-accepted)" ) ) );
                            }
                        }
                        /* ACC5PROBE-END */
                        if ( WiredIntel_PathReachesThroughGate( door, bs->origin, ir->origin )
                                && !WiredIntel_ButtonBelowClosedLeaf( door, ir->origin )
                                && WiredIntel_StandPointTouchesButton( ir->origin, btn ) ) {
                            walkIdx = bi;
                            break;
                        }
                    }
                    if ( poseDegenerate ) {
                        /* NO VERDICT this tick. The scan above was skipped, so walkIdx
                           carries no information — the gate is neither confirmed nor
                           refused. Commit nothing (falling through to the stamp below
                           would COMMIT it on no evidence) and clear nothing (clearing IS
                           the refusal outcome, and it would let the pass-1 fallback pick a
                           nearer wrong door on the strength of a pose artifact). Return
                           outright: the rate-limited detector re-asks on a later tick from
                           a pose the mesh can route from, and that tick decides. */
                        return qfalse;
                    } else if ( walkIdx >= 0 ) {
                        /* Commit the gate on the walk-pressable button; DRIVE to it (skip any
                           earlier unreachable OR-gate partner, e.g. a top-side plate whose
                           counterpart control is walk-reachable).  NO jump-touch — the button
                           is walk-pressed. */
                        ds->activateIndex = walkIdx;
                        ds->activateByJumpTouch = qfalse;   /* plan is a walk-press */
                        allReach = qtrue;
                    } else {
                        /* No WALK-pressable button on this gate.  Fall back to the jump-touch
                           toolbox — reserved for genuine jump-to-press buttons (e.g. a secret),
                           never the primary path when a walk-press exists.  One jump-touch per
                           detection; the first feasible button wins. */
                        for ( bi = 0; bi < nbtn; bi++ ) {
                            const beliefInteractable_t *ir = Belief_Interactable( bs, bi );
                            gentity_t *btn = ( ir && ir->entitynum >= 0 && ir->entitynum < level.num_entities )
                                           ? &g_entities[ir->entitynum] : NULL;
                            if ( !jumpTouchArmed && ir && btn
                                 && WiredIntel_TryInstallJumpTouch( bs, btn, door, ir->origin ) ) {
                                jumpTouchArmed = qtrue;
                                /* This gate's actuation plan is the SEQ, not a walk.  If the
                                   SEQ later dies, the queue must retire with it. */
                                ds->activateByJumpTouch = qtrue;
                                ds->activateIndex       = bi;
                                break;
                            }
                        }
                        allReach = qfalse;
                    }
                    if ( !allReach && !jumpTouchArmed ) {
                        /* No button walk-reachable and none jump-touchable — do NOT
                           latch.  Clear the queue so no activation goal is committed and
                           try the next candidate gate.  (Unreachable here is a real
                           refusal: a degenerate pose already returned above without
                           reaching this point.) */
                        Belief_InteractablesClear( bs );
                        ds->activateIndex       = 0;
                        ds->activateDoor        = -1;
                        ds->activateByJumpTouch = qfalse;
                        if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
                            vec3_t dcen; VectorAdd( door->r.absmin, door->r.absmax, dcen ); VectorScale( dcen, 0.5f, dcen );
                            Com_Log( SEV_INFO, LOG_CH(ch_botai),
                                "cl=%d gate door=%d cls=%s tn=%s model=%s cen=(%.0f %.0f %.0f) button unreachable under closed navmesh"
                                " — not committing, falling through\n",
                                bs->client, (int)( door - g_entities ),
                                door->classname ? door->classname : "?",
                                door->targetname ? door->targetname : "(none)",
                                door->model ? door->model : "(none)",
                                dcen[0], dcen[1], dcen[2] );
                        }
                        continue;   /* try the next nearest gate */
                    }
                }

                /* Nearest gate (route-confirmed in pass 0, or the nearest pressable in
                   the fallback pass) whose button chain is pressable AND reachable now.
                   Stamp the gate onto the belief interactable records so each button
                   knows which door it opens. */
                ds->activateDoor = (int)( door - g_entities );
                Belief_InteractablesSetDoor( bs, ds->activateDoor );
                return qtrue;
            }
        }
    }

    return qfalse;   /* no gate with a reachable, pressable button */
}

/*
==================
WiredIntel_TrySetActivationGoal
==================
If the bot has an active button-activation queue (or can detect a blocking gate
now), drive it onto the current queued button with an LTG_GETITEM goal (walk the
bbox onto the brush → Q1_Button_Touch fires) and return qtrue. The press is
confirmed from the button's own mover state, then the queue advances; when the
gate's door leaves its closed state the queue is retired. Returns qfalse when
there is nothing to activate, so the caller falls through to the exit goal.

Priority sits ABOVE the exit goal (it opens the gate that blocks the route to the
exit). Combat does NOT preempt it: this runs on the DIR_GOTO_EXIT path, which is
directive-locked, so a visible enemy is fought defensively (aim+fire) while the
bot keeps advancing onto the button rather than abandoning the goal.

'goalOrigin' is where the bot is otherwise trying to go (the exit, typically) —
used to decide whether a detected door is actually in the way.
*/
qboolean WiredIntel_TrySetActivationGoal( bot_state_t *bs, const vec3_t goalOrigin ) {
    botDirectiveState_t *ds;
    gentity_t *door;
    float *buttonOrigin;
    vec3_t delta;

    if ( !BotCanActivate( bs ) ) {
        return qfalse;
    }
    ds = &bs->directives;

    /* A jump-touch already in flight owns the goal — the SEQ driver steers the
       run-jump and confirms the press; do NOT also stamp an LTG_GETITEM walk-on
       goal that would fight it.  Report the gate as being handled. */
    if ( bs->sequencedGoal.active && bs->sequencedGoal.sequenceId == WI_JUMPTOUCH_SEQUENCE_ID ) {
        return qtrue;
    }

    /* ── retire a finished/stale queue ────────────────────────────────────
       CLASS RULE: a plan's SUCCESS condition must be evaluated BEFORE its
       failure/abort condition, whenever both can be true on the same think.
       Otherwise the abort branch consumes the state the success branch reads and
       the outcome is recorded as a failure that in fact succeeded — the success is
       not merely mis-logged, it is never observed at all.

       That is exactly the race this ordering fixes.  A jump-touch press and the
       SEQ's own teardown complete on the SAME actuation: the button is touched at
       the apex, the SEQ has delivered its payload and clears itself, and the
       button's mover then fires the door on reaching pos2 a few frames later.  So
       on the very think where the door has just started opening, BOTH conditions
       hold — "the SEQ is gone" (the abort condition) and "the door is no longer a
       closed gate" (the success condition).  With the abort tested first it won
       every time: it cleared activateDoor to -1, and the success test below, which
       reads activateDoor to find the door, then had nothing left to look at.  The
       gate was re-detected on the next tick and the bot pressed the same button
       again, forever — measured as 6 presses that each really opened the door,
       with door-opened=0 because the observation was consumed before it was made.

       Testing success first is the general form: an opened door retires the queue
       as DONE regardless of what happened to the actuation plan, and the plan-died
       branch below only sees queues whose door is genuinely still shut. */
    if ( Belief_InteractableCount( bs ) > 0 ) {
        door = ( ds->activateDoor >= 0 && ds->activateDoor < level.num_entities )
             ? &g_entities[ds->activateDoor] : NULL;
        /* The door opened (chain fired) — the gate is cleared, drop the queue. */
        if ( !door || !WiredIntel_DoorIsClosedGate( door ) ) {
            if ( door && trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
                Com_Log( SEV_INFO, LOG_CH(ch_botai),
                    "cl=%d gate door=%d opened (moverState=%d) — activation done\n",
                    bs->client, ds->activateDoor, door->moverState );
            }
            Belief_InteractablesClear( bs );
            ds->activateIndex       = 0;
            ds->activateDoor        = -1;
            ds->activateByJumpTouch = qfalse;
        }
    }

    /* ── retire a queue whose ACTUATION PLAN died ────────────────────────
       CLASS RULE: when the actuation plan for a gate dies, that gate's activation
       queue must retire, so the solver can re-arm rather than chase a dead goal.

       A jump-touch queue is actuated ONLY by its SEQ.  Every SEQ abort path (the
       progress watchdog, exhausted launch attempts, an inferred mover stall) clears
       the sequence from inside the driver, which has no reach into the activation
       queue.  So the queue outlived its own plan: it stayed committed to the gate,
       kept stamping the walk-on goal at the button's resolved approach spot, and
       that spot is by construction NOT walk-pressable (it is why a jump-touch was
       armed instead).  The bot then chases a goal that can never fire the button,
       for the rest of the life, and the gate is never re-attempted.

       Retiring here closes the loop: the queue drops, detection is free to run
       again on the next rate-limit tick, and the solver re-arms — either with a
       fresh jump-touch or with whatever the world now affords.  Retirement is not a
       failure verdict on the gate, only on this attempt at it.

       Runs AFTER the opened-check above so a SUCCESSFUL jump-touch — whose SEQ is
       also gone, because delivering the press is how it ends — is retired as done
       rather than as a dead plan (see the class rule there). */
    if ( Belief_InteractableCount( bs ) > 0 && ds->activateByJumpTouch
         && !( bs->sequencedGoal.active
               && bs->sequencedGoal.sequenceId == WI_JUMPTOUCH_SEQUENCE_ID ) ) {
        if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
            Com_Log( SEV_INFO, LOG_CH(ch_botai),
                "cl=%d gate door=%d jump-touch plan died — retiring queue, re-arming\n",
                bs->client, ds->activateDoor );
        }
        Belief_InteractablesClear( bs );
        ds->activateIndex       = 0;
        ds->activateDoor        = -1;
        ds->activateByJumpTouch = qfalse;
    }

    /* ── detect a new gate when idle ────────────────────────────────────── */
    if ( Belief_InteractableCount( bs ) <= 0 ) {
        /* Rate-limit detection so a map with no gate (or an unreachable one) does
           not rescan every think. */
        if ( FloatTime() < ds->activateRetryTime ) {
            return qfalse;
        }
        ds->activateRetryTime = FloatTime() + 1.0f;
        if ( !WiredIntel_FindBlockingGateButtons( bs, goalOrigin ) ) {
            return qfalse;
        }
        if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
            Com_Log( SEV_INFO, LOG_CH(ch_botai),
                "cl=%d gate door=%d -> %d button(s) queued\n",
                bs->client, ds->activateDoor, Belief_InteractableCount( bs ) );
        }
        /* If the gate was committed by ARMING A JUMP-TOUCH (a beyond-reach wall
           plate — its button is not walk-pressable, so FindBlockingGateButtons
           installed a jump-touch SEQ for it), the SEQ owns the goal from this frame
           on.  Yield NOW — do NOT fall through to stamp an LTG_GETITEM walk-on goal
           for the same button: the walk goal (the resolved approach spot, ~85u out
           from the recessed plate) is unreachable-to-press and fights the SEQ's
           drive to the launch foot, milling the bot until the SEQ watchdog aborts.
           This mirrors the SEQ-yield at the top of the function, applied on the
           install frame. */
        if ( bs->sequencedGoal.active && bs->sequencedGoal.sequenceId == WI_JUMPTOUCH_SEQUENCE_ID ) {
            return qtrue;
        }
    }

    /* ── advance past buttons that have actually fired ──────────────────────
       A press is confirmed from the button's own state, not from proximity: a
       Q1 button leaves MOVER_POS1 the instant Q1_Button_Touch drives its mover
       (or it is already mid-cycle from a prior press), so a queued button no
       longer at rest has fired. Proximity alone would falsely report a press the
       bot never made. Skip a button whose entity slot is stale (freed/reused). */
    while ( ds->activateIndex < Belief_InteractableCount( bs ) ) {
        const beliefInteractable_t *ir = Belief_Interactable( bs, ds->activateIndex );
        int       bent = ir ? ir->entitynum : -1;
        gentity_t *button = ( bent >= 0 && bent < level.num_entities )
                          ? &g_entities[bent] : NULL;
        qboolean fired = qfalse;
        if ( ir && ir->kind == BELIEF_ACT_TOUCH ) {
            /* A TOUCH actuator has no mover of its own, so "did it fire" cannot be
               read from the actuator. Read it from the GATE instead: the trigger's
               whole effect is to drive the door, so a door off its closed rest
               position is the actuation confirmed at the only place it is visible.
               A trigger_once also frees itself once fired, so a stale/absent slot
               is equally a completion — the same reasoning the button arm uses. */
            gentity_t *gate = ( ds->activateDoor >= 0 && ds->activateDoor < level.num_entities )
                            ? &g_entities[ds->activateDoor] : NULL;
            if ( !button || !button->inuse ) {
                fired = qtrue;   /* trigger consumed itself (trigger_once) — done */
            } else if ( gate && gate->moverState != MOVER_POS1 ) {
                fired = qtrue;   /* the gate left its closed rest → the trigger fired */
            }
        } else if ( !button || !button->inuse || !G_ClassnameIs( button, "func_button" ) ) {
            fired = qtrue;   /* button gone (chain rebuilt/freed) — treat as done */
        } else if ( button->moverState != MOVER_POS1 ) {
            fired = qtrue;   /* left its rest position → the touch fired the mover */
        }
        if ( fired ) {
            WiredIntel_ExitChainCount( 1 );   /* instrument only: a queued button fired */
            if ( button && trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
                Com_Log( SEV_INFO, LOG_CH(ch_botai),
                    "cl=%d button %d/%d fired (ent=%d moverState=%d)\n",
                    bs->client, ds->activateIndex + 1, Belief_InteractableCount( bs ),
                    bent, button->moverState );
            }
            ds->activateIndex++;
        } else {
            break;
        }
    }

    /* All queued buttons fired but the door hasn't registered open yet — the
       counter may need its last tick, or the door mover is starting. Hold: the
       retire check above clears the queue once the door leaves its closed state.
       Fall through to normal AI meanwhile (no more buttons left to walk to). */
    if ( ds->activateIndex >= Belief_InteractableCount( bs ) ) {
        return qfalse;
    }

    /* ── drive onto the current button (LTG_GETITEM: walk the bbox onto it) ──
       Build a synthetic GFL_ITEM goal AT the button, sized to the button's own
       collision extents, and select LTG_GETITEM. That goal type copies teamgoal
       to the movement goal every frame with NO defend-style back-off, so the bot
       walks its bbox literally onto the button brush — the client-bbox overlap
       that G_TouchTriggers needs to dispatch Q1_Button_Touch. Arrival is the same
       AABB overlap (BotReachedGoal → GFL_ITEM → trap_BotTouchingGoal against the
       goal box), so sizing the box to the button's real mins/maxs makes "reached"
       coincide with the region that fires the button. Under Recast the follower
       routes by goal.origin and ignores areanum (always 0), so BotPointAreaNum is
       only a harmless cache key. */
    {
        const beliefInteractable_t *ir = Belief_Interactable( bs, ds->activateIndex );
        int       bent   = ir ? ir->entitynum : -1;
        gentity_t *button = ( bent >= 0 && bent < level.num_entities )
                          ? &g_entities[bent] : NULL;
        bot_goal_t goal;

        buttonOrigin = ir ? (float *)ir->origin : bs->origin;

        /* ── HELD-GOAL REACHABILITY RETIRE ────────────────────────────────────
           CLASS RULE: an activation goal whose target has become unreachable from
           the agent's CURRENT position must be abandoned, so the agent is not held
           to a goal it can no longer act on.

           The queue above advances only when a button FIRES and retires only when
           the door opens or a jump-touch plan dies.  None of those is reachability,
           so a bot that walks onto terrain from which the button is unroutable holds
           that goal for the rest of its life, re-pathing forever against a corridor
           Detour cannot supply.

           THE PREDICATE, and why it is persistence rather than a single query:
           a corridor to the target is DEGENERATE when Detour returns <=2 waypoints
           AND flags the result partial — origin and goal with nothing between them.
           A single degenerate result does NOT mean unreachable: a gate shut ahead on
           the route produces exactly the same reading, and that is the case this
           whole activation path exists to serve.  What separates them is PERSISTENCE.
           Measured on the e1m1 specimen (2026-07-31, one full run, 904 samples): the
           only degenerate episode the bot recovered from ran 4 consecutive
           evaluations; the terminal trap ran 465 and never recovered.  A 116x margin,
           with nothing in between.  Directional symmetry (does the reverse corridor
           reach?) was measured in the same run and does NOT separate them — the stall
           signature appears 119 times while travelling — so it is not used.

           The threshold is stated in SECONDS of wall time, not in samples, so it does
           not silently change meaning if the evaluation rate changes; it is derived
           from the existing goal-refresh period rather than introduced as a new
           tuning value.  Retiring is not a verdict on the gate: the queue drops, the
           rate-limited detector re-arms on the next tick, and the bot re-attempts the
           gate from wherever it then stands. */
        {
            static navPath_t s_reachProbe;
            int n = trap_Nav_FindPath( bs->origin, buttonOrigin,
                                       WI_NAV_AGENT_PLAYER, &s_reachProbe );
            qboolean degenerate = ( NAV_FINDPATH_COUNT( n ) <= 2 &&
                                    ( n & NAV_FINDPATH_PARTIAL ) ) ? qtrue : qfalse;
            if ( !degenerate ) {
                ds->activateUnreachSince = 0;
            } else {
                if ( !ds->activateUnreachSince )
                    ds->activateUnreachSince = level.time;
                /* Threshold = 2x the gate-detector's own re-arm period (the 1.0s
                 * activateRetryTime cadence below).  Two full detector cycles is the
                 * shortest window in which a transient block would have cleared and
                 * re-armed; measured, the only recovered episode lasted 4 samples and
                 * the terminal trap 465, so any threshold in that gap works and this
                 * one is stated from an identity already in the file rather than
                 * fitted to the specimen. */
                else if ( level.time - ds->activateUnreachSince >= 2000 ) {
                    if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
                        Com_Log( SEV_INFO, LOG_CH(ch_botai),
                            "cl=%d gate door=%d target unreachable for %dms — retiring queue, re-arming\n",
                            bs->client, ds->activateDoor,
                            level.time - ds->activateUnreachSince );
                    }
                    Belief_InteractablesClear( bs );
                    ds->activateIndex        = 0;
                    ds->activateDoor         = -1;
                    ds->activateByJumpTouch  = qfalse;
                    ds->activateUnreachSince = 0;
                    return qfalse;   /* fall through to the next goal rung */
                }
            }
        }

        memset( &goal, 0, sizeof( goal ) );
        VectorCopy( buttonOrigin, goal.origin );
        goal.areanum   = BotPointAreaNum( buttonOrigin );
        goal.entitynum = bent >= 0 ? bent : -1;
        goal.flags     = GFL_ITEM;
        if ( button ) {
            /* the button's real half-extents, so the arrival AABB coincides with
               the brush region G_TouchTriggers tests for the touch press */
            VectorCopy( button->r.mins, goal.mins );
            VectorCopy( button->r.maxs, goal.maxs );
        } else {
            VectorSet( goal.mins, -16, -16, -16 );
            VectorSet( goal.maxs,  16,  16,  16 );
        }
        bs->teamgoal     = goal;
        bs->ltgtype      = LTG_GETITEM;
        bs->teamgoal_time = FloatTime() + 1.0f;   /* refreshed each frame */
    }

    if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
        static float s_actLog[MAX_CLIENTS];
        if ( FloatTime() - s_actLog[bs->client] > 2.0f ) {
            const beliefInteractable_t *ir = Belief_Interactable( bs, ds->activateIndex );
            s_actLog[bs->client] = FloatTime();
            buttonOrigin = ir ? (float *)ir->origin : bs->origin;
            VectorSubtract( buttonOrigin, bs->origin, delta );
            Com_Log( SEV_INFO, LOG_CH(ch_botai),
                "cl=%d -> button %d/%d (%.0f %.0f %.0f) dist=%.0f\n",
                bs->client, ds->activateIndex + 1, Belief_InteractableCount( bs ),
                buttonOrigin[0], buttonOrigin[1], buttonOrigin[2],
                VectorLength( delta ) );
        }
    }
    return qtrue;
}

/*
==================
BotDirective_FrameUpdate
==================
Translates the active botDirective_t into bs->ltgtype / bs->teamgoal every
frame.  Called before BotDeathmatchAI(bs, thinktime) in ai_main.c.

Directives are authoritative; ltgtype is a derived cache used by the
existing AINode_Seek_LTG navigation engine in ai_dmnet.c.
*/
void BotDirective_FrameUpdate( bot_state_t *bs ) {
    botDirective_t *d  = &bs->directives.tactical;
    float           now = FloatTime();

    if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
        static float s_dirLogTime[MAX_CLIENTS];
        if ( FloatTime() - s_dirLogTime[bs->client] > 2.0f ) {
            s_dirLogTime[bs->client] = FloatTime();
            Com_Log( SEV_INFO, LOG_CH(ch_botai), "cl=%d type=%d target_cl=%d expire=%.1f\n",
                bs->client, d->type, d->target_client,
                d->expire_time > 0.0f ? d->expire_time - now : -1.0f );
        }
    }

    /* ── post-respawn baseline re-snapshot ──────────────────────────────── */
    if ( bs->directives.needsBaselineReset && bs->cur_ps.pm_type != PM_DEAD ) {
        bs->directives.armorAtStart   = bs->cur_ps.stats[STAT_ARMOR];
        bs->directives.healthAtStart  = bs->cur_ps.stats[STAT_HEALTH];
        bs->directives.needsBaselineReset = qfalse;
    }

    /* ── expiry check ─────────────────────────────────────────────────── */
    if ( d->type != DIR_NONE && d->expire_time > 0.0f && now > d->expire_time ) {
        d->type = DIR_NONE;
        bs->directives.directiveLocked = qfalse;
        BotDirective_UpdateConfigstring( bs );
    }

    if ( d->type == DIR_NONE ) {
        /* No directive active — let existing AI choose its own goals.
           Do not touch ltgtype or teamgoal_time here; the bot may have
           set them via normal BotMatchMessage handling. */
        return;
    }

    /* ── ltgtype translation bridge ───────────────────────────────────── */
    bs->ordered        = qtrue;
    bs->decisionmaker  = d->source_client >= 0 ? d->source_client : bs->client;
    bs->order_time     = d->issue_time;

    switch ( d->type ) {
        case DIR_FOLLOW:
            /* "follow carrier": re-resolve the flag carrier every frame so
               the directive automatically re-targets if the carrier changes. */
            if ( Q_stricmp( d->target_name, "flag carrier" ) == 0 ) {
                d->target_client = BotTeamFlagCarrier( bs );
            }
            if ( d->target_client >= 0 &&
                 d->target_client < MAX_CLIENTS &&
                 level.clients[d->target_client].pers.connected == CON_CONNECTED ) {
                /* ACCOMPANY LIFECYCLE — one of TWO, and the difference is
                   deliberate.

                   This is the DIRECTIVE entry point, and it runs on every frame
                   a directive is live. The short expiry below is a HEARTBEAT,
                   not a budget: it is re-armed on each pass, so the goal stays
                   alive exactly as long as the directive does and lapses about a
                   second after the directive stops. That is what lets a revoked
                   or replaced directive release the goal without anyone having
                   to clear ltgtype explicitly.

                   The other entry point is the CHAT path in ai_cmd.c
                   (BotMatch_Accompany), which sets a long one-shot budget
                   because nothing re-asserts a chat order after it is given.
                   Do NOT unify the two: a continuously re-asserted directive and
                   a fire-and-forget order want opposite persistence semantics,
                   and collapsing them would either strand a chat order or make a
                   directive outlive its own revocation. */
                bs->ltgtype        = LTG_TEAMACCOMPANY;
                bs->teammate       = d->target_client;
                bs->teamgoal_time  = now + 1.0f;   /* heartbeat; see above */
                bs->formation_dist = 3.5f * 32.0f;
            }
            break;

        case DIR_DEFEND_AREA:
            BotSetOriginGoal( bs, d->area_origin,
                              d->area_radius > 0 ? d->area_radius : 200.0f );
            break;

        case DIR_CAMP_SPOT:
            /* LTG_CAMPORDER is defined but not handled in BotGetLongTermGoal.
               LTG_DEFENDKEYAREA implements the same navigate-then-hold pattern.
               A very tight hold radius keeps the bot pinned to the spot. */
            BotSetOriginGoal( bs, d->area_origin, 80.0f );   /* tight — barely roams */
            bs->ordered           = qtrue;
            break;

        case DIR_GOTO_EXIT: {
            /* Head to the level's exit trigger for forward progress. Resolve the
               exit center and drive it through the shared origin-goal path (same
               as DEFEND). A modest hold radius lets the bot settle onto the
               trigger so its touch fires the map change. On a map with no exit
               (an arena) or an exit not on the nav mesh, leave ltgtype untouched
               so normal AI runs. */
            vec3_t exitOrigin;
            if ( WiredIntel_FindExitOrigin( bs->origin, exitOrigin ) ) {
                /* Same priority as the idle path: open a blocking gate first. */
                if ( WiredIntel_TrySetActivationGoal( bs, exitOrigin ) ) {
                    break;
                }
                /* Then, as on the idle path, board a rideable mover that the nav
                   corridor cannot cross on foot. Ordered after activation because
                   the button clearing the path to the elevator may itself be
                   gated; a no-op on a map with no ride-requiring mover. */
                if ( WiredIntel_TrySetRideGoal( bs, exitOrigin ) ) {
                    break;
                }
                BotSetOriginGoal( bs, exitOrigin, 96.0f );
                if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
                    static float s_ordExitLog[MAX_CLIENTS];
                    if ( FloatTime() - s_ordExitLog[bs->client] > 2.0f ) {
                        vec3_t delta;
                        s_ordExitLog[bs->client] = FloatTime();
                        VectorSubtract( exitOrigin, bs->origin, delta );
                        Com_Log( SEV_INFO, LOG_CH(ch_botai),
                            "cl=%d ordered -> exit goal (%.0f %.0f %.0f) dist=%.0f\n",
                            bs->client, exitOrigin[0], exitOrigin[1], exitOrigin[2],
                            VectorLength( delta ) );
                    }
                }
            }
            break;
        }

        case DIR_PATROL:
            if ( bs->patrolpoints ) {
                bs->ltgtype       = LTG_PATROL;
                bs->teamgoal_time = now + 1.0f;
            }
            /* If no patrol waypoints, fall through to normal AI */
            break;

        case DIR_SEEK_ITEM: {
            const char *resolved = BotResolveItemName( d->item_classname );
            if ( !resolved ) {
                char rej[MAX_SAY_TEXT];
                Com_sprintf( rej, sizeof(rej), "Unknown item: %s", d->item_classname );
                trap_EA_Say( bs->client, rej );
                BotDirective_Abort( bs );
                break;
            }

            /* Pickup detection (Approach A): poll player state each frame.
               If the stat corresponding to this item type increased since the
               directive was issued, the bot acquired the item — clear and announce. */
            if ( bs->directives.directiveLocked && Q_stricmp( resolved, "flag" ) != 0 ) {
                qboolean picked_up = qfalse;
                const gitem_t *it;
                for ( it = bg_itemlist + 1; it->classname; it++ ) {
                    if ( it->pickup_name && Q_stricmp( it->pickup_name, resolved ) == 0 ) {
                        if ( it->giType == IT_ARMOR &&
                             bs->cur_ps.stats[STAT_ARMOR] > bs->directives.armorAtStart ) {
                            picked_up = qtrue;
                        } else if ( it->giType == IT_HEALTH &&
                                     bs->cur_ps.stats[STAT_HEALTH] > bs->directives.healthAtStart ) {
                            picked_up = qtrue;
                        } else if ( it->giType == IT_WEAPON ) {
                            int wn = it->giTag;
                            if ( wn > 0 && wn < MAX_WEAPONS &&
                                 ( bs->cur_ps.stats[STAT_WEAPONS] & ( 1 << wn ) ) ) {
                                picked_up = qtrue;
                            }
                        }
                        break;
                    }
                }
                /* For armor/health, also require proximity to the target item
                   to reject false positives from picking up a different item of
                   the same type (e.g. "get Heavy Armor" while grabbing YA nearby)
                   or from the baseline being taken before the respawn stat reset. */
                if ( picked_up &&
                     ( it->giType == IT_ARMOR || it->giType == IT_HEALTH ) ) {
                    if ( bs->directives.seekPosition[0] != 0.0f ||
                         bs->directives.seekPosition[1] != 0.0f ||
                         bs->directives.seekPosition[2] != 0.0f ) {
                        vec3_t diff;
                        VectorSubtract( bs->origin, bs->directives.seekPosition, diff );
                        if ( VectorLength( diff ) > 200.0f ) {
                            picked_up = qfalse;   /* too far — different item */
                        }
                    }
                }
                if ( picked_up ) {
                    if ( bs->wiredIntelActive )
                        trap_EA_SayTeam( bs->client, "Got it!" );
                    d->type = DIR_NONE;
                    bs->directives.directiveLocked = qfalse;
                    bs->ordered = qfalse;
                    bs->ltgtype = 0;
                    bs->teamgoal_time = 0;
                    break;
                }
            }

            /* ── raw-position movement path (AAS entirely unavailable) ──── */
            if ( bs->directives.useRawPosition ) {
                /* 10-second timeout before giving up */
                if ( FloatTime() - bs->directives.rawMoveStartTime > 10.0f ) {
                    char rej[MAX_SAY_TEXT];
                    Com_sprintf( rej, sizeof(rej), "Can't reach: %s", resolved );
                    trap_EA_Say( bs->client, rej );
                    BotDirective_Abort( bs );
                    break;
                }
                /* Walk toward the item's world coordinates via EA movement */
                {
                    vec3_t dir, angles;
                    VectorSubtract( bs->directives.seekPosition, bs->origin, dir );
                    dir[2] = 0.0f;
                    if ( VectorNormalize( dir ) > 32.0f ) {
                        vectoangles( dir, angles );
                        bs->ideal_viewangles[YAW]   = angles[YAW];
                        bs->ideal_viewangles[PITCH] = 0.0f;
                        if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 2 )
                            BotAI_Print( PRT_MESSAGE, "[AimSet/wb-seek] client %d pitch=%.1f yaw=%.1f\n",
                                         bs->client, bs->ideal_viewangles[PITCH], bs->ideal_viewangles[YAW] );
                        trap_EA_MoveForward( bs->client );
                    }
                }
                bs->teamgoal_time = now + 1.0f;
                break;
            }

            /* ── Goal setting: navigate toward the item via AAS ─────────── */
            if ( Q_stricmp( resolved, "flag" ) == 0 ) {
                bs->ltgtype = LTG_GETFLAG;
            } else {
                /* Resolve to AAS level goal.
                   trap_BotGetLevelItemGoal indexes items by entity classname
                   (e.g. "item_armor_body"), not pickup_name ("Heavy Armor").
                   BotPickupToClassname converts pickup_name to classname. */
                const char *item_cn = BotPickupToClassname( resolved );
                bot_goal_t item_goal;
                int item_result;
                memset( &item_goal, 0, sizeof( item_goal ) );
                item_result = trap_BotGetLevelItemGoal( -1, (char *)item_cn, &item_goal );
                if ( item_result >= 0 && item_goal.areanum > 0 ) {
                    /* Store world position for pickup proximity detection */
                    if ( VectorLength( bs->directives.seekPosition ) < 1.0f ) {
                        VectorCopy( item_goal.origin, bs->directives.seekPosition );
                    }
                    bs->teamgoal  = item_goal;
                    bs->ltgtype   = LTG_GETITEM;
                } else {
                    /* AAS goal list failed — search world entities for the item
                       and synthesize a goal from its world position + AAS area. */
                    gentity_t  *ent = NULL;
                    int         ei;
                    for ( ei = 0; ei < level.num_entities; ei++ ) {
                        gentity_t *e = &g_entities[ei];
                        if ( !e->inuse || !e->item || !e->item->pickup_name ) continue;
                        if ( Q_stricmp( e->item->pickup_name, resolved ) == 0 ) {
                            ent = e;
                            break;
                        }
                    }
                    if ( ent ) {
                        int area = BotPointAreaNum( ent->r.currentOrigin );
                        if ( area <= 0 ) {
                            /* Items at exact floor/boundary positions return 0.
                               Probe slightly above the origin to find the area. */
                            vec3_t probe;
                            VectorCopy( ent->r.currentOrigin, probe );
                            probe[2] += 24.0f;
                            area = BotPointAreaNum( probe );
                            if ( area <= 0 ) {
                                probe[2] = ent->r.currentOrigin[2] + 48.0f;
                                area = BotPointAreaNum( probe );
                            }
                        }
                        if ( area > 0 ) {
                            /* Build a synthetic goal from the entity's world position.
                               Set GFL_ITEM so BotReachedGoal uses item-proximity
                               logic, and provide a reasonable bounding box. */
                            qboolean first_seek = ( VectorLength( bs->directives.seekPosition ) < 1.0f );
                            memset( &item_goal, 0, sizeof( item_goal ) );
                            VectorCopy( ent->r.currentOrigin, item_goal.origin );
                            item_goal.areanum   = area;
                            item_goal.entitynum = (int)( ent - g_entities );
                            item_goal.flags     = GFL_ITEM;
                            VectorSet( item_goal.mins, -15, -15, -15 );
                            VectorSet( item_goal.maxs,  15,  15,  15 );
                            VectorCopy( ent->r.currentOrigin, bs->directives.seekPosition );
                            bs->teamgoal = item_goal;
                            bs->ltgtype  = LTG_GETITEM;
                            if ( first_seek )
                                trap_EA_SayTeam( bs->client, "On my way!" );
                        } else {
                            /* No AAS coverage — fall back to raw direction movement */
                            qboolean first_seek = ( VectorLength( bs->directives.seekPosition ) < 1.0f );
                            VectorCopy( ent->r.currentOrigin, bs->directives.seekPosition );
                            bs->directives.useRawPosition  = qtrue;
                            bs->directives.rawMoveStartTime = FloatTime();
                            if ( first_seek )
                                trap_EA_SayTeam( bs->client, "On my way!" );
                            bs->teamgoal_time = now + 1.0f;
                            break;
                        }
                    } else {
                        /* Item doesn't exist on this map at all */
                        char rej[MAX_SAY_TEXT];
                        Com_sprintf( rej, sizeof(rej), "No such item: %s", resolved );
                        trap_EA_Say( bs->client, rej );
                        BotDirective_Abort( bs );
                        break;
                    }
                }
            }

            /* ── diagnostic ─────────────────────────────────────────────── */
            if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
                static float s_dirMoveLog[MAX_CLIENTS];
                if ( FloatTime() - s_dirMoveLog[bs->client] > 1.0f ) {
                    vec3_t diff;
                    s_dirMoveLog[bs->client] = FloatTime();
                    VectorSubtract( bs->origin, bs->teamgoal.origin, diff );
                    Com_Log( SEV_INFO, LOG_CH(ch_game), "DIRECTIVE MOVE: cl=%d item=%s dist=%.0f goal=[%.0f,%.0f,%.0f] area=%d ltg=%d\n",
                        bs->client, d->item_classname, VectorLength( diff ),
                        bs->teamgoal.origin[0], bs->teamgoal.origin[1], bs->teamgoal.origin[2],
                        bs->teamgoal.areanum, bs->ltgtype );
                }
            }

            /* ── near-spawn patrol (item not yet respawned) ───────────────
               When the bot has arrived at the item location but the entity
               hasn't respawned yet, patrol defensively rather than standing
               idle.  We break early so stuck detection doesn't misfire. */
            if ( bs->teamgoal.entitynum >= 0
                 && bs->teamgoal.entitynum < MAX_GENTITIES
                 && Q_stricmp( resolved, "flag" ) != 0 ) {
                float distToGoal = Distance( bs->origin, bs->teamgoal.origin );
                if ( distToGoal < 128.0f ) {
                    gentity_t *eg = &g_entities[bs->teamgoal.entitynum];
                    qboolean   itemPresent = (eg->r.contents & CONTENTS_TRIGGER) != 0;
                    if ( !itemPresent ) {
                        WiredIntel_DefensiveCombat( bs );
                        if ( level.time > bs->directives.nextPatrolTime ) {
                            vec3_t patrolPoint, pdir;
                            patrolPoint[0] = bs->teamgoal.origin[0] + crandom() * 150.0f;
                            patrolPoint[1] = bs->teamgoal.origin[1] + crandom() * 150.0f;
                            patrolPoint[2] = bs->teamgoal.origin[2];
                            VectorSubtract( patrolPoint, bs->origin, pdir );
                            pdir[2] = 0.0f;
                            if ( VectorNormalize( pdir ) > 16.0f ) {
                                vec3_t pangles;
                                vectoangles( pdir, pangles );
                                bs->ideal_viewangles[YAW]   = pangles[YAW];
                                bs->ideal_viewangles[PITCH] = 0.0f;
                                if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 2 )
                                    BotAI_Print( PRT_MESSAGE, "[AimSet/wb-patrol] client %d pitch=%.1f yaw=%.1f\n",
                                                 bs->client, bs->ideal_viewangles[PITCH], bs->ideal_viewangles[YAW] );
                                trap_EA_Move( bs->client, pdir, 200 );
                            }
                            bs->directives.nextPatrolTime =
                                level.time + 1500 + ( rand() % 2000 );
                        }
                        /* Not stuck — reset baseline so check restarts when item appears */
                        bs->directives.lastProgressCheck = 0;
                        bs->teamgoal_time = now + 1.0f;
                        break;
                    }
                }
            }

            /* ── stuck detection ──────────────────────────────────────────
               Sample distance to goal every 5 s. If the bot hasn't closed
               the gap by at least 32 units it is stuck — abort the directive
               so the bot returns to normal AI rather than standing idle. */
            {
                float dist = Distance( bs->origin, bs->teamgoal.origin );
                if ( bs->directives.lastProgressCheck == 0 ) {
                    bs->directives.lastProgressDist  = dist;
                    bs->directives.lastProgressCheck = level.time;
                } else if ( level.time - bs->directives.lastProgressCheck > 5000 ) {
                    if ( dist >= bs->directives.lastProgressDist - 32.0f ) {
                        trap_EA_SayTeam( bs->client, "Can't get there." );
                        BotDirective_Abort( bs );
                        break;
                    }
                    bs->directives.lastProgressDist  = dist;
                    bs->directives.lastProgressCheck = level.time;
                }
            }

            bs->teamgoal_time = now + 1.0f;
            break;
        }

        case DIR_RUSH_BASE:
            bs->ltgtype         = LTG_RUSHBASE;
            bs->teamgoal_time   = now + 1.0f;
            bs->rushbaseaway_time = 0;
            break;

        case DIR_RETURN_FLAG:
            bs->ltgtype         = LTG_RETURNFLAG;
            bs->teamgoal_time   = now + 1.0f;
            break;

        case DIR_ATTACK_BASE:
            bs->ltgtype         = LTG_ATTACKENEMYBASE;
            bs->teamgoal_time   = now + 1.0f;
            bs->attackaway_time = 0;
            break;

        case DIR_KILL_TARGET:
            if ( d->target_client >= 0 &&
                 d->target_client < MAX_CLIENTS &&
                 level.clients[d->target_client].pers.connected == CON_CONNECTED &&
                 level.clients[d->target_client].ps.stats[STAT_HEALTH] > 0 ) {
                bs->ltgtype              = LTG_KILL;
                bs->teamgoal.entitynum   = d->target_client;
                bs->teamgoal_time        = now + 1.0f;
                bs->ordered              = qtrue;
            } else {
                /* Target dead or disconnected — announce if we were actively hunting */
                if ( bs->directives.directiveLocked && bs->wiredIntelActive )
                    trap_EA_SayTeam( bs->client, "Target down!" );
                d->type = DIR_NONE;
                bs->directives.directiveLocked = qfalse;
            }
            break;

        case DIR_HARVEST:
            bs->ltgtype        = LTG_HARVEST;
            bs->teamgoal_time  = now + 1.0f;
            bs->harvestaway_time = 0;
            break;

        case DIR_ROAM:
            /* Cancel any current LTG goal and let the bot wander */
            bs->ltgtype        = 0;
            bs->ordered        = qfalse;
            bs->teamgoal_time  = 0;
            break;

        case DIR_SET_SUBTEAM:
            /* One-shot: write subteam name (or clear it) and self-expire */
            Q_strncpyz( bs->directives.subteam, d->target_name,
                        sizeof( bs->directives.subteam ) );
            d->type = DIR_NONE;
            break;

        default:
            break;
    }
}

/* =========================================================================
   DIRECTIVE LOCK — DEFENSIVE COMBAT
   ========================================================================= */

/*
==================
WiredIntel_DefensiveCombat
==================
Called from AINode_Seek_LTG / AINode_Seek_NBG when directiveLocked is set.

The bot keeps navigating toward its directive objective while returning fire
at any visible enemy.  It does NOT switch into a Battle node — movement
continues on the LTG/NBG path, but aiming and firing happen here.
*/
void WiredIntel_DefensiveCombat( bot_state_t *bs ) {
    /* BotFindEnemy sets bs->enemy to the most threatening visible foe.
       BotAimAtEnemy adjusts bs->ideal_viewangles toward that enemy.
       BotCheckAttack fires if range and conditions allow.
       None of these calls switch the AI node — the bot stays in Seek_LTG/NBG
       and continues moving toward its objective. */
    if ( !BotFindEnemy( bs, -1 ) ) {
        return;
    }

    /* FIGHT-WHILE-ADVANCING NEEDS SOMETHING TO ADVANCE ALONG.  The whole premise of
       this routine is that movement continues on the LTG/NBG path while aiming and
       firing happen here — the agent trades a little progress for staying alive.
       On a pose the mesh cannot route FROM, that premise fails: there is no corridor
       to follow, so "fight while advancing" collapses into "fight in place", and the
       agent can hold that state indefinitely.  Measured on the e1m1 specimen: 283
       directive-locked frames across two stays on a walkable pocket the navmesh
       represents as an unroutable island, during which the agent asked its gate
       question 36 times from ground where no answer was possible.
       Aiming is what pins it: BotAimAtEnemy drives ideal_viewangles at the enemy
       every frame, so the agent faces the fight instead of turning to leave.
       Detect the state with the SAME predicate the activation guard uses — one
       definition of "the mesh cannot route from here", two consumers — and in it
       still SHOOT (BotCheckAttack fires along the current facing, so the agent
       defends itself) but stop re-aiming, which frees its facing to follow the
       movement it is already being given.  No timer, no distance, no retry count:
       the condition is a property of WHERE THE AGENT STANDS — the predicate is
       target-independent, so this consumer and the activation guard cannot drift
       apart — and it clears the instant the agent stands somewhere routable. */
    {
        qboolean held = qfalse;
        if ( bs->ltgtype ) {
            /* Only meaningful when the agent HAS an objective to advance along; with
               no long-term goal there is no advance to protect and combat is
               unchanged.  The second argument is unused by the predicate (the verdict
               is a pose property); the goal is passed so the call site still reads as
               "can I get from here toward what I am doing". */
            held = WiredIntel_OriginIsDegenerate( bs->origin, bs->teamgoal.origin );
        }
        if ( !held ) {
            BotAimAtEnemy( bs );
        }
    }
    BotCheckAttack( bs );
}

/* =========================================================================
   AUTHORIZATION
   ========================================================================= */

/*
==================
BotAuthorizeOrder
==================
Returns qtrue if issuer_client may give orders to this bot.

Rules:
  - issuer_client < 0 → server console: always authorized
  - Same team: authorized
  - Different team or spectator: denied
*/
qboolean BotAuthorizeOrder( bot_state_t *bs, int issuer_client ) {
    if ( issuer_client < 0 ) {
        return qtrue;   /* server console */
    }
    if ( issuer_client >= MAX_CLIENTS ) {
        return qfalse;
    }
    if ( level.clients[issuer_client].pers.connected != CON_CONNECTED ) {
        return qfalse;
    }
    /* stateless clients are authorized for all bots on all teams */
    if ( level.clients[issuer_client].sess.isStatelessClient ) {
        return qtrue;
    }
    return BotSameTeam( bs, issuer_client );
}

/* =========================================================================
   DIRECTIVE DELIVERY
   ========================================================================= */

/*
==================
ResolveClientByName  (internal)
==================
Finds a connected client whose name matches (case-insensitive prefix match).
Returns client num or -1.
*/
static int ResolveClientByName( const char *name ) {
    int   i;
    char  clean[MAX_NETNAME];

    if ( !name || !name[0] ) {
        return -1;
    }

    for ( i = 0; i < MAX_CLIENTS; i++ ) {
        if ( level.clients[i].pers.connected != CON_CONNECTED ) {
            continue;
        }
        Q_strncpyz( clean, level.clients[i].pers.netname, sizeof( clean ) );
        Q_CleanStr( clean );
        if ( Q_stricmpn( clean, name, strlen( name ) ) == 0 ) {
            return i;
        }
    }
    return -1;
}

/*
==================
BotReceiveDirective
==================
Parse an order string and store it as the bot's active directive.
Called from BotDirective_ConsoleOrder and BotMatchMessage.

Order syntax examples:
  "follow Sarge"
  "defend"  / "defend base"
  "camp"
  "patrol"
  "get flag"
  "rush base"
  "return flag"
  "attack base"
  "kill Sarge"
  "harvest"
  "roam"
  "hunt Sarge"     ← tactic, not a goal directive
  "avoid Sarge"
  "retreat"
  "rush"           ← tactic RUSH (do not stop for items)
  "ambush"
  "join <subteam>" ← join a named subteam
  "leave"          ← leave current subteam
*/
void BotReceiveDirective( bot_state_t *bs, int issuer_client, const char *order ) {
    char            tok[64];
    char            rest[128];
    const char     *space;
    int             toklen;
    botDirective_t *d = &bs->directives.tactical;
    float           now = FloatTime();

    if ( !BotAuthorizeOrder( bs, issuer_client ) ) {
        return;
    }

    /* tokenize: first word into tok, remainder into rest */
    space = strchr( order, ' ' );
    if ( space ) {
        toklen = (int)( space - order );
        if ( toklen >= (int)sizeof( tok ) ) {
            toklen = (int)sizeof( tok ) - 1;
        }
        memcpy( tok, order, toklen );
        tok[toklen] = '\0';
        /* skip spaces between tok and rest */
        while ( *space == ' ' || *space == '\t' ) {
            space++;
        }
        Q_strncpyz( rest, space, sizeof( rest ) );
        /* strip trailing whitespace from rest */
        {
            int len = (int)strlen( rest ) - 1;
            while ( len >= 0 && ( rest[len] == ' ' || rest[len] == '\t' ||
                                   rest[len] == '\r' || rest[len] == '\n' ) ) {
                rest[len--] = '\0';
            }
        }
    } else {
        Q_strncpyz( tok, order, sizeof( tok ) );
        rest[0] = '\0';
    }

    /* ── tactic modifiers (do not replace the tactical directive) ──── */
    if ( Q_stricmp( tok, "hunt" ) == 0 ) {
        bs->directives.tactic        = TACTIC_HUNT;
        bs->directives.tactic_target = ResolveClientByName( rest );
        bs->directives.tactic_active = qtrue;
        if ( !TeamPlayIsOn() )
            trap_SendServerCommand( -1, va( "print \"^5[Bot %s]^7 Tactic: hunt %s\n\"",
                level.clients[bs->client].pers.netname, rest ) );
        return;
    }
    if ( Q_stricmp( tok, "avoid" ) == 0 ) {
        bs->directives.tactic        = TACTIC_AVOID;
        bs->directives.tactic_target = ResolveClientByName( rest );
        bs->directives.tactic_active = qtrue;
        if ( !TeamPlayIsOn() )
            trap_SendServerCommand( -1, va( "print \"^5[Bot %s]^7 Tactic: avoid %s\n\"",
                level.clients[bs->client].pers.netname, rest ) );
        return;
    }
    if ( Q_stricmp( tok, "retreat" ) == 0 ) {
        bs->directives.tactic        = TACTIC_RETREAT;
        bs->directives.tactic_target = -1;
        bs->directives.tactic_active = qtrue;
        if ( !TeamPlayIsOn() )
            trap_SendServerCommand( -1, va( "print \"^5[Bot %s]^7 Tactic: retreat\n\"",
                level.clients[bs->client].pers.netname ) );
        return;
    }
    if ( Q_stricmp( tok, "ambush" ) == 0 ) {
        bs->directives.tactic        = TACTIC_AMBUSH;
        bs->directives.tactic_target = -1;
        bs->directives.tactic_active = qtrue;
        if ( !TeamPlayIsOn() )
            trap_SendServerCommand( -1, va( "print \"^5[Bot %s]^7 Tactic: ambush\n\"",
                level.clients[bs->client].pers.netname ) );
        return;
    }
    /* "rush" alone is a TACTIC_RUSH; "rush base" is a directive handled below */
    if ( Q_stricmp( tok, "rush" ) == 0 && ( !rest[0] || Q_stricmp( rest, "base" ) != 0 ) ) {
        bs->directives.tactic        = TACTIC_RUSH;
        bs->directives.tactic_target = -1;
        bs->directives.tactic_active = qtrue;
        if ( !TeamPlayIsOn() )
            trap_SendServerCommand( -1, va( "print \"^5[Bot %s]^7 Tactic: rush\n\"",
                level.clients[bs->client].pers.netname ) );
        return;
    }

    /* ── tactical directives ──────────────────────────────────────── */
    memset( d, 0, sizeof( *d ) );
    d->source_client = issuer_client;
    d->issue_time    = now;
    d->expire_time   = 0;
    d->target_client = -1;

    if ( Q_stricmp( tok, "follow" ) == 0 ||
         Q_stricmp( tok, "escort" ) == 0 ||
         Q_stricmp( tok, "protect" ) == 0 ) {
        /* "follow/escort/protect carrier|the carrier|flag carrier" → follow
           whoever on the team currently holds the enemy flag.  The target is
           re-resolved every frame in FrameUpdate so the directive stays live
           if the carrier dies and a teammate picks up the flag. */
        qboolean carrierTarget =
            Q_stricmp( rest, "carrier" )          == 0 ||
            Q_stricmp( rest, "the carrier" )       == 0 ||
            Q_stricmp( rest, "flag carrier" )      == 0 ||
            Q_stricmp( rest, "the flag carrier" )  == 0;
        d->type        = DIR_FOLLOW;
        d->expire_time = now + TEAM_ACCOMPANY_TIME;
        if ( carrierTarget ) {
            Q_strncpyz( d->target_name, "flag carrier", sizeof( d->target_name ) );
            /* Resolve now; -1 means nobody has the flag yet — FrameUpdate retries. */
            d->target_client = BotTeamFlagCarrier( bs );
        } else if ( Q_stricmp( rest, "me" ) == 0 || Q_stricmp( rest, "myself" ) == 0 ) {
            /* "follow me" — target is the issuer themselves */
            d->target_client = issuer_client;
            if ( issuer_client >= 0 && issuer_client < MAX_CLIENTS ) {
                ClientName( issuer_client, d->target_name, sizeof( d->target_name ) );
                Q_CleanStr( d->target_name );
            } else {
                Q_strncpyz( d->target_name, "me", sizeof( d->target_name ) );
            }
        } else {
            Q_strncpyz( d->target_name, rest, sizeof( d->target_name ) );
            d->target_client = ResolveClientByName( rest );
            if ( d->target_client < 0 ) {
                d->type = DIR_NONE;
                trap_Cvar_Set( "wiredbot_ack", va( "rejected: unknown target '%s'", rest ) );
                WiredIntel_Announce( bs, WI_ACK_UNKNOWN_TARGET, rest );
                return;
            }
        }
    }
    else if ( Q_stricmp( tok, "defend" ) == 0 ||
              Q_stricmp( tok, "guard" )  == 0 ) {
        /* "defend/guard flag|the flag|our flag" in a CTF game → defend the
           team's own flag spawn using the pre-computed ctf_redflag/blueflag
           goal (same globals BotVoiceChat_Defend uses). */
        qboolean flagTarget =
            Q_stricmp( rest, "flag" )      == 0 ||
            Q_stricmp( rest, "the flag" )  == 0 ||
            Q_stricmp( rest, "our flag" )  == 0;
        d->type = DIR_DEFEND_AREA;
        d->expire_time = now + TEAM_DEFENDKEYAREA_TIME;
        if ( flagTarget && ( gametype == GT_CTF || gametype == GT_1FCTF ) ) {
            bot_goal_t *flagGoal = NULL;
            switch ( BotTeam( bs ) ) {
                case TEAM_RED:  flagGoal = &ctf_redflag;  break;
                case TEAM_BLUE: flagGoal = &ctf_blueflag; break;
                default: break;
            }
            if ( flagGoal && flagGoal->areanum ) {
                VectorCopy( flagGoal->origin, d->area_origin );
                d->area_radius = 300.0f;
            } else {
                /* Fallback: defend current position if goal not yet initialised */
                VectorCopy( bs->origin, d->area_origin );
                d->area_radius = 200.0f;
            }
        } else {
            /* Generic "defend/guard" with no flag target → defend current pos */
            VectorCopy( bs->origin, d->area_origin );
            d->area_radius = 200.0f;
        }
    }
    else if ( Q_stricmp( tok, "camp" ) == 0 ) {
        d->type = DIR_CAMP_SPOT;
        VectorCopy( bs->origin, d->area_origin );
        d->expire_time = now + TEAM_CAMP_TIME;
    }
    else if ( Q_stricmp( tok, "patrol" ) == 0 ) {
        d->type        = DIR_PATROL;
        d->expire_time = now + TEAM_PATROL_TIME;
    }
    else if ( Q_stricmp( tok, "get" ) == 0 ) {
        d->type = DIR_SEEK_ITEM;
        Q_strncpyz( d->item_classname, rest, sizeof( d->item_classname ) );
        d->expire_time = now + TEAM_GETITEM_TIME;
    }
    else if ( Q_stricmp( tok, "rush" ) == 0 && Q_stricmp( rest, "base" ) == 0 ) {
        d->type        = DIR_RUSH_BASE;
        d->expire_time = now + CTF_RUSHBASE_TIME;
    }
    else if ( Q_stricmp( tok, "return" ) == 0 ) {
        d->type        = DIR_RETURN_FLAG;
        d->expire_time = now + CTF_RETURNFLAG_TIME;
    }
    else if ( Q_stricmp( tok, "attack" ) == 0 ) {
        d->type        = DIR_ATTACK_BASE;
        d->expire_time = now + TEAM_ATTACKENEMYBASE_TIME;
    }
    else if ( Q_stricmp( tok, "kill" ) == 0 ) {
        d->target_client = ResolveClientByName( rest );
        if ( d->target_client < 0 ) {
            trap_Cvar_Set( "wiredbot_ack", va( "rejected: unknown target '%s'", rest ) );
            WiredIntel_Announce( bs, WI_ACK_UNKNOWN_TARGET, rest );
            return;
        }
        d->type          = DIR_KILL_TARGET;
        Q_strncpyz( d->target_name, rest, sizeof( d->target_name ) );
        d->expire_time   = now + TEAM_KILL_SOMEONE;
    }
    else if ( Q_stricmp( tok, "harvest" ) == 0 ) {
        d->type        = DIR_HARVEST;
        d->expire_time = now + TEAM_HARVEST_TIME;
    }
    else if ( Q_stricmp( tok, "exit" ) == 0 ||
              ( Q_stricmp( tok, "goto" ) == 0 && Q_stricmp( rest, "exit" ) == 0 ) ) {
        /* head to the level's exit trigger; persists until reached */
        d->type        = DIR_GOTO_EXIT;
        d->expire_time = 0;
    }
    else if ( Q_stricmp( tok, "roam" ) == 0 ) {
        d->type        = DIR_ROAM;
        d->expire_time = now + 30.0f;   /* auto-clear roam after 30 s */
    }
    else if ( Q_stricmp( tok, "join" ) == 0 && rest[0] ) {
        /* "join <subteam>" — one-shot; FrameUpdate applies it immediately */
        d->type = DIR_SET_SUBTEAM;
        Q_strncpyz( d->target_name, rest, sizeof( d->target_name ) );
        d->expire_time = 0;
    }
    else if ( Q_stricmp( tok, "leave" ) == 0 && !rest[0] ) {
        /* "leave" with no argument — clears subteam membership */
        d->type = DIR_SET_SUBTEAM;
        d->target_name[0] = '\0';
        d->expire_time = 0;
    }
    else {
        /* Unknown order — clear directive, do not disrupt existing goal */
        d->type = DIR_NONE;
    }

    /* Lock the directive and snapshot player state for pickup detection.
       Done before the ack so the baseline is accurate at issue time. A locked
       directive makes the bot pursue its objective and fight only defensively:
       Seek_LTG's locked branch aims and fires at a visible enemy but does NOT
       abandon the goal — it falls through to the nav path, so the bot keeps
       advancing while shooting (fight-while-advance). The exit goal is locked for
       exactly this: on a monster-dense level an unlocked exit goal would be
       preempted into a full stop-and-fight every time an enemy is seen and never
       reach the exit (or the button chain that unblocks the route to it). The
       activation goal rides this same lock — it is set on the DIR_GOTO_EXIT path
       and checked under the same directiveLocked branch, so locking exit covers
       activation with no separate flag. */
    WiredIntel_ArmDirective( bs );

    /* Acknowledge recognised tactical directives with a team chat message.
       Tactic modifiers (hunt/avoid/retreat/ambush/rush) return early above
       and do not reach this point. */
    if ( d->type != DIR_NONE ) {
        trap_Cvar_Set( "wiredbot_ack",
                       va( "accepted: %s (directiveLocked=1)",
                           level.clients[bs->client].pers.netname ) );
        WiredIntel_Announce( bs, WI_ACK_YES, NULL );
        /* In non-team modes WI_ACK_YES is team_only and never fires.
           Print the directive explicitly so the player can see it. */
        if ( !TeamPlayIsOn() ) {
            trap_SendServerCommand( -1, va( "print \"^5[Bot %s]^7 Directive: %s\n\"",
                level.clients[bs->client].pers.netname, order ) );
        }
    }

    BotDirective_UpdateConfigstring( bs );
}

/*
==================
BotDirective_IssueToDirect
==================
Direct (non-chat) delivery used by BotCTFOrders.
Bypasses string parsing; caller supplies already-resolved values.
*/
void BotDirective_IssueToDirect( bot_state_t *bs,
                                  int issuer_client,
                                  directiveType_t type,
                                  int target_client,
                                  const vec3_t area_origin ) {
    botDirective_t *d   = &bs->directives.tactical;
    float           now = FloatTime();

    if ( !BotAuthorizeOrder( bs, issuer_client ) ) {
        return;
    }

    memset( d, 0, sizeof( *d ) );
    d->type          = type;
    d->source_client = issuer_client;
    d->issue_time    = now;
    d->target_client = target_client;

    if ( area_origin ) {
        VectorCopy( area_origin, d->area_origin );
    }

    /* set expiry per directive type */
    switch ( type ) {
        case DIR_FOLLOW:         d->expire_time = now + TEAM_ACCOMPANY_TIME;        break;
        case DIR_DEFEND_AREA:    d->expire_time = now + TEAM_DEFENDKEYAREA_TIME;    break;
        case DIR_CAMP_SPOT:      d->expire_time = now + TEAM_CAMP_TIME;             break;
        case DIR_PATROL:         d->expire_time = now + TEAM_PATROL_TIME;           break;
        case DIR_SEEK_ITEM:      d->expire_time = now + TEAM_GETITEM_TIME;          break;
        case DIR_RUSH_BASE:      d->expire_time = now + CTF_RUSHBASE_TIME;          break;
        case DIR_RETURN_FLAG:    d->expire_time = now + CTF_RETURNFLAG_TIME;        break;
        case DIR_ATTACK_BASE:    d->expire_time = now + TEAM_ATTACKENEMYBASE_TIME;  break;
        case DIR_KILL_TARGET:    d->expire_time = now + TEAM_KILL_SOMEONE;          break;
        case DIR_HARVEST:        d->expire_time = now + TEAM_HARVEST_TIME;          break;
        case DIR_ROAM:           d->expire_time = now + 30.0f;                      break;
        case DIR_GOTO_EXIT:      d->expire_time = 0;  /* persist until the exit is reached */ break;
        default:                 d->expire_time = 0;                                break;
    }

    /* Lock directive and snapshot state for pickup detection */
    if ( type != DIR_NONE && type != DIR_ROAM ) {
        bs->directives.directiveLocked   = qtrue;
        bs->directives.armorAtStart      = bs->cur_ps.stats[STAT_ARMOR];
        bs->directives.healthAtStart     = bs->cur_ps.stats[STAT_HEALTH];
        bs->directives.needsBaselineReset = qfalse;
        /* Reset seek state — will be populated by FrameUpdate on first pursue */
        VectorClear( bs->directives.seekPosition );
        bs->directives.useRawPosition    = qfalse;
        bs->directives.rawMoveStartTime  = 0.0f;
    }

    BotDirective_UpdateConfigstring( bs );
}

/* =========================================================================
   CHAT PARSING
   ========================================================================= */

/*
==================
BotDirective_ParseChatOrder
==================
Called at the end of BotMatchMessage() for messages not matched by existing
MSG_* patterns.  Parses free-form team-chat and
routes to BotReceiveDirective().

Only responds to messages that start with the bot's own name or "all".
*/
void BotDirective_ParseChatOrder( bot_state_t *bs, int talker, const char *msg ) {
    char name[MAX_NETNAME];
    char botname[MAX_NETNAME];
    char msgcopy[MAX_MESSAGE_SIZE];
    char *colon;
    char *body;

    if ( !msg || !msg[0] ) {
        return;
    }

    if ( !BotAuthorizeOrder( bs, talker ) ) {
        return;
    }

    Q_strncpyz( msgcopy, msg, sizeof( msgcopy ) );

    /* Expected format: "<name>: <order>" or just "<order>" */
    colon = strchr( msgcopy, ':' );
    if ( colon ) {
        *colon = '\0';
        Q_strncpyz( name, msgcopy, sizeof( name ) );
        Q_CleanStr( name );
        body = colon + 1;
        while ( *body == ' ' || *body == '\t' ) body++;

        /* Check if addressed to this bot or to "all" */
        trap_BotLibVarGet( "name", botname, sizeof( botname ) );
        /* Fall through: if name is "all" or matches bot's name, process it */
        if ( Q_stricmp( name, "all" ) != 0 ) {
            char cleanbot[MAX_NETNAME];
            Q_strncpyz( cleanbot, botname, sizeof( cleanbot ) );
            Q_CleanStr( cleanbot );
            if ( Q_stricmpn( cleanbot, name, strlen( name ) ) != 0 ) {
                return;   /* addressed to a different bot */
            }
        }
    } else {
        body = msgcopy;
    }

    BotReceiveDirective( bs, talker, body );
}

/* =========================================================================
   CHAT RESPONSE
   ========================================================================= */

/*
==================
BotDirective_RespondTeamChat
==================
Send a team-chat acknowledgement to the directive issuer.
Uses the bot's configured chat system.  Falls back gracefully if the
"order_ack" or "cmd_accompany" chat keys don't exist in this bot's file.
*/
void BotDirective_RespondTeamChat( bot_state_t *bs, int talker, const char *msg ) {
    (void)msg;   /* not inspected for the response */

    if ( trap_BotNumInitialChats( bs->cs, "order_ack" ) > 0 ) {
        BotAI_BotInitialChat( bs, "order_ack", NULL );
        trap_BotEnterChat( bs->cs, talker, CHAT_TELL );
    } else if ( trap_BotNumInitialChats( bs->cs, "cmd_accompany" ) > 0 ) {
        BotAI_BotInitialChat( bs, "cmd_accompany", NULL );
        trap_BotEnterChat( bs->cs, talker, CHAT_TELL );
    }
    /* If neither key exists, the bot stays silent (acceptable) */
}

/* =========================================================================
   CONSOLE COMMAND
   ========================================================================= */

/* The @mention resolver is defined later in this file, with the rest of the
   mention-addressing system. Forward-declare it so BotDirective_ConsoleOrder
   can share the one selector vocabulary instead of growing a second one. */
static int *WiredIntel_ResolveMention( const char *mention, int senderClient, int *count );

/*
==================
BotDirective_ConsoleOrder
==================
Handler for the "bot_order <botname|@selector> <order>" server console command.

Two addressing forms:

  bare name  — "bot_order visor exit". UNCHANGED: the original prefix match
               against the cleaned netname, first match wins, delivered and
               logged exactly as before. This is the form the nav harness
               drives, so it keeps its own matcher rather than routing through
               the resolver: the resolver additionally requires
               pers.connected == CON_CONNECTED, and taking on that extra
               condition would be a silent behaviour change on the harness's
               own command.

  '@selector' — "bot_order @everyone exit", "bot_order @squad:2 exit".
               Resolved through WiredIntel_ResolveMention, the SAME selector
               vocabulary chat already uses (@everyone/@all/@team, @closest,
               @farthest, @random, @defenders, @attackers, @idle, @red:/@blue:/
               @free:, @squad:N, and a bare name), then delivered to each
               recipient through the same BotReceiveDirective seam. senderClient
               is -1 (server console), which BotAuthorizeOrder always permits
               and which makes the resolver's team fallback "all bots".

Called from G_BotOrder() in g_bot.c.
*/
void BotDirective_ConsoleOrder( const char *bot_name, const char *order ) {
    int   i;
    char  clean[MAX_NETNAME];

    if ( !bot_name || !bot_name[0] || !order || !order[0] ) {
        Com_Log( SEV_INFO, LOG_CH(ch_game), "Usage: bot_order <botname|@selector> <order>\n" );
        return;
    }

    /* ── '@selector' form: fan out over the resolved recipient list ────── */
    if ( bot_name[0] == '@' ) {
        int *recipients;
        int  count, delivered = 0, skipped = 0;

        recipients = WiredIntel_ResolveMention( bot_name, -1 /* console */, &count );
        for ( i = 0; i < count; i++ ) {
            bot_state_t *bs = botstates[ recipients[i] ];
            if ( !bs || !bs->inuse ) {
                continue;
            }
            /* A member running a sequenced goal is SKIPPED, not queued and not
               overridden. This generalises the invariant the ride producer
               already states at WiredIntel_TrySetRideGoal ("Do not stomp some
               other in-flight sequence"): stomping a live sequence mid-flight
               strands the bot in a state where BotGetLongTermGoal — the
               authority gate that drives the sequence — is never reached, so
               the goal is silently abandoned. A broadcast is the mass form of
               exactly that stomp. Nothing queues because there is no queue:
               the rest of the squad still receives the directive this frame. */
            if ( bs->sequencedGoal.active ) {
                skipped++;
                continue;
            }
            BotReceiveDirective( bs, -1 /* console */, order );
            delivered++;
        }

        if ( delivered == 0 && skipped == 0 ) {
            Com_Log( SEV_INFO, LOG_CH(ch_game),
                        "bot_order: '%s' matched no active bot\n", bot_name );
        } else {
            Com_Log( SEV_INFO, LOG_CH(ch_game),
                        "Directive sent to %d bot(s) via %s: %s (%d busy)\n",
                        delivered, bot_name, order, skipped );
        }
        return;
    }

    /* ── bare-name form: unchanged from the original implementation ────── */
    for ( i = 0; i < MAX_CLIENTS; i++ ) {
        if ( !botstates[i] || !botstates[i]->inuse ) {
            continue;
        }
        Q_strncpyz( clean, level.clients[i].pers.netname, sizeof( clean ) );
        Q_CleanStr( clean );
        if ( Q_stricmpn( clean, bot_name, strlen( bot_name ) ) == 0 ) {
            BotReceiveDirective( botstates[i], -1 /* console */, order );
            Com_Log( SEV_INFO, LOG_CH(ch_game), "Directive sent to %s: %s\n",
                        level.clients[i].pers.netname, order );
            return;
        }
    }
    Com_Log( SEV_INFO, LOG_CH(ch_game), "bot_order: no active bot named '%s'\n", bot_name );
}

/*
==================
BotDirective_ConsoleTeleport
==================
Handler for the "bot_teleport <botname> <x> <y> <z> [yaw]" server console
command. Looks up an active bot by name (same match as bot_order) and places
its client entity at the given world origin with zero velocity, so the bot
stands at that point and its own AI takes over from there. Cheat-gated.

This exists for the deterministic playthrough harness: the seeded spawn +
physics settle can drop the bot onto a disconnected nav pocket, so the harness
places it on a known real, nav-connected start (info_player_start / start-floor)
before ordering it to the exit — a placement, not a launch, so the run measures
the real spawn->exit route from a connected component.

Called from ConsoleCommand() in g_svcmds.c.
*/
void BotDirective_ConsoleTeleport( const char *bot_name, vec3_t origin, float yaw ) {
    int   i;
    char  clean[MAX_NETNAME];

    if ( !bot_name || !bot_name[0] ) {
        Com_Log( SEV_INFO, LOG_CH(ch_game),
                 "Usage: bot_teleport <botname> <x> <y> <z> [yaw]\n" );
        return;
    }

    for ( i = 0; i < MAX_CLIENTS; i++ ) {
        gentity_t *ent;
        vec3_t     angles;

        if ( !botstates[i] || !botstates[i]->inuse ) {
            continue;
        }
        Q_strncpyz( clean, level.clients[i].pers.netname, sizeof( clean ) );
        Q_CleanStr( clean );
        if ( Q_stricmpn( clean, bot_name, strlen( bot_name ) ) != 0 ) {
            continue;
        }

        ent = &g_entities[i];
        if ( !ent->inuse || !ent->client ) {
            Com_Log( SEV_INFO, LOG_CH(ch_game),
                     "bot_teleport: bot '%s' has no live entity\n", bot_name );
            return;
        }

        /* Placement (not a launch): unlink, set origin, clear velocity, set the
           standing view yaw, sync the entity state, relink — the settle tail of
           TeleportPlayer without the exit-speed kick or knockback hold, so the
           bot is stationary at the point and immediately steerable. */
        VectorSet( angles, 0.0f, yaw, 0.0f );

        trap_UnlinkEntity( ent );

        VectorCopy( origin, ent->client->ps.origin );
        ent->client->ps.origin[2] += 1.0f;
        VectorClear( ent->client->ps.velocity );
        ent->client->ps.pm_time  = 0;
        ent->client->ps.pm_flags &= ~PMF_TIME_KNOCKBACK;
        SetClientViewAngle( ent, angles );

        /* not-lerp bit so the client does not interpolate across the jump */
        ent->client->ps.eFlags ^= EF_TELEPORT_BIT;

        BG_PlayerStateToEntityState( &ent->client->ps, &ent->s, qtrue );
        VectorCopy( ent->client->ps.origin, ent->r.currentOrigin );

        trap_LinkEntity( ent );

        Com_Log( SEV_INFO, LOG_CH(ch_game),
                 "bot_teleport: %s placed at (%.0f %.0f %.0f) yaw=%.0f\n",
                 level.clients[i].pers.netname,
                 ent->client->ps.origin[0], ent->client->ps.origin[1],
                 ent->client->ps.origin[2], yaw );
        return;
    }
    Com_Log( SEV_INFO, LOG_CH(ch_game),
             "bot_teleport: no active bot named '%s'\n", bot_name );
}

/* =========================================================================
   GOAL OVERRIDE
   ========================================================================= */

/*
==================
BotDirective_OverrideGoal
==================
If the active directive specifies a fixed position (DEFEND/CAMP), replaces
the passed goal with one pointing at the directive's area_origin.

Called from AINode_Seek_LTG when ltgtype matches.
*/
void BotDirective_OverrideGoal( bot_state_t *bs, bot_goal_t *goal ) {
    botDirective_t *d = &bs->directives.tactical;

    /* For seek-item directives, enforce the goal computed in FrameUpdate.
       BotLongTermGoal calls trap_BotGetLevelItemGoal independently each frame
       and may return a stale/empty goal when the item isn't in the AAS goal
       list.  Overriding here ensures our synthetic goal (including fallbacks
       built from entity world position) is what the movement engine uses. */
    if ( d->type == DIR_SEEK_ITEM && bs->directives.directiveLocked
         && bs->teamgoal.areanum > 0 ) {
        *goal = bs->teamgoal;
        return;
    }

    if ( d->type != DIR_DEFEND_AREA && d->type != DIR_CAMP_SPOT ) {
        return;
    }

    memset( goal, 0, sizeof( *goal ) );
    VectorCopy( d->area_origin, goal->origin );
    goal->areanum  = BotPointAreaNum( d->area_origin );
    goal->entitynum = -1;
    goal->flags    = 0;
}

/* =========================================================================
   ITEM SCORING
   ========================================================================= */

/*
==================
BotDirective_ScoreItem
==================
Adjusts the item desirability score based on the active directive and tactic.

Returns qtrue if the score was modified so that the caller can decide
whether to apply it.
*/
/* =========================================================================
   ANNOUNCEMENT SYSTEM
   ========================================================================= */

typedef struct {
    const char *text;       /* team-chat text; "" = voice-only              */
    qboolean    team_only;  /* qtrue = use trap_EA_SayTeam, else EA_Say     */
    float       cooldown;   /* per-type minimum seconds between firings     */
    const char *voice_id;   /* vsay_team / vsay command id; NULL = no voice */
} wbAnnounceEntry_t;

static const wbAnnounceEntry_t s_announcements[WI_ANNOUNCE_NUM_TYPES] = {
    /* WI_ACK_YES         */ { "Yes!",                qfalse,  2.0f, "yes"         },
    /* WI_STATUS_DEFENSE  */ { "On defense.",          qtrue,  15.0f, "imondefense" },
    /* WI_STATUS_INPOS    */ { "In position.",         qtrue,  10.0f, "inposition"  },
    /* WI_STATUS_GETFLAG  */ { "Going for the flag!",  qtrue,  15.0f, "ongetflag"   },
    /* WI_STATUS_RTNFLAG  */ { "Returning the flag.",  qtrue,  15.0f, "onreturnflag"},
    /* WI_STATUS_OFFENSE  */ { "On offense.",          qtrue,  15.0f, "imonoffense" },
    /* WI_STATUS_LEADER   */ { "I'm leading.",         qtrue,  30.0f, "startleader" },
    /* WI_TAUNT_GENERIC   */ { "",                     qtrue,  20.0f, "taunt"       },
    /* WI_TAUNT_PRAISE    */ { "Good game!",           qtrue,  30.0f, "taunt"       },
    /* WI_TAUNT_DEATH     */ { "",                     qtrue,  10.0f, "taunt"       },
    /* WI_TAUNT_KILL      */ { "",                     qtrue,  10.0f, "taunt"       },
    /* WI_ACK_UNKNOWN_TARGET */ { "",                 qtrue,   3.0f, NULL          },
};

static qboolean WiredIntel_ShouldAnnounce( wbAnnounceType_t type ) {
    /* Status / ack / rejection types always fire — functional information. */
    if ( type < WI_TAUNT_GENERIC || type == WI_ACK_UNKNOWN_TARGET ) {
        return qtrue;
    }
    /* Taunts are flavor — fire ~50 % of the time to avoid repetition. */
    return ( random() < 0.5f ) ? qtrue : qfalse;
}

/*
==================
WiredIntel_Announce
==================
Send a team-chat status message and (if FEAT_TA_VOICECHAT is enabled) a
matching voice command.  Cooldown tracking prevents spam.

context is reserved for future use (e.g., mentioning a player name).
*/
void WiredIntel_Announce( bot_state_t *bs, wbAnnounceType_t type, const char *context ) {
    const wbAnnounceEntry_t *entry;
    float now;

    if ( !bs || !bs->wiredIntelActive ) {
        return;
    }
    if ( type < 0 || type >= WI_ANNOUNCE_NUM_TYPES ) {
        return;
    }

    entry = &s_announcements[type];
    now   = FloatTime();

    /* per-type cooldown */
    if ( now - bs->directives.lastAnnounceTime[type] < entry->cooldown ) {
        return;
    }
    /* global cooldown: 2 s minimum gap between any two announcements */
    if ( now - bs->directives.lastAnnounceAnyTime < 2.0f ) {
        return;
    }

    if ( !WiredIntel_ShouldAnnounce( type ) ) {
        return;
    }

    /* stamp cooldowns before sending (prevents re-entry stacking) */
    bs->directives.lastAnnounceTime[type] = now;
    bs->directives.lastAnnounceAnyTime    = now;

    /* dynamic rejection: "Who is X?" */
    if ( type == WI_ACK_UNKNOWN_TARGET ) {
        char msg[MAX_SAY_TEXT];
        if ( context && context[0] ) {
            Com_sprintf( msg, sizeof(msg), "Who is %s?", context );
        } else {
            Q_strncpyz( msg, "Who?", sizeof(msg) );
        }
        if ( entry->team_only && TeamPlayIsOn() ) {
            trap_EA_SayTeam( bs->client, msg );
        } else {
            trap_EA_Say( bs->client, msg );
        }
        return;
    }

    /* text message */
    if ( entry->text && entry->text[0] ) {
        if ( entry->team_only && TeamPlayIsOn() ) {
            trap_EA_SayTeam( bs->client, (char *)entry->text );
        } else if ( !entry->team_only ) {
            trap_EA_Say( bs->client, (char *)entry->text );
        }
    }

    /* voice command */
    if ( entry->voice_id ) {
#if FEAT_TA_VOICECHAT
        if ( entry->team_only && TeamPlayIsOn() ) {
            trap_EA_Command( bs->client, va( "vsay_team %s", entry->voice_id ) );
        } else {
            trap_EA_Command( bs->client, va( "vsay %s", entry->voice_id ) );
        }
#endif
    }
}

/* =========================================================================
   ITEM SCORING
   ========================================================================= */

qboolean BotDirective_ScoreItem( bot_state_t *bs, bot_goal_t *goal, float *score_multiplier ) {
    botDirective_t *d = &bs->directives.tactical;

    if ( !goal || !score_multiplier ) {
        return qfalse;
    }

    /* RUSH_BASE / ATTACK_BASE: deprioritise all item pickups so the bot
       keeps moving toward the objective instead of detouring. */
    if ( d->type == DIR_RUSH_BASE || d->type == DIR_ATTACK_BASE ) {
        *score_multiplier *= 0.4f;
        return qtrue;
    }

    /* TACTIC_RUSH: same deprioritisation for item pickups */
    if ( bs->directives.tactic == TACTIC_RUSH && bs->directives.tactic_active ) {
        *score_multiplier *= 0.5f;
        return qtrue;
    }

    /* SEEK_ITEM: boost the item we're after; the classname check is a
       best-effort string match against the goal's entity classname. */
    if ( d->type == DIR_SEEK_ITEM && d->item_classname[0] ) {
        if ( goal->entitynum >= 0 && goal->entitynum < MAX_GENTITIES ) {
            const gentity_t *ent = &g_entities[goal->entitynum];
            if ( ent->classname &&
                 Q_stricmp( ent->classname, d->item_classname ) == 0 ) {
                *score_multiplier *= 3.0f;
                return qtrue;
            }
        }
    }

    return qfalse;
}

/* =========================================================================
   @ MENTION ADDRESSING SYSTEM
   ========================================================================= */

/* ── Private helpers ─────────────────────────────────────────────────── */

/* Case-insensitive prefix match against the bot's cleaned netname. */
static qboolean WiredIntel_NameMatches( bot_state_t *bs, const char *name ) {
    char clean[MAX_NETNAME];
    Q_strncpyz( clean, level.clients[bs->client].pers.netname, sizeof( clean ) );
    Q_CleanStr( clean );
    return ( Q_stricmpn( clean, name, strlen( name ) ) == 0 ) ? qtrue : qfalse;
}

/* Returns the client number of the enemy carrying OUR flag, or -1. */
static int BotEnemyFlagCarrier( bot_state_t *bs ) {
    int      myTeam, i;
    gentity_t *ent;

    if ( bs->client < 0 || bs->client >= MAX_CLIENTS ) {
        return -1;
    }
    if ( !g_entities[bs->client].client ) {
        return -1;
    }
    myTeam = g_entities[bs->client].client->sess.sessionTeam;

    for ( i = 0; i < level.maxclients; i++ ) {
        ent = &g_entities[i];
        if ( !ent->inuse || !ent->client ) {
            continue;
        }
        if ( level.clients[i].pers.connected != CON_CONNECTED ) {
            continue;
        }
        if ( ent->client->sess.sessionTeam == myTeam ) {
            continue;   /* skip own team */
        }
        /* Enemy carrying OUR flag */
        if ( myTeam == TEAM_RED ) {
            if ( ent->client->ps.powerups[PW_REDFLAG] ) {
                return i;
            }
        } else {
            if ( ent->client->ps.powerups[PW_BLUEFLAG] ) {
                return i;
            }
        }
    }
    return -1;
}

/* Returns the client number of the bot on sender's team closest to sender. */
static int WiredIntel_FindClosestBot( int senderClient ) {
    vec3_t senderOrigin, diff;
    int    senderTeam, i, best;
    float  bestDistSq, distSq;

    if ( senderClient < 0 || senderClient >= MAX_CLIENTS ) {
        return -1;
    }
    if ( !g_entities[senderClient].client ) {
        return -1;
    }
    VectorCopy( g_entities[senderClient].client->ps.origin, senderOrigin );
    senderTeam = g_entities[senderClient].client->sess.sessionTeam;

    best = -1;
    bestDistSq = 1e30f;

    for ( i = 0; i < MAX_CLIENTS; i++ ) {
        if ( !botstates[i] || !botstates[i]->inuse ) {
            continue;
        }
        if ( !g_entities[i].client ) {
            continue;
        }
        if ( g_entities[i].client->sess.sessionTeam != senderTeam ) {
            continue;
        }
        if ( level.clients[i].pers.connected != CON_CONNECTED ) {
            continue;
        }
        VectorSubtract( senderOrigin, g_entities[i].client->ps.origin, diff );
        distSq = DotProduct( diff, diff );
        if ( distSq < bestDistSq ) {
            bestDistSq = distSq;
            best = i;
        }
    }
    return best;
}

/* Returns the client number of the bot on sender's team farthest from sender. */
static int WiredIntel_FindFarthestBot( int senderClient ) {
    vec3_t senderOrigin, diff;
    int    senderTeam, i, best;
    float  bestDistSq, distSq;

    if ( senderClient < 0 || senderClient >= MAX_CLIENTS ) {
        return -1;
    }
    if ( !g_entities[senderClient].client ) {
        return -1;
    }
    VectorCopy( g_entities[senderClient].client->ps.origin, senderOrigin );
    senderTeam = g_entities[senderClient].client->sess.sessionTeam;

    best = -1;
    bestDistSq = -1.0f;

    for ( i = 0; i < MAX_CLIENTS; i++ ) {
        if ( !botstates[i] || !botstates[i]->inuse ) {
            continue;
        }
        if ( !g_entities[i].client ) {
            continue;
        }
        if ( g_entities[i].client->sess.sessionTeam != senderTeam ) {
            continue;
        }
        if ( level.clients[i].pers.connected != CON_CONNECTED ) {
            continue;
        }
        VectorSubtract( senderOrigin, g_entities[i].client->ps.origin, diff );
        distSq = DotProduct( diff, diff );
        if ( distSq > bestDistSq ) {
            bestDistSq = distSq;
            best = i;
        }
    }
    return best;
}

/*
==================
WiredIntel_ResolveMention  (internal)
==================
Resolves a recipient @mention string to a list of matching bot client numbers.
Returns a pointer to a static array; *count is set to the number of entries.
*/
static int *WiredIntel_ResolveMention( const char *mention, int senderClient, int *count ) {
    static int results[MAX_CLIENTS];
    const char *name;
    int         senderTeam, i;
    int         resolveTeam;   /* -1 = all bots; >=0 = filter by that team_t value */
    int         resolveSquad;  /*  0 = no squad filter; >0 = filter by that squad  */
    int         candidates[MAX_CLIENTS];
    int         numCandidates;

    *count = 0;

    if ( !mention || mention[0] != '@' ) {
        return results;
    }
    name = mention + 1;   /* skip '@' */

    senderTeam = TEAM_FREE;
    if ( senderClient >= 0 && senderClient < MAX_CLIENTS &&
         g_entities[senderClient].client ) {
        senderTeam = g_entities[senderClient].client->sess.sessionTeam;
    }

    /* ── prefix: @red:selector, @blue:selector, @free:selector, @squad:N:selector
       Parse an optional prefix before the selector keyword/name.
       Stateless clients and regular players can both use this to target
       a specific team.  When no prefix is present, stateless clients
       address all bots; regular players address only their own team.

       "squad:N" is the same prefix idiom on a SECOND, INDEPENDENT axis
       (gentity_t.squad) rather than a third team value: squad membership is
       explicitly allowed to cut across sessionTeam, so a squad prefix
       suppresses the team fallback below instead of narrowing within it.
       Otherwise a squad spanning two teams would be silently truncated to
       the sender's own side. */
    resolveTeam  = -2;   /* sentinel: not yet determined */
    resolveSquad = 0;    /* 0 = no squad filter */
    {
        const char *colon = strchr( name, ':' );
        if ( colon && colon > name ) {
            int  prefixLen = (int)( colon - name );
            char prefix[16];
            if ( prefixLen < (int)sizeof( prefix ) ) {
                Q_strncpyz( prefix, name, prefixLen + 1 );
                if      ( Q_stricmp( prefix, "red"  ) == 0 ) resolveTeam = TEAM_RED;
                else if ( Q_stricmp( prefix, "blue" ) == 0 ) resolveTeam = TEAM_BLUE;
                else if ( Q_stricmp( prefix, "free" ) == 0 ) resolveTeam = TEAM_FREE;
                else if ( Q_stricmp( prefix, "squad" ) == 0 ) {
                    /* "@squad:N:selector" — N is the squad number, and what
                       follows is an ordinary selector.  "@squad:N" alone (no
                       trailing selector) addresses the whole squad.  Squad 0 is
                       the no-squad sentinel and is never addressable: it would
                       mean "every entity that was never assigned a squad". */
                    const char *num = colon + 1;
                    const char *after;
                    resolveSquad = atoi( num );
                    if ( resolveSquad > 0 ) {
                        after = strchr( num, ':' );
                        if ( after ) {
                            name = after + 1;      /* selector follows the number */
                        } else {
                            name = "everyone";     /* bare "@squad:N" = whole squad */
                        }
                        resolveTeam = -1;          /* squad cuts across teams */
                    } else {
                        resolveSquad = 0;          /* malformed or squad 0 — no filter */
                    }
                }
            }
            if ( resolveTeam >= 0 ) {
                name = colon + 1;   /* advance past the "team:" prefix */
            }
        }
    }
    if ( resolveTeam == -2 ) {
        /* no valid team prefix found — fall back to per-sender rules */
        qboolean isStateless = ( senderClient >= 0 && senderClient < MAX_CLIENTS &&
                                  g_entities[senderClient].client &&
                                  g_entities[senderClient].client->sess.isStatelessClient );
        resolveTeam = isStateless ? -1 : (int)senderTeam;
    }

/* Iterates all bots, optionally filtered to resolveTeam and/or resolveSquad.
   resolveTeam  == -1 → no team filter (all bots).
   resolveSquad ==  0 → no squad filter.
   The two are independent axes; both apply when both are set.

   🔴 SCOPE LIMIT, by construction: this iterates i < MAX_CLIENTS and requires
   both botstates[i] and g_entities[i].client, so it addresses BOTS ONLY.
   Non-client behavior monsters carry gentity_t.squad too, but they can never
   be selected here — and that is correct for a DIRECTIVE selector, because a
   monster has no bot_state_t and does not take DIR_* orders. Addressing
   monsters would need a different delivery mechanism, not a wider iterator. */
#define WI_TEAM_BOT_ITER( body ) \
    for ( i = 0; i < MAX_CLIENTS; i++ ) { \
        if ( !botstates[i] || !botstates[i]->inuse ) continue; \
        if ( !g_entities[i].client ) continue; \
        if ( resolveTeam >= 0 && \
             g_entities[i].client->sess.sessionTeam != (team_t)resolveTeam ) continue; \
        if ( resolveSquad > 0 && g_entities[i].squad != resolveSquad ) continue; \
        if ( level.clients[i].pers.connected != CON_CONNECTED ) continue; \
        body \
    }

    /* ── group keywords ──────────────────────────────────────────────── */
    if ( Q_stricmp( name, "everyone" ) == 0 ||
         Q_stricmp( name, "all" )      == 0 ||
         Q_stricmp( name, "team" )     == 0 ) {
        WI_TEAM_BOT_ITER( results[(*count)++] = i; )
        return results;
    }

    if ( Q_stricmp( name, "closest" ) == 0 ) {
        int best = WiredIntel_FindClosestBot( senderClient );
        if ( best >= 0 ) {
            results[(*count)++] = best;
        }
        return results;
    }

    if ( Q_stricmp( name, "farthest" ) == 0 ) {
        int best = WiredIntel_FindFarthestBot( senderClient );
        if ( best >= 0 ) {
            results[(*count)++] = best;
        }
        return results;
    }

    if ( Q_stricmp( name, "random" )    == 0 ||
         Q_stricmp( name, "somebody" )  == 0 ||
         Q_stricmp( name, "anyone" )    == 0 ) {
        numCandidates = 0;
        WI_TEAM_BOT_ITER( candidates[numCandidates++] = i; )
        if ( numCandidates > 0 ) {
            results[(*count)++] = candidates[ (int)( random() * numCandidates ) % numCandidates ];
        }
        return results;
    }

    if ( Q_stricmp( name, "defenders" ) == 0 ) {
        WI_TEAM_BOT_ITER(
            if ( botstates[i]->directives.preference & TEAMTP_DEFENDER ) {
                results[(*count)++] = i;
            }
        )
        return results;
    }

    if ( Q_stricmp( name, "attackers" ) == 0 ) {
        WI_TEAM_BOT_ITER(
            if ( botstates[i]->directives.preference & TEAMTP_ATTACKER ) {
                results[(*count)++] = i;
            }
        )
        return results;
    }

    if ( Q_stricmp( name, "idle" ) == 0 ) {
        WI_TEAM_BOT_ITER(
            if ( botstates[i]->directives.tactical.type == DIR_NONE &&
                 botstates[i]->enemy < 0 ) {
                results[(*count)++] = i;
            }
        )
        return results;
    }

#undef WI_TEAM_BOT_ITER

    /* ── named bot — prefix match against cleaned netname ────────────── */
    for ( i = 0; i < MAX_CLIENTS; i++ ) {
        if ( !botstates[i] || !botstates[i]->inuse ) {
            continue;
        }
        if ( level.clients[i].pers.connected != CON_CONNECTED ) {
            continue;
        }
        if ( resolveTeam >= 0 && g_entities[i].client &&
             g_entities[i].client->sess.sessionTeam != (team_t)resolveTeam ) {
            continue;
        }
        /* Same second axis as WI_TEAM_BOT_ITER: "@squad:N:name" addresses that
           bot only if it is actually in squad N. */
        if ( resolveSquad > 0 && g_entities[i].squad != resolveSquad ) {
            continue;
        }
        if ( WiredIntel_NameMatches( botstates[i], name ) ) {
            results[(*count)++] = i;
            return results;   /* name match is always a single bot */
        }
    }

    return results;
}

/*
==================
WiredIntel_ResolveTargetMention  (internal)
==================
Resolves a target @mention within an order body to a single client number.
Returns -1 if unresolvable.
*/
static int WiredIntel_ResolveTargetMention( bot_state_t *bs, const char *mention ) {
    const char *name;

    if ( !mention || mention[0] != '@' ) {
        return -1;
    }
    name = mention + 1;

    if ( Q_stricmp( name, "carrier" ) == 0 ) {
        return BotTeamFlagCarrier( bs );
    }
    if ( Q_stricmp( name, "enemycarrier" ) == 0 ) {
        return BotEnemyFlagCarrier( bs );
    }
    if ( Q_stricmp( name, "leader" ) == 0 ) {
        if ( bs->directives.teamleader[0] ) {
            return ResolveClientByName( bs->directives.teamleader );
        }
        return -1;
    }

    /* Any connected player — any team */
    return ResolveClientByName( name );
}

/*
==================
WiredIntel_ExtractMention  (internal)
==================
Extracts the @word at the start of text into mention[].
Returns pointer to the remainder of the string (the order body).
Returns NULL if text does not start with '@'.
*/
static const char *WiredIntel_ExtractMention( const char *text, char *mention, int mentionSize ) {
    int i;

    if ( !text || text[0] != '@' ) {
        return NULL;
    }

    i = 0;
    while ( text[i] && text[i] != ' ' && i < mentionSize - 1 ) {
        mention[i] = text[i];
        i++;
    }
    mention[i] = '\0';

    /* skip whitespace between mention and order */
    while ( text[i] == ' ' ) {
        i++;
    }
    return &text[i];
}

/*
==================
WiredIntel_ExtractTargetMention  (internal)
==================
Scans order text for a @word that is at a word boundary (preceded by space
or at string start).  Extracts it into targetMention[] and writes the
cleaned order text (without the @word) to cleanOrder[].
*/
static void WiredIntel_ExtractTargetMention( const char *order,
                                             char *cleanOrder, int cleanSize,
                                             char *targetMention, int targetSize ) {
    const char *at, *p, *s;
    int         ci, ti;

    targetMention[0] = '\0';

    /* Find first @ that is at a word boundary */
    at = NULL;
    for ( p = order; *p; p++ ) {
        if ( *p == '@' && ( p == order || *(p - 1) == ' ' ) ) {
            at = p;
            break;
        }
    }

    if ( !at ) {
        Q_strncpyz( cleanOrder, order, cleanSize );
        return;
    }

    /* Extract @word */
    p = at;
    ti = 0;
    while ( *p && *p != ' ' && ti < targetSize - 1 ) {
        targetMention[ti++] = *p++;
    }
    targetMention[ti] = '\0';

    /* Build clean order: text before @ (strip trailing space) + text after @word */
    ci = 0;
    for ( s = order; s < at && ci < cleanSize - 1; s++ ) {
        cleanOrder[ci++] = *s;
    }
    while ( ci > 0 && cleanOrder[ci - 1] == ' ' ) {
        ci--;
    }
    /* skip spaces after @word, then append remainder */
    while ( *p == ' ' ) {
        p++;
    }
    if ( *p && ci < cleanSize - 1 ) {
        cleanOrder[ci++] = ' ';
        while ( *p && ci < cleanSize - 1 ) {
            cleanOrder[ci++] = *p++;
        }
    }
    cleanOrder[ci] = '\0';
}

/*
==================
WiredIntel_BuildFinalOrder  (internal)
==================
Combines a clean verb with a resolved target to produce the final order
string for BotReceiveDirective.

Special cases:
  "attack <player>"   → "kill <player>"   (bare "attack" = DIR_ATTACK_BASE,
                                           we want DIR_KILL_TARGET)
  any verb + "flag carrier" keyword
                      → "<verb> flag carrier"
*/
static void WiredIntel_BuildFinalOrder( const char *cleanOrder,
                                        const char *targetMention,
                                        bot_state_t *bs,
                                        char *finalOrder, int finalSize ) {
    char  verb[64];
    const char *space;
    int   verbLen;
    int   targetClient;
    char  targetStr[MAX_NETNAME];
    qboolean isFollowVerb;

    if ( !targetMention || !targetMention[0] ) {
        Q_strncpyz( finalOrder, cleanOrder, finalSize );
        return;
    }

    /* extract verb (first word of cleanOrder) */
    space = strchr( cleanOrder, ' ' );
    if ( space ) {
        verbLen = (int)( space - cleanOrder );
        if ( verbLen >= (int)sizeof( verb ) ) {
            verbLen = (int)sizeof( verb ) - 1;
        }
        memcpy( verb, cleanOrder, verbLen );
        verb[verbLen] = '\0';
    } else {
        Q_strncpyz( verb, cleanOrder, sizeof( verb ) );
    }

    isFollowVerb = ( Q_stricmp( verb, "follow" )  == 0 ||
                     Q_stricmp( verb, "escort" )  == 0 ||
                     Q_stricmp( verb, "protect" ) == 0 ) ? qtrue : qfalse;

    /* @carrier + follow-like verb → use the "flag carrier" sentinel so
       BotReceiveDirective sets dynamic re-targeting in FrameUpdate */
    if ( isFollowVerb && Q_stricmp( targetMention + 1, "carrier" ) == 0 ) {
        Com_sprintf( finalOrder, finalSize, "%s flag carrier", verb );
        return;
    }

    /* resolve target to a client number */
    targetClient = WiredIntel_ResolveTargetMention( bs, targetMention );
    if ( targetClient < 0 ) {
        /* target not found — issue order without target rather than silently dropping */
        Q_strncpyz( finalOrder, cleanOrder, finalSize );
        return;
    }

    /* get target's clean display name */
    ClientName( targetClient, targetStr, sizeof( targetStr ) );
    Q_CleanStr( targetStr );
    if ( !targetStr[0] ) {
        Q_strncpyz( finalOrder, cleanOrder, finalSize );
        return;
    }

    /* verb → order string mapping */
    if ( Q_stricmp( verb, "attack" ) == 0 ) {
        /* "attack @X" has no player-target syntax in BotReceiveDirective,
           but "kill <name>" does (DIR_KILL_TARGET) */
        Com_sprintf( finalOrder, finalSize, "kill %s", targetStr );
    } else {
        Com_sprintf( finalOrder, finalSize, "%s %s", verb, targetStr );
    }
}

/* ── Public API ──────────────────────────────────────────────────────── */

/*
==================
WiredIntel_ProcessChat
==================
Central entry point for @ mention directive dispatch.  Called once per
chat message from G_Say BEFORE the text is relayed to clients.

  1.  If the message starts with '@', extracts the recipient mention and
      the order body.
  2.  Resolves the recipient to a list of bot client numbers.
  3.  Optionally resolves a target '@' mention within the order body.
  4.  Issues the directive to each recipient via BotReceiveDirective.
  5.  Populates *result for the caller (G_Say uses it to colorize the
      displayed text).

Messages without a leading '@' are left for the per-bot legacy path in
BotDirective_ParseChatOrder; result->hasMentions is set to qfalse.
*/
void WiredIntel_ProcessChat( int senderClient, const char *message,
                             wbParseResult_t *result ) {
    char        recipientMention[MAX_NETNAME];
    char        targetMention[MAX_NETNAME];
    char        cleanOrder[256];
    char        finalOrder[256];
    const char *orderStart;
    int        *recipients;
    int         count, i;

    memset( result, 0, sizeof( *result ) );

    if ( !message || message[0] != '@' ) {
        return;
    }

    /* extract recipient @mention from the start of the message */
    orderStart = WiredIntel_ExtractMention( message, recipientMention,
                                           sizeof( recipientMention ) );
    if ( !orderStart || !orderStart[0] ) {
        /* bare '@mention' with no order — still colorize but no directive */
        if ( recipientMention[0] ) {
            result->hasMentions = qtrue;
            Q_strncpyz( result->recipientMention, recipientMention,
                        sizeof( result->recipientMention ) );
        }
        return;
    }

    /* resolve recipients */
    recipients = WiredIntel_ResolveMention( recipientMention, senderClient, &count );
    if ( count == 0 ) {
        return;
    }

    /* fill result */
    result->hasMentions  = qtrue;
    result->numRecipients = count;
    Q_strncpyz( result->recipientMention, recipientMention,
                sizeof( result->recipientMention ) );
    for ( i = 0; i < count && i < MAX_CLIENTS; i++ ) {
        result->recipientClients[i] = recipients[i];
    }

    /* extract target @mention from order body (if any) */
    WiredIntel_ExtractTargetMention( orderStart, cleanOrder, sizeof( cleanOrder ),
                                    targetMention, sizeof( targetMention ) );
    if ( targetMention[0] ) {
        Q_strncpyz( result->targetMention, targetMention,
                    sizeof( result->targetMention ) );
    }

    /* deliver directive to each recipient */
    for ( i = 0; i < count; i++ ) {
        bot_state_t *bs = botstates[recipients[i]];
        if ( !bs || !bs->inuse ) {
            continue;
        }

        if ( targetMention[0] ) {
            WiredIntel_BuildFinalOrder( cleanOrder, targetMention, bs,
                                       finalOrder, sizeof( finalOrder ) );
        } else {
            Q_strncpyz( finalOrder, cleanOrder, sizeof( finalOrder ) );
        }

        if ( finalOrder[0] ) {
            BotReceiveDirective( bs, senderClient, finalOrder );
        }
    }
}

/*
==================
WiredIntel_ColorizeMentions
==================
Colorizes @mentions in chat text for display to human clients.

  Recipient @mentions → ^5 (cyan)
  Target @mentions    → ^3 (yellow)

Only @words at word boundaries are colorized (same rule as extraction).
Output is guaranteed NUL-terminated and at most outputSize-1 characters.
*/
void WiredIntel_ColorizeMentions( const char *input, char *output, int outputSize,
                                  const char *recipientMention,
                                  const char *targetMention ) {
    const char *p;
    int         o;
    char        mention[MAX_NETNAME];
    int         mi;
    const char *color;

    if ( !input || !output || outputSize <= 0 ) {
        if ( output && outputSize > 0 ) {
            output[0] = '\0';
        }
        return;
    }

    o = 0;
    p = input;

    while ( *p && o < outputSize - 12 ) {
        /* detect @ at a word boundary */
        if ( *p == '@' && ( p == input || *(p - 1) == ' ' ) ) {
            /* extract the @word */
            mi = 0;
            while ( *p && *p != ' ' && mi < (int)sizeof( mention ) - 1 ) {
                mention[mi++] = *p++;
            }
            mention[mi] = '\0';

            /* pick color: recipient (cyan) or target (yellow) */
            if ( recipientMention && Q_stricmp( mention, recipientMention ) == 0 ) {
                color = "^5";
            } else if ( targetMention && targetMention[0] &&
                        Q_stricmp( mention, targetMention ) == 0 ) {
                color = "^3";
            } else {
                color = "^5";   /* unknown @ defaults to cyan */
            }

            /* write: ^color @word ^7 */
            output[o++] = color[0];
            output[o++] = color[1];
            {
                const char *s;
                for ( s = mention; *s && o < outputSize - 4; s++ ) {
                    output[o++] = *s;
                }
            }
            output[o++] = '^';
            output[o++] = '7';
        } else {
            output[o++] = *p++;
        }
    }

    output[o] = '\0';
}
