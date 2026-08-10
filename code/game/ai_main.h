// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//

/*****************************************************************************
 * name:		ai_main.h
 *
 * desc:		Quake3 bot AI
 *
 * $Archive: /source/code/botai/ai_chat.c $
 *
 *****************************************************************************/

//#define DEBUG
#define CTF

#include "../qcommon/q_feats.h"
#include "wired/bots/g_wiredintel.h"
#include "wired/bots/g_belief.h"
#include "wired/bots/g_sequenced_goal.h"
#include "ai_movement.h"

#define MAX_ITEMS					256
//bot flags
#define BFL_STRAFERIGHT				1	//strafe to the right
#define BFL_ATTACKED				2	//bot has attacked last ai frame
#define BFL_ATTACKJUMPED			4	//bot jumped during attack last frame
#define BFL_AIMATENEMY				8	//bot aimed at the enemy this frame
#define BFL_AVOIDRIGHT				16	//avoid obstacles by going to the right
#define BFL_IDEALVIEWSET			32	//bot has ideal view angles set
#define BFL_FIGHTSUICIDAL			64	//bot is in a suicidal fight
//long term goal types
#define LTG_TEAMHELP				1	//help a team mate
#define LTG_TEAMACCOMPANY			2	//accompany a team mate
#define LTG_DEFENDKEYAREA			3	//defend a key area
#define LTG_GETFLAG					4	//get the enemy flag
#define LTG_RUSHBASE				5	//rush to the base
#define LTG_RETURNFLAG				6	//return the flag
#define LTG_CAMP					7	//camp somewhere
#define LTG_CAMPORDER				8	//ordered to camp somewhere
#define LTG_PATROL					9	//patrol
#define LTG_GETITEM					10	//get an item
#define LTG_KILL					11	//kill someone
#define LTG_HARVEST					12	//harvest skulls
#define LTG_ATTACKENEMYBASE			13	//attack the enemy base
#define LTG_MAKELOVE_UNDER			14
#define LTG_MAKELOVE_ONTOP			15
//some goal dedication times
#define TEAM_HELP_TIME				60	//1 minute teamplay help time
#define TEAM_ACCOMPANY_TIME			600	//10 minutes teamplay accompany time
#define TEAM_DEFENDKEYAREA_TIME		600	//10 minutes ctf defend base time
#define TEAM_CAMP_TIME				600	//10 minutes camping time
#define TEAM_PATROL_TIME			600	//10 minutes patrolling time
#define TEAM_LEAD_TIME				600	//10 minutes taking the lead
#define TEAM_GETITEM_TIME			60	//1 minute
#define	TEAM_KILL_SOMEONE			180	//3 minute to kill someone
#define TEAM_ATTACKENEMYBASE_TIME	600	//10 minutes
#define TEAM_HARVEST_TIME			120	//2 minutes
#define CTF_GETFLAG_TIME			600	//10 minutes ctf get flag time
#define CTF_RUSHBASE_TIME			120	//2 minutes ctf rush base time
#define CTF_RETURNFLAG_TIME			180	//3 minutes to return the flag
#define CTF_ROAM_TIME				60	//1 minute ctf roam time
// Battle_Fight: how long (ms) the enemy can stay out of sight before the bot
// gives up combat. Matches Battle_Retreat/Chase which use a 4-second window;
// this is shorter because brief occlusion (pillars, doorways) is common. Two
// consumers carry the SAME duration: (1) the Fight->Seek_LTG cluster-exit still
// compares enemyvisible_time against this in AINode_Battle_Fight (the bot leaves
// combat to nav when it loses sight and does NOT want to chase); (2) the
// Fight/Chase sub-composite uses it as its incumbent-hysteresis margin so the
// Fight->Chase transition holds through the same grace window before Chase
// displaces Fight (see AI_BuildRootComposite / fightChaseComposite).
#define BATTLE_FIGHT_VIS_GRACE_MS	500	//0.5 seconds — hysteresis against single-tick occlusion
//patrol flags
#define PATROL_LOOP					1
#define PATROL_REVERSE				2
#define PATROL_BACK					4
//teamplay task preference
#define TEAMTP_DEFENDER				1
#define TEAMTP_ATTACKER				2
//CTF strategy
#define CTFS_AGRESSIVE				1
//copied from the aas file header
#define PRESENCE_NONE				1
#define PRESENCE_NORMAL				2
#define PRESENCE_CROUCH				4

// ── bot AI improvements: shared types ─────────────────────────────────

// entity reference with type validation (guards against entity slot reuse)
typedef struct {
	int		entnum;		// entity slot number
	int		eType;		// expected entity type (ET_MISSILE, ET_ITEM, ET_PLAYER)
} tracked_ent_t;

#define MAX_MISSILE_DODGE		8	// max tracked missiles per bot per frame
#define MAX_AWARE_ENTITIES		12	// max entities in awareness list

// awareness entry
typedef struct {
	tracked_ent_t	ent;
	float			first_noted;	// level.time when first detected
	float			react_time;		// level.time when bot can act on this
	qboolean		visual;			// qtrue if visually confirmed
} bot_aware_t;

// per-weapon accuracy tracking (per combat zone)
#define ZONE_NEAR		0	// < 300 units
#define ZONE_MID		1	// < 700
#define ZONE_FAR		2	// < 1200
#define ZONE_VERYFAR	3	// 1200+
#define NUM_ZONES		4

typedef struct {
	int		shots;
	int		hits;
	int		damage;
} bot_accuracy_t;

// item timing entry
#define MAX_TIMED_ITEMS		32

typedef struct {
	tracked_ent_t	ent;
	int				itemType;		// IT_* item type
	vec3_t			origin;			// item origin
	int				areaNum;		// AAS area
	int				respawnTime;	// respawn interval in ms
	int				pickupTime;		// level.time when picked up (0 = available or unknown)
} bot_itemtime_t;

//check points
typedef struct bot_waypoint_s
{
	int			inuse;
	char		name[32];
	bot_goal_t	goal;
	struct		bot_waypoint_s *next, *prev;
} bot_waypoint_t;

// ── AI composite tree substrate ───────────────────────────────────────────
//
// The bot AI is a flat FSM of function-pointer nodes: each leaf has the
// signature `int AINode_X(bot_state_t *bs)` and returns qtrue (done this
// frame) / qfalse (transitioned, run the successor next). The composite tree
// generalizes that: an inner node holds children and arbitrates which
// child(ren) run this tick, while remaining callable through the SAME
// qtrue/qfalse contract as a leaf — so composites nest arbitrarily.
//
// Unified callable contract:
//   Every node — leaf or composite — is an aiNode_t with a `tick` function of
//   type aiNodeTick_t: int tick(bot_state_t *bs, aiNode_t *self). It returns
//   the leaf qtrue/qfalse semantics. A LEAF wraps a plain AINode_X via
//   AI_LeafTick, which calls node->u.leaf.fn(bs). A COMPOSITE runs AI_CompositeTick,
//   which selects and runs its children per its kind. Because both expose the
//   same tick signature, a composite can be a child of another composite with
//   no special case.
//
// A node's precondition is an optional predicate int pre(bot_state_t *bs,
// aiNode_t *self) returning qtrue when the node is eligible this tick. A NULL
// precondition means "always eligible".

struct aiNode_s;

// unified callable contract: leaf and composite tick share this signature
typedef int (*aiNodeTick_t)(struct bot_state_s *bs, struct aiNode_s *self);
// optional per-node eligibility predicate (NULL = always eligible)
typedef int (*aiNodePre_t)(struct bot_state_s *bs, struct aiNode_s *self);

// composite arbitration kinds (domain names — not landing labels)
typedef enum {
	AI_COMPOSITE_PRIORITIZED,	// first eligible child in priority order, with incumbent-hysteresis
	AI_COMPOSITE_SIMULTANEOUS,	// run every eligible child this tick
	AI_COMPOSITE_FIRST_AVAILABLE	// run the first eligible child, short-circuit (no hysteresis)
} aiCompositeKind_t;

#define AI_MAX_COMPOSITE_CHILDREN	8

typedef struct aiComposite_s {
	aiCompositeKind_t	kind;
	struct aiNode_s		*children[AI_MAX_COMPOSITE_CHILDREN];
	int					numChildren;
	// PRIORITIZED incumbent-hysteresis state:
	//   incumbent = index of the child chosen last tick (-1 = none yet).
	//   The incumbent keeps the slot while its precondition still passes; a
	//   higher-priority challenger only displaces it once the challenger has
	//   been eligible continuously for hysteresisMargin ms (measured from
	//   challengeStart). This damps per-tick thrashing between two nodes whose
	//   preconditions flicker on the same borderline.
	int					incumbent;
	int					challenger;		// index of the pending higher-priority challenger (-1 = none)
	float				challengeStart;	// FloatTime() when the current challenger first became eligible
	float				hysteresisMargin;	// ms the challenger must persist before it displaces the incumbent
} aiComposite_t;

typedef struct aiNode_s {
	aiNodeTick_t	tick;		// unified callable: how this node runs
	aiNodePre_t		pre;		// optional eligibility predicate (NULL = always)
	const char		*name;		// debug label
	union {
		// leaf: wraps an existing AINode_X function-pointer
		struct { int (*fn)(struct bot_state_s *bs); } leaf;
		// composite: children + arbitration
		aiComposite_t composite;
	} u;
} aiNode_t;

//bot state
typedef struct bot_state_s
{
	int inuse;										//true if this state is used by a bot client
	int botthink_residual;							//residual for the bot thinks
	int client;										//client number of the bot
	int entitynum;									//entity number of the bot
	playerState_t cur_ps;							//current player state
	int last_eFlags;								//last ps flags
	usercmd_t lastucmd;								//usercmd from last frame
	int entityeventTime[MAX_GENTITIES];				//last entity event time
	//
	bot_settings_t settings;						//several bot settings
	int (*ainode)(struct bot_state_s *bs);			//current AI node
	float thinktime;								//time the bot thinks this frame
	vec3_t origin;									//origin of the bot
	vec3_t velocity;								//velocity of the bot
	int presencetype;								//presence type of the bot
	vec3_t eye;										//eye coordinates of the bot
	int areanum;									//the number of the area the bot is in
	int inventory[MAX_ITEMS];						//string with items amounts the bot has
	int tfl;										//the travel flags the bot uses
	int flags;										//several flags
	int respawn_wait;								//wait until respawned
	int lasthealth;									//health value previous frame
	int lastkilledplayer;							//last killed player
	int lastkilledby;								//player that last killed this bot
	int botdeathtype;								//the death type of the bot
	int enemydeathtype;								//the death type of the enemy
	int botsuicide;									//true when the bot suicides
	int enemysuicide;								//true when the enemy of the bot suicides
	int setupcount;									//true when the bot has just been setup
	int map_restart;									//true when the map is being restarted
	int entergamechat;								//true when the bot used an enter game chat
	qboolean wiredIntelActive;						// true when character comes from BOTLUA path
	int num_deaths;									//number of time this bot died
	int num_kills;									//number of kills of this bot
	int current_streak;								//consecutive kill streak (reset on death)
	int prev_rank;									//rank at last milestone check
	float last_kill_time;							//time of last kill (for double-kill window)
	float last_streak_ack;							//time the last streak chat fired
	int revenge_enemy;								//the revenge enemy
	int revenge_kills;								//number of kills the enemy made
	int lastframe_health;							//health value the last frame
	int lasthitcount;								//number of hits last frame
	int chatto;										//chat to all or team
	float walker;									//walker charactertic
	float luaSkillNormalized;
	float ltime;									//local bot time
	float entergame_time;							//time the bot entered the game
	float ltg_time;									//long term goal time
	float nbg_time;									//nearby goal time
	float respawn_time;								//time the bot takes to respawn
	float respawnchat_time;							//time the bot started a chat during respawn
	float chase_time;								//time the bot will chase the enemy
	float enemyvisible_time;						//time the enemy was last visible
	float check_time;								//time to check for nearby items
	float stand_time;								//time the bot is standing still
	float lastchat_time;							//time the bot last selected a chat
	float kamikaze_time;							//time to check for kamikaze usage
	float deflector_time;							//time to check for deflector usage
	float standfindenemy_time;						//time to find enemy while standing
	float attackstrafe_time;						//time the bot is strafing in one dir
	float attackcrouch_time;						//time the bot will stop crouching
	float attackchase_time;							//time the bot chases during actual attack
	float attackjump_time;							//time the bot jumped during attack
	float enemysight_time;							//time before reacting to enemy
	float enemydeath_time;							//time the enemy died
	float enemyposition_time;						//time the position and velocity of the enemy were stored
	float defendaway_time;							//time away while defending
	float defendaway_range;							//max travel time away from defend area
	float rushbaseaway_time;						//time away from rushing to the base
	float attackaway_time;							//time away from attacking the enemy base
	float harvestaway_time;							//time away from harvesting
	float ctfroam_time;								//time the bot is roaming in ctf
	float killedenemy_time;							//time the bot killed the enemy
	float arrive_time;								//time arrived (at companion)
	float lastair_time;								//last time the bot had air
	float teleport_time;							//last time the bot teleported
	float camp_time;								//last time camped
	float weaponchange_time;						//time the bot started changing weapons
	float firethrottlewait_time;					//amount of time to wait
	float firethrottleshoot_time;					//amount of time to shoot
	float notblocked_time;							//last time the bot was not blocked
	float blockedbyavoidspot_time;					//time blocked by an avoid spot
	float predictobstacles_time;					//last time the bot predicted obstacles
	int predictobstacles_goalareanum;				//last goal areanum the bot predicted obstacles for
	vec3_t aimtarget;
	vec3_t enemyvelocity;							//enemy velocity 0.5 secs ago during battle
	vec3_t enemyorigin;								//enemy origin 0.5 secs ago during battle
	//
	int kamikazebody;								//kamikaze body
	//
	int character;									//the bot character
	int ms;											//move state of the bot
	int gs;											//goal state of the bot
	int cs;											//chat state of the bot
	int ws;											//weapon state of the bot
	//
	int enemy;										//enemy entity number
	int lastenemyareanum;							//last reachability area the enemy was in
	vec3_t lastenemyorigin;							//last origin of the enemy in the reachability area
	int weaponnum;									//current weapon number
	vec3_t viewangles;								//current view angles
	vec3_t ideal_viewangles;						//ideal view angles
	vec3_t viewanglespeed;
	//
	int ltgtype;									//long term goal type
	// team goals
	int teammate;									//team mate involved in this team goal
	int decisionmaker;								//player who decided to go for this goal
	int ordered;									//true if ordered to do something
	float order_time;								//time ordered to do something
	int owndecision_time;							//time the bot made its own decision
	bot_goal_t teamgoal;							//the team goal
	bot_goal_t altroutegoal;						//alternative route goal
	float reachedaltroutegoal_time;					//time the bot reached the alt route goal
	float teammessage_time;							//time to message team mates what the bot is doing
	float teamgoal_time;							//time to stop helping team mate
	float teammatevisible_time;						//last time the team mate was NOT visible
	/* teamtaskpreference moved to bs->directives.preference */
	// last ordered team goal
	int lastgoal_decisionmaker;
	int lastgoal_ltgtype;
	int lastgoal_teammate;
	bot_goal_t lastgoal_teamgoal;
	// for leading team mates
	int lead_teammate;								//team mate the bot is leading
	bot_goal_t lead_teamgoal;						//team goal while leading
	float lead_time;								//time leading someone
	float leadvisible_time;							//last time the team mate was visible
	float leadmessage_time;							//last time a messaged was sent to the team mate
	float leadbackup_time;							//time backing up towards team mate
	//
	/* teamleader moved to bs->directives.teamleader */
	float askteamleader_time;						//time asked for team leader
	float becometeamleader_time;					//time the bot will become the team leader
	float teamgiveorders_time;						//time to give team orders
	float lastflagcapture_time;						//last time a flag was captured
	int numteammates;								//number of team mates
	int redflagstatus;								//0 = at base, 1 = not at base
	int blueflagstatus;								//0 = at base, 1 = not at base
	int neutralflagstatus;							//0 = at base, 1 = our team has flag, 2 = enemy team has flag, 3 = enemy team dropped the flag
	int flagstatuschanged;							//flag status changed
	int forceorders;								//true if forced to give orders
	int flagcarrier;								//team mate carrying the enemy flag
	int ctfstrategy;								//ctf strategy
	/* subteam moved to bs->directives.subteam */
	float formation_dist;							//formation team mate intervening space

	bot_waypoint_t *checkpoints;					//check points
	bot_waypoint_t *patrolpoints;					//patrol points
	bot_waypoint_t *curpatrolpoint;					//current patrol point the bot is going for
	int patrolflags;								//patrol flags

	// ── missile avoidance ─────────────────────────────────────────────
	tracked_ent_t	missile_dodge[MAX_MISSILE_DODGE];
	int				num_missiles;			// number of tracked missiles this frame

	// ── entity awareness ──────────────────────────────────────────────
	bot_aware_t		aware[MAX_AWARE_ENTITIES];
	int				num_aware;				// current entries in awareness list

	// ── strafejumping ─────────────────────────────────────────────────
	vec3_t			strafejump_angles;		// ideal view angles for strafejump
	qboolean		strafejump_active;		// currently strafejumping
	int				strafejump_side;		// alternating left(-1) / right(1)
	qboolean		strafejump_landed;		// qtrue after side flip on this landing

	// ── advanced movement ─────────────────────────────────────────────
	bot_doublejump_t	doublejump;			// double-jump sequence state
	bot_walljump_t		walljump;			// wall-jump detection/execution state
	float			edge_block_until;		// FloatTime() deadline: back away from detected void edge
	vec3_t			last_ground_pos;		// last position where bot was on solid ground (for void avoid spots)
	vec3_t			void_spots[8];			// persistent ledge positions that caused void deaths this session
	int			num_void_spots;			// number of entries in void_spots[]

	// ── item respawn timing ───────────────────────────────────────────
	bot_itemtime_t	timed_items[MAX_TIMED_ITEMS];
	int				num_timed_items;		// registered items
	qboolean		items_initialized;		// qtrue after first scan

	// ── dynamic weapon selection ──────────────────────────────────────
	bot_accuracy_t	accuracy[MAX_WEAPONS][NUM_ZONES];	// per-weapon per-zone stats
	int				best_weapon;			// currently selected weapon (DPS-based)
	int				weapon_reason;			// 0=default, 1=dps, 2=ammo, 3=range

	// ── auto-calibration ──────────────────────────────────────────────
	float			autoskill;				// floating-point skill [1.0 - 5.0]
	float			autoskill_time;			// next evaluation time
	int				kills_vs_humans;		// kills against human players (sliding window)
	int				deaths_vs_humans;		// deaths from human players (sliding window)
	float			autoskill_window_start;	// start of current K/D window

	// ── dodge direction for current frame ─────────────────────────────
	vec3_t			dodge_dir;				// computed dodge direction (zero if no dodge)
	qboolean		dodge_active;			// qtrue if dodging this frame

	// ── WiredIntel directive state ─────────────────────────────────────
	botDirectiveState_t directives;			// active tactical directive and tactic

	// ── Belief store (per-brain addressable world-knowledge) ───────────
	// The single addressable home for the bot's world-knowledge: discovered
	// interactables (buttons/gates — backs the directive activation queue),
	// last-seen threats (mirrors the enemy-tracking fields), and a bounded
	// visited-area ring (the new explored-set producer). The item-belief is a
	// VIEW over the shared, event-driven item-goal DB (s_mapGoalDb), not stored
	// here. Game-VM-internal (never crosses the botlib/engine ABI). This is the
	// seam the goal tree and the future coroutine layer query; the existing
	// subsystems are its first clients. See wired/bots/g_belief.h.
	beliefStore_t     beliefStore;

	// ── Sequenced-goal cursor state (per-brain, own storage) ───────────
	// Multi-step goals whose steps run across frames with wait-until gating,
	// driven by WiredIntel_StepSequencedGoal before BotAI each tick. Generalizes
	// the G_RunScriptDispatcher cursor+yield+advance+wait-until substrate to bot
	// goals; steps write the goal through the directive projection (ltgtype/
	// teamgoal + directiveLocked), never trap_BotPushGoal. Kept in the brain's
	// OWN storage — NOT the botlib goal stack bs->gs — so a combat-edge goal-
	// stack clear cannot discard an in-flight sequence. Game-VM-internal (no
	// syscall/enum/wire/savegame). Idle by default → the driver is inert.
	// See wired/bots/g_sequenced_goal.h.
	botSequencedGoal_t sequencedGoal;

	// ── Peripheral awareness (WCE sound ring) ─────────────────────────
	bot_sound_event_t heardSounds[MAX_BOT_SOUND_EVENTS];
	int               heardSoundCount;  // valid entries in heardSounds[] this think

	// Last-heard summary (highest-volume event this think or recent past)
	vec3_t            lastsoundpos;
	float             lastsoundtime;    // FloatTime() when set
	int               lastsoundentity;  // sourceClientNum of loudest event
	wce_event_type_t  lastsoundtype;
	float             lastsoundvolume;

	// Investigation target — set when bot hears but has no LOS
	vec3_t            investigatepos;
	float             investigatetime;  // FloatTime() when set; age out after ~3 s

	// ── AI composite tree ─────────────────────────────────────────────
	// Root of the composite tree that wraps the FSM dispatch. The root is a
	// PRIORITIZED composite whose highest-priority children are the three
	// cluster-exit guards (observer > intermission > dead), followed by the
	// live-selection leaf as the lowest-priority, always-eligible child that
	// ticks the current bs->ainode. The guards used to be re-checked as the
	// first statement inside each of the six nav/battle nodes; they are hoisted
	// here so the observer/intermission/dead exit is one top-level decision
	// instead of six copies. Each guard child is eligible only when the live
	// node is one of those six (AI_GuardApplies) AND its pure predicate holds,
	// so the top-level arbitration reproduces the old per-node prologue exactly
	// — a guard never fires while the live node is Observer/Intermission/
	// Respawn/Stand (those never carried the prologue). aiRootInit gates
	// one-time construction (BotResetState memsets bot_state_t to zero, so a
	// false value forces reconstruction after a reset — no stale pointers
	// survive).
	aiNode_t          aiRoot;                 // root composite (guards + live leaf)
	aiNode_t          aiLiveLeaf;             // leaf child that ticks the live bs->ainode (lowest priority)
	aiNode_t          aiObserverGuardLeaf;    // guard child 0: observer  -> AIEnter_Observer
	aiNode_t          aiIntermissionGuardLeaf;// guard child 1: intermission -> AIEnter_Intermission
	aiNode_t          aiDeadGuardLeaf;        // guard child 2: dead      -> AIEnter_Respawn
	qboolean          aiRootInit;             // qtrue once aiRoot + children are built

	// ── Fight/Chase visibility arbitration ────────────────────────────
	// A PRIORITIZED sub-composite that decides Battle_Fight vs Battle_Chase on
	// line-of-sight, using the substrate's incumbent-hysteresis to damp the
	// doorway/pillar single-tick occlusion flicker. Two children: Chase (higher
	// priority, eligible only when the enemy is currently unseen AND the bot
	// wants to chase) and Fight (lower priority, always-eligible fallback
	// incumbent). The hysteresis dwell holds Fight for the grace margin before
	// Chase can displace it — replacing the hand-rolled enemyvisible_time grace
	// for the Fight->Chase edge. Built lazily by AI_BuildRootComposite (cleared
	// by the reset-memset alongside aiRootInit, so no stale self-pointers).
	aiNode_t          fightChaseComposite;   // PRIORITIZED Fight/Chase arbiter
	aiNode_t          fightLeaf;             // Fight child (lower priority)
	aiNode_t          chaseLeaf;             // Chase child (higher priority)
	// The ainode value the sub-composite last drove. If bs->ainode differs from
	// this on entry, the node was changed out-of-band (a leaf edge such as
	// Retreat->Chase, or a battle re-entry from nav), so the stale hysteresis
	// dwell must be cleared — otherwise a challenger clock left over from a prior
	// engagement would skip the grace on re-entry.
	int             (*fightChaseLastNode)(struct bot_state_s *bs);
} bot_state_t;

extern bot_state_t *botstates[MAX_CLIENTS];

//resets the whole bot state
void BotResetState(bot_state_t *bs);
//returns the number of bots in the game
int NumBots(void);
//returns info about the entity
void BotEntityInfo(int entnum, aas_entityinfo_t *info);
float AngleDifference(float ang1, float ang2);

extern float floattime;
#define FloatTime() floattime

// ── AI composite tree substrate (see ai_dmnet.c) ──────────────────────────
// Unified tick for a leaf node: calls the wrapped AINode_X.
int  AI_LeafTick(bot_state_t *bs, aiNode_t *self);
// Unified tick for a composite node: selects + runs child(ren) per kind.
int  AI_CompositeTick(bot_state_t *bs, aiNode_t *self);
// Build the pass-through root that reproduces bs->ainode byte-identically.
void AI_BuildRootComposite(bot_state_t *bs);
// Tick the bot's composite root for this frame (byte-identical to bs->ainode(bs)).
int  AI_TickRoot(bot_state_t *bs);
// Composite-node helpers (used by the tree and the self-test).
void AI_CompositeInit(aiNode_t *node, aiCompositeKind_t kind, const char *name, float hysteresisMargin);
void AI_CompositeAddChild(aiNode_t *node, aiNode_t *child);
void AI_LeafInit(aiNode_t *node, int (*fn)(bot_state_t *bs), aiNodePre_t pre, const char *name);

// from the game source
void	QDECL BotAI_Print(int type, char *fmt, ...) FORMAT_PRINTF(2, 3);
void	QDECL QDECL BotAI_BotInitialChat( bot_state_t *bs, char *type, ... );
void	BotAI_Trace(bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask);
int		BotAI_GetClientState( int clientNum, playerState_t *state );
int		BotAI_GetEntityState( int entityNum, entityState_t *state );
int		BotAI_GetSnapshotEntity( int clientNum, int sequence, entityState_t *state );
int		BotTeamLeader(bot_state_t *bs);
